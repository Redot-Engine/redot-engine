/**************************************************************************/
/*  mcp_server.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "mcp_server.h"

#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "mcp_bridge.h"

#include <cstdio>
#include <iostream>
#include <string>

MCPServer *MCPServer::singleton = nullptr;

MCPServer::MCPServer() {
	singleton = this;
	protocol = memnew(MCPProtocol);
}

MCPServer::~MCPServer() {
	stop();

	// Wait for server loop to exit to prevent use-after-free of protocol
	uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	while (running) {
		if (OS::get_singleton()->get_ticks_msec() - start_time > 3000) { // 3s timeout
			fprintf(stderr, "[MCP] Server shutdown timed out\n");
			break;
		}
		OS::get_singleton()->delay_usec(1000);
	}

	if (protocol) {
		memdelete(protocol);
		protocol = nullptr;
	}
	singleton = nullptr;
}

void MCPServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start"), &MCPServer::start);
	ClassDB::bind_method(D_METHOD("stop"), &MCPServer::stop);
	ClassDB::bind_method(D_METHOD("is_running"), &MCPServer::is_running);
}

String MCPServer::_read_line() {
	std::string line;
	if (std::getline(std::cin, line)) {
		return String::utf8(line.c_str());
	}
	// EOF or error
	should_stop = true;
	return String();
}

void MCPServer::_write_line(const String &p_line) {
	CharString utf8 = p_line.utf8();
	fprintf(stdout, "%s\n", utf8.get_data());
	fflush(stdout);
}

void MCPServer::_bridge_thread_func(void *p_userdata) {
	MCPServer *ms = (MCPServer *)p_userdata;
	while (!ms->should_stop) {
		if (MCPBridge::get_singleton()) {
			MCPBridge::get_singleton()->update();
		}
		ms->_check_game_process();
		OS::get_singleton()->delay_usec(10000); // 10ms
	}
}

void MCPServer::_check_game_process() {
	MutexLock lock(process_mutex);
	if (game_pid != 0) {
		if (!OS::get_singleton()->is_process_running(game_pid)) {
			// Zombie reaper: In Godot/Redot, calling is_process_running internally calls waitpid with WNOHANG on Linux.
			// If it returns false, it means the process has exited and been reaped.
			fprintf(stderr, "[MCP] Game process %d exited.\n", (int)game_pid);
			game_pid = 0;
		}
	}
}

Error MCPServer::start_game_process(const List<String> &p_args, const String &p_log_path) {
	MutexLock lock(process_mutex);
	if (game_pid != 0) {
		lock.~MutexLock(); // Unlock before calling stop_game_process to avoid recursion deadlock if it locks too
		stop_game_process();
		process_mutex.lock(); // Relock
	}
	game_log_path = p_log_path;
	return OS::get_singleton()->create_process(OS::get_singleton()->get_executable_path(), p_args, &game_pid);
}

Error MCPServer::stop_game_process() {
	MutexLock lock(process_mutex);
	if (game_pid == 0) {
		return ERR_DOES_NOT_EXIST;
	}
	Error err = OS::get_singleton()->kill(game_pid);
	if (err == OK) {
		game_pid = 0; // cleared immediately, or wait for reaper? kill sends signal.
		// We should probably wait for it to exit or let reaper handle it.
		// For now, assume it will exit.
	}
	return err;
}

bool MCPServer::is_game_running() const {
	MutexLock lock(process_mutex);
	if (game_pid == 0) {
		return false;
	}
	return OS::get_singleton()->is_process_running(game_pid);
}

String MCPServer::get_game_log_path() const {
	MutexLock lock(process_mutex);
	return game_log_path;
}

void MCPServer::_server_loop() {
	// running = true; // Handled by start() CAS
	should_stop = false;

	// Start bridge thread
	bridge_thread.start(_bridge_thread_func, this);

	// Log startup to stderr (not stdout, which is for JSON-RPC)
	fprintf(stderr, "[MCP] Redot MCP Server started\n");
	fflush(stderr);

	while (!should_stop) {
		String line = _read_line();

		if (line.is_empty()) {
			if (should_stop) {
				break; // EOF reached
			}
			continue; // Empty line, skip
		}

		// Trim whitespace
		line = line.strip_edges();
		if (line.is_empty()) {
			continue;
		}

		// Process the JSON-RPC message
		if (protocol && !should_stop) {
			String response = protocol->process_string(line);

			// Only send response if there is one (notifications don't get responses)
			if (!response.is_empty()) {
				_write_line(response);
			}
		}
	}

	should_stop = true;
	bridge_thread.wait_to_finish();

	// running = false; // Handled by start() exit
	fprintf(stderr, "[MCP] Redot MCP Server stopped\n");
	fflush(stderr);
}

void MCPServer::start() {
	bool expected = false;
	if (!running.compare_exchange_strong(expected, true)) {
		return;
	}

	_server_loop();
	running = false;
}

void MCPServer::stop() {
	if (!running) {
		return;
	}

	should_stop = true;
}

void MCPServer::run_tests(const String &p_script_path) {
	fprintf(stderr, "[MCP] Running tests from: %s\n", p_script_path.utf8().get_data());

	Error err;
	Ref<Resource> res = ResourceLoader::load(p_script_path, "", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	if (err != OK || res.is_null()) {
		fprintf(stderr, "[MCP] Failed to load test script: %s (Error: %d)\n", p_script_path.utf8().get_data(), err);
		return;
	}

	Ref<Script> script = res;
	if (script.is_null()) {
		fprintf(stderr, "[MCP] Resource is not a script: %s\n", p_script_path.utf8().get_data());
		return;
	}

	Object *obj = ClassDB::instantiate(script->get_instance_base_type());
	if (!obj) {
		fprintf(stderr, "[MCP] Failed to instantiate base type: %s\n", String(script->get_instance_base_type()).utf8().get_data());
		return;
	}

	obj->set_script(script);

	ScriptInstance *si = obj->get_script_instance();
	if (si) {
		Callable::CallError ce;
		Variant ret = obj->callp("run", nullptr, 0, ce);

		if (ce.error == Callable::CallError::CALL_OK) {
			fprintf(stderr, "[MCP] Test finished. Return value: %s\n", ret.get_construct_string().utf8().get_data());
		} else if (ce.error == Callable::CallError::CALL_ERROR_INVALID_METHOD) {
			fprintf(stderr, "[MCP] Test script missing 'run()' method or script not fully initialized\n");
		} else {
			fprintf(stderr, "[MCP] Error calling 'run()': %d\n", ce.error);
		}
	} else {
		fprintf(stderr, "[MCP] Script instance could not be created for %s\n", p_script_path.utf8().get_data());
	}

	// If it's not refcounted, delete it.
	if (!Object::cast_to<RefCounted>(obj)) {
		memdelete(obj);
	}
}

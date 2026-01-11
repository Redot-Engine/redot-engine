/**************************************************************************/
/*  mcp_server.h                                                          */
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

#pragma once

#include "mcp_protocol.h"

#include "core/object/class_db.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/thread_safe.h"

#include <atomic>

class MCPServer : public Object {
	GDCLASS(MCPServer, Object)

private:
	static MCPServer *singleton;

	MCPProtocol *protocol = nullptr;
	std::atomic<bool> running = false;
	std::atomic<bool> should_stop = false;

	Thread bridge_thread;
	static void _bridge_thread_func(void *p_userdata);

	// Server loop - runs on main thread for now (headless mode)
	void _server_loop();

	// Read a line from stdin
	String _read_line();

	// Write a line to stdout
	void _write_line(const String &p_line);

protected:
	static void _bind_methods();

public:
	static MCPServer *get_singleton() { return singleton; }

	MCPServer();
	~MCPServer();

	// Start the MCP server (blocking call for headless mode)
	void start();

	// Stop the MCP server
	void stop();

	// Run unit tests headlessly
	static void run_tests(const String &p_script_path);

	// Check if server is running
	bool is_running() const { return running; }

	// Game Process Management
	Error start_game_process(const List<String> &p_args, const String &p_log_path);
	Error stop_game_process();
	bool is_game_running() const;
	String get_game_log_path() const;
	OS::ProcessID get_game_pid() const;

private:
	OS::ProcessID game_pid = 0;
	String game_log_path;
	mutable Mutex process_mutex; // Protects game_pid and game_log_path
	void _check_game_process(); // Reaper logic
	int wake_fds[2];
	String stdin_buffer;
};

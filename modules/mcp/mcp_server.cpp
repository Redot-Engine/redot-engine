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
#include "core/os/os.h"

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

void MCPServer::_server_loop() {
	running = true;
	should_stop = false;

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
		String response = protocol->process_string(line);

		// Only send response if there is one (notifications don't get responses)
		if (!response.is_empty()) {
			_write_line(response);
		}
	}

	running = false;
	fprintf(stderr, "[MCP] Redot MCP Server stopped\n");
	fflush(stderr);
}

void MCPServer::start() {
	if (running) {
		return;
	}

	_server_loop();
}

void MCPServer::stop() {
	if (!running) {
		return;
	}

	should_stop = true;
}

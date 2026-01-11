/**************************************************************************/
/*  mcp_bridge.h                                                          */
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

#include "core/io/stream_peer_tcp.h"
#include "core/io/tcp_server.h"
#include "core/object/class_db.h"
#include "core/variant/dictionary.h"

class MCPBridge : public Object {
	GDCLASS(MCPBridge, Object)

private:
	static MCPBridge *singleton;

	Ref<TCPServer> server;
	Ref<StreamPeerTCP> connection;

	bool is_host = false;
	int port = 0;
	String partial_data;

	// Internal command handling
	Dictionary _process_command(const Dictionary &p_cmd);
	void _trigger_action_event(const StringName &p_action);

protected:
	static void _bind_methods();

public:
	static MCPBridge *get_singleton() { return singleton; }

	MCPBridge();
	~MCPBridge();

	// Host (MCP Server) side
	Error start_server(int p_port = 0); // 0 = find available
	int get_port() const { return port; }
	bool is_client_connected() const;

	// Client (Game) side
	Error connect_to_server(const String &p_host, int p_port);

	// Communication
	Dictionary send_command(const String &p_action, const Dictionary &p_args = Dictionary());
	void update(); // Called by MainLoop or Server loop
};

/**************************************************************************/
/*  mcp_protocol.cpp                                                      */
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

/**
 * @file mcp_protocol.cpp
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "mcp_protocol.h"

#include "mcp_tools.h"

#include "core/io/json.h"

MCPProtocol::MCPProtocol() {
	tools = memnew(MCPTools);

	// Register MCP methods
	set_method("initialize", Callable(this, "_handle_initialize"));
	set_method("notifications/initialized", Callable(this, "_handle_initialized_notification"));
	set_method("ping", Callable(this, "_handle_ping"));
	set_method("tools/list", Callable(this, "_handle_tools_list"));
	set_method("tools/call", Callable(this, "_handle_tools_call"));
}

MCPProtocol::~MCPProtocol() {
	if (tools) {
		memdelete(tools);
		tools = nullptr;
	}
}

void MCPProtocol::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_initialized"), &MCPProtocol::is_initialized);

	ClassDB::bind_method(D_METHOD("_handle_initialize", "params"), &MCPProtocol::_handle_initialize, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("_handle_initialized_notification", "params"), &MCPProtocol::_handle_initialized_notification, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("_handle_ping", "params"), &MCPProtocol::_handle_ping, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("_handle_tools_list", "params"), &MCPProtocol::_handle_tools_list, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("_handle_tools_call", "params"), &MCPProtocol::_handle_tools_call, DEFVAL(Variant()));
}

Dictionary MCPProtocol::_make_capabilities() const {
	Dictionary caps;
	Dictionary tools_cap;
	tools_cap["listChanged"] = false;
	caps["tools"] = tools_cap;
	return caps;
}

Dictionary MCPProtocol::_make_server_info() const {
	Dictionary info;
	info["name"] = MCP_SERVER_NAME;
	info["version"] = MCP_SERVER_VERSION;
	return info;
}

String MCPProtocol::_negotiate_version(const String &p_client_version) {
	if (p_client_version == MCP_PROTOCOL_VERSION_LATEST || p_client_version == MCP_PROTOCOL_VERSION_LEGACY) {
		return p_client_version;
	}
	return MCP_PROTOCOL_VERSION_LATEST;
}

Variant MCPProtocol::_handle_initialize(const Variant &p_params) {
	String client_version;
	if (p_params.get_type() == Variant::DICTIONARY) {
		Dictionary params = p_params;
		Variant pv = params.get("protocolVersion", Variant());
		if (pv.get_type() == Variant::STRING) {
			client_version = pv;
		}
	}

	Dictionary result;
	result["protocolVersion"] = _negotiate_version(client_version);
	result["capabilities"] = _make_capabilities();
	result["serverInfo"] = _make_server_info();
	return result;
}

Array MCPProtocol::_get_tool_definitions() const {
	return MCPTools::get_tool_definitions();
}

Variant MCPProtocol::_handle_tools_list(const Variant &p_params) {
	// Defense-in-depth: process_string gates this, but guard anyway in case
	// dispatch bypasses the override (e.g. via base pointer or a notification).
	if (!initialized) {
		Dictionary result;
		result["tools"] = Array();
		return result;
	}
	Dictionary result;
	result["tools"] = _get_tool_definitions();
	return result;
}

Variant MCPProtocol::_handle_tools_call(const Variant &p_params) {
	// Defense-in-depth: never execute a tool before the init handshake.
	if (!initialized) {
		return _make_tool_result_error("Protocol not initialized");
	}
	Dictionary params;
	if (p_params.get_type() == Variant::DICTIONARY) {
		params = p_params;
	} else if (p_params.get_type() == Variant::ARRAY) {
		Array arr = p_params;
		if (!arr.is_empty() && arr[0].get_type() == Variant::DICTIONARY) {
			params = arr[0];
		} else {
			return _make_tool_result_error("Tool call parameters must be an object (or array with object)");
		}
	} else {
		return _make_tool_result_error("Tool call parameters must be an object");
	}

	String tool_name = params.get("name", "");
	if (tool_name.is_empty()) {
		return _make_tool_result_error("Missing tool name");
	}

	Dictionary arguments;
	if (params.has("arguments")) {
		Variant args_var = params["arguments"];
		if (args_var.get_type() == Variant::DICTIONARY) {
			arguments = args_var;
		}
	}

	// Execute the tool
	MCPTools::ToolResult result = tools->execute_tool(tool_name, arguments);

	return _make_tool_result(result.content, !result.success);
}

Dictionary MCPProtocol::_make_tool_result(const Array &p_content, bool p_is_error) const {
	Dictionary result;
	result["content"] = p_content;
	if (p_is_error) {
		result["isError"] = true;
	}
	return result;
}

Dictionary MCPProtocol::_make_tool_result_error(const String &p_message) const {
	Dictionary content;
	content["type"] = "text";
	content["text"] = "Error: " + p_message;

	Array content_arr;
	content_arr.push_back(content);

	Dictionary result;
	result["content"] = content_arr;
	result["isError"] = true;
	return result;
}

Variant MCPProtocol::_handle_initialized_notification(const Variant &p_params) {
	// Per spec, the operation phase begins after this notification.
	initialized = true;
	return Variant();
}

Variant MCPProtocol::_handle_ping(const Variant &p_params) {
	return Dictionary(); // Empty object
}

String MCPProtocol::process_string(const String &p_input) {
	// Gate tools/* requests before initialization at the protocol level,
	// returning a proper JSON-RPC error (not a double-wrapped result).
	// Notifications (no id) are silently dropped so the tool never executes.
	if (!initialized && !p_input.is_empty()) {
		JSON json;
		if (json.parse(p_input) == OK) {
			Variant data = json.get_data();
			if (data.get_type() == Variant::DICTIONARY) {
				Dictionary req = data;
				String method = req.get("method", "");
				if (method == "tools/list" || method == "tools/call") {
					if (req.has("id")) {
						Variant id = req["id"];
						if (id.get_type() == Variant::FLOAT && id.operator float() == (float)(id.operator int())) {
							id = id.operator int();
						}
						Dictionary resp = make_response_error(INVALID_REQUEST, "Protocol not initialized", id);
						return JSON::stringify(resp);
					}
					// Notification before init: drop silently (no response permitted).
					return String();
				}
			}
		}
	}
	return JSONRPC::process_string(p_input);
}

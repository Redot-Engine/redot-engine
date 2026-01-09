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
	// We support tools
	Dictionary tools_cap;
	caps["tools"] = tools_cap;
	return caps;
}

Dictionary MCPProtocol::_make_server_info() const {
	Dictionary info;
	info["name"] = MCP_SERVER_NAME;
	info["version"] = MCP_SERVER_VERSION;
	return info;
}

Variant MCPProtocol::_handle_initialize(const Variant &p_params) {
	Dictionary result;
	result["protocolVersion"] = MCP_PROTOCOL_VERSION;
	result["capabilities"] = _make_capabilities();
	result["serverInfo"] = _make_server_info();

	initialized = true;
	return result;
}

Array MCPProtocol::_get_tool_definitions() const {
	return MCPTools::get_tool_definitions();
}

Variant MCPProtocol::_handle_tools_list(const Variant &p_params) {
	Dictionary result;
	result["tools"] = _get_tool_definitions();
	return result;
}

Variant MCPProtocol::_handle_tools_call(const Variant &p_params) {
	Dictionary params;
	if (p_params.get_type() == Variant::DICTIONARY) {
		params = p_params;
	} else if (p_params.get_type() == Variant::ARRAY) {
		Array arr = p_params;
		if (!arr.is_empty() && arr[0].get_type() == Variant::DICTIONARY) {
			params = arr[0];
		} else {
			return make_response_error(INVALID_PARAMS, "Tool call parameters must be an object (or array with object)");
		}
	} else {
		return make_response_error(INVALID_PARAMS, "Tool call parameters must be an object");
	}

	String tool_name = params.get("name", "");
	if (tool_name.is_empty()) {
		return make_response_error(INVALID_PARAMS, "Missing tool name");
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

Dictionary MCPProtocol::_make_text_content(const String &p_text) const {
	Dictionary content;
	content["type"] = "text";
	content["text"] = p_text;
	return content;
}

Variant MCPProtocol::_handle_initialized_notification(const Variant &p_params) {
	// Notification, no return value needed (but Variant() is void)
	return Variant();
}

Variant MCPProtocol::_handle_ping(const Variant &p_params) {
	return Dictionary(); // Empty object
}

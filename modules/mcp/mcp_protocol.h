/**************************************************************************/
/*  mcp_protocol.h                                                        */
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

#include "mcp_types.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"
#include "modules/jsonrpc/jsonrpc.h"

class MCPTools;

class MCPProtocol : public JSONRPC {
	GDCLASS(MCPProtocol, JSONRPC)

private:
	bool initialized = false;
	MCPTools *tools = nullptr;

	// MCP method handlers
	Variant _handle_initialize(const Variant &p_params);
	Variant _handle_tools_list(const Variant &p_params);
	Variant _handle_tools_call(const Variant &p_params);
	Variant _handle_initialized_notification(const Variant &p_params);
	Variant _handle_ping(const Variant &p_params);

	// Helper methods
	Dictionary _make_capabilities() const;
	Dictionary _make_server_info() const;
	Array _get_tool_definitions() const;

	// Make MCP-formatted tool result
	Dictionary _make_tool_result(const Array &p_content, bool p_is_error = false) const;
	Dictionary _make_text_content(const String &p_text) const;

protected:
	static void _bind_methods();

public:
	MCPProtocol();
	~MCPProtocol();

	bool is_initialized() const { return initialized; }
};

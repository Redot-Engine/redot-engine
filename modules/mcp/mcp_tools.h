/**************************************************************************/
/*  mcp_tools.h                                                           */
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
#include "core/object/object.h"
#include "core/os/os.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class MCPTools : public Object {
	GDCLASS(MCPTools, Object)

public:
	// Result structure for tool execution
	struct ToolResult {
		bool success = true;
		String error_message;
		Array content; // Array of content items (text, image, resource)

		void add_text(const String &p_text) {
			Dictionary d;
			d["type"] = "text";
			d["text"] = p_text;
			content.push_back(d);
		}

		void set_error(const String &p_message) {
			success = false;
			error_message = p_message;
			content.clear();
			add_text("Error: " + p_message);
		}
	};

private:
	// Path utilities
	static String normalize_path(const String &p_path);
	static bool validate_path(const String &p_path);
	static String get_absolute_path(const String &p_path);

protected:
	static void _bind_methods();

public:
	MCPTools();
	~MCPTools();

	// Get all tool definitions
	static Array get_tool_definitions();

	// Execute a tool by name
	ToolResult execute_tool(const String &p_name, const Dictionary &p_arguments);

	// === Master Controllers ===
	ToolResult tool_scene_action(const Dictionary &p_args);
	ToolResult tool_resource_action(const Dictionary &p_args);
	ToolResult tool_code_intel(const Dictionary &p_args);
	ToolResult tool_project_config(const Dictionary &p_args);
	ToolResult tool_game_control(const Dictionary &p_args);

private:
	// Helpers
	static Variant _json_to_variant(const Variant &p_json, Variant::Type p_type = Variant::NIL);
	static Error _ensure_callback_exists(const String &p_script_path, const String &p_callback_name);

	static OS::ProcessID last_game_pid;
	static String last_log_path;
	static int bridge_port;
};

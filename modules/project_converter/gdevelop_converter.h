/**************************************************************************/
/*  gdevelop_converter.h                                                  */
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

#ifdef TOOLS_ENABLED

#include "core/error/error_list.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

// Converts a GDevelop (.gdg or .json) project to Godot scenes (.tscn) with
// GDScript logic derived from GDevelop event sheets.
class GDevelopConverter {
public:
	// Parse p_gdg_path and write converted scenes/scripts to p_output_dir.
	static Error convert(const String &p_gdg_path, const String &p_output_dir);

private:
	// Translate an array of GDevelop events into GDScript text
	static String _events_to_gdscript(const Array &p_events, const Dictionary &p_project);
	// Translate a single event Dictionary into GDScript statement(s)
	static String _event_to_gdscript(const Dictionary &p_event, int p_indent);
	// Translate a GDevelop condition into a GDScript boolean expression
	static String _condition_to_expr(const Dictionary &p_condition);
	// Translate a GDevelop action into a GDScript statement
	static String _action_to_stmt(const Dictionary &p_action, int p_indent);
	// Build .tscn text for one GDevelop layout
	static String _generate_layout_tscn(const Dictionary &p_layout, const String &p_base_dir, const String &p_script_path);
	// Copy resource files referenced by the project into p_output_dir
	static Error _copy_resources(const String &p_project_dir, const String &p_output_dir, const Array &p_resources);
};

#endif // TOOLS_ENABLED

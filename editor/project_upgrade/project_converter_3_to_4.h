/**************************************************************************/
/*  project_converter_3_to_4.h                                            */
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
 * @file project_converter_3_to_4.h
 *
 * [Add any documentation that applies to the entire file here!]
 */

#pragma once

#ifndef DISABLE_DEPRECATED

#include "core/string/ustring.h"
#include "core/templates/local_vector.h"
#include "core/templates/vector.h"

struct SourceLine {
	String line;
	bool is_comment;
};

class RegEx;

class ProjectConverter3To4 {
	class RegExContainer;

	uint64_t maximum_file_size;
	uint64_t maximum_line_length;

	/// In godot4, "tool" became "@tool" and must be located at the top of the file.
	void fix_tool_declaration(Vector<SourceLine> &source_lines, const RegExContainer &reg_container);
	/// In Godot 3, the pause_mode 2 equals the PAUSE_MODE_PROCESS value.
	/// In Godot 4, the pause_mode PAUSE_MODE_PROCESS was renamed to PROCESS_MODE_ALWAYS and equals the number 3.
	/// We therefore convert pause_mode = 2 to pause_mode = 3.
	void fix_pause_mode(Vector<SourceLine> &source_lines, const RegExContainer &reg_container);

	void rename_colors(Vector<SourceLine> &source_lines, const RegExContainer &reg_container);
	/// Convert hexadecimal colors from ARGB to RGBA
	void convert_hexadecimal_colors(Vector<SourceLine> &source_lines, const RegExContainer &reg_container);
	Vector<String> check_for_rename_colors(Vector<String> &lines, const RegExContainer &reg_container);

	void rename_classes(Vector<SourceLine> &source_lines, const RegExContainer &reg_container);
	Vector<String> check_for_rename_classes(Vector<String> &lines, const RegExContainer &reg_container);

	void rename_gdscript_functions(Vector<SourceLine> &source_lines, const RegExContainer &reg_container, bool builtin);
	Vector<String> check_for_rename_gdscript_functions(Vector<String> &lines, const RegExContainer &reg_container, bool builtin);
	void process_gdscript_line(String &line, const RegExContainer &reg_container, bool builtin);

	void rename_csharp_functions(Vector<SourceLine> &source_lines, const RegExContainer &reg_container);
	Vector<String> check_for_rename_csharp_functions(Vector<String> &lines, const RegExContainer &reg_container);
	void process_csharp_line(String &line, const RegExContainer &reg_container);

	void rename_csharp_attributes(Vector<SourceLine> &source_lines, const RegExContainer &reg_container);
	Vector<String> check_for_rename_csharp_attributes(Vector<String> &lines, const RegExContainer &reg_container);

	void rename_gdscript_keywords(Vector<SourceLine> &r_source_lines, const RegExContainer &p_reg_container, bool p_builtin);
	Vector<String> check_for_rename_gdscript_keywords(Vector<String> &r_lines, const RegExContainer &p_reg_container, bool p_builtin);

	void rename_input_map_scancode(Vector<SourceLine> &source_lines, const RegExContainer &reg_container);
	Vector<String> check_for_rename_input_map_scancode(Vector<String> &lines, const RegExContainer &reg_container);

	void rename_joypad_buttons_and_axes(Vector<SourceLine> &source_lines, const RegExContainer &reg_container);
	Vector<String> check_for_rename_joypad_buttons_and_axes(Vector<String> &lines, const RegExContainer &reg_container);

	void custom_rename(Vector<SourceLine> &source_lines, const String &from, const String &to);
	Vector<String> check_for_custom_rename(Vector<String> &lines, const String &from, const String &to);

	void rename_common(const char *array[][2], LocalVector<RegEx *> &cached_regexes, Vector<SourceLine> &source_lines);
	Vector<String> check_for_rename_common(const char *array[][2], LocalVector<RegEx *> &cached_regexes, Vector<String> &lines);

	/// Collect files which will be checked, excluding ".txt", ".mp4", ".wav" etc. files.
	Vector<String> check_for_files();

	/// Returns arguments from given function execution, this cannot be really done as regex.
	/// `abc(d,e(f,g),h)` -> [d], [e(f,g)], [h]
	Vector<String> parse_arguments(const String &line);
	/// Finds latest parenthesis owned by function.
	/// `function(abc(a,b),DD)):` finds this parenthess `function(abc(a,b),DD => ) <= ):`
	int get_end_parenthesis(const String &line) const;
	/// Merges multiple arguments into a single String.
	/// Needed when after processing e.g. 2 arguments, later arguments are not changed in any way.
	String connect_arguments(const Vector<String> &line, int from, int to = -1) const;
	/// Returns the indentation (spaces and tabs) at the start of the line e.g. `\t\tmove_this` returns `\t\t`.
	String get_starting_space(const String &line) const;
	/// Returns the object that’s executing the function in the line.
	/// e.g. Passing the line "var roman = kieliszek.funkcja()" to this function returns "kieliszek".
	String get_object_of_execution(const String &line) const;
	bool contains_function_call(const String &line, const String &function) const;

	/// Prints full info about renamed things e.g.:
	/// Line (67) remove -> remove_at  -  LINE """ doubler._blacklist.remove(0) """
	String line_formatter(int current_line, String from, String to, String line);
	/// Prints only full lines e.g.:
	/// Line (1) - FULL LINES - """yield(get_tree().create_timer(3), 'timeout')"""  =====>  """ await get_tree().create_timer(3).timeout """
	String simple_line_formatter(int current_line, String old_line, String line);
	/// Collects string from vector strings
	String collect_string_from_vector(Vector<SourceLine> &vector);
	Vector<SourceLine> split_lines(const String &text);

	/// Validates the array to prevent cyclic renames, such as `Node` -> `Node2D`, then `Node2D` -> `2DNode`.
	/// Also checks if names contain leading or trailing spaces.
	bool test_single_array(const char *array[][2], bool ignore_second_check = false);
	/// Test expected results of gdscript
	bool test_conversion_gdscript_builtin(const String &name, const String &expected, void (ProjectConverter3To4::*func)(Vector<SourceLine> &, const RegExContainer &, bool), const String &what, const RegExContainer &reg_container, bool builtin);
	bool test_conversion_with_regex(const String &name, const String &expected, void (ProjectConverter3To4::*func)(Vector<SourceLine> &, const RegExContainer &), const String &what, const RegExContainer &reg_container);
	bool test_conversion_basic(const String &name, const String &expected, const char *array[][2], LocalVector<RegEx *> &regex_cache, const String &what);
	/// Validate in all arrays if names don't do cyclic renames "Node" -> "Node2D" | "Node2D" -> "2DNode"
	bool test_array_names();
	/// Validate if conversions are proper.
	bool test_conversion(RegExContainer &reg_container);

public:
	ProjectConverter3To4(int, int);
	/// Function responsible for validating project conversion.
	bool validate_conversion();
	/// Function responsible for converting project.
	bool convert();
};

#endif // DISABLE_DEPRECATED

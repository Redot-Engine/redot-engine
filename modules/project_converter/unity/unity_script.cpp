/**************************************************************************/
/*  unity_script.cpp                                                      */
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

#include "unity_script.h"

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

namespace {

String _tabs(int p_count) {
	String s;
	for (int i = 0; i < p_count; i++) {
		s += "\t";
	}
	return s;
}

bool _is_identifier_char(char32_t p_char) {
	return (p_char >= 'a' && p_char <= 'z') || (p_char >= 'A' && p_char <= 'Z') || (p_char >= '0' && p_char <= '9') || p_char == '_';
}

int _find_top_level(const String &p_text, char32_t p_char, int p_start = 0) {
	int depth = 0;
	bool in_single = false;
	bool in_double = false;
	for (int i = p_start; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if (in_double) {
			if (c == '\\') {
				i++;
			} else if (c == '"') {
				in_double = false;
			}
			continue;
		}
		if (in_single) {
			if (c == '\'') {
				in_single = false;
			}
			continue;
		}
		if (c == '"') {
			in_double = true;
		} else if (c == '\'') {
			in_single = true;
		} else if (c == '(' || c == '[' || c == '{') {
			depth++;
		} else if (c == ')' || c == ']' || c == '}') {
			depth--;
		} else if (c == p_char && depth == 0) {
			return i;
		}
	}
	return -1;
}

Array _split_top_level(const String &p_text, char32_t p_separator) {
	Array result;
	String current;
	int depth = 0;
	bool in_single = false;
	bool in_double = false;
	for (int i = 0; i < p_text.length(); i++) {
		const char32_t c = p_text[i];
		if (in_double) {
			current += c;
			if (c == '\\' && i + 1 < p_text.length()) {
				current += p_text[++i];
			} else if (c == '"') {
				in_double = false;
			}
			continue;
		}
		if (in_single) {
			current += c;
			if (c == '\'') {
				in_single = false;
			}
			continue;
		}
		if (c == '"') {
			in_double = true;
			current += c;
		} else if (c == '\'') {
			in_single = true;
			current += c;
		} else if (c == '(' || c == '[' || c == '{') {
			depth++;
			current += c;
		} else if (c == ')' || c == ']' || c == '}') {
			depth--;
			current += c;
		} else if (c == p_separator && depth == 0) {
			result.push_back(current.strip_edges());
			current = String();
		} else {
			current += c;
		}
	}
	if (!current.strip_edges().is_empty()) {
		result.push_back(current.strip_edges());
	}
	return result;
}

String _strip_attributes(const String &p_line) {
	String line = p_line;
	while (true) {
		const int start = line.find("[");
		if (start == -1) {
			break;
		}
		const int end = line.find("]", start);
		if (end == -1) {
			break;
		}
		line = line.substr(0, start) + line.substr(end + 1);
	}
	return line.strip_edges();
}

String _remove_modifiers(const String &p_text) {
	static const char *modifiers[] = {
		"public", "private", "protected", "internal", "static", "virtual", "override",
		"abstract", "sealed", "async", "extern", "unsafe", "new", "partial", "readonly",
		"const", "volatile"
	};
	String result = p_text;
	for (const char *modifier : modifiers) {
		const String token = String(modifier) + " ";
		while (result.begins_with(token)) {
			result = result.substr(token.length());
		}
		while (result.contains(" " + token)) {
			result = result.replace(" " + token, " ");
		}
	}
	return result.strip_edges();
}

String _map_type(const String &p_type) {
	String type = p_type.strip_edges();
	if (type.ends_with("?")) {
		type = type.substr(0, type.length() - 1);
	}
	if (type.begins_with("List<") || type.begins_with("IList<") || type.begins_with("IEnumerable<")) {
		return "Array";
	}
	if (type.ends_with("[]")) {
		return "Array";
	}
	if (type == "int" || type == "long" || type == "short" || type == "byte" || type == "uint") {
		return "int";
	}
	if (type == "float" || type == "double" || type == "decimal") {
		return "float";
	}
	if (type == "bool") {
		return "bool";
	}
	if (type == "string" || type == "String") {
		return "String";
	}
	if (type == "Vector2") {
		return "Vector2";
	}
	if (type == "Vector3") {
		return "Vector3";
	}
	if (type == "Vector4") {
		return "Vector4";
	}
	if (type == "Quaternion") {
		return "Quaternion";
	}
	if (type == "Color") {
		return "Color";
	}
	if (type == "GameObject" || type == "Transform" || type == "MonoBehaviour") {
		return "Node";
	}
	if (type == "Rigidbody") {
		return "RigidBody3D";
	}
	if (type == "AudioSource") {
		return "AudioStreamPlayer3D";
	}
	if (type == "Animator") {
		return "AnimationPlayer";
	}
	if (type == "Camera") {
		return "Camera3D";
	}
	if (type == "Light") {
		return "Light3D";
	}
	if (type == "Collider") {
		return "CollisionShape3D";
	}
	return type;
}

String _default_value_for_type(const String &p_type) {
	if (p_type == "int") {
		return "0";
	}
	if (p_type == "float") {
		return "0.0";
	}
	if (p_type == "bool") {
		return "false";
	}
	if (p_type == "String") {
		return "\"\"";
	}
	if (p_type == "Vector2") {
		return "Vector2.ZERO";
	}
	if (p_type == "Vector3") {
		return "Vector3.ZERO";
	}
	if (p_type == "Vector4") {
		return "Vector4.ZERO";
	}
	if (p_type == "Quaternion") {
		return "Quaternion.IDENTITY";
	}
	if (p_type == "Color") {
		return "Color.WHITE";
	}
	if (p_type == "Array") {
		return "[]";
	}
	return "null";
}

String _convert_interpolated(const String &p_expr) {
	String result;
	int i = 0;
	while (i < p_expr.length()) {
		if (p_expr[i] == '$' && i + 1 < p_expr.length() && p_expr[i + 1] == '"') {
			int j = i + 2;
			String literal;
			Array parts;
			while (j < p_expr.length() && p_expr[j] != '"') {
				if (p_expr[j] == '\\' && j + 1 < p_expr.length()) {
					literal += p_expr[j];
					literal += p_expr[j + 1];
					j += 2;
					continue;
				}
				if (p_expr[j] == '{') {
					int end = j + 1;
					int depth = 1;
					while (end < p_expr.length() && depth > 0) {
						if (p_expr[end] == '{') {
							depth++;
						} else if (p_expr[end] == '}') {
							depth--;
						}
						end++;
					}
					if (!literal.is_empty()) {
						parts.push_back("\"" + literal + "\"");
						literal = String();
					}
					parts.push_back("str(" + p_expr.substr(j + 1, end - j - 2).strip_edges() + ")");
					j = end;
					continue;
				}
				literal += p_expr[j];
				j++;
			}
			if (!literal.is_empty() || parts.is_empty()) {
				parts.push_back("\"" + literal + "\"");
			}
			if (parts.size() == 1) {
				result += parts[0];
			} else {
				result += "(";
				for (int p = 0; p < parts.size(); p++) {
					if (p > 0) {
						result += " + ";
					}
					result += String(parts[p]);
				}
				result += ")";
			}
			i = (j < p_expr.length()) ? j + 1 : j;
			continue;
		}
		result += p_expr[i];
		i++;
	}
	return result;
}

String _convert_ternary(const String &p_expr) {
	const int question = _find_top_level(p_expr, '?');
	if (question == -1) {
		return p_expr;
	}
	const int colon = _find_top_level(p_expr, ':', question + 1);
	if (colon == -1) {
		return p_expr;
	}
	const String condition = p_expr.substr(0, question).strip_edges();
	const String when_true = p_expr.substr(question + 1, colon - question - 1).strip_edges();
	const String when_false = p_expr.substr(colon + 1).strip_edges();
	return vformat("(%s if %s else %s)", when_true, condition, when_false);
}

String _convert_generic_call(const String &p_expr, const String &p_csharp_name, const String &p_godot_name, bool p_find_child = false) {
	String result = p_expr;
	const String needle = p_csharp_name + "<";
	int start = 0;
	while (true) {
		const int pos = result.find(needle, start);
		if (pos == -1) {
			break;
		}
		int depth = 0;
		int end = pos + needle.length() - 1;
		for (; end < result.length(); end++) {
			if (result[end] == '<') {
				depth++;
			} else if (result[end] == '>') {
				depth--;
				if (depth == 0) {
					break;
				}
			}
		}
		if (end >= result.length()) {
			break;
		}
		const String type = result.substr(pos + needle.length(), end - pos - needle.length()).strip_edges();
		const String replacement = p_find_child
				? vformat("%s(\"%s\", true, false)", p_godot_name, type)
				: vformat("%s(\"%s\")", p_godot_name, type);
		result = result.substr(0, pos) + replacement + result.substr(end + 1);
		start = pos + replacement.length();
	}
	return result;
}

String _convert_expression(const String &p_expr) {
	String expr = p_expr;

	expr = _convert_interpolated(expr);
	expr = _convert_ternary(expr);

	expr = _convert_generic_call(expr, "GetComponentInChildren", "find_child", true);
	expr = _convert_generic_call(expr, "GetComponent", "get_node_or_null");
	expr = _convert_generic_call(expr, "FindObjectOfType", "get_tree().get_first_node_in_group");

	// Constructors and object creation.
	expr = expr.replace("new Vector2(", "Vector2(");
	expr = expr.replace("new Vector3(", "Vector3(");
	expr = expr.replace("new Vector4(", "Vector4(");
	expr = expr.replace("new Quaternion(", "Quaternion(");
	expr = expr.replace("new Color(", "Color(");
	// `new List<T>()` / `new T[]` -> `[]`
	while (true) {
		const int list_pos = expr.find("new List<");
		if (list_pos == -1) {
			break;
		}
		int depth = 0;
		int end = list_pos + 8;
		for (; end < expr.length(); end++) {
			if (expr[end] == '<') {
				depth++;
			} else if (expr[end] == '>') {
				depth--;
				if (depth == 0) {
					break;
				}
			}
		}
		if (end >= expr.length()) {
			break;
		}
		int tail = end + 1;
		while (tail < expr.length() && expr[tail] == ' ') {
			tail++;
		}
		if (tail + 1 < expr.length() && expr[tail] == '(' && expr[tail + 1] == ')') {
			tail += 2;
		}
		expr = expr.substr(0, list_pos) + "[]" + expr.substr(tail);
	}
	expr = expr.replace("new ", "");

	// Unity API -> Godot API.
	expr = expr.replace("Debug.LogWarning(", "push_warning(");
	expr = expr.replace("Debug.LogError(", "push_error(");
	expr = expr.replace("Debug.Log(", "print(");
	expr = expr.replace("Debug.Assert(", "assert(");
	expr = expr.replace("Debug.DrawLine(", "# Debug.DrawLine(");

	expr = expr.replace("Time.deltaTime", "delta");
	expr = expr.replace("Time.fixedDeltaTime", "delta");
	expr = expr.replace("Time.unscaledDeltaTime", "delta");
	expr = expr.replace("Time.time", "(Time.get_ticks_msec() / 1000.0)");
	expr = expr.replace("Time.timeScale", "Engine.time_scale");
	expr = expr.replace("Time.fixedTime", "(Time.get_ticks_msec() / 1000.0)");

	expr = expr.replace("Mathf.Abs(", "abs(");
	expr = expr.replace("Mathf.Clamp01(", "clampf(");
	expr = expr.replace("Mathf.Clamp(", "clampf(");
	expr = expr.replace("Mathf.Lerp(", "lerpf(");
	expr = expr.replace("Mathf.Min(", "minf(");
	expr = expr.replace("Mathf.Max(", "maxf(");
	expr = expr.replace("Mathf.Sin(", "sin(");
	expr = expr.replace("Mathf.Cos(", "cos(");
	expr = expr.replace("Mathf.Tan(", "tan(");
	expr = expr.replace("Mathf.Sqrt(", "sqrt(");
	expr = expr.replace("Mathf.Pow(", "pow(");
	expr = expr.replace("Mathf.Round(", "roundf(");
	expr = expr.replace("Mathf.Floor(", "floorf(");
	expr = expr.replace("Mathf.Ceil(", "ceilf(");
	expr = expr.replace("Mathf.MoveTowards(", "move_toward(");
	expr = expr.replace("Mathf.Deg2Rad", "DEG_TO_RAD");
	expr = expr.replace("Mathf.Rad2Deg", "RAD_TO_DEG");
	expr = expr.replace("Mathf.PI", "PI");
	expr = expr.replace("Mathf.Infinity", "INF");

	expr = expr.replace("Vector3.zero", "Vector3.ZERO");
	expr = expr.replace("Vector3.one", "Vector3.ONE");
	expr = expr.replace("Vector3.forward", "Vector3.FORWARD");
	expr = expr.replace("Vector3.back", "Vector3.BACK");
	expr = expr.replace("Vector3.up", "Vector3.UP");
	expr = expr.replace("Vector3.down", "Vector3.DOWN");
	expr = expr.replace("Vector3.left", "Vector3.LEFT");
	expr = expr.replace("Vector3.right", "Vector3.RIGHT");
	expr = expr.replace("Vector2.zero", "Vector2.ZERO");
	expr = expr.replace("Vector2.one", "Vector2.ONE");
	expr = expr.replace("Vector2.up", "Vector2.UP");
	expr = expr.replace("Vector2.down", "Vector2.DOWN");
	expr = expr.replace("Vector2.left", "Vector2.LEFT");
	expr = expr.replace("Vector2.right", "Vector2.RIGHT");
	expr = expr.replace("Quaternion.identity", "Quaternion.IDENTITY");
	expr = expr.replace("Quaternion.Euler(", "Quaternion.from_euler(");
	expr = expr.replace("Quaternion.LookRotation(", "Basis.looking_at(");
	expr = expr.replace("Quaternion.Slerp(", "Quaternion.IDENTITY.slerp(");

	expr = expr.replace("transform.localPosition", "position");
	expr = expr.replace("transform.localRotation", "quaternion");
	expr = expr.replace("transform.localScale", "scale");
	expr = expr.replace("transform.position", "position");
	expr = expr.replace("transform.rotation", "quaternion");
	expr = expr.replace("transform.forward", "-basis.z");
	expr = expr.replace("transform.right", "basis.x");
	expr = expr.replace("transform.up", "basis.y");
	expr = expr.replace("transform.Translate(", "translate(");
	expr = expr.replace("transform.Rotate(", "rotate(");
	expr = expr.replace("transform.LookAt(", "look_at(");
	expr = expr.replace("transform.localEulerAngles", "rotation");
	expr = expr.replace("transform.eulerAngles", "global_rotation");

	expr = expr.replace("gameObject.SetActive(", "set_active(");
	expr = expr.replace("gameObject.activeSelf", "visible");
	expr = expr.replace("gameObject.name", "name");
	expr = expr.replace(".SetActive(", ".set_active(");
	expr = expr.replace(".activeSelf", ".visible");
	expr = expr.replace(".enabled", ".visible");

	expr = expr.replace("Destroy(", "free_object(");
	expr = expr.replace("Instantiate(", "instantiate_object(");
	expr = expr.replace("FindObjectOfType<", "find_object_of_type<");
	expr = expr.replace("Application.Quit()", "get_tree().quit()");
	expr = expr.replace("SceneManager.LoadScene(", "get_tree().change_scene_to_file(");

	expr = expr.replace("Input.GetAxisRaw(\"Horizontal\")", "Input.get_axis(\"ui_left\", \"ui_right\")");
	expr = expr.replace("Input.GetAxis(\"Horizontal\")", "Input.get_axis(\"ui_left\", \"ui_right\")");
	expr = expr.replace("Input.GetAxisRaw(\"Vertical\")", "Input.get_axis(\"ui_up\", \"ui_down\")");
	expr = expr.replace("Input.GetAxis(\"Vertical\")", "Input.get_axis(\"ui_up\", \"ui_down\")");
	expr = expr.replace("Input.GetKeyDown(", "Input.is_key_pressed(");
	expr = expr.replace("Input.GetKeyUp(", "Input.is_key_pressed(");
	expr = expr.replace("Input.GetKey(", "Input.is_key_pressed(");
	expr = expr.replace("Input.GetMouseButtonDown(", "Input.is_mouse_button_pressed(");
	expr = expr.replace("Input.GetMouseButtonUp(", "Input.is_mouse_button_pressed(");
	expr = expr.replace("Input.GetMouseButton(", "Input.is_mouse_button_pressed(");
	expr = expr.replace("KeyCode.", "KEY_");
	expr = expr.replace("MOUSE_BUTTON_LEFT", "MOUSE_BUTTON_LEFT");
	expr = expr.replace("Random.Range(", "randf_range(");
	expr = expr.replace("Random.value", "randf()");
	expr = expr.replace("PlayerPrefs.", "# PlayerPrefs.");
	expr = expr.replace("Physics.Raycast(", "# Physics.Raycast(");

	expr = expr.replace("string.IsNullOrEmpty(", "is_empty_string(");
	expr = expr.replace(".Length", ".size()");
	expr = expr.replace(".Count", ".size()");
	expr = expr.replace("this.", "");
	expr = expr.replace("base.", "super.");
	expr = expr.replace("null", "null");

	// Operators.
	expr = expr.replace("!=", "__REDOT_NE__");
	expr = expr.replace(" && ", " and ");
	expr = expr.replace(" || ", " or ");
	expr = expr.replace("!", "not ");
	expr = expr.replace("__REDOT_NE__", "!=");

	return expr.strip_edges();
}

String _convert_condition(const String &p_condition) {
	String condition = p_condition.strip_edges();
	while (condition.begins_with("(") && condition.ends_with(")")) {
		condition = condition.substr(1, condition.length() - 2).strip_edges();
	}
	return _convert_expression(condition);
}

String _convert_parameter(const String &p_param) {
	String param = p_param.strip_edges();
	if (param.is_empty()) {
		return String();
	}
	// Drop default values and parameter modifiers.
	const int equals = param.find("=");
	if (equals != -1) {
		param = param.substr(0, equals).strip_edges();
	}
	param = param.replace("ref ", "").replace("out ", "").replace("in ", "").replace("params ", "");
	const Vector<String> bits = param.split(" ", false);
	if (bits.size() < 2) {
		return param;
	}
	const String name = bits[bits.size() - 1];
	String type;
	for (int i = 0; i < bits.size() - 1; i++) {
		if (!type.is_empty()) {
			type += " ";
		}
		type += bits[i];
	}
	return vformat("%s: %s", name, _map_type(type));
}

String _convert_statement(const String &p_statement, int p_indent, Vector<String> &r_out, bool p_in_method) {
	String line = p_statement.strip_edges();
	if (line.is_empty()) {
		r_out.push_back(String());
		return String();
	}
	if (line.begins_with("//")) {
		r_out.push_back(_tabs(p_indent) + "#" + line.substr(2));
		return String();
	}
	if (line.begins_with("#")) {
		r_out.push_back(_tabs(p_indent) + "# " + line);
		return String();
	}
	if (line.begins_with("[")) {
		return String();
	}

	if (line.begins_with("if ")) {
		String condition = line.substr(3).strip_edges();
		const int brace = condition.find("{");
		if (brace != -1) {
			condition = condition.substr(0, brace).strip_edges();
		}
		r_out.push_back(_tabs(p_indent) + "if " + _convert_condition(condition) + ":");
		return String();
	}
	if (line.begins_with("else if")) {
		String condition = line.substr(7).strip_edges();
		const int brace = condition.find("{");
		if (brace != -1) {
			condition = condition.substr(0, brace).strip_edges();
		}
		r_out.push_back(_tabs(p_indent) + "elif " + _convert_condition(condition) + ":");
		return String();
	}
	if (line.begins_with("else")) {
		r_out.push_back(_tabs(p_indent) + "else:");
		return String();
	}
	if (line.begins_with("while ")) {
		String condition = line.substr(6).strip_edges();
		const int brace = condition.find("{");
		if (brace != -1) {
			condition = condition.substr(0, brace).strip_edges();
		}
		r_out.push_back(_tabs(p_indent) + "while " + _convert_condition(condition) + ":");
		return String();
	}
	if (line.begins_with("for ")) {
		const int open = line.find("(");
		const int close = line.rfind(")");
		if (open != -1 && close > open) {
			const String header = line.substr(open + 1, close - open - 1);
			const Array parts = _split_top_level(header, ';');
			if (parts.size() == 3) {
				String init = String(parts[0]).strip_edges();
				const String cond = String(parts[1]).strip_edges();
				const String incr = String(parts[2]).strip_edges();
				const int equals = init.find("=");
				String var_name;
				String start_value;
				if (equals != -1) {
					var_name = init.substr(0, equals).strip_edges().split(" ", false)[init.substr(0, equals).strip_edges().split(" ", false).size() - 1];
					start_value = _convert_expression(init.substr(equals + 1));
				}
				if (!var_name.is_empty() && cond.contains(var_name) && incr.begins_with(var_name + "++")) {
					String limit = cond;
					limit = limit.replace(var_name, "").strip_edges();
					limit = limit.trim_prefix("<").trim_prefix("<=").strip_edges();
					limit = _convert_expression(limit);
					if (start_value == "0") {
						r_out.push_back(_tabs(p_indent) + vformat("for %s in range(%s):", var_name, limit));
					} else {
						r_out.push_back(_tabs(p_indent) + vformat("for %s in range(%s, %s):", var_name, start_value, limit));
					}
					return String();
				}
				r_out.push_back(_tabs(p_indent) + vformat("# TODO: convert for loop: for (%s; %s; %s)", init, cond, incr));
				r_out.push_back(_tabs(p_indent) + "pass");
				return String();
			}
		}
		r_out.push_back(_tabs(p_indent) + "# TODO: convert for loop: " + line);
		return String();
	}
	if (line.begins_with("foreach ")) {
		const int open = line.find("(");
		const int close = line.rfind(")");
		if (open != -1 && close > open) {
			String header = line.substr(open + 1, close - open - 1).strip_edges();
			header = header.replace("var ", "");
			const int in_pos = header.find(" in ");
			if (in_pos != -1) {
				String var_part = header.substr(0, in_pos).strip_edges();
				const Vector<String> bits = var_part.split(" ", false);
				const String var_name = bits[bits.size() - 1];
				const String iterable = _convert_expression(header.substr(in_pos + 4));
				r_out.push_back(_tabs(p_indent) + vformat("for %s in %s:", var_name, iterable));
				return String();
			}
		}
		r_out.push_back(_tabs(p_indent) + "# TODO: convert foreach: " + line);
		return String();
	}
	if (line.begins_with("switch ")) {
		const int open = line.find("(");
		const int close = line.find(")", open + 1);
		String value = (open != -1 && close != -1) ? line.substr(open + 1, close - open - 1) : "value";
		r_out.push_back(_tabs(p_indent) + "match " + _convert_expression(value) + ":");
		return String();
	}
	if (line.begins_with("case ")) {
		String value = line.substr(5).strip_edges().trim_suffix(":");
		r_out.push_back(_tabs(p_indent) + _convert_expression(value) + ":");
		return String();
	}
	if (line.begins_with("default:")) {
		r_out.push_back(_tabs(p_indent) + "_:");
		return String();
	}
	if (line.begins_with("try") || line.begins_with("catch") || line.begins_with("finally")) {
		r_out.push_back(_tabs(p_indent) + "# " + line);
		return String();
	}
	if (line.begins_with("return")) {
		const String value = line.substr(6).strip_edges().trim_suffix(";");
		if (value.is_empty()) {
			r_out.push_back(_tabs(p_indent) + "return");
		} else {
			r_out.push_back(_tabs(p_indent) + "return " + _convert_expression(value));
		}
		return String();
	}
	if (line == "break;" || line == "continue;") {
		r_out.push_back(_tabs(p_indent) + line.trim_suffix(";"));
		return String();
	}
	if (line.begins_with("yield return new WaitForSeconds(")) {
		const int open = line.find("(");
		const int close = line.find(")", open + 1);
		if (open != -1 && close != -1) {
			const String seconds = line.substr(open + 1, close - open - 1).strip_edges();
			r_out.push_back(_tabs(p_indent) + vformat("await get_tree().create_timer(%s).timeout", _convert_expression(seconds)));
			return String();
		}
	}
	if (line.begins_with("yield return null")) {
		r_out.push_back(_tabs(p_indent) + "await get_tree().process_frame");
		return String();
	}
	if (line.begins_with("using ") || line.begins_with("namespace ") || line.begins_with("#region") || line.begins_with("#endregion")) {
		return String();
	}

	// Variable declaration.
	const int semicolon = line.rfind(";");
	String statement = semicolon != -1 ? line.substr(0, semicolon) : line;
	const bool is_const = statement.begins_with("const ");
	if (is_const) {
		statement = statement.substr(6).strip_edges();
	}
	const int equals = _find_top_level(statement, '=');
	if (equals != -1) {
		const String left = statement.substr(0, equals).strip_edges();
		const String right = _convert_expression(statement.substr(equals + 1));
		const Vector<String> left_bits = left.split(" ", false);
		if (left_bits.size() >= 2 && !left.contains(".") && !left.contains("(") && !left.contains("[")) {
			const String name = left_bits[left_bits.size() - 1];
			String type;
			for (int i = 0; i < left_bits.size() - 1; i++) {
				if (!type.is_empty()) {
					type += " ";
				}
				type += left_bits[i];
			}
			if (is_const) {
				r_out.push_back(_tabs(p_indent) + vformat("const %s = %s", name, right));
			} else if (!type.is_empty() && type != "var") {
				r_out.push_back(_tabs(p_indent) + vformat("var %s: %s = %s", name, _map_type(type), right));
			} else {
				r_out.push_back(_tabs(p_indent) + vformat("var %s = %s", name, right));
			}
			return String();
		}
		r_out.push_back(_tabs(p_indent) + _convert_expression(statement.substr(0, equals)) + " = " + right);
		return String();
	}

	if (statement.ends_with("++")) {
		r_out.push_back(_tabs(p_indent) + statement.substr(0, statement.length() - 2).strip_edges() + " += 1");
		return String();
	}
	if (statement.ends_with("--")) {
		r_out.push_back(_tabs(p_indent) + statement.substr(0, statement.length() - 2).strip_edges() + " -= 1");
		return String();
	}

	r_out.push_back(_tabs(p_indent) + _convert_expression(statement));
	return String();
}

String _convert_body(const String &p_body, int p_base_indent) {
	Vector<String> out;
	const Vector<String> lines = p_body.replace("\r\n", "\n").split("\n");
	int indent = p_base_indent;

	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (line.is_empty()) {
			out.push_back(String());
			continue;
		}
		if (line.begins_with("//")) {
			out.push_back(_tabs(indent) + "#" + line.substr(2));
			continue;
		}
		if (line.begins_with("/*")) {
			while (i < lines.size() && !lines[i].contains("*/")) {
				i++;
			}
			continue;
		}
		if (line.begins_with("{")) {
			indent++;
			line = line.substr(1).strip_edges();
			if (line.is_empty()) {
				continue;
			}
		}
		while (line.ends_with("}")) {
			line = line.substr(0, line.length() - 1).strip_edges();
			if (!line.is_empty()) {
				_convert_statement(line, indent, out, true);
			}
			indent = MAX(0, indent - 1);
			line = String();
			break;
		}
		if (line.is_empty()) {
			continue;
		}
		if (line.ends_with("{")) {
			_convert_statement(line, indent, out, true);
			indent++;
			continue;
		}
		// Join continuation lines until the statement is complete.
		while (line.count("(") > line.count(")") && i + 1 < lines.size()) {
			i++;
			line += " " + lines[i].strip_edges();
		}
		_convert_statement(line, indent, out, true);
	}

	// Drop a trailing blank line.
	while (!out.is_empty() && out[out.size() - 1].strip_edges().is_empty()) {
		out.remove_at(out.size() - 1);
	}

	String result;
	for (int i = 0; i < out.size(); i++) {
		if (i > 0) {
			result += "\n";
		}
		result += out[i];
	}
	return result;
}

struct MethodInfo {
	String name;
	String return_type;
	String params;
	String body;
	bool is_constructor = false;
};

Vector<MethodInfo> _extract_methods(const String &p_class_body, const String &p_class_name, Vector<String> &r_fields) {
	Vector<MethodInfo> methods;
	const Vector<String> lines = p_class_body.replace("\r\n", "\n").split("\n");
	int depth = 0;
	int i = 0;

	while (i < lines.size()) {
		String raw = lines[i];
		String line = _strip_attributes(raw.strip_edges());

		if (line.is_empty() || line.begins_with("//")) {
			i++;
			continue;
		}
		if (line.begins_with("#")) {
			i++;
			continue;
		}

		const int open_braces = line.count("{");
		const int close_braces = line.count("}");

		if (depth == 0 && !line.contains("{") && line.ends_with(";") && !line.contains("(")) {
			r_fields.push_back(line);
			i++;
			continue;
		}

		if (depth == 0 && line.contains("(") && !line.begins_with("if ") && !line.begins_with("for ") &&
				!line.begins_with("while ") && !line.begins_with("switch ") && !line.begins_with("using ") &&
				!line.begins_with("return ")) {
			// Gather the full signature, including multi-line parameter lists.
			String signature = line;
			int j = i;
			while (signature.count("(") > signature.count(")") && j + 1 < lines.size()) {
				j++;
				signature += " " + lines[j].strip_edges();
			}
			const int open = signature.find("(");
			const int close = signature.find(")", open + 1);
			if (open != -1 && close != -1) {
				const String before = _remove_modifiers(signature.substr(0, open).strip_edges());
				const String after = signature.substr(close + 1).strip_edges();
				const bool has_body = after.contains("{") || (j + 1 < lines.size() && lines[j + 1].strip_edges().begins_with("{"));
				if (has_body) {
					const Vector<String> before_bits = before.split(" ", false);
					if (!before_bits.is_empty()) {
						MethodInfo method;
						method.name = before_bits[before_bits.size() - 1].strip_edges();
						if (before_bits.size() >= 2) {
							String ret;
							for (int b = 0; b < before_bits.size() - 1; b++) {
								if (!ret.is_empty()) {
									ret += " ";
								}
								ret += before_bits[b];
							}
							method.return_type = ret;
						} else {
							method.return_type = "void";
						}
						method.params = signature.substr(open + 1, close - open - 1);
						method.is_constructor = (method.name == p_class_name);

						// Find the opening brace and matching close brace.
						int body_start = j;
						while (body_start < lines.size() && !lines[body_start].contains("{")) {
							body_start++;
						}
						if (body_start < lines.size()) {
							String remainder;
							for (int k = body_start; k < lines.size(); k++) {
								if (k > body_start) {
									remainder += "\n";
								}
								remainder += lines[k];
							}
							const int brace_open = remainder.find("{");
							int brace_depth = 0;
							int brace_close = -1;
							for (int c = brace_open; c < remainder.length(); c++) {
								if (remainder[c] == '{') {
									brace_depth++;
								} else if (remainder[c] == '}') {
									brace_depth--;
									if (brace_depth == 0) {
										brace_close = c;
										break;
									}
								}
							}
							if (brace_close != -1) {
								method.body = remainder.substr(brace_open + 1, brace_close - brace_open - 1);
								methods.push_back(method);
								i = body_start + remainder.substr(0, brace_close).count("\n") + 1;
								continue;
							}
						}
					}
				}
			}
		}

		depth += open_braces - close_braces;
		depth = MAX(0, depth);
		i++;
	}

	return methods;
}

String _convert_field(const String &p_field, bool &r_exported) {
	String field = p_field.strip_edges().trim_suffix(";");
	const bool has_serialize = field.contains("SerializeField") || field.begins_with("public ");
	field = _strip_attributes(field);
	const bool is_const = field.begins_with("const ");
	field = _remove_modifiers(field);

	const int equals = _find_top_level(field, '=');
	String left = equals != -1 ? field.substr(0, equals).strip_edges() : field;
	String value = equals != -1 ? _convert_expression(field.substr(equals + 1)) : String();

	const Vector<String> bits = left.split(" ", false);
	if (bits.size() < 2) {
		return String();
	}
	const String name = bits[bits.size() - 1];
	String type;
	for (int i = 0; i < bits.size() - 1; i++) {
		if (!type.is_empty()) {
			type += " ";
		}
		type += bits[i];
	}
	const String mapped = _map_type(type);
	if (value.is_empty()) {
		value = _default_value_for_type(mapped);
	}

	r_exported = has_serialize && !is_const;
	if (is_const) {
		return vformat("const %s = %s", name, value);
	}
	if (r_exported) {
		return vformat("@export var %s: %s = %s", name, mapped, value);
	}
	return vformat("var %s: %s = %s", name, mapped, value);
}

String _lifecycle_name(const String &p_name) {
	if (p_name == "Awake" || p_name == "Start") {
		return "_ready";
	}
	if (p_name == "OnEnable") {
		return "_enter_tree";
	}
	if (p_name == "OnDisable" || p_name == "OnDestroy") {
		return "_exit_tree";
	}
	if (p_name == "Update" || p_name == "LateUpdate") {
		return "_process";
	}
	if (p_name == "FixedUpdate") {
		return "_physics_process";
	}
	if (p_name == "OnCollisionEnter") {
		return "_on_body_entered";
	}
	if (p_name == "OnTriggerEnter") {
		return "_on_area_entered";
	}
	return String();
}

} // namespace

UnityScriptConverter::Result UnityScriptConverter::convert(const String &p_csharp, const String &p_expected_class) {
	Result result;
	String source = p_csharp.replace("\r\n", "\n").replace("\r", "\n");
	if (source.begins_with("\xef\xbb\xbf")) {
		source = source.substr(3);
	}

	// Find the class declaration.
	int class_pos = source.find("class ");
	String class_name = p_expected_class;
	while (class_pos != -1) {
		int name_start = class_pos + 6;
		while (name_start < source.length() && (source[name_start] == ' ' || source[name_start] == '\t')) {
			name_start++;
		}
		int name_end = name_start;
		while (name_end < source.length() && _is_identifier_char(source[name_end])) {
			name_end++;
		}
		const String candidate = source.substr(name_start, name_end - name_start);
		if (!candidate.is_empty() && candidate != "MonoBehaviour" && candidate != "ScriptableObject") {
			class_name = candidate;
			break;
		}
		class_pos = source.find("class ", class_pos + 6);
	}

	if (class_name.is_empty()) {
		result.warnings.push_back("No class declaration found; using 'UnityScript'.");
		class_name = "UnityScript";
	}
	result.class_name = class_name;

	// Base type.
	String base_type = "Node3D";
	const int class_keyword = source.find("class " + class_name);
	if (class_keyword != -1) {
		const int colon = source.find(":", class_keyword);
		const int brace = source.find("{", class_keyword);
		if (colon != -1 && (brace == -1 || colon < brace)) {
			String declared_base = source.substr(colon + 1, (brace == -1 ? source.length() : brace) - colon - 1).strip_edges();
			declared_base = declared_base.split(",")[0].strip_edges();
			if (declared_base == "ScriptableObject") {
				base_type = "Resource";
			} else if (declared_base == "MonoBehaviour" || declared_base.is_empty()) {
				base_type = "Node3D";
			} else if (declared_base == "StateMachineBehaviour") {
				base_type = "Node";
			} else if (!declared_base.is_empty()) {
				base_type = _map_type(declared_base);
			}
		}
	}
	if (source.contains("RectTransform") || source.contains("CanvasGroup") || source.contains("UnityEngine.UI")) {
		base_type = "Control";
	}
	result.base_type = base_type;

	// Isolate the class body.
	String class_body;
	const int first_brace = source.find("{", class_keyword == -1 ? 0 : class_keyword);
	if (first_brace != -1) {
		int depth = 0;
		for (int i = first_brace; i < source.length(); i++) {
			if (source[i] == '{') {
				depth++;
			} else if (source[i] == '}') {
				depth--;
				if (depth == 0) {
					class_body = source.substr(first_brace + 1, i - first_brace - 1);
					break;
				}
			}
		}
	}
	if (class_body.is_empty()) {
		result.warnings.push_back("Could not isolate the class body; emitting a stub.");
		result.code = vformat("# Auto-converted from Unity C#\n# Original class: %s\nextends %s\n\n# TODO: the original script could not be parsed.\n", class_name, base_type);
		result.success = true;
		return result;
	}

	Vector<String> fields;
	const Vector<MethodInfo> methods = _extract_methods(class_body, class_name, fields);

	// Group lifecycle methods so Awake + Start end up in one _ready.
	HashMap<String, Vector<String>> lifecycle_bodies;
	HashMap<String, String> lifecycle_params;
	Vector<MethodInfo> regular_methods;

	for (const MethodInfo &method : methods) {
		if (method.is_constructor) {
			continue;
		}
		const String lifecycle = _lifecycle_name(method.name);
		if (!lifecycle.is_empty()) {
			lifecycle_bodies[lifecycle].push_back(method.body);
			if (!method.params.strip_edges().is_empty()) {
				lifecycle_params[lifecycle] = method.params;
			}
		} else {
			regular_methods.push_back(method);
		}
	}

	String code;
	code += "# Auto-converted from Unity C# by the Redot Unity importer.\n";
	code += vformat("# Original class: %s\n", class_name);
	code += "# Review TODOs before shipping; Unity APIs without a direct Godot\n";
	code += "# equivalent are preserved as comments.\n";
	code += vformat("extends %s\n", base_type);
	if (class_name.is_valid_identifier() && !class_name.is_empty()) {
		code += vformat("class_name %s\n", class_name);
	}
	code += "\n";

	// Exported fields first.
	for (const String &field : fields) {
		bool exported = false;
		const String converted = _convert_field(field, exported);
		if (!converted.is_empty()) {
			code += converted + "\n";
			if (exported) {
				result.exports.push_back(converted.split(" ")[1]);
			}
		}
	}

	if (!fields.is_empty()) {
		code += "\n";
	}

	for (const MethodInfo &method : regular_methods) {
		String method_name = method.name;
		if (method_name == "ToString") {
			method_name = "_to_string";
		}
		String return_type = _map_type(method.return_type);
		if (method.return_type == "void" || method.return_type == "IEnumerator" || method.return_type.is_empty()) {
			return_type = String();
		}

		String params;
		const Array param_list = _split_top_level(method.params, ',');
		for (int i = 0; i < param_list.size(); i++) {
			const String converted = _convert_parameter(param_list[i]);
			if (converted.is_empty()) {
				continue;
			}
			if (!params.is_empty()) {
				params += ", ";
			}
			params += converted;
		}

		code += vformat("func %s(%s)", method_name, params);
		if (!return_type.is_empty()) {
			code += vformat(" -> %s", return_type);
		}
		code += ":\n";
		const String body = _convert_body(method.body, 1);
		code += body.is_empty() ? "\tpass\n" : body + "\n";
		code += "\n";
	}

	for (const KeyValue<String, Vector<String>> &entry : lifecycle_bodies) {
		const String &name = entry.key;
		String params;
		if (name == "_process" || name == "_physics_process") {
			params = "delta";
		}
		if (lifecycle_params.has(name)) {
			const Array param_list = _split_top_level(lifecycle_params[name], ',');
			params = String();
			for (int i = 0; i < param_list.size(); i++) {
				const String converted = _convert_parameter(param_list[i]);
				if (!converted.is_empty()) {
					if (!params.is_empty()) {
						params += ", ";
					}
					params += converted;
				}
			}
		}

		code += vformat("func %s(%s):\n", name, params);
		bool wrote = false;
		for (const String &body : entry.value) {
			const String converted = _convert_body(body, 1);
			if (!converted.is_empty()) {
				code += converted + "\n";
				wrote = true;
			}
		}
		if (!wrote) {
			code += "\tpass\n";
		}
		code += "\n";
	}

	code += "# Original C# source, kept for reference while porting:\n";
	const Vector<String> original_lines = source.split("\n");
	for (const String &line : original_lines) {
		code += "# " + line + "\n";
	}

	result.code = code;
	result.success = true;
	return result;
}

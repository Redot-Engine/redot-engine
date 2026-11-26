/**************************************************************************/
/*  unity_yaml.cpp                                                        */
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

#include "unity_yaml.h"

#include "core/io/file_access.h"
#include "core/math/math_funcs.h"

namespace {

int _indent_of(const String &p_line) {
	int indent = 0;
	while (indent < p_line.length() && (p_line[indent] == ' ' || p_line[indent] == '\t')) {
		indent++;
	}
	return indent;
}

bool _is_blank(const String &p_line) {
	return p_line.strip_edges().is_empty();
}

int _next_non_blank(const Vector<String> &p_lines, int p_index) {
	while (p_index < p_lines.size() && _is_blank(p_lines[p_index])) {
		p_index++;
	}
	return p_index;
}

// Finds the colon that separates a key from its value, ignoring colons inside
// flow collections and quoted strings.
int _mapping_colon(const String &p_text) {
	int depth = 0;
	bool in_single = false;
	bool in_double = false;
	for (int i = 0; i < p_text.length(); i++) {
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
		} else if (c == '{' || c == '[') {
			depth++;
		} else if (c == '}' || c == ']') {
			depth--;
		} else if (c == ':' && depth == 0) {
			if (i + 1 >= p_text.length() || p_text[i + 1] == ' ' || p_text[i + 1] == '\t') {
				return i;
			}
		}
	}
	return -1;
}

Array _split_top_level(const String &p_text, char32_t p_separator) {
	Array parts;
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
		} else if (c == '{' || c == '[') {
			depth++;
			current += c;
		} else if (c == '}' || c == ']') {
			depth--;
			current += c;
		} else if (c == p_separator && depth == 0) {
			parts.push_back(current.strip_edges());
			current = String();
		} else {
			current += c;
		}
	}
	if (!current.strip_edges().is_empty()) {
		parts.push_back(current.strip_edges());
	}
	return parts;
}

class Parser {
	Vector<String> lines;
	int index = 0;

public:
	explicit Parser(const Vector<String> &p_lines) :
			lines(p_lines) {}

	Variant parse_block(int p_indent) {
		const int start = _next_non_blank(lines, index);
		if (start >= lines.size()) {
			return Variant();
		}
		const String first = lines[start].substr(_indent_of(lines[start]));
		if (first.begins_with("- ") || first == "-") {
			return parse_sequence(p_indent);
		}
		return parse_mapping(p_indent);
	}

	Dictionary parse_mapping(int p_indent, const String &p_first_key = String(), const String &p_first_rest = String()) {
		Dictionary result;
		bool use_first = !p_first_key.is_empty();

		while (index < lines.size()) {
			if (_is_blank(lines[index])) {
				index++;
				continue;
			}
			const int indent = _indent_of(lines[index]);
			if (indent < p_indent) {
				break;
			}
			if (indent > p_indent) {
				// Stray deeper line (malformed asset). Skip it rather than
				// corrupting the enclosing map.
				index++;
				continue;
			}

			const String text = lines[index].substr(p_indent).strip_edges(false, true);
			if (text.begins_with("- ")) {
				break;
			}

			String key;
			String rest;
			if (use_first) {
				key = p_first_key;
				rest = p_first_rest;
				use_first = false;
			} else {
				const int colon = _mapping_colon(text);
				if (colon == -1) {
					index++;
					continue;
				}
				key = text.substr(0, colon).strip_edges();
				rest = text.substr(colon + 1).strip_edges();
				index++;
			}

			if (key.is_empty()) {
				continue;
			}

			if (rest.is_empty()) {
				const int next = _next_non_blank(lines, index);
				if (next < lines.size()) {
					const int next_indent = _indent_of(lines[next]);
					const bool next_is_sequence = lines[next].substr(next_indent).begins_with("-");
					// Unity emits sequences at the same indentation as their key.
					if (next_indent > p_indent || (next_indent == p_indent && next_is_sequence)) {
						index = next;
						result[key] = parse_block(next_indent);
						continue;
					}
				}
				result[key] = Variant();
			} else if (rest == "|" || rest == ">" || rest == "|-" || rest == ">-") {
				const bool folded = rest.begins_with(">");
				String block;
				int block_indent = -1;
				while (index < lines.size()) {
					if (_is_blank(lines[index])) {
						block += "\n";
						index++;
						continue;
					}
					const int line_indent = _indent_of(lines[index]);
					if (line_indent <= p_indent) {
						break;
					}
					if (block_indent == -1) {
						block_indent = line_indent;
					}
					if (!block.is_empty() && !block.ends_with("\n")) {
						block += folded ? " " : "\n";
					}
					block += lines[index].substr(MIN(block_indent, line_indent));
					index++;
				}
				result[key] = block;
			} else {
				result[key] = UnityYaml::parse_scalar(rest);
			}
		}
		return result;
	}

	Array parse_sequence(int p_indent) {
		Array result;
		while (index < lines.size()) {
			if (_is_blank(lines[index])) {
				index++;
				continue;
			}
			const int indent = _indent_of(lines[index]);
			if (indent < p_indent) {
				break;
			}
			if (indent > p_indent) {
				index++;
				continue;
			}

			String text = lines[index].substr(p_indent);
			if (!text.begins_with("-")) {
				break;
			}
			String content = text.substr(1).strip_edges();

			if (content.is_empty()) {
				index++;
				const int next = _next_non_blank(lines, index);
				if (next < lines.size() && _indent_of(lines[next]) > p_indent) {
					index = next;
					result.push_back(parse_block(_indent_of(lines[next])));
				} else {
					result.push_back(Variant());
				}
				continue;
			}

			const int colon = _mapping_colon(content);
			if (colon == -1) {
				result.push_back(UnityYaml::parse_scalar(content));
				index++;
				continue;
			}

			// A sequence entry that starts a map (`- key: value`). The rest of
			// the map is indented relative to the dash.
			const String key = content.substr(0, colon).strip_edges();
			const String rest = content.substr(colon + 1).strip_edges();
			index++;

			Dictionary entry;
			if (rest.is_empty()) {
				const int next = _next_non_blank(lines, index);
				if (next < lines.size() && _indent_of(lines[next]) > p_indent) {
					index = next;
					entry[key] = parse_block(_indent_of(lines[next]));
				} else {
					entry[key] = Variant();
				}
			} else {
				entry[key] = UnityYaml::parse_scalar(rest);
			}

			const int next = _next_non_blank(lines, index);
			if (next < lines.size() && _indent_of(lines[next]) > p_indent) {
				index = next;
				const Dictionary more = parse_mapping(_indent_of(lines[next]));
				const Array keys = more.keys();
				for (int i = 0; i < keys.size(); i++) {
					entry[keys[i]] = more[keys[i]];
				}
			}
			result.push_back(entry);
		}
		return result;
	}
};

String _unescape_single(const String &p_text) {
	String inner = p_text.substr(1, p_text.length() - 2);
	return inner.replace("''", "'");
}

String _unescape_double(const String &p_text) {
	String inner = p_text.substr(1, p_text.length() - 2);
	return inner.c_unescape();
}

} // namespace

Error UnityYaml::parse_text(const String &p_text, Vector<UnityYamlDocument> &r_documents) {
	const String normalized = p_text.replace("\r\n", "\n").replace("\r", "\n");
	Vector<String> lines = normalized.split("\n");

	UnityYamlDocument current;
	bool in_document = false;

	auto flush = [&]() {
		if (!in_document) {
			return;
		}
		if (current.class_id != 0 || !current.type_name.is_empty()) {
			r_documents.push_back(current);
		}
		current = UnityYamlDocument();
		in_document = false;
	};

	for (int i = 0; i < lines.size(); i++) {
		const String line = lines[i];
		const String trimmed = line.strip_edges();

		if (trimmed.begins_with("---")) {
			flush();

			const int class_pos = trimmed.find("!u!");
			if (class_pos == -1) {
				continue;
			}
			int class_end = class_pos + 3;
			while (class_end < trimmed.length() && trimmed[class_end] >= '0' && trimmed[class_end] <= '9') {
				class_end++;
			}
			const int amp_pos = trimmed.find("&", class_end);
			if (amp_pos == -1) {
				continue;
			}
			int id_end = amp_pos + 1;
			while (id_end < trimmed.length() && trimmed[id_end] >= '0' && trimmed[id_end] <= '9') {
				id_end++;
			}

			current.class_id = trimmed.substr(class_pos + 3, class_end - class_pos - 3).to_int();
			current.file_id = trimmed.substr(amp_pos + 1, id_end - amp_pos - 1).to_int();
			current.stripped = trimmed.contains(" stripped");
			in_document = true;

			// Skip directives and blank lines up to the type name.
			int j = i + 1;
			while (j < lines.size() && (_is_blank(lines[j]) || lines[j].strip_edges().begins_with("%"))) {
				j++;
			}
			if (j < lines.size()) {
				const String type_line = lines[j].strip_edges();
				if (type_line.ends_with(":") && !type_line.contains(" ")) {
					current.type_name = type_line.substr(0, type_line.length() - 1);
					i = j;
				} else if (!type_line.begins_with("---")) {
					// Some documents omit the type line; parse the body as-is.
					i = j - 1;
				}
			}
			continue;
		}

		if (!in_document) {
			continue;
		}

		// Directives between documents.
		if (trimmed.begins_with("%")) {
			continue;
		}

		if (trimmed.is_empty()) {
			continue;
		}

		// The first meaningful line after the type header is the body. Parse
		// the remaining lines as one block and let the parser advance.
		const int body_indent = _indent_of(line);
		if (body_indent <= 0) {
			// A top-level key that is not part of the type header. Unity does
			// not emit this, but tolerate it.
			continue;
		}

		Parser parser(lines);
		parser.index = i;
		current.body = parser.parse_mapping(body_indent);
		i = parser.index - 1;
	}

	flush();
	return r_documents.is_empty() ? ERR_FILE_UNRECOGNIZED : OK;
}

Error UnityYaml::parse_file(const String &p_path, Vector<UnityYamlDocument> &r_documents) {
	Error err = OK;
	const String text = FileAccess::get_file_as_string(p_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot read Unity YAML file '%s'.", p_path));
	return parse_text(text, r_documents);
}

Dictionary UnityYaml::parse_flow_mapping(const String &p_text) {
	Dictionary result;
	String text = p_text.strip_edges();
	if (text.begins_with("{")) {
		text = text.substr(1);
	}
	if (text.ends_with("}")) {
		text = text.substr(0, text.length() - 1);
	}

	const Array parts = _split_top_level(text, ',');
	for (int i = 0; i < parts.size(); i++) {
		const String part = parts[i];
		const int colon = _mapping_colon(part);
		if (colon == -1) {
			continue;
		}
		const String key = part.substr(0, colon).strip_edges();
		const String value = part.substr(colon + 1).strip_edges();
		result[key] = parse_scalar(value);
	}
	return result;
}

Variant UnityYaml::parse_scalar(const String &p_text) {
	const String text = p_text.strip_edges();
	if (text.is_empty() || text == "~" || text == "null") {
		return Variant();
	}
	if (text.begins_with("{") && text.ends_with("}")) {
		return parse_flow_mapping(text);
	}
	if (text.begins_with("[") && text.ends_with("]")) {
		const String inner = text.substr(1, text.length() - 2);
		const Array parts = _split_top_level(inner, ',');
		Array result;
		for (int i = 0; i < parts.size(); i++) {
			result.push_back(parse_scalar(parts[i]));
		}
		return result;
	}
	if (text.length() >= 2 && text.begins_with("'") && text.ends_with("'")) {
		return _unescape_single(text);
	}
	if (text.length() >= 2 && text.begins_with("\"") && text.ends_with("\"")) {
		return _unescape_double(text);
	}
	if (text == "true") {
		return true;
	}
	if (text == "false") {
		return false;
	}
	if (text == "Infinity" || text == "+Infinity") {
		return Math_INF;
	}
	if (text == "-Infinity") {
		return -Math_INF;
	}
	if (text == "NaN") {
		return Math_NAN;
	}
	if (text.is_valid_int()) {
		return text.to_int();
	}
	if (text.is_valid_float()) {
		return text.to_float();
	}
	return text;
}

int64_t UnityYaml::ref_file_id(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		const Dictionary d = p_value;
		return d.get("fileID", 0);
	}
	if (p_value.get_type() == Variant::INT) {
		return p_value;
	}
	return 0;
}

String UnityYaml::ref_guid(const Variant &p_value) {
	if (p_value.get_type() == Variant::DICTIONARY) {
		const Dictionary d = p_value;
		return d.get("guid", String());
	}
	return String();
}

Vector3 UnityYaml::get_vector3(const Dictionary &p_dict, const String &p_key, const Vector3 &p_default) {
	const Variant value = p_dict.get(p_key, Variant());
	if (value.get_type() != Variant::DICTIONARY) {
		return p_default;
	}
	const Dictionary d = value;
	return Vector3(
			d.get("x", p_default.x),
			d.get("y", p_default.y),
			d.get("z", p_default.z));
}

Quaternion UnityYaml::get_quaternion(const Dictionary &p_dict, const String &p_key, const Quaternion &p_default) {
	const Variant value = p_dict.get(p_key, Variant());
	if (value.get_type() != Variant::DICTIONARY) {
		return p_default;
	}
	const Dictionary d = value;
	return Quaternion(
			d.get("x", p_default.x),
			d.get("y", p_default.y),
			d.get("z", p_default.z),
			d.get("w", p_default.w));
}

Color UnityYaml::get_color(const Dictionary &p_dict, const String &p_key, const Color &p_default) {
	const Variant value = p_dict.get(p_key, Variant());
	if (value.get_type() != Variant::DICTIONARY) {
		return p_default;
	}
	const Dictionary d = value;
	return Color(
			d.get("r", p_default.r),
			d.get("g", p_default.g),
			d.get("b", p_default.b),
			d.get("a", p_default.a));
}

bool UnityYaml::get_bool(const Dictionary &p_dict, const String &p_key, bool p_default) {
	const Variant value = p_dict.get(p_key, Variant());
	if (value.get_type() == Variant::BOOL) {
		return value;
	}
	if (value.get_type() == Variant::INT) {
		return (int64_t)value != 0;
	}
	return p_default;
}

double UnityYaml::get_number(const Dictionary &p_dict, const String &p_key, double p_default) {
	const Variant value = p_dict.get(p_key, Variant());
	if (value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT) {
		return value;
	}
	return p_default;
}

int64_t UnityYaml::get_int(const Dictionary &p_dict, const String &p_key, int64_t p_default) {
	const Variant value = p_dict.get(p_key, Variant());
	if (value.get_type() == Variant::INT) {
		return value;
	}
	if (value.get_type() == Variant::FLOAT) {
		return (int64_t)(double)value;
	}
	return p_default;
}

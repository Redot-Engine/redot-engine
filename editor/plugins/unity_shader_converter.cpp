/**************************************************************************/
/*  unity_shader_converter.cpp                                            */
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

#include "unity_shader_converter.h"

#include "core/io/file_access.h"
#include "core/string/print_string.h"

const HashSet<String> UnityShaderConverter::rubbish_tokens = []() {
	HashSet<String> set;
	set.insert("CGPROGRAM");
	set.insert("ENDCG");
	set.insert("#pragma");
	return set;
}();

const HashMap<String, String> UnityShaderConverter::unity_to_godot_functions = []() {
	HashMap<String, String> map;
	map["UnityObjectToClipPos"] = "(PROJECTION_MATRIX * MODELVIEW_MATRIX * vec4";
	map["mul"] = "*";
	map["tex2D"] = "texture";
	map["lerp"] = "mix";
	map["saturate"] = "clamp";
	map["frac"] = "fract";
	map["ddx"] = "dFdx";
	map["ddy"] = "dFdy";
	map["atan2"] = "atan";
	map["rsqrt"] = "inversesqrt";
	return map;
}();

const HashMap<String, String> UnityShaderConverter::hlsl_to_glsl_types = []() {
	HashMap<String, String> map;
	map["float4"] = "vec4";
	map["float3"] = "vec3";
	map["float2"] = "vec2";
	map["fixed4"] = "vec4";
	map["fixed3"] = "vec3";
	map["fixed2"] = "vec2";
	map["fixed"] = "float";
	map["half4"] = "vec4";
	map["half3"] = "vec3";
	map["half2"] = "vec2";
	map["half"] = "float";
	map["sampler2D"] = "sampler2D";
	map["samplerCUBE"] = "samplerCube";
	return map;
}();

const HashMap<String, String> UnityShaderConverter::unity_semantics = []() {
	HashMap<String, String> map;
	map["POSITION"] = "VERTEX";
	map["SV_POSITION"] = "VERTEX";
	map["NORMAL"] = "NORMAL";
	map["TEXCOORD0"] = "UV";
	map["TEXCOORD1"] = "UV2";
	map["COLOR"] = "COLOR";
	map["SV_Target"] = "ALBEDO";
	return map;
}();

Error UnityShaderConverter::convert_shaderlab_to_godot(const String &p_shaderlab_content, String &r_godot_shader) {
	ShaderLabToken *tokens = tokenize_shaderlab(p_shaderlab_content);
	if (!tokens) {
		return ERR_PARSE_ERROR;
	}

	ShaderNode *shader_ast = parse_shader_ast(tokens);
	delete tokens;

	if (!shader_ast) {
		return ERR_PARSE_ERROR;
	}

	r_godot_shader = generate_godot_shader(shader_ast);
	delete shader_ast;

	return r_godot_shader.is_empty() ? ERR_PARSE_ERROR : OK;
}

ShaderLabToken *UnityShaderConverter::tokenize_shaderlab(const String &p_content) {
	ShaderLabToken *head = memnew(ShaderLabToken);
	ShaderLabToken *current = head;

	Vector<String> lines = p_content.split("\n");
	String buffer;
	bool buffer_is_comment = false;

	for (int line_idx = 0; line_idx < lines.size(); line_idx++) {
		const String &line = lines[line_idx];

		for (int i = 0; i < line.length(); i++) {
			char32_t c = line[i];
			bool current_char_is_token = false;

			if (_get_token_type(String::chr(c), true) == ShaderLabTokenType::NONE || buffer.is_empty()) {
				buffer += String::chr(c);
			} else {
				current_char_is_token = true;
			}

			ShaderLabTokenType potential_token = _get_token_type(buffer, false);

			// Check for comments
			if (buffer.length() >= 2 && buffer[0] == '/' && buffer[1] == '/') {
				buffer_is_comment = true;
			}

			if (buffer_is_comment) {
				continue;
			}

			// If ends on a token, mark as unidentified
			if (current_char_is_token) {
				potential_token = ShaderLabTokenType::UNIDENTIFIED;
			}

			// Handle variables/datatypes at end of line
			if (potential_token == ShaderLabTokenType::NONE && i == line.length() - 1) {
				potential_token = ShaderLabTokenType::UNIDENTIFIED;
			}

			if (potential_token != ShaderLabTokenType::NONE) {
				current->type = potential_token;
				current->original_data = buffer;
				current->next = memnew(ShaderLabToken);
				current = current->next;
				buffer = "";
			}

			if (current_char_is_token) {
				current->type = _get_token_type(String::chr(c), true);
				current->original_data = String::chr(c);
				current->next = memnew(ShaderLabToken);
				current = current->next;
			}
		}

		if (buffer_is_comment) {
			current->type = ShaderLabTokenType::COMMENT;
			current->next = memnew(ShaderLabToken);
			current = current->next;
			buffer = "";
			buffer_is_comment = false;
		}

		// Add endline token
		current->type = ShaderLabTokenType::ENDLINE;
		current->next = memnew(ShaderLabToken);
		current = current->next;
		buffer = "";
	}

	return head;
}

ShaderNode *UnityShaderConverter::parse_shader_ast(ShaderLabToken *p_tokens) {
	ShaderLabToken *stripped = _strip_whitespace(p_tokens);
	ShaderNode *shader = memnew(ShaderNode);

	ShaderLabToken *current = stripped;
	bool in_properties = false;
	bool in_cgprogram = false;
	String cgprogram_buffer;

	while (current && current->next) {
		// Parse shader name
		if (current->original_data == "Shader" && current->next->type == ShaderLabTokenType::STRING_LITERAL) {
			shader->name = current->next->original_data.trim_prefix("\"").trim_suffix("\"");
			current = current->next;
		}

		// Parse properties section
		if (current->original_data == "Properties" && current->next->type == ShaderLabTokenType::OPEN_CURLY) {
			in_properties = true;
			if (!shader->properties) {
				shader->properties = memnew(ShaderPropertiesNode);
			}
			current = current->next;
			_parse_properties(current, shader->properties);
			in_properties = false;
			continue;
		}

		// Parse CGPROGRAM section
		if (current->original_data == "CGPROGRAM") {
			in_cgprogram = true;
			current = current->next;
			continue;
		}

		if (current->original_data == "ENDCG") {
			in_cgprogram = false;
			// Process accumulated CGPROGRAM content
			if (!cgprogram_buffer.is_empty()) {
				if (!shader->pass) {
					shader->pass = memnew(ShaderPassNode);
				}
				// Parse structs and functions from CGPROGRAM
				ShaderLabToken *cg_tokens = tokenize_shaderlab(cgprogram_buffer);
				ShaderLabToken *cg_stripped = _strip_whitespace(cg_tokens);
				ShaderLabToken *cg_current = cg_stripped;
				
				while (cg_current && cg_current->next) {
					// Parse structs
					if (cg_current->original_data == "struct") {
						ShaderStruct s;
						_parse_struct(cg_current, s);
						shader->structs.push_back(s);
						continue;
					}
					
					// Parse functions (detect by return type + name + parentheses)
					if (cg_current->next && cg_current->next->next &&
						cg_current->next->type == ShaderLabTokenType::OPEN_BRACKET) {
						ShaderFunction func;
						_parse_function(cg_current, func);
						shader->functions.push_back(func);
						continue;
					}
					
					cg_current = cg_current->next;
				}
				
				delete cg_tokens;
				cgprogram_buffer = "";
			}
			current = current->next;
			continue;
		}

		if (in_cgprogram) {
			cgprogram_buffer += current->original_data + " ";
		}

		current = current->next;
	}

	delete stripped;
	return shader;
}

String UnityShaderConverter::generate_godot_shader(ShaderNode *p_shader_node) {
	if (!p_shader_node) {
		return "";
	}

	String godot_shader = "shader_type spatial;\n";
	godot_shader += "render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;\n\n";

	// Convert properties to uniforms
	if (p_shader_node->properties && p_shader_node->properties->properties) {
		godot_shader += "// Uniforms converted from Unity properties\n";
		ShaderPropertyNode *prop = p_shader_node->properties->properties;
		while (prop) {
			String godot_type = _translate_unity_type(prop->type_name);
			godot_shader += "uniform " + godot_type + " " + prop->name;
			if (!prop->type_name.is_empty()) {
				if (prop->type_name.contains("Color")) {
					godot_shader += " : source_color = vec4(1.0)";
				} else if (prop->type_name.contains("2D")) {
					godot_shader += " : source_color";
				}
			}
			godot_shader += ";\n";
			prop = prop->next_property;
		}
		godot_shader += "\n";
	}

	// Find vertex and fragment functions
	ShaderFunction *vertex_func = nullptr;
	ShaderFunction *fragment_func = nullptr;
	
	for (int i = 0; i < p_shader_node->functions.size(); i++) {
		const ShaderFunction &func = p_shader_node->functions[i];
		if (func.name == "vert" || func.return_semantic.contains("POSITION")) {
			vertex_func = const_cast<ShaderFunction *>(&func);
		} else if (func.name == "frag" || func.return_semantic.contains("Target")) {
			fragment_func = const_cast<ShaderFunction *>(&func);
		}
	}

	// Generate vertex function
	godot_shader += "void vertex() {\n";
	if (vertex_func) {
		String converted_vertex = _convert_hlsl_to_glsl(vertex_func->body, true);
		godot_shader += converted_vertex;
	} else {
		godot_shader += "\t// Default vertex shader\n";
		godot_shader += "\tVERTEX = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;\n";
	}
	godot_shader += "}\n\n";

	// Generate fragment function
	godot_shader += "void fragment() {\n";
	if (fragment_func) {
		String converted_fragment = _convert_hlsl_to_glsl(fragment_func->body, false);
		godot_shader += converted_fragment;
	} else {
		godot_shader += "\t// Default fragment shader\n";
		godot_shader += "\tALBEDO = vec3(1.0);\n";
	}
	godot_shader += "}\n";

	return godot_shader;
}

String UnityShaderConverter::get_token_symbol(ShaderLabTokenType p_type) {
	switch (p_type) {
		case ShaderLabTokenType::UNIDENTIFIED:
			return "(undefined)";
		case ShaderLabTokenType::NONE:
			return "(none)";
		case ShaderLabTokenType::STRING_LITERAL:
			return "(string literal)";
		case ShaderLabTokenType::FLOAT_LITERAL:
			return "(float literal)";
		case ShaderLabTokenType::INT_LITERAL:
			return "(integer literal)";
		case ShaderLabTokenType::CHAR_LITERAL:
			return "(character literal)";
		case ShaderLabTokenType::VARIABLE:
			return "(variable)";
		case ShaderLabTokenType::ASSIGNMENT:
			return "=";
		case ShaderLabTokenType::ADDITION:
			return "+";
		case ShaderLabTokenType::SUBTRACTION:
			return "-";
		case ShaderLabTokenType::ENDLINE:
			return "\n";
		case ShaderLabTokenType::SPACE:
			return " ";
		case ShaderLabTokenType::RUBBISH:
			return "(rubbish)";
		case ShaderLabTokenType::OPEN_BRACKET:
			return "(";
		case ShaderLabTokenType::CLOSE_BRACKET:
			return ")";
		case ShaderLabTokenType::OPEN_CURLY:
			return "{";
		case ShaderLabTokenType::CLOSE_CURLY:
			return "}";
		case ShaderLabTokenType::COMMA:
			return ",";
		case ShaderLabTokenType::STRUCT:
			return "struct";
		case ShaderLabTokenType::NORMALIZE:
			return "normalize";
		case ShaderLabTokenType::COMMENT:
			return "// comment";
		case ShaderLabTokenType::DOT:
			return ".";
		case ShaderLabTokenType::INHERITANCE:
			return ":";
		case ShaderLabTokenType::DATATYPE:
			return "(datatype)";
		case ShaderLabTokenType::RETURN:
			return "return";
		case ShaderLabTokenType::SEMICOLON:
			return ";";
		case ShaderLabTokenType::FUNCTION:
			return "(function)";
	}
	return "";
}

String UnityShaderConverter::get_token_name(ShaderLabTokenType p_type) {
	switch (p_type) {
		case ShaderLabTokenType::UNIDENTIFIED:
			return "undefined";
		case ShaderLabTokenType::NONE:
			return "none";
		case ShaderLabTokenType::STRING_LITERAL:
			return "string literal";
		case ShaderLabTokenType::INT_LITERAL:
			return "integer literal";
		case ShaderLabTokenType::VARIABLE:
			return "variable";
		case ShaderLabTokenType::ASSIGNMENT:
			return "assignment";
		case ShaderLabTokenType::OPEN_BRACKET:
			return "open bracket";
		case ShaderLabTokenType::CLOSE_BRACKET:
			return "close bracket";
		case ShaderLabTokenType::SEMICOLON:
			return "semicolon";
		default:
			return get_token_symbol(p_type);
	}
}

ShaderLabTokenType UnityShaderConverter::_get_token_type(const String &p_value, bool p_is_extra) {
	if (p_value.is_empty()) {
		return ShaderLabTokenType::NONE;
	}

	// String literal
	if (p_value.length() > 1 && p_value[0] == '\"' && p_value[p_value.length() - 1] == '\"') {
		return ShaderLabTokenType::STRING_LITERAL;
	}

	// Integer literal
	if (_is_valid_integer(p_value) && !p_is_extra) {
		return ShaderLabTokenType::INT_LITERAL;
	}

	// Single-character tokens
	if (p_value == " ") {
		return ShaderLabTokenType::SPACE;
	}
	if (p_value == "{") {
		return ShaderLabTokenType::OPEN_CURLY;
	}
	if (p_value == "}") {
		return ShaderLabTokenType::CLOSE_CURLY;
	}
	if (p_value == "(") {
		return ShaderLabTokenType::OPEN_BRACKET;
	}
	if (p_value == ")") {
		return ShaderLabTokenType::CLOSE_BRACKET;
	}
	if (p_value == ",") {
		return ShaderLabTokenType::COMMA;
	}
	if (p_value == "+") {
		return ShaderLabTokenType::ADDITION;
	}
	if (p_value == "=") {
		return ShaderLabTokenType::ASSIGNMENT;
	}
	if (p_value == ".") {
		return ShaderLabTokenType::DOT;
	}
	if (p_value == ":") {
		return ShaderLabTokenType::INHERITANCE;
	}
	if (p_value == ";") {
		return ShaderLabTokenType::SEMICOLON;
	}
	if (p_value == "-") {
		return ShaderLabTokenType::SUBTRACTION;
	}

	// Keywords
	if (p_value == "return") {
		return ShaderLabTokenType::RETURN;
	}
	if (p_value == "struct") {
		return ShaderLabTokenType::STRUCT;
	}
	if (p_value == "normalize") {
		return ShaderLabTokenType::NORMALIZE;
	}

	// Rubbish tokens
	if (_is_rubbish_token(p_value)) {
		return ShaderLabTokenType::RUBBISH;
	}

	return ShaderLabTokenType::NONE;
}

bool UnityShaderConverter::_is_valid_integer(const String &p_str) {
	if (p_str.is_empty()) {
		return false;
	}
	return p_str.is_valid_int();
}

bool UnityShaderConverter::_is_rubbish_token(const String &p_value) {
	return rubbish_tokens.has(p_value);
}

ShaderLabToken *UnityShaderConverter::_strip_whitespace(ShaderLabToken *p_tokens) {
	if (!p_tokens) {
		return nullptr;
	}

	ShaderLabToken *head = memnew(ShaderLabToken);
	ShaderLabToken *current_new = head;
	ShaderLabToken *current_old = p_tokens;

	while (current_old && current_old->next) {
		ShaderLabTokenType t = current_old->next->type;
		if (t != ShaderLabTokenType::SPACE && t != ShaderLabTokenType::ENDLINE) {
			current_new->next = memnew(ShaderLabToken);
			current_new->next->type = current_old->next->type;
			current_new->next->original_data = current_old->next->original_data;
			current_new = current_new->next;
		}
		current_old = current_old->next;
	}

	return head;
}

String UnityShaderConverter::_translate_unity_function(const String &p_function_call) {
	for (const KeyValue<String, String> &E : unity_to_godot_functions) {
		if (p_function_call.contains(E.key)) {
			return p_function_call.replace(E.key, E.value);
		}
	}
	return p_function_call;
}

String UnityShaderConverter::_translate_unity_type(const String &p_type) {
	if (hlsl_to_glsl_types.has(p_type)) {
		return hlsl_to_glsl_types[p_type];
	}
	if (p_type.contains("2D")) {
		return "sampler2D";
	}
	if (p_type.contains("Color")) {
		return "vec4";
	}
	return "vec4";
}

String UnityShaderConverter::_translate_semantic(const String &p_semantic) {
	if (unity_semantics.has(p_semantic)) {
		return unity_semantics[p_semantic];
	}
	return p_semantic;
}

void UnityShaderConverter::_parse_properties(ShaderLabToken *&p_current, ShaderPropertiesNode *p_properties) {
	ShaderPropertyNode *last_prop = nullptr;
	int brace_depth = 1;
	
	while (p_current && brace_depth > 0) {
		if (p_current->type == ShaderLabTokenType::OPEN_CURLY) {
			brace_depth++;
		} else if (p_current->type == ShaderLabTokenType::CLOSE_CURLY) {
			brace_depth--;
			if (brace_depth == 0) {
				break;
			}
		}
		
		// Parse property: _Name("Display Name", Type) = default
		if (p_current->original_data.begins_with("_")) {
			ShaderPropertyNode *prop = memnew(ShaderPropertyNode);
			prop->name = p_current->original_data;
			
			// Skip to type (after opening paren and string)
			while (p_current && p_current->type != ShaderLabTokenType::COMMA) {
				p_current = p_current->next;
			}
			if (p_current && p_current->next) {
				p_current = p_current->next;
				prop->type_name = p_current->original_data;
			}
			
			if (!p_properties->properties) {
				p_properties->properties = prop;
			} else {
				last_prop->next_property = prop;
			}
			last_prop = prop;
		}
		
		if (!p_current) {
			break;
		}
		p_current = p_current->next;
	}
}

void UnityShaderConverter::_parse_struct(ShaderLabToken *&p_current, ShaderStruct &r_struct) {
	if (!p_current) {
		return;
	}
	p_current = p_current->next; // Skip 'struct'
	if (p_current) {
		r_struct.name = p_current->original_data;
		p_current = p_current->next;
	}
	
	// Skip to opening brace
	while (p_current && p_current->type != ShaderLabTokenType::OPEN_CURLY) {
		p_current = p_current->next;
	}
	if (p_current) {
		p_current = p_current->next;
	}
	
	ShaderStructMember *last_member = nullptr;
	
	// Parse members until closing brace
	while (p_current && p_current->type != ShaderLabTokenType::CLOSE_CURLY) {
		ShaderStructMember *member = memnew(ShaderStructMember);
		member->type = p_current->original_data;
		p_current = p_current->next;
		
		if (p_current) {
			member->name = p_current->original_data;
			p_current = p_current->next;
		}
		
		// Check for semantic
		if (p_current && p_current->type == ShaderLabTokenType::INHERITANCE) {
			p_current = p_current->next;
			if (p_current) {
				member->semantic = p_current->original_data;
				p_current = p_current->next;
			}
		}
		
		if (!r_struct.members) {
			r_struct.members = member;
		} else {
			last_member->next = member;
		}
		last_member = member;
		
		// Skip semicolon
		if (p_current && p_current->type == ShaderLabTokenType::SEMICOLON) {
			p_current = p_current->next;
		}
	}
}

void UnityShaderConverter::_parse_function(ShaderLabToken *&p_current, ShaderFunction &r_function) {
	r_function.return_type = p_current->original_data;
	p_current = p_current->next;
	
	if (p_current) {
		r_function.name = p_current->original_data;
		p_current = p_current->next;
	}
	
	// Parse parameters
	if (p_current && p_current->type == ShaderLabTokenType::OPEN_BRACKET) {
		p_current = p_current->next;
		while (p_current && p_current->type != ShaderLabTokenType::CLOSE_BRACKET) {
			r_function.parameters += p_current->original_data + " ";
			p_current = p_current->next;
		}
		if (p_current) {
			p_current = p_current->next;
		}
	}
	
	// Check for return semantic
	if (p_current && p_current->type == ShaderLabTokenType::INHERITANCE) {
		p_current = p_current->next;
		if (p_current) {
			r_function.return_semantic = p_current->original_data;
			p_current = p_current->next;
		}
	}
	
	// Parse body
	if (p_current && p_current->type == ShaderLabTokenType::OPEN_CURLY) {
		p_current = p_current->next;
		int brace_depth = 1;
		while (p_current && brace_depth > 0) {
			if (p_current->type == ShaderLabTokenType::OPEN_CURLY) {
				brace_depth++;
			} else if (p_current->type == ShaderLabTokenType::CLOSE_CURLY) {
				brace_depth--;
				if (brace_depth == 0) {
					break;
				}
			}
			r_function.body += p_current->original_data + " ";
			p_current = p_current->next;
		}
	}
}

String UnityShaderConverter::_convert_hlsl_to_glsl(const String &p_hlsl_code, bool p_is_vertex) {
	String glsl_code = p_hlsl_code;
	
	// Translate HLSL types to GLSL
	for (const KeyValue<String, String> &E : hlsl_to_glsl_types) {
		glsl_code = glsl_code.replace(E.key, E.value);
	}
	
	// Translate Unity functions
	for (const KeyValue<String, String> &E : unity_to_godot_functions) {
		glsl_code = glsl_code.replace(E.key, E.value);
	}
	
	// Translate semantics
	for (const KeyValue<String, String> &E : unity_semantics) {
		glsl_code = glsl_code.replace(E.key, E.value);
	}
	
	// Add proper indentation
	Vector<String> lines = glsl_code.split("\n");
	String result;
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (!line.is_empty()) {
			result += "\t" + line + "\n";
		}
	}
	
	return result;
}

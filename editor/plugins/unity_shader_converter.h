/**************************************************************************/
/*  unity_shader_converter.h                                              */
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

#ifndef UNITY_SHADER_CONVERTER_H
#define UNITY_SHADER_CONVERTER_H

#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"

// Token types for ShaderLab parsing
enum class ShaderLabTokenType {
	UNIDENTIFIED = -2,
	NONE = -1,
	STRING_LITERAL = 0,
	FLOAT_LITERAL = 1,
	INT_LITERAL = 2,
	CHAR_LITERAL = 3,
	VARIABLE = 4,
	ASSIGNMENT = 5,
	ADDITION = 6,
	SUBTRACTION = 7,
	ENDLINE = 8,
	SPACE = 9,
	RUBBISH = 10,
	OPEN_BRACKET = 11,
	CLOSE_BRACKET = 12,
	OPEN_CURLY = 13,
	CLOSE_CURLY = 14,
	COMMA = 15,
	STRUCT = 16,
	NORMALIZE = 17,
	COMMENT = 18,
	DOT = 19,
	INHERITANCE = 20,
	DATATYPE = 21,
	RETURN = 22,
	SEMICOLON = 23,
	FUNCTION = 24,
};

struct ShaderLabToken {
	ShaderLabTokenType type = ShaderLabTokenType::NONE;
	String original_data;
	ShaderLabToken *next = nullptr;

	~ShaderLabToken() {
		if (next) {
			delete next;
		}
	}
};

// AST Node structures
struct ShaderASTNode {
	ShaderASTNode *left = nullptr;
	ShaderASTNode *middle = nullptr;
	ShaderASTNode *right = nullptr;
	virtual ~ShaderASTNode() {
		if (left) {
			delete left;
		}
		if (middle) {
			delete middle;
		}
		if (right) {
			delete right;
		}
	}
};

struct ShaderPropertyNode : ShaderASTNode {
	String name;
	String type_name;
	ShaderPropertyNode *next_property = nullptr;
};

struct ShaderPropertiesNode : ShaderASTNode {
	ShaderPropertyNode *properties = nullptr;
};

struct ShaderPassNode : ShaderASTNode {
	String vertex_code;
	String fragment_code;
	String tags;
};

struct ShaderStructMember {
	String type;
	String name;
	String semantic;
	ShaderStructMember *next = nullptr;
};

struct ShaderStruct {
	String name;
	ShaderStructMember *members = nullptr;
};

struct ShaderFunction {
	String return_type;
	String name;
	String parameters;
	String body;
	String return_semantic;
};

struct ShaderNode : ShaderASTNode {
	String name;
	ShaderPropertiesNode *properties = nullptr;
	ShaderPassNode *pass = nullptr;
	Vector<ShaderStruct> structs;
	Vector<ShaderFunction> functions;
	HashMap<String, String> variables;
};

class UnityShaderConverter {
public:
	// Main conversion function: Unity ShaderLab -> Godot Shader
	static Error convert_shaderlab_to_godot(const String &p_shaderlab_content, String &r_godot_shader);

	// Tokenize ShaderLab source
	static ShaderLabToken *tokenize_shaderlab(const String &p_content);

	// Parse tokens into AST
	static ShaderNode *parse_shader_ast(ShaderLabToken *p_tokens);

	// Generate Godot shader from AST
	static String generate_godot_shader(ShaderNode *p_shader_node);

	// Utility functions
	static String get_token_symbol(ShaderLabTokenType p_type);
	static String get_token_name(ShaderLabTokenType p_type);

private:
	static ShaderLabTokenType _get_token_type(const String &p_value, bool p_is_extra);
	static bool _is_valid_integer(const String &p_str);
	static bool _is_rubbish_token(const String &p_value);
	static ShaderLabToken *_strip_whitespace(ShaderLabToken *p_tokens);
	static String _translate_unity_function(const String &p_function_call);
	static String _translate_unity_type(const String &p_type);
	static String _translate_semantic(const String &p_semantic);
	static void _parse_properties(ShaderLabToken *&p_current, ShaderPropertiesNode *p_properties);
	static void _parse_struct(ShaderLabToken *&p_current, ShaderStruct &r_struct);
	static void _parse_function(ShaderLabToken *&p_current, ShaderFunction &r_function);
	static String _convert_hlsl_to_glsl(const String &p_hlsl_code, bool p_is_vertex);

	static const HashSet<String> rubbish_tokens;
	static const HashMap<String, String> unity_to_godot_functions;
	static const HashMap<String, String> hlsl_to_glsl_types;
	static const HashMap<String, String> unity_semantics;
};

#endif // UNITY_SHADER_CONVERTER_H

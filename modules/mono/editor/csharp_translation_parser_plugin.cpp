/**************************************************************************/
/*  csharp_translation_parser_plugin.cpp                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors (see AUTHORS.md). */
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

#include "csharp_translation_parser_plugin.h"
#include "../csharp_script.h"

#include "core/io/resource_loader.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"


void CSharpEditorTranslationParserPlugin::get_recognized_extensions(List<String>* r_extensions) const {
	r_extensions->push_back("cs");
}

Error CSharpEditorTranslationParserPlugin::parse_file(const String &p_path, Vector<String> *r_ids, Vector<Vector<String>> *r_ids_ctx_plural) {
	Error err;
	Ref<Resource> loaded_res = ResourceLoader::load(p_path, "", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to load " + p_path);

	ids = r_ids;
	ids_ctx_plural = r_ids_ctx_plural;
	Ref<CSharpScript> csharp = loaded_res;
	String source_code = csharp->get_source_code();

	// Iterate through all matches.
	_find_translation_ids(source_code);

	return OK;
}

void CSharpEditorTranslationParserPlugin::_find_translation_ids(const String &source_code) {
	// Extract translation ids from Tr calls.
	int i = 0;
	Vector<Vector<String>> args = _find_method_args(source_code, tr_func);
	for (Vector<Vector<String>>::Iterator itr = args.begin(); itr != args.end(); ++itr, ++i) {
		_parse_tr_atr_args(&args[i]);
	}

	// Extract translation ids from TrN calls.
	i = 0;
	args = _find_method_args(source_code, trn_func);
	for (Vector<Vector<String>>::Iterator itr = args.begin(); itr != args.end(); ++itr, ++i) {
		_parse_trn_atrn_args(&args[i]);
	}

	// Extract translation ids from Atr calls.
	i = 0;
	args = _find_method_args(source_code, atr_func);
	for (Vector<Vector<String>>::Iterator itr = args.begin(); itr != args.end(); ++itr, ++i) {
		_parse_tr_atr_args(&args[i]);
	}

	// Extract translation ids from AtrN calls.
	i = 0;
	args = _find_method_args(source_code, atrn_func);
	for (Vector<Vector<String>>::Iterator itr = args.begin(); itr != args.end(); ++itr, ++i) {
		_parse_trn_atrn_args(&args[i]);
	}
}

Vector<Vector<String>> CSharpEditorTranslationParserPlugin::_find_method_args(const String &source_code, const String method_name) {
	Vector<Vector<String>> all_args;
	int start = 0;
	while ((start = source_code.find(method_name + "(", start)) != -1) {
		// Locate the opening and closing parentheses.
		int open_paren = source_code.find("(", start);
		int close_paren = source_code.find(")", open_paren);

		if (open_paren == -1 || close_paren == -1) {
			break;
		}

		// Extract arguments between the parentheses.
		Vector<String> args = source_code.substr(open_paren + 1, close_paren - open_paren - 1).split(",");

		if (args.size() == 0) {
			continue;
		}

		all_args.push_back(args);

		// Move past this method call for the next iteration.
		start = close_paren + 1;
	}

	return all_args;
}

void CSharpEditorTranslationParserPlugin::_parse_tr_atr_args(const Vector<String> *args) {
	if (args->size() < 1) {
		return;
	}

	String msg_id_arg = args->get(0);
	Vector<String> id_ctx;
	id_ctx.resize(3);

	// Skip non string literals.
	if (!_is_string_literal(msg_id_arg)) {
		return;
	}

	// Extract message id.
	String msg_id = msg_id_arg.strip_edges().substr(1, msg_id_arg.length() - 2);
	id_ctx.set(0, msg_id);

	// Extract context.
	if (args->size() == 2) {
		String ctx_arg = args->get(1);

		if (_is_string_literal(ctx_arg)) {
			String ctx = ctx_arg.strip_edges().substr(1, ctx_arg.length() - 2);
			id_ctx.set(1, ctx);
		}
	}

	ids_ctx_plural->push_back(id_ctx);
}

void CSharpEditorTranslationParserPlugin::_parse_trn_atrn_args(const Vector<String> *args) {
	if (args->size() < 3) {
		return;
	}

	String msg_id_singular_arg = args->get(0);
	String msg_id_plural_arg = args->get(1);
	Vector<String> id_ctx_plural;
	id_ctx_plural.resize(3);

	// Skip non string literals.
	if (!_is_string_literal(msg_id_singular_arg) || !_is_string_literal(msg_id_plural_arg)) {
		return;
	}

	// Extract singular message id.
	String msg_id_singular = msg_id_singular_arg.strip_edges().substr(1, msg_id_singular_arg.length() - 2);
	id_ctx_plural.set(0, msg_id_singular);

	// Extract plural message id.
	String msg_id_plural = msg_id_plural_arg.strip_edges().substr(1, msg_id_plural_arg.length() - 2);
	id_ctx_plural.set(2, msg_id_singular);

	bool extract_ctx = false;

	// Extract context.
	if (args->size() == 4) {
		String ctx_arg = args->get(3);

		if (_is_string_literal(ctx_arg)) {
			String ctx = ctx_arg.strip_edges().substr(1, ctx_arg.length() - 2);
			id_ctx_plural.set(1, ctx);
		}
	}

	ids_ctx_plural->push_back(id_ctx_plural);
}

bool CSharpEditorTranslationParserPlugin::_is_string_literal(const String code) {
	return code.begins_with("\"") || code.ends_with("\"");
}

CSharpEditorTranslationParserPlugin::CSharpEditorTranslationParserPlugin() {}

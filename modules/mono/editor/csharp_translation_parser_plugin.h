/**************************************************************************/
/*  csharp_translation_parser_plugin.h                                    */
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

#ifndef CSHARP_TRANSLATION_PARSER_PLUGIN_H
#define CSHARP_TRANSLATION_PARSER_PLUGIN_H

#include "core/templates/hash_set.h"
#include "editor/editor_translation_parser.h"
#include <core/error/error_list.h>
#include <core/object/object.h>
#include <core/string/string_name.h>
#include <core/string/ustring.h>
#include <core/templates/list.h>
#include <core/templates/vector.h>

class CSharpEditorTranslationParserPlugin : public EditorTranslationParserPlugin {
	GDCLASS(CSharpEditorTranslationParserPlugin, EditorTranslationParserPlugin);

	Vector<String> *ids = nullptr;
	Vector<Vector<String>> *ids_ctx_plural = nullptr;

	// List of patterns used for extracting translation strings.
	StringName tr_func = "Tr";
	StringName trn_func = "TrN";
	StringName atr_func = "Atr";
	StringName atrn_func = "AtrN";
	HashSet<StringName> assignment_patterns;
	HashSet<StringName> first_arg_patterns;
	HashSet<StringName> second_arg_patterns;
	// FileDialog patterns.
	StringName fd_add_filter = "add_filter";
	StringName fd_set_filter = "set_filters";
	StringName fd_filters = "filters";

	void _find_translation_ids(const String &source_code);
	Vector<Vector<String>> _find_method_args(const String &source_code, const String method_name);
	void _parse_tr_atr_args(const Vector<String> *args);
	void _parse_trn_atrn_args(const Vector<String> *args);
	bool _is_string_literal(const String code);

public:
	virtual Error parse_file(const String &p_path, Vector<String> *r_ids, Vector<Vector<String>> *r_ids_ctx_plural) override;
	virtual void get_recognized_extensions(List<String> *r_extensions) const override;

	CSharpEditorTranslationParserPlugin();
};

#endif // CSHARP_TRANSLATION_PARSER_PLUGIN_H

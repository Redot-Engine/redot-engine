/**************************************************************************/
/*  test_lookup.h                                                         */
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

#include "../gdscript_analyzer.h"
#include "../gdscript_parser.h"
#include "gdscript_test_runner.h"

#include "core/io/file_access.h"
#include "tests/test_macros.h"

namespace GDScriptTests {

static void check_member_lookup(const String &p_code, const String &p_path, const String &p_expression, const String &p_symbol, const String &p_expected_path, int p_expected_line, ScriptLanguage::LookupResultType p_expected_type, const String &p_expected_class, const String &p_expected_member) {
	INFO("Looking up ", p_symbol, " in ", p_path, " using ", p_expression);
	const String code = p_code + "\nfunc lookup_test() -> void:\n\t" + p_expression.replace("|", String::chr(0xFFFF)) + "\n";
	ScriptLanguage::LookupResult result;
	const Error error = GDScriptLanguage::get_singleton()->lookup_code(code, p_symbol, p_path, nullptr, result);
	CHECK(error == OK);
	if (error != OK) {
		return;
	}
	CHECK(result.script_path == p_expected_path);
	CHECK(result.script.is_valid());
	if (result.script.is_valid()) {
		CHECK(result.script->get_path() == p_expected_path);
	}
	CHECK(result.location == p_expected_line);
	CHECK(result.type == p_expected_type);
	CHECK(result.class_name == p_expected_class);
	CHECK(result.class_member == p_expected_member);
}

TEST_CASE("[Modules][GDScript] Lookup trait member declarations") {
	init_language("modules/gdscript/tests/scripts");
	const String trait_path = "res://lookup/trait_members.notest.gd";
	const String consumer_path = "res://lookup/trait_consumer.notest.gd";
	ScriptServer::add_global_class("LookupTrait", "RefCounted", "GDScript", trait_path, false, false);

	const String consumer_code = FileAccess::get_file_as_string(consumer_path);
	struct LookupScenario {
		const char *name;
		String path;
		String code;
	};
	const String child_path = "res://lookup/trait_child.notest.gd";
	const LookupScenario scenarios[] = {
		{ "Trait used by path", consumer_path, consumer_code },
		{ "Trait used by global name", consumer_path, consumer_code.replace("\"res://lookup/trait_members.notest.gd\"", "LookupTrait") },
		{ "Trait used through another trait", consumer_path, consumer_code.replace("trait_members.notest.gd", "trait_intermediate.notest.gd") },
		{ "Trait members inherited from a parent class", child_path, FileAccess::get_file_as_string(child_path) },
	};

	for (const LookupScenario &scenario : scenarios) {
		INFO(scenario.name);
		const String &path = scenario.path;
		const String &code = scenario.code;

		{
			GDScriptParser parser;
			CHECK(parser.parse(code, path, false) == OK);
			GDScriptAnalyzer analyzer(&parser);
			CHECK(analyzer.analyze() == OK);
		}

		check_member_lookup(code, path, "print(trait_value|)", "trait_value", trait_path, 6, ScriptLanguage::LOOKUP_RESULT_CLASS_PROPERTY, "LookupTrait", "trait_value");
		check_member_lookup(code, path, "print(self.trait_value|)", "trait_value", trait_path, 6, ScriptLanguage::LOOKUP_RESULT_CLASS_PROPERTY, "LookupTrait", "trait_value");
		check_member_lookup(code, path, "trait_signal|.emit()", "trait_signal", trait_path, 4, ScriptLanguage::LOOKUP_RESULT_CLASS_SIGNAL, "LookupTrait", "trait_signal");
		check_member_lookup(code, path, "trait_method|()", "trait_method", trait_path, 10, ScriptLanguage::LOOKUP_RESULT_CLASS_METHOD, "LookupTrait", "trait_method");
		check_member_lookup(code, path, "print(TRAIT_CONSTANT|)", "TRAIT_CONSTANT", trait_path, 8, ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT, "LookupTrait", "TRAIT_CONSTANT");
		check_member_lookup(code, path, "print(NamedEnum|)", "NamedEnum", trait_path, 18, ScriptLanguage::LOOKUP_RESULT_CLASS_ENUM, "LookupTrait", "NamedEnum");
		check_member_lookup(code, path, "print(NamedEnum.NAMED_VALUE|)", "NAMED_VALUE", trait_path, 19, ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT, "LookupTrait.NamedEnum", "NAMED_VALUE");
		check_member_lookup(code, path, "print(UNNAMED_VALUE|)", "UNNAMED_VALUE", trait_path, 23, ScriptLanguage::LOOKUP_RESULT_CLASS_CONSTANT, "LookupTrait", "UNNAMED_VALUE");
		check_member_lookup(code, path, "var value: TraitStruct|", "TraitStruct", trait_path, 26, ScriptLanguage::LOOKUP_RESULT_SCRIPT_LOCATION, String(), String());

		// Trait overrides and implementations must still navigate to the consuming class.
		const String consumer_class = "\"lookup/trait_consumer.notest.gd\"";
		check_member_lookup(code, path, "overridden_method|()", "overridden_method", consumer_path, 4, ScriptLanguage::LOOKUP_RESULT_CLASS_METHOD, consumer_class, "overridden_method");
		check_member_lookup(code, path, "implemented_method|()", "implemented_method", consumer_path, 7, ScriptLanguage::LOOKUP_RESULT_CLASS_METHOD, consumer_class, "implemented_method");
	}

	const String nested_path = "res://lookup/nested_trait.notest.gd";
	ScriptServer::add_global_class("LookupTraitHolder", "RefCounted", "GDScript", nested_path, false, false);
	const String code = "extends RefCounted\nuses LookupTraitHolder.InnerTrait\n";
	check_member_lookup(code, consumer_path, "print(nested_value|)", "nested_value", nested_path, 4, ScriptLanguage::LOOKUP_RESULT_CLASS_PROPERTY, "LookupTraitHolder.InnerTrait", "nested_value");

	finish_language();
}

} // namespace GDScriptTests

#endif // TOOLS_ENABLED

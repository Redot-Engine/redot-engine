/**************************************************************************/
/*  test_highlighter.h                                                    */
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

#include "../editor/gdscript_highlighter.h"
#include "../gdscript.h"
#include "../gdscript_cache.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/code_edit.h"

#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace GDScriptTests {

TEST_CASE("[Editor][Modules][GDScript] Static variables use member highlighting") {
	GDScriptLanguage::get_singleton()->init();
	const String base_path = TestUtils::get_temp_path("static_highlighting_base.gd");
	const String base_source = "extends Node\n\nvar test: int\nstatic var static_test: int\n\nfunc _ready() -> void:\n\tprint(test + static_test)\n";
	{
		Ref<FileAccess> file = FileAccess::open(base_path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string(base_source);
	}
	Ref<GDScript> base_script = ResourceLoader::load(base_path);
	REQUIRE(base_script.is_valid());
	REQUIRE(base_script->is_valid());

	const String source = "extends \"" + base_path + "\"\n"
													 "var derived_test: int\n"
													 "static var derived_static_test: int\n"
													 "func read_members() -> int:\n"
													 "\tvar resource_name: int = 0\n"
													 "\treturn test + static_test + derived_test + derived_static_test + resource_name\n";
	Ref<GDScript> script;
	script.instantiate();
	script->set_source_code(source);
	REQUIRE(script->reload() == OK);

	CodeEdit *code_edit = memnew(CodeEdit);
	SceneTree::get_singleton()->get_root()->add_child(code_edit);
	code_edit->add_theme_color_override("font_color", Color(0.8, 0.8, 0.8));
	Ref<GDScriptSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	highlighter->_set_edited_resource(base_script);
	code_edit->set_syntax_highlighter(highlighter);

	auto check_word_color = [&](int p_line, const String &p_word, const Color &p_expected) {
		const int column = code_edit->get_line(p_line).find(p_word);
		CHECK(column >= 0);
		const Dictionary regions = highlighter->get_line_syntax_highlighting(p_line);
		Array keys = regions.keys();
		keys.sort();
		Color color = code_edit->get_theme_color("font_color");
		for (int i = 0; i < keys.size(); i++) {
			if (int(keys[i]) > column) {
				break;
			}
			const Dictionary region = regions[keys[i]];
			color = region["color"];
		}
		INFO("Line: ", p_line, ", word: ", p_word);
		CHECK(color == p_expected);
	};

	EditorSettings *settings = EditorSettings::get_singleton();
	const String setting = "text_editor/theme/highlighting/member_variable_color";
	const Color original_member_color = settings->get_setting(setting);
	for (const Color &member_color : { original_member_color, Color(0.2, 0.4, 0.6) }) {
		settings->set_setting(setting, member_color);
		highlighter->_set_edited_resource(base_script);
		code_edit->set_text(base_source);
		highlighter->update_cache();
		check_word_color(2, "test", member_color);
		check_word_color(3, "static_test", member_color);
		check_word_color(6, "test", member_color);
		check_word_color(6, "static_test", member_color);

		highlighter->_set_edited_resource(script);
		code_edit->set_text(source);
		highlighter->update_cache();
		check_word_color(1, "derived_test", member_color);
		check_word_color(2, "derived_static_test", member_color);
		for (const String &word : { "test", "static_test", "derived_test", "derived_static_test" }) {
			check_word_color(5, word, member_color);
		}
		// Resource's built-in properties are not members of the edited script.
		const Color font_color = code_edit->get_theme_color("font_color");
		check_word_color(4, "resource_name", font_color);
		check_word_color(5, "resource_name", font_color);
	}
	settings->set_setting(setting, original_member_color);

	memdelete(code_edit);
	highlighter.unref();
	// Static accesses can retain script references in bytecode constants.
	script->clear();
	base_script->clear();
	GDScriptCache::remove_static_script(script->get_fully_qualified_name());
	GDScriptCache::remove_static_script(base_script->get_fully_qualified_name());
	script.unref();
	base_script.unref();
	GDScriptCache::remove_parser(base_path);
	CHECK(DirAccess::remove_absolute(base_path) == OK);
}

TEST_CASE("[Editor][Modules][GDScript] Trait and struct source highlighting") {
	CodeEdit *code_edit = memnew(CodeEdit);
	SceneTree::get_singleton()->get_root()->add_child(code_edit);
	const Color font_color(0.8, 0.8, 0.8);
	code_edit->add_theme_color_override("font_color", font_color);
	Ref<GDScriptSyntaxHighlighter> highlighter;
	highlighter.instantiate();
	code_edit->set_syntax_highlighter(highlighter);
	const Color member_color = EDITOR_GET("text_editor/theme/highlighting/member_variable_color");
	const Color type_color = EDITOR_GET("text_editor/theme/highlighting/user_type_color");

	auto check_word = [&](const String &p_line, const String &p_word, const Color &p_expected, int p_occurrence = 0) {
		int line = -1;
		for (int i = 0; i < code_edit->get_line_count(); i++) {
			if (code_edit->get_line(i).strip_edges() == p_line) {
				line = i;
				break;
			}
		}
		REQUIRE(line >= 0);
		int column = -1;
		for (int i = 0; i <= p_occurrence; i++) {
			column = code_edit->get_line(line).find(p_word, column + 1);
			REQUIRE(column >= 0);
		}
		const Dictionary regions = highlighter->get_line_syntax_highlighting(line);
		Array keys = regions.keys();
		keys.sort();
		Color color = font_color;
		for (const Variant &key : keys) {
			if (int(key) > column) {
				break;
			}
			const Dictionary region = regions[key];
			color = region["color"];
		}
		INFO("Line: ", p_line, ", word: ", p_word);
		CHECK(color == p_expected);
	};

	SUBCASE("Local traits, composition, nested types and local shadowing") {
		code_edit->set_text(R"GD(extends Node
trait TestTrait:
	var inner_var: float = 3.14
	signal changed(inner_var: float)
	static var static_inner: float = 1.0
	struct InnerStruct:
		var field: float = 2.0
	func get_inner() -> float:
		var local_var: float = 1.0
		return inner_var + static_inner + local_var
	func parameter_shadow(inner_var: float) -> float:
		return inner_var
	func local_shadow() -> float:
		var inner_var: float = 0.0
		return inner_var + 1.0
	func lambda_shadow() -> Callable:
		return func(inner_var: float): return inner_var + 2.0
	func loop_shadow() -> void:
		for inner_var in [1.0]:
			print(inner_var)
	func match_shadow(value: Variant) -> float:
		match value:
			var inner_var:
				return inner_var + 3.0
		return inner_var + 4.0
trait DerivedTrait:
	uses TestTrait
	func get_inherited() -> float:
		return inner_var + 5.0
class Holder:
	struct OtherStruct:
		var other_field: int
func outside() -> int:
	var inner_var: int = 0
	var point := TestTrait.InnerStruct.new()
	var other: Holder.OtherStruct = Holder.OtherStruct.new()
	var value := {"InnerStruct": 0}
	return inner_var + int(point.field) + value.InnerStruct
)GD");
		highlighter->update_cache();
		check_word("trait TestTrait:", "TestTrait", type_color);
		check_word("var inner_var: float = 3.14", "inner_var", member_color);
		check_word("signal changed(inner_var: float)", "inner_var", font_color);
		check_word("static var static_inner: float = 1.0", "static_inner", member_color);
		check_word("return inner_var + static_inner + local_var", "inner_var", member_color);
		check_word("return inner_var + static_inner + local_var", "static_inner", member_color);
		check_word("return inner_var + static_inner + local_var", "local_var", font_color);
		check_word("var local_var: float = 1.0", "local_var", font_color);
		check_word("func parameter_shadow(inner_var: float) -> float:", "inner_var", font_color);
		check_word("return inner_var", "inner_var", font_color);
		check_word("var inner_var: float = 0.0", "inner_var", font_color);
		check_word("return inner_var + 1.0", "inner_var", font_color);
		check_word("return func(inner_var: float): return inner_var + 2.0", "inner_var", font_color);
		check_word("return func(inner_var: float): return inner_var + 2.0", "inner_var", font_color, 1);
		check_word("for inner_var in [1.0]:", "inner_var", font_color);
		check_word("print(inner_var)", "inner_var", font_color);
		check_word("var inner_var:", "inner_var", font_color);
		check_word("return inner_var + 3.0", "inner_var", font_color);
		check_word("return inner_var + 4.0", "inner_var", member_color);
		check_word("uses TestTrait", "TestTrait", type_color);
		check_word("return inner_var + 5.0", "inner_var", member_color);
		check_word("struct InnerStruct:", "InnerStruct", type_color);
		check_word("var field: float = 2.0", "field", member_color);
		check_word("var point := TestTrait.InnerStruct.new()", "TestTrait", type_color);
		check_word("var point := TestTrait.InnerStruct.new()", "InnerStruct", type_color);
		check_word("var other: Holder.OtherStruct = Holder.OtherStruct.new()", "OtherStruct", type_color);
		check_word("var other: Holder.OtherStruct = Holder.OtherStruct.new()", "OtherStruct", type_color, 1);
		check_word("var inner_var: int = 0", "inner_var", font_color);
		check_word("return inner_var + int(point.field) + value.InnerStruct", "inner_var", font_color);
		check_word("return inner_var + int(point.field) + value.InnerStruct", "field", member_color);
		check_word("return inner_var + int(point.field) + value.InnerStruct", "InnerStruct", member_color);
	}

	SUBCASE("Property accessors keep their own locals") {
		code_edit->set_text(R"GD(trait PropertyTrait:
	var inner_var: int = 0
	var inline_field: int:
		get:
			var inner_var: int = 1
			return inner_var
		set(inner_var):
			inline_field = inner_var
	var named_field: int: get = read_field, set = write_field
	func read_field() -> int:
		return inner_var + 1
	func write_field(value: int) -> void:
		inner_var = value
)GD");
		highlighter->update_cache();
		check_word("var inner_var: int = 0", "inner_var", member_color);
		check_word("var inline_field: int:", "inline_field", member_color);
		check_word("var inner_var: int = 1", "inner_var", font_color);
		check_word("return inner_var", "inner_var", font_color);
		check_word("set(inner_var):", "inner_var", font_color);
		check_word("inline_field = inner_var", "inline_field", member_color);
		check_word("inline_field = inner_var", "inner_var", font_color);
		check_word("var named_field: int: get = read_field, set = write_field", "named_field", member_color);
		check_word("return inner_var + 1", "inner_var", member_color);
		check_word("inner_var = value", "inner_var", member_color);
		check_word("inner_var = value", "value", font_color);
	}

	SUBCASE("External trait composition and qualified types use source declarations") {
		if (GDScriptLanguage::get_singleton()->get_global_map().is_empty()) {
			GDScriptLanguage::get_singleton()->init();
		}
		const String base_path = TestUtils::get_temp_path("highlighter_external_trait.gd");
		const String derived_path = TestUtils::get_temp_path("highlighter_derived_trait.gd");
		const String base_source = "trait_name HighlighterExternalTrait\nvar inherited: int = 1\nstruct ExternalStruct:\n\tvar field: int\n";
		const String derived_source = "trait_name HighlighterDerivedTrait\nuses \"" + base_path + "\"\n"
																								  "func read_field() -> int:\n\treturn inherited\n"
																								  "func read_type() -> int:\n\tvar point := HighlighterExternalTrait.ExternalStruct.new()\n\treturn point.field\n"
																								  "trait LocalDerived:\n\tuses \"" +
				base_path + "\"\n\tfunc read_field() -> int:\n\t\treturn inherited + 1\n";
		for (const KeyValue<String, String> &file : { KeyValue<String, String>(base_path, base_source), KeyValue<String, String>(derived_path, derived_source) }) {
			Ref<FileAccess> output = FileAccess::open(file.key, FileAccess::WRITE);
			REQUIRE(output.is_valid());
			output->store_string(file.value);
		}
		const StringName trait_name = "HighlighterExternalTrait";
		REQUIRE_FALSE(ScriptServer::is_global_class(trait_name));
		ScriptServer::add_global_class(trait_name, "RefCounted", "GDScript", base_path, false, false);
		Ref<GDScript> derived;
		derived.instantiate();
		derived->set_path(derived_path);
		derived->set_source_code(derived_source);
		highlighter->_set_edited_resource(derived);
		code_edit->set_text(derived_source);
		highlighter->update_cache();
		check_word("return inherited", "inherited", member_color);
		check_word("return inherited + 1", "inherited", member_color);
		check_word("var point := HighlighterExternalTrait.ExternalStruct.new()", "HighlighterExternalTrait", type_color);
		check_word("var point := HighlighterExternalTrait.ExternalStruct.new()", "ExternalStruct", type_color);
		check_word("return point.field", "field", member_color);
		// Removing a use must also remove the imported members during an edit.
		code_edit->set_text("trait_name HighlighterDerivedTrait\nfunc read_field():\n\treturn inherited\n");
		check_word("return inherited", "inherited", font_color);
		highlighter->_set_edited_resource(Ref<Resource>());
		derived->clear();
		derived.unref();
		ScriptServer::remove_global_class(trait_name);
		GDScriptCache::remove_script(derived_path);
		GDScriptCache::remove_script(base_path);
		CHECK(DirAccess::remove_absolute(derived_path) == OK);
		CHECK(DirAccess::remove_absolute(base_path) == OK);
	}

	SUBCASE("Global trait fields") {
		code_edit->set_text("trait_name GlobalTrait\nvar global_field: float = 1.0\nfunc read_field() -> float:\n\treturn global_field\n");
		highlighter->update_cache();
		check_word("trait_name GlobalTrait", "GlobalTrait", type_color);
		check_word("var global_field: float = 1.0", "global_field", member_color);
		check_word("return global_field", "global_field", member_color);
	}

	SUBCASE("Comments and strings do not declare types") {
		code_edit->set_text(R"GD(extends Node
const DESCRIPTION = """
struct PhantomStruct:
	var phantom_field: float
trait PhantomTrait:
	var phantom_member: int
"""
# struct CommentStruct:
# trait CommentTrait:
func get_text() -> int:
	var PhantomStruct: int = 0
	var PhantomTrait: int = 0
	var CommentStruct: int = 0
	var CommentTrait: int = 0
	return PhantomStruct + PhantomTrait + CommentStruct + CommentTrait
)GD");
		highlighter->update_cache();
		for (const String &name : { "PhantomStruct", "PhantomTrait", "CommentStruct", "CommentTrait" }) {
			check_word("var " + name + ": int = 0", name, font_color);
			check_word("return PhantomStruct + PhantomTrait + CommentStruct + CommentTrait", name, font_color);
		}
	}

	SUBCASE("Recovered declarations and editing invalidate earlier references") {
		code_edit->set_text("func before():\n\treturn LaterTrait\ntrait LaterTrait:\n\tvar field: int = 0\n\tfunc broken():\n\t\treturn field +\n");
		highlighter->update_cache();
		check_word("return LaterTrait", "LaterTrait", type_color);
		check_word("var field: int = 0", "field", member_color);
		check_word("return field +", "field", member_color);
		// A later declaration changes an already cached line without update_cache().
		code_edit->set_line(2, "trait RenamedTrait:");
		check_word("return LaterTrait", "LaterTrait", font_color);
		check_word("trait RenamedTrait:", "RenamedTrait", type_color);
	}

	memdelete(code_edit);
}

} // namespace GDScriptTests

#endif // TOOLS_ENABLED

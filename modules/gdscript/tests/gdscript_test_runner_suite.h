/**************************************************************************/
/*  gdscript_test_runner_suite.h                                          */
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

/**
 * @file gdscript_test_runner_suite.h
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "gdscript_test_runner.h"

#include "../gdscript_analyzer.h"
#include "../gdscript_parser.h"
#include "../gdscript_warning.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "scene/resources/packed_scene.h"

#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace GDScriptTests {

#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
TEST_CASE("[Modules][GDScript] Experimental struct warning") {
	const String warning_setting = GDScriptWarning::get_settings_path_from_code(GDScriptWarning::EXPERIMENTAL_STRUCT);
	const String show_warning_setting = "debug/gdscript/warnings/show_experimental_struct_warning";
	const int original_warning_level = GLOBAL_GET(warning_setting);
	const bool original_show_warning = GLOBAL_GET(show_warning_setting);
	const bool original_warnings_enabled = GLOBAL_GET("debug/gdscript/warnings/enable");
	ProjectSettings::get_singleton()->set_setting("debug/gdscript/warnings/enable", true);

	auto count_struct_warnings = [&](bool p_show_warning, GDScriptWarning::WarnLevel p_level) {
		ProjectSettings::get_singleton()->set_setting(show_warning_setting, p_show_warning);
		ProjectSettings::get_singleton()->set_setting(warning_setting, p_level);

		GDScriptParser parser;
		const Error parse_error = parser.parse(R"(
struct First:
	var value: int

struct Second:
	var value: int
)",
				"res://experimental_struct_warning.gd", false);
		CHECK(parse_error == OK);

		GDScriptAnalyzer analyzer(&parser);
		const Error analyze_error = analyzer.analyze();
		CHECK(analyze_error == OK);

		int warning_count = 0;
		for (const GDScriptWarning &warning : parser.get_warnings()) {
			if (warning.code == GDScriptWarning::EXPERIMENTAL_STRUCT) {
				warning_count++;
			}
		}
		return warning_count;
	};

	CHECK(count_struct_warnings(true, GDScriptWarning::WARN) == 1);
	CHECK(count_struct_warnings(false, GDScriptWarning::WARN) == 0);
	CHECK(count_struct_warnings(true, GDScriptWarning::IGNORE) == 0);

	ProjectSettings::get_singleton()->set_setting(warning_setting, original_warning_level);
	ProjectSettings::get_singleton()->set_setting(show_warning_setting, original_show_warning);
	ProjectSettings::get_singleton()->set_setting("debug/gdscript/warnings/enable", original_warnings_enabled);
}
#endif

/// @todo Handle some cases failing on release builds. See: https://github.com/godotengine/godot/pull/88452
#ifdef TOOLS_ENABLED
TEST_SUITE("[Modules][GDScript]") {
	TEST_CASE("[SceneTree] Script compilation and runtime") {
		bool print_filenames = OS::get_singleton()->get_cmdline_args().find("--print-filenames") != nullptr;
		bool use_binary_tokens = OS::get_singleton()->get_cmdline_args().find("--use-binary-tokens") != nullptr;
		GDScriptTestRunner runner("modules/gdscript/tests/scripts", true, print_filenames, use_binary_tokens);
		int fail_count = runner.run_tests();
		INFO("Make sure `*.out` files have expected results.");
		REQUIRE_MESSAGE(fail_count == 0, "All GDScript tests should pass.");
	}
}
#endif // TOOLS_ENABLED

#ifdef TOOLS_ENABLED
TEST_CASE("[Modules][GDScript] Trait edits refresh consumer exports") {
	if (ProjectSettings::get_singleton()->get_resource_path().is_empty()) {
		REQUIRE(ProjectSettings::get_singleton()->setup("modules/gdscript/tests/scripts", String(), true) == OK);
	}
	GDScriptLanguage::get_singleton()->init();
	const String fixture_dir = ProjectSettings::get_singleton()->localize_path(TestUtils::get_executable_dir().path_join("../modules/gdscript/tests/scripts/Traits/analyzer/features").simplify_path());
	const String trait_path = fixture_dir.path_join("trait_export_refresh_trait.notest.gd");
	const String consumer_path = fixture_dir.path_join("trait_export_refresh_consumer.notest.gd");
	const String transitive_path = fixture_dir.path_join("trait_export_refresh_transitive.notest.gd");
	const String inner_path = fixture_dir.path_join("trait_export_refresh_inner.notest.gd");
	const String extra_path = fixture_dir.path_join("trait_export_refresh_extra.notest.gd");
	Ref<GDScript> trait = ResourceLoader::load(trait_path);
	Ref<GDScript> consumer = ResourceLoader::load(consumer_path);
	Ref<GDScript> transitive = ResourceLoader::load(transitive_path);
	Ref<GDScript> inner_root = ResourceLoader::load(inner_path);
	Ref<GDScript> extra = ResourceLoader::load(extra_path);
	CHECK(trait.is_valid());
	CHECK(consumer.is_valid());
	CHECK(transitive.is_valid());
	CHECK(inner_root.is_valid());
	CHECK(extra.is_valid());
	if (trait.is_null() || consumer.is_null() || transitive.is_null() || inner_root.is_null() || extra.is_null()) {
		return;
	}
	CHECK(trait_path.is_resource_file());
	CHECK(trait->get_path().is_resource_file());
	CHECK_FALSE(trait->get_fully_qualified_name().is_empty());
	CHECK(consumer->has_trait(StringName(trait->get_fully_qualified_name())));
	REQUIRE(consumer->is_valid());
	REQUIRE(transitive->is_valid());
	REQUIRE(inner_root->is_valid());
	if (!consumer->is_valid() || !transitive->is_valid() || !inner_root->is_valid()) {
		return;
	}
	Ref<GDScript> inner = inner_root->find_class("Inner");
	REQUIRE(inner.is_valid());
	if (inner.is_null()) {
		return;
	}
	CHECK_FALSE(inner_root->has_trait(StringName(trait->get_fully_qualified_name())));
	CHECK(inner->has_trait(StringName(trait->get_fully_qualified_name())));
	const bool old_editor_hint = Engine::get_singleton()->is_editor_hint();
	Engine::get_singleton()->set_editor_hint(false);
	Ref<RefCounted> live_object;
	live_object.instantiate();
	live_object->set_script(consumer);
	ScriptInstance *live_instance = live_object->get_script_instance();
	CHECK(live_instance != nullptr);
	if (live_instance == nullptr) {
		Engine::get_singleton()->set_editor_hint(old_editor_hint);
		return;
	}
	CHECK_FALSE(live_instance->is_placeholder());
	live_object->set(SNAME("own"), 42);
	CHECK(int(live_object->get(SNAME("own"))) == 42);

	Ref<RefCounted> object;
	object.instantiate();
	PlaceHolderScriptInstance *placeholder = consumer->placeholder_instance_create(object.ptr());
	Ref<RefCounted> transitive_object;
	transitive_object.instantiate();
	PlaceHolderScriptInstance *transitive_placeholder = transitive->placeholder_instance_create(transitive_object.ptr());
	Ref<RefCounted> inner_object;
	inner_object.instantiate();
	PlaceHolderScriptInstance *inner_placeholder = inner->placeholder_instance_create(inner_object.ptr());
	Ref<RefCounted> inner_root_object;
	inner_root_object.instantiate();
	PlaceHolderScriptInstance *inner_root_placeholder = inner_root->placeholder_instance_create(inner_root_object.ptr());
	const String original_source = trait->get_source_code();
	const String original_extra_source = extra->get_source_code();
	Engine::get_singleton()->set_editor_hint(true);

	auto has_export = [](PlaceHolderScriptInstance *p_placeholder, const StringName &p_name) {
		List<PropertyInfo> properties;
		p_placeholder->get_property_list(&properties);
		for (const PropertyInfo &property : properties) {
			if (property.name == p_name) {
				return true;
			}
		}
		return false;
	};

	CHECK(has_export(placeholder, SNAME("own")));
	CHECK_FALSE(has_export(placeholder, SNAME("added")));
	CHECK_FALSE(has_export(transitive_placeholder, SNAME("added")));
	CHECK(has_export(inner_placeholder, SNAME("inner_own")));
	CHECK(has_export(inner_placeholder, SNAME("original")));
	CHECK_FALSE(has_export(inner_placeholder, SNAME("added")));
	CHECK_FALSE(has_export(inner_root_placeholder, SNAME("original")));
	CHECK(placeholder->set(SNAME("own"), 42));
	trait->set_source_code(original_source + "\n@export var added: int = 3\n");
	trait->update_exports();
	CHECK(has_export(placeholder, SNAME("added")));
	CHECK(has_export(transitive_placeholder, SNAME("added")));
	CHECK(has_export(inner_placeholder, SNAME("added")));
	CHECK(consumer->debug_get_member_indices().has(SNAME("added")));
	CHECK(inner->debug_get_member_indices().has(SNAME("added")));
	CHECK(int(live_object->get(SNAME("own"))) == 42);
	Variant own_value;
	CHECK(placeholder->get(SNAME("own"), own_value));
	CHECK(int(own_value) == 42);
	trait->set_source_code(original_source);
	trait->update_exports();
	CHECK_FALSE(has_export(placeholder, SNAME("added")));
	CHECK_FALSE(has_export(transitive_placeholder, SNAME("added")));
	CHECK_FALSE(has_export(inner_placeholder, SNAME("added")));
	CHECK_FALSE(consumer->debug_get_member_indices().has(SNAME("added")));
	CHECK(has_export(placeholder, SNAME("own")));

	trait->set_source_code(original_source.replace("trait_name ExportRefreshTrait", "trait_name ExportRefreshTrait\nuses \"trait_export_refresh_extra.notest.gd\""));
	trait->update_exports();
	CHECK(consumer->has_trait(StringName(extra->get_fully_qualified_name())));
	extra->set_source_code(original_extra_source + "\n@export var extra_added: int = 6\n");
	extra->update_exports();
	CHECK(has_export(placeholder, SNAME("extra_added")));
	CHECK(consumer->debug_get_member_indices().has(SNAME("extra_added")));
	extra->set_source_code(original_extra_source);
	extra->update_exports();
	trait->set_source_code(original_source);
	trait->update_exports();

	Engine::get_singleton()->set_editor_hint(old_editor_hint);
	memdelete(placeholder);
	memdelete(transitive_placeholder);
	memdelete(inner_placeholder);
	memdelete(inner_root_placeholder);
}
#endif // TOOLS_ENABLED

#ifdef TOOLS_ENABLED
TEST_CASE("[Modules][GDScript] Trait-typed exported arrays retain node references") {
	if (ProjectSettings::get_singleton()->get_resource_path().is_empty()) {
		REQUIRE(ProjectSettings::get_singleton()->setup("modules/gdscript/tests/scripts", String(), true) == OK);
	}
	GDScriptLanguage::get_singleton()->init();
	const String script_path = ProjectSettings::get_singleton()->localize_path(TestUtils::get_executable_dir().path_join("../modules/gdscript/tests/scripts/Traits/analyzer/features/trait_exported_array_scene.notest.gd").simplify_path());
	Ref<GDScript> script = ResourceLoader::load(script_path);
	REQUIRE(script.is_valid());
	REQUIRE(script->is_valid());
	Ref<GDScript> owner_script = script->find_class("Owner");
	Ref<GDScript> enemy_script = script->find_class("Enemy");
	REQUIRE(owner_script.is_valid());
	REQUIRE(enemy_script.is_valid());

	Node *owner = memnew(Node);
	owner->set_name("Owner");
	owner->set_script(owner_script);
	Node *enemy = memnew(Node);
	enemy->set_name("Enemy");
	enemy->set_script(enemy_script);
	owner->add_child(enemy);
	enemy->set_owner(owner);

	Array targets = owner->get("targets");
	const StringName damageable_trait = script->get_fully_qualified_name() + "::Damageable";
	CHECK(targets.get_typed_builtin() == Variant::OBJECT);
	CHECK(targets.get_typed_class_name() == damageable_trait);
	CHECK(enemy_script->has_trait(targets.get_typed_class_name()));
	targets.push_back(enemy);
	CHECK(targets.size() == 1);
	Array nodes;
	nodes.set_typed(Variant::OBJECT, "Node", Variant());
	nodes.assign(targets);
	CHECK(nodes.size() == 1);
	Array converted;
	converted.set_typed(Variant::OBJECT, targets.get_typed_class_name(), Variant());
	converted.assign(nodes);
	CHECK(converted.size() == 1);

	Node *unrelated = memnew(Node);
	ERR_PRINT_OFF;
	targets.push_back(unrelated);
	ERR_PRINT_ON;
	CHECK(targets.size() == 1);
	nodes.push_back(unrelated);
	ERR_PRINT_OFF;
	converted.assign(nodes);
	ERR_PRINT_ON;
	CHECK(converted.size() == 1);
	memdelete(unrelated);

	bool valid = false;
	owner->set("targets", targets, &valid);
	CHECK(valid);
	CHECK(Array(owner->get("targets")).size() == 1);

	Ref<PackedScene> scene;
	scene.instantiate();
	CHECK(scene->pack(owner) == OK);
	Node *loaded = scene->instantiate();
	REQUIRE(loaded != nullptr);
	Array loaded_targets = loaded->get("targets");
	REQUIRE(loaded_targets.size() == 1);
	CHECK(loaded_targets.get_typed_class_name() == targets.get_typed_class_name());
	Object *loaded_target = loaded_targets[0];
	CHECK(loaded_target == loaded->get_node(NodePath("Enemy")));

	memdelete(loaded);
	memdelete(owner);
}
#endif // TOOLS_ENABLED

TEST_CASE("[Modules][GDScript] Load source code dynamically and run it") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> gdscript = memnew(GDScript);
	gdscript->set_source_code(R"(
extends RefCounted

func _init():
	set_meta("result", 42)
)");
	// A spurious `Condition "err" is true` message is printed (despite parsing being successful and returning `OK`).
	// Silence it.
	ERR_PRINT_OFF;
	const Error error = gdscript->reload();
	ERR_PRINT_ON;
	CHECK_MESSAGE(error == OK, "The script should parse successfully.");

	// Run the script by assigning it to a reference-counted object.
	Ref<RefCounted> ref_counted = memnew(RefCounted);
	ref_counted->set_script(gdscript);
	CHECK_MESSAGE(int(ref_counted->get_meta("result")) == 42, "The script should assign object metadata successfully.");
}

TEST_CASE("[Modules][GDScript] Validate built-in API") {
	GDScriptLanguage *lang = GDScriptLanguage::get_singleton();

	// Validate methods.
	List<MethodInfo> builtin_methods;
	lang->get_public_functions(&builtin_methods);

	SUBCASE("[Modules][GDScript] Validate built-in methods") {
		for (const MethodInfo &mi : builtin_methods) {
			for (int64_t i = 0; i < mi.arguments.size(); ++i) {
				TEST_COND((mi.arguments[i].name.is_empty() || mi.arguments[i].name.begins_with("_unnamed_arg")),
						vformat("Unnamed argument in position %d of built-in method '%s'.", i, mi.name));
			}
		}
	}

	// Validate annotations.
	List<MethodInfo> builtin_annotations;
	lang->get_public_annotations(&builtin_annotations);

	SUBCASE("[Modules][GDScript] Validate built-in annotations") {
		for (const MethodInfo &ai : builtin_annotations) {
			for (int64_t i = 0; i < ai.arguments.size(); ++i) {
				TEST_COND((ai.arguments[i].name.is_empty() || ai.arguments[i].name.begins_with("_unnamed_arg")),
						vformat("Unnamed argument in position %d of built-in annotation '%s'.", i, ai.name));
			}
		}
	}
}

} // namespace GDScriptTests

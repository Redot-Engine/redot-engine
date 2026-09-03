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

#include "core/config/project_settings.h"

#include "tests/test_macros.h"

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

/**************************************************************************/
/*  test_completion.h                                                     */
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

#include "tests/test_macros.h"

#include "../gdscript.h"
#include "gdscript_test_runner.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/script_language.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "editor/settings/editor_settings.h"
#include "scene/resources/packed_scene.h"
#include "scene/theme/theme_db.h"

#include "modules/modules_enabled.gen.h" // For mono.

namespace GDScriptTests {

static bool match_option(const Dictionary p_expected, const ScriptLanguage::CodeCompletionOption p_got) {
	if (p_expected.get("display", p_got.display) != p_got.display) {
		return false;
	}
	if (p_expected.get("insert_text", p_got.insert_text) != p_got.insert_text) {
		return false;
	}
	if (p_expected.get("kind", p_got.kind) != Variant(p_got.kind)) {
		return false;
	}
	if (p_expected.get("location", p_got.location) != Variant(p_got.location)) {
		return false;
	}
	return true;
}

static void to_dict_list(Variant p_variant, List<Dictionary> &p_list) {
	ERR_FAIL_COND(!p_variant.is_array());

	Array arr = p_variant;
	for (int i = 0; i < arr.size(); i++) {
		if (arr[i].get_type() == Variant::DICTIONARY) {
			p_list.push_back(arr[i]);
		}
	}
}

static void test_directory(const String &p_dir) {
	Error err = OK;
	Ref<DirAccess> dir = DirAccess::open(p_dir, &err);

	if (err != OK) {
		FAIL("Invalid test directory.");
		return;
	}

	String path = dir->get_current_dir();

	dir->list_dir_begin();
	String next = dir->get_next();

	while (!next.is_empty()) {
		if (dir->current_is_dir()) {
			if (next == "." || next == "..") {
				next = dir->get_next();
				continue;
			}
			test_directory(path.path_join(next));
		} else if (next.ends_with(".gd") && !next.ends_with(".notest.gd")) {
			Ref<FileAccess> acc = FileAccess::open(path.path_join(next), FileAccess::READ, &err);

			if (err != OK) {
				next = dir->get_next();
				continue;
			}

			String code = acc->get_as_utf8_string();
			// For ease of reading ➡ (0x27A1) acts as sentinel char instead of 0xFFFF in the files.
			code = code.replace_first(String::chr(0x27A1), String::chr(0xFFFF));
			// Require pointer sentinel char in scripts.
			int location = code.find_char(0xFFFF);
			CHECK(location != -1);

			String res_path = ProjectSettings::get_singleton()->localize_path(path.path_join(next));

			ConfigFile conf;
			if (conf.load(path.path_join(next.get_basename() + ".cfg")) != OK) {
				FAIL("No config file found.");
			}

#ifndef MODULE_MONO_ENABLED
			if (conf.get_value("input", "cs", false)) {
				next = dir->get_next();
				continue;
			}
#endif

			EditorSettings::get_singleton()->set_setting("text_editor/completion/use_single_quotes", conf.get_value("input", "use_single_quotes", false));
			EditorSettings::get_singleton()->set_setting("text_editor/completion/add_node_path_literals", conf.get_value("input", "add_node_path_literals", false));
			EditorSettings::get_singleton()->set_setting("text_editor/completion/add_string_name_literals", conf.get_value("input", "add_string_name_literals", false));

			List<Dictionary> include;
			to_dict_list(conf.get_value("output", "include", Array()), include);

			List<Dictionary> exclude;
			to_dict_list(conf.get_value("output", "exclude", Array()), exclude);

			List<ScriptLanguage::CodeCompletionOption> options;
			String call_hint;
			bool forced;

			Node *scene = nullptr;
			if (conf.has_section_key("input", "scene")) {
				Ref<PackedScene> packed_scene = ResourceLoader::load(conf.get_value("input", "scene"), "PackedScene", ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP);
				if (packed_scene.is_valid()) {
					scene = packed_scene->instantiate();
				}
			} else if (dir->file_exists(next.get_basename() + ".tscn")) {
				Ref<PackedScene> packed_scene = ResourceLoader::load(path.path_join(next.get_basename() + ".tscn"), "PackedScene");
				if (packed_scene.is_valid()) {
					scene = packed_scene->instantiate();
				}
			}
			Node *owner = nullptr;
			if (scene != nullptr) {
				owner = scene->get_node(conf.get_value("input", "node_path", "."));
			}

			// The only requirement is for the script to be parsable, warnings and errors from the analyzer might happen and completion should still work.
			ERR_PRINT_OFF;
			if (owner != nullptr) {
				// Remove the line which contains the sentinel char, to get a valid script.
				Ref<GDScript> scr;
				scr.instantiate();
				int start = location;
				int end = location;
				for (; start >= 0; --start) {
					if (code.get(start) == '\n') {
						break;
					}
				}
				for (; end < code.size(); ++end) {
					if (code.get(end) == '\n') {
						break;
					}
				}
				scr->set_source_code(code.erase(start, end - start));
				scr->reload();
				scr->set_path(res_path);
				owner->set_script(scr);
			}

			GDScriptLanguage::get_singleton()->complete_code(code, res_path, owner, &options, forced, call_hint);
			ERR_PRINT_ON;

			String contains_excluded;
			for (ScriptLanguage::CodeCompletionOption &option : options) {
				for (const Dictionary &E : exclude) {
					if (match_option(E, option)) {
						contains_excluded = option.display;
						break;
					}
				}
				if (!contains_excluded.is_empty()) {
					break;
				}

				for (const Dictionary &E : include) {
					if (match_option(E, option)) {
						include.erase(E);
						break;
					}
				}
			}
			CHECK_MESSAGE(contains_excluded.is_empty(), "Autocompletion suggests illegal option '", contains_excluded, "' for '", path.path_join(next), "'.");
			CHECK(include.is_empty());

			String expected_call_hint = conf.get_value("output", "call_hint", call_hint);
			bool expected_forced = conf.get_value("output", "forced", forced);

			CHECK(expected_call_hint == call_hint);
			CHECK(expected_forced == forced);

			if (scene) {
				memdelete(scene);
			}
		}
		next = dir->get_next();
	}
}

static void setup_global_classes(const String &p_dir) {
	Error err = OK;
	Ref<DirAccess> dir = DirAccess::open(p_dir, &err);

	if (err != OK) {
		FAIL("Invalid test directory.");
		return;
	}

	String path = dir->get_current_dir();

	dir->list_dir_begin();
	String next = dir->get_next();

	while (!next.is_empty()) {
		if (dir->current_is_dir() && next != "." && next != "..") {
			setup_global_classes(path.path_join(next));
		} else if (next.ends_with(".gd")) {
			String base_type;
			bool is_abstract;
			bool is_tool;
			String source_file = path.path_join(next);
			String class_name = GDScriptLanguage::get_singleton()->get_global_class_name(source_file, &base_type, nullptr, &is_abstract, &is_tool);
			if (class_name.is_empty()) {
				next = dir->get_next();
				continue;
			}
			ERR_FAIL_COND_MSG(ScriptServer::is_global_class(class_name),
					"Class name \"" + class_name + "\" from \"" + source_file + "\" is already used in \"" + ScriptServer::get_global_class_path(class_name) + "\".");

			ScriptServer::add_global_class(class_name, base_type, GDScriptLanguage::get_singleton()->get_name(), source_file, is_abstract, is_tool);
		}
		next = dir->get_next();
	}
}

TEST_SUITE("[Modules][GDScript][Completion]") {
	TEST_CASE("[Editor] Check suggestion list") {
		// Set all editor settings that code completion relies on.
		EditorSettings::get_singleton()->set_setting("text_editor/completion/use_single_quotes", false);
		init_language("modules/gdscript/tests/scripts");

		setup_global_classes("modules/gdscript/tests/scripts/completion");
		test_directory("modules/gdscript/tests/scripts/completion");

		finish_language();
	}
}
} // namespace GDScriptTests

#endif

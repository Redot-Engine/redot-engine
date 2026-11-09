/**************************************************************************/
/*  editor_shell.cpp                                                      */
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

/**
 * @file editor_shell.cpp
 *
 * Shared functional logic of the editor (menu dispatch, save/open flows,
 * filesystem-scan reactions, title updates, dialogs, quit/restart flow),
 * provided by `EditorShell` and reused by every concrete editor shell.
 */

#include "editor_shell.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_uid.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/string/translation_server.h"
#include "core/version.h"
#include "main/main.h"

#include "editor/debugger/editor_debugger_node.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/docks/import_dock.h"
#include "editor/docks/inspector_dock.h"
#include "editor/docks/node_dock.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_log.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/export/editor_export.h"
#include "editor/export/export_template_manager.h"
#include "editor/export/project_export.h"
#include "editor/export/project_zip_packer.h"
#include "editor/file_system/dependency_editor.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/file_system/editor_paths.h"
#include "editor/file_system/uid_viewer.h"
#include "editor/gui/editor_about.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_quick_open_dialog.h"
#include "editor/gui/editor_toaster.h"
#include "editor/import/fbx_importer_manager.h"
#include "editor/inspector/editor_resource_preview.h"
#include "editor/inspector/multi_node_edit.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/project_upgrade/project_upgrade_tool.h"
#include "editor/run/editor_run_bar.h"
#include "editor/scene/3d/mesh_library_editor_plugin.h"
#include "editor/scene/canvas_item_editor_plugin.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_build_profile.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_feature_profile.h"
#include "editor/settings/editor_settings.h"
#include "editor/settings/editor_settings_dialog.h"
#include "editor/settings/project_settings_editor.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/window.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/packed_scene.h"
#include "scene/theme/theme_db.h"
#include "servers/display/display_server.h"
#include "servers/rendering/rendering_server.h"

static const String REMOVE_ANDROID_BUILD_TEMPLATE_MESSAGE = TTRC("The Android build template is already installed in this project and it won't be overwritten.\nRemove the \"%s\" directory manually before attempting this operation again.");
static const String INSTALL_ANDROID_BUILD_TEMPLATE_MESSAGE = TTRC("This will set up your project for gradle Android builds by installing the source template to \"%s\".\nNote that in order to make gradle builds instead of using pre-built APKs, the \"Use Gradle Build\" option should be enabled in the Android export preset.");

static bool overrides_external_editor(Object *p_object) {
	Script *script = Object::cast_to<Script>(p_object);

	if (!script) {
		return false;
	}

	return script->get_language()->overrides_external_editor();
}

void EditorShell::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PREDELETE: {
			// The quick-open dialogs are owned by `EditorCore` but parented into
			// this shell's GUI tree. Unparent them now — which runs before the
			// shell's subtree (and thus the dialogs) is freed — so that
			// `EditorCore`'s destructor remains the single deletion point and
			// does not double-free them during SceneTree teardown.
			EditorCore *core = EditorCore::get_singleton();
			if (core) {
				if (core->get_quick_open_dialog() && core->get_quick_open_dialog()->get_parent() != nullptr) {
					core->get_quick_open_dialog()->get_parent()->remove_child(core->get_quick_open_dialog());
				}
				if (core->get_quick_open_color_palette() && core->get_quick_open_color_palette()->get_parent() != nullptr) {
					core->get_quick_open_color_palette()->get_parent()->remove_child(core->get_quick_open_color_palette());
				}
			}
		} break;

		case NOTIFICATION_PROCESS: {
			if (requested_first_scan) {
				requested_first_scan = false;

				OS::get_singleton()->benchmark_begin_measure("Editor", "First Scan");

				EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &EditorShell::_on_first_scan_completed), CONNECT_ONE_SHOT);
				EditorFileSystem::get_singleton()->scan();
			}
		} break;

		case NOTIFICATION_WM_CLOSE_REQUEST: {
			_menu_option_confirm(SCENE_QUIT, false);
		} break;
	}
}

void EditorShell::_on_first_scan_completed() {
	// Route through a plain (virtual) call rather than connecting
	// `callable_mp(this, &EditorShell::_execute_upgrades)` directly: member
	// pointers do not virtual-dispatch, and concrete shells override
	// `_execute_upgrades`.
	_execute_upgrades();
}

void EditorShell::_update_theme(bool p_skip_creation) {
	Ref<Theme> theme;
	if (!p_skip_creation) {
		theme = EditorThemeManager::generate_theme(EditorCore::get_singleton()->get_editor_theme());
		EditorCore::get_singleton()->set_editor_theme(theme);
		DisplayServer::set_early_window_clear_color_override(true, theme->get_color(SNAME("background"), EditorStringName(Editor)));
	} else {
		theme = EditorCore::get_singleton()->get_editor_theme();
	}

	Vector<Ref<Theme>> editor_themes;
	editor_themes.push_back(theme);
	editor_themes.push_back(ThemeDB::get_singleton()->get_default_theme());

	ThemeContext *node_tc = ThemeDB::get_singleton()->get_theme_context(this);
	if (node_tc) {
		node_tc->set_themes(editor_themes);
	} else {
		ThemeDB::get_singleton()->create_theme_context(this, editor_themes);
	}

	Window *window = get_window();
	if (window) {
		ThemeContext *window_tc = ThemeDB::get_singleton()->get_theme_context(window);
		if (window_tc) {
			window_tc->set_themes(editor_themes);
		} else {
			ThemeDB::get_singleton()->create_theme_context(window, editor_themes);
		}
	}

	if (CanvasItemEditor::get_singleton()->get_theme_preview() == CanvasItemEditor::THEME_PREVIEW_EDITOR) {
		EditorCore::get_singleton()->update_preview_themes(CanvasItemEditor::THEME_PREVIEW_EDITOR);
	}

	_apply_theme_styles(theme);
}

void EditorShell::_update_vsync_mode() {
	const DisplayServer::VSyncMode window_vsync_mode = DisplayServer::VSyncMode(int(EDITOR_GET("interface/editor/vsync_mode")));
	DisplayServer::get_singleton()->window_set_vsync_mode(window_vsync_mode);
}

void EditorShell::_update_title() {
	const String appname = GLOBAL_GET("application/config/name");
	String title = (appname.is_empty() ? TTR("Unnamed Project") : appname);
	const String edited = get_editor_data().get_edited_scene_root() ? get_editor_data().get_edited_scene_root()->get_scene_file_path() : String();
	if (!edited.is_empty()) {
		// Display the edited scene name before the program name so that it can be seen in the OS task bar.
		title = vformat("%s - %s", edited.get_file(), title);
	}
	if (EditorSceneManager::get_singleton()->has_unsaved_changes()) {
		// Display the "modified" mark before anything else so that it can always be seen in the OS task bar.
		title = vformat("(*) %s", title);
	}
	DisplayServer::get_singleton()->window_set_title(title + String(" - ") + REDOT_VERSION_NAME);
	_update_title_label(title);
}

void EditorShell::_fs_changed() {
	EditorCore *core = EditorCore::get_singleton();
	for (FileDialog *E : core->get_file_dialogs()) {
		E->invalidate();
	}

	for (EditorFileDialog *E : core->get_editor_file_dialogs()) {
		E->invalidate();
	}

	EditorSceneManager::get_singleton()->_mark_unsaved_scenes();
}

void EditorShell::_sources_changed(bool p_exist) {
	if (waiting_for_first_scan) {
		waiting_for_first_scan = false;

		OS::get_singleton()->benchmark_end_measure("Editor", "First Scan");

		// Reload the global shader variables, but this time
		// loading textures, as they are now properly imported.
		RenderingServer::get_singleton()->global_shader_parameters_load_settings(true);

		_load_editor_layout_on_first_scan();

		const String &defer_load_scene = EditorSceneManager::get_singleton()->get_defer_load_scene();
		if (!defer_load_scene.is_empty()) {
			OS::get_singleton()->benchmark_begin_measure("Editor", "Load Scene");

			EditorSceneManager::get_singleton()->load_scene(defer_load_scene);
			EditorSceneManager::get_singleton()->clear_defer_load_scene();

			OS::get_singleton()->benchmark_end_measure("Editor", "Load Scene");
			OS::get_singleton()->benchmark_dump();
		}

		// Start preview thread now that it's safe.
		if (!cmdline_mode) {
			EditorResourcePreview::get_singleton()->start();
		}
		get_tree()->create_timer(1.0f)->connect("timeout", callable_mp(this, &EditorShell::_remove_lock_file));
	}
}

void EditorShell::_resources_changed(const Vector<String> &p_resources) {
	List<Ref<Resource>> changed;

	int rc = p_resources.size();
	for (int i = 0; i < rc; i++) {
		Ref<Resource> res = ResourceCache::get_ref(p_resources.get(i));
		if (res.is_null()) {
			continue;
		}

		if (!res->editor_can_reload_from_file()) {
			continue;
		}
		if (!res->get_path().is_resource_file() && !res->get_path().is_absolute_path()) {
			continue;
		}
		if (!FileAccess::exists(res->get_path())) {
			continue;
		}

		if (!res->get_import_path().is_empty()) {
			// This is an imported resource, will be reloaded if reimported via the _resources_reimported() callback.
			continue;
		}

		changed.push_back(res);
	}

	if (changed.size()) {
		for (Ref<Resource> &res : changed) {
			res->reload_from_file();
		}
	}
}

void EditorShell::_remove_lock_file() {
	OS::get_singleton()->remove_lock_file();
}

void EditorShell::_begin_first_scan() {
	if (!waiting_for_first_scan) {
		return;
	}
	requested_first_scan = true;
}

void EditorShell::_menu_option(int p_option) {
	_menu_option_confirm(p_option, false);
}

void EditorShell::_menu_confirm_current() {
	_menu_option_confirm(current_menu_option, true);
}

void EditorShell::trigger_menu_option(int p_option, bool p_confirmed) {
	_menu_option_confirm(p_option, p_confirmed);
}

void EditorShell::_dialog_display_save_error(String p_file, Error p_error) {
	if (p_error) {
		switch (p_error) {
			case ERR_FILE_CANT_WRITE: {
				show_accept(TTR("Can't open file for writing:") + " " + p_file.get_extension(), TTR("OK"));
			} break;
			case ERR_FILE_UNRECOGNIZED: {
				show_accept(TTR("Requested file format unknown:") + " " + p_file.get_extension(), TTR("OK"));
			} break;
			default: {
				show_accept(TTR("Error while saving."), TTR("OK"));
			} break;
		}
	}
}

void EditorShell::_dialog_display_load_error(String p_file, Error p_error) {
	if (p_error) {
		switch (p_error) {
			case ERR_CANT_OPEN: {
				show_accept(vformat(TTR("Can't open file '%s'. The file could have been moved or deleted."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_PARSE_ERROR: {
				show_accept(vformat(TTR("Error while parsing file '%s'."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_CORRUPT: {
				show_accept(vformat(TTR("Scene file '%s' appears to be invalid/corrupt."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_NOT_FOUND: {
				show_accept(vformat(TTR("Missing file '%s' or one of its dependencies."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_UNRECOGNIZED: {
				show_accept(vformat(TTR("File '%s' is saved in a format that is newer than the formats supported by this version of Redot, so it can't be opened."), p_file.get_file()), TTR("OK"));
			} break;
			default: {
				show_accept(vformat(TTR("Error while loading file '%s'."), p_file.get_file()), TTR("OK"));
			} break;
		}
	}
}

void EditorShell::_dialog_action(String p_file) {
	switch (current_menu_option) {
		case SCENE_NEW_INHERITED_SCENE: {
			Node *scene = get_editor_data().get_edited_scene_root();
			// If the previous scene is rootless, just close it in favor of the new one.
			if (!scene) {
				_menu_option_confirm(SCENE_CLOSE, true);
			}

			EditorSceneManager::get_singleton()->load_scene(p_file, false, true);
		} break;
		case SCENE_OPEN_SCENE: {
			EditorSceneManager::get_singleton()->load_scene(p_file);
		} break;
		case SETTINGS_PICK_MAIN_SCENE: {
			ProjectSettings::get_singleton()->set("application/run/main_scene", ResourceUID::path_to_uid(p_file));
			ProjectSettings::get_singleton()->save();
			/// @todo Would be nice to show the project manager opened with the highlighted field.

			get_project_run_bar()->play_main_scene((bool)get_pick_main_scene_dialog()->get_meta("from_native", false));
		} break;
		case SCENE_CLOSE:
		case SCENE_TAB_CLOSE:
		case SCENE_SAVE_SCENE:
		case SCENE_MULTI_SAVE_AS_SCENE:
		case SCENE_SAVE_AS_SCENE: {
			int scene_idx = (current_menu_option == SCENE_SAVE_SCENE ||
									current_menu_option == SCENE_SAVE_AS_SCENE ||
									current_menu_option == SCENE_MULTI_SAVE_AS_SCENE)
					? -1
					: EditorSceneManager::get_singleton()->get_tab_closing_idx();

			if (get_file_dialog()->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				bool same_open_scene = false;
				for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
					if (get_editor_data().get_scene_path(i) == p_file && i != scene_idx) {
						same_open_scene = true;
					}
				}

				if (same_open_scene) {
					show_warning(TTR("Can't overwrite scene that is still open!"));
					return;
				}

				EditorCore::get_singleton()->save_default_environment();
				_save_scene_with_preview(p_file, scene_idx);
				_add_to_recent_scenes(p_file);
				save_editor_layout_delayed();

				if (scene_idx != -1) {
					EditorSceneManager::get_singleton()->_discard_changes();
				} else {
					// Update the path of the edited scene to ensure later do/undo action history matches.
					get_editor_data().set_scene_path(get_editor_data().get_edited_scene(), p_file);
				}
			}

			if (current_menu_option == SCENE_MULTI_SAVE_AS_SCENE) {
				EditorSceneManager::get_singleton()->_proceed_save_asing_scene_tabs();
			}

		} break;

		case SAVE_AND_RUN: {
			if (get_file_dialog()->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				EditorCore::get_singleton()->save_default_environment();
				_save_scene_with_preview(p_file);
				get_project_run_bar()->play_custom_scene(p_file);
			}
		} break;

		case SAVE_AND_RUN_MAIN_SCENE: {
			ProjectSettings::get_singleton()->set("application/run/main_scene", ResourceUID::path_to_uid(p_file));
			ProjectSettings::get_singleton()->save();

			if (get_file_dialog()->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				EditorCore::get_singleton()->save_default_environment();
				_save_scene_with_preview(p_file);
				get_project_run_bar()->play_main_scene((bool)get_pick_main_scene_dialog()->get_meta("from_native", false));
			}
		} break;

		case FILE_EXPORT_MESH_LIBRARY: {
			EditorFileDialog *file_export_lib = get_export_lib_file_dialog();
			ERR_FAIL_NULL(file_export_lib);
			const Dictionary &fd_options = file_export_lib->get_selected_options();
			bool merge_with_existing_library = fd_options.get(TTR("Merge With Existing"), true);
			bool apply_mesh_instance_transforms = fd_options.get(TTR("Apply MeshInstance Transforms"), false);

			Ref<MeshLibrary> ml;
			if (merge_with_existing_library && FileAccess::exists(p_file)) {
				ml = ResourceLoader::load(p_file, "MeshLibrary");

				if (ml.is_null()) {
					show_accept(TTR("Can't load MeshLibrary for merging!"), TTR("OK"));
					return;
				}
			}

			if (ml.is_null()) {
				ml.instantiate();
			}

			MeshLibraryEditor::update_library_file(get_editor_data().get_edited_scene_root(), ml, merge_with_existing_library, apply_mesh_instance_transforms);

			Error err = ResourceSaver::save(ml, p_file);
			if (err) {
				show_accept(TTR("Error saving MeshLibrary!"), TTR("OK"));
				return;
			} else if (ResourceCache::has(p_file)) {
				// Make sure MeshLibrary is updated in the editor.
				ResourceLoader::load(p_file)->reload_from_file();
			}

		} break;

		case PROJECT_PACK_AS_ZIP: {
			ProjectZIPPacker::pack_project_zip(p_file);
			{
				Ref<FileAccess> f = FileAccess::open(p_file, FileAccess::READ);
				ERR_FAIL_COND_MSG(f.is_null(), vformat("Unable to create ZIP file at: %s. Check for write permissions and whether you have enough disk space left.", p_file));
			}

		} break;

		case RESOURCE_SAVE:
		case RESOURCE_SAVE_AS: {
			ERR_FAIL_COND(EditorCore::get_singleton()->get_saving_resource().is_null());
			EditorCore::get_singleton()->save_resource_in_path(EditorCore::get_singleton()->get_saving_resource(), p_file);

			EditorCore::get_singleton()->set_saving_resource(Ref<Resource>());
			ObjectID current_id = get_editor_selection_history()->get_current();
			Object *current_obj = current_id.is_valid() ? ObjectDB::get_instance(current_id) : nullptr;
			ERR_FAIL_NULL(current_obj);
			current_obj->notify_property_list_changed();
		} break;
		case LAYOUT_SAVE: {
			_save_layout(p_file);
		} break;
		case LAYOUT_DELETE: {
			_delete_layout(p_file);
		} break;
		default: {
			// Save scene?
			if (get_file_dialog()->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				_save_scene_with_preview(p_file);
			}

		} break;
	}
}

bool EditorShell::_is_class_editor_disabled_by_feature_profile(const StringName &p_class) {
	Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
	if (profile.is_null()) {
		return false;
	}

	StringName class_name = p_class;

	while (class_name != StringName()) {
		if (profile->is_class_disabled(class_name)) {
			return true;
		}
		if (profile->is_class_editor_disabled(class_name)) {
			return true;
		}
		class_name = ClassDB::get_parent_class(class_name);
	}

	return false;
}

void EditorShell::_add_to_history(const Object *p_object, const String &p_property, bool p_inspector_only) {
	ObjectID id = p_object->get_instance_id();
	ObjectID history_id = get_editor_selection_history()->get_current();
	if (id != history_id) {
		const MultiNodeEdit *multi_node_edit = Object::cast_to<const MultiNodeEdit>(p_object);
		const MultiNodeEdit *history_multi_node_edit = ObjectDB::get_instance<MultiNodeEdit>(history_id);
		if (multi_node_edit && history_multi_node_edit && multi_node_edit->is_same_selection(history_multi_node_edit)) {
			return;
		}
		if (p_inspector_only) {
			get_editor_selection_history()->add_object(id, String(), true);
		} else if (p_property.is_empty()) {
			get_editor_selection_history()->add_object(id);
		} else {
			get_editor_selection_history()->add_object(id, p_property);
		}
	}
}

void EditorShell::_edit_current(bool p_skip_foreign, bool p_skip_inspector_update) {
	ObjectID current_id = get_editor_selection_history()->get_current();
	Object *current_obj = current_id.is_valid() ? ObjectDB::get_instance(current_id) : nullptr;

	Ref<Resource> res = Object::cast_to<Resource>(current_obj);
	if (p_skip_foreign && res.is_valid()) {
		const int current_tab = get_scene_tabs()->get_current_tab();
		if (res->get_path().contains("::") && res->get_path().get_slice("::", 0) != get_editor_data().get_scene_path(current_tab)) {
			// Trying to edit resource that belongs to another scene; abort.
			current_obj = nullptr;
		}
	}

	bool inspector_only = get_editor_selection_history()->is_current_inspector_only();
	EditorSceneManager::get_singleton()->set_current_object(current_obj);

	if (!current_obj) {
		SceneTreeDock::get_singleton()->set_selected(nullptr);
		InspectorDock::get_inspector_singleton()->edit(nullptr);
		NodeDock::get_singleton()->set_node(nullptr);
		InspectorDock::get_singleton()->update(nullptr);
		EditorDebuggerNode::get_singleton()->clear_remote_tree_selection();
		EditorSceneManager::get_singleton()->hide_unused_editors();
		return;
	}

	// Update the use folding setting and state.
	bool disable_folding = bool(EDITOR_GET("interface/inspector/disable_folding")) || current_obj->is_class("EditorDebuggerRemoteObjects");
	if (InspectorDock::get_inspector_singleton()->is_using_folding() == disable_folding) {
		InspectorDock::get_inspector_singleton()->set_use_folding(!disable_folding, false);
	}

	bool is_resource = current_obj->is_class("Resource");
	bool is_node = current_obj->is_class("Node");
	bool stay_in_script_editor_on_node_selected = bool(EDITOR_GET("text_editor/behavior/navigation/stay_in_script_editor_on_node_selected"));
	bool skip_main_plugin = false;

	String editable_info; // None by default.
	bool info_is_warning = false;

	if (current_obj->has_method("_is_read_only")) {
		if (current_obj->call("_is_read_only")) {
			editable_info = TTR("This object is marked as read-only, so it's not editable.");
		}
	}

	if (is_resource) {
		Resource *current_res = Object::cast_to<Resource>(current_obj);
		ERR_FAIL_NULL(current_res);

		if (!p_skip_inspector_update) {
			InspectorDock::get_inspector_singleton()->edit(current_res);
			SceneTreeDock::get_singleton()->set_selected(nullptr);
			NodeDock::get_singleton()->set_node(nullptr);
			InspectorDock::get_singleton()->update(nullptr);
			EditorDebuggerNode::get_singleton()->clear_remote_tree_selection();
			ImportDock::get_singleton()->set_edit_path(current_res->get_path());
		}

		int subr_idx = current_res->get_path().find("::");
		if (subr_idx != -1) {
			String base_path = current_res->get_path().substr(0, subr_idx);
			if (FileAccess::exists(base_path + ".import")) {
				if (!base_path.is_resource_file()) {
					if (EditorSceneManager::get_singleton()->get_edited_scene() && EditorSceneManager::get_singleton()->get_edited_scene()->get_scene_file_path() == base_path) {
						info_is_warning = true;
					}
				}
				editable_info = TTR("This resource belongs to a scene that was imported, so it's not editable.\nPlease read the documentation relevant to importing scenes to better understand this workflow.");
			} else if ((!EditorSceneManager::get_singleton()->get_edited_scene() ||
							   EditorSceneManager::get_singleton()->get_edited_scene()->get_scene_file_path() != base_path) &&
					ResourceLoader::get_resource_type(base_path) == "PackedScene") {
				editable_info = TTR("This resource belongs to a scene that was instantiated or inherited.\nChanges to it must be made inside the original scene.");
			}
		} else if (current_res->get_path().is_resource_file()) {
			if (FileAccess::exists(current_res->get_path() + ".import")) {
				editable_info = TTR("This resource was imported, so it's not editable. Change its settings in the import panel and then re-import.");
			}
		}
	} else if (is_node) {
		Node *current_node = Object::cast_to<Node>(current_obj);
		ERR_FAIL_NULL(current_node);

		InspectorDock::get_inspector_singleton()->edit(current_node);
		if (current_node->is_inside_tree()) {
			NodeDock::get_singleton()->set_node(current_node);
			SceneTreeDock::get_singleton()->set_selected(current_node);
			SceneTreeDock::get_singleton()->set_selection({ current_node });
			InspectorDock::get_singleton()->update(current_node);
			if (!inspector_only && !skip_main_plugin) {
				if (!ScriptEditor::get_singleton()->is_editor_floating() && ScriptEditor::get_singleton()->is_visible_in_tree()) {
					skip_main_plugin = stay_in_script_editor_on_node_selected;
				} else {
					skip_main_plugin = !get_editor_main_screen()->can_auto_switch_screens();
				}
			}
		} else {
			NodeDock::get_singleton()->set_node(nullptr);
			SceneTreeDock::get_singleton()->set_selected(nullptr);
			InspectorDock::get_singleton()->update(nullptr);
		}
		EditorDebuggerNode::get_singleton()->clear_remote_tree_selection();

		if (EditorSceneManager::get_singleton()->get_edited_scene() && !EditorSceneManager::get_singleton()->get_edited_scene()->get_scene_file_path().is_empty()) {
			String source_scene = EditorSceneManager::get_singleton()->get_edited_scene()->get_scene_file_path();
			if (FileAccess::exists(source_scene + ".import")) {
				editable_info = TTR("This scene was imported, so changes to it won't be kept.\nInstantiating or inheriting it will allow you to make changes to it.\nPlease read the documentation relevant to importing scenes to better understand this workflow.");
				info_is_warning = true;
			}
		}
	} else {
		Node *selected_node = nullptr;

		Vector<Node *> multi_nodes;
		if (current_obj->is_class("MultiNodeEdit")) {
			Node *scene = EditorSceneManager::get_singleton()->get_edited_scene();
			if (scene) {
				MultiNodeEdit *multi_node_edit = Object::cast_to<MultiNodeEdit>(current_obj);
				int node_count = multi_node_edit->get_node_count();
				if (node_count > 0) {
					for (int node_index = 0; node_index < node_count; ++node_index) {
						Node *node = scene->get_node(multi_node_edit->get_node(node_index));
						if (node) {
							multi_nodes.push_back(node);
						}
					}
					if (!multi_nodes.is_empty()) {
						// Pick the top-most node.
						multi_nodes.sort_custom<Node::Comparator>();
						selected_node = multi_nodes[0];
					}
				}
			}
		}

		if (!current_obj->is_class("EditorDebuggerRemoteObjects")) {
			EditorDebuggerNode::get_singleton()->clear_remote_tree_selection();
		}

		InspectorDock::get_inspector_singleton()->edit(current_obj);
		NodeDock::get_singleton()->set_node(nullptr);
		SceneTreeDock::get_singleton()->set_selected(selected_node);
		SceneTreeDock::get_singleton()->set_selection(multi_nodes);
		InspectorDock::get_singleton()->update(nullptr);
	}

	InspectorDock::get_singleton()->set_info(
			info_is_warning ? TTR("Changes may be lost!") : TTR("This object is read-only."),
			editable_info,
			info_is_warning);

	Object *editor_owner = (is_node || current_obj->is_class("MultiNodeEdit")) ? (Object *)SceneTreeDock::get_singleton() : is_resource ? (Object *)InspectorDock::get_inspector_singleton()
																																		: (Object *)this;

	// Take care of the main editor plugin.

	if (!inspector_only) {
		EditorPlugin *main_plugin = get_editor_data().get_handling_main_editor(current_obj);

		int plugin_index = get_editor_main_screen()->get_plugin_index(main_plugin);
		if (main_plugin && plugin_index >= 0 && !get_editor_main_screen()->is_button_enabled(plugin_index)) {
			main_plugin = nullptr;
		}
		EditorPlugin *editor_plugin_screen = get_editor_main_screen()->get_selected_plugin();

		ObjectID editor_owner_id = editor_owner->get_instance_id();
		if (main_plugin && !skip_main_plugin) {
			// Special case if use of external editor is true.
			Resource *current_res = Object::cast_to<Resource>(current_obj);
			if (main_plugin->get_plugin_name() == "Script" && current_res && !current_res->is_built_in() && (bool(EDITOR_GET("text_editor/external/use_external_editor")) || overrides_external_editor(current_obj))) {
				if (!EditorSceneManager::get_singleton()->is_changing_scene()) {
					main_plugin->edit(current_obj);
				}
			} else if (Object::cast_to<Script>(current_obj)) {
				get_editor_main_screen()->select(plugin_index);
				main_plugin->edit(current_obj);
			} else if (main_plugin != editor_plugin_screen) {
				// Unedit previous plugin.
				editor_plugin_screen->edit(nullptr);
				EditorPluginManager::get_singleton()->get_active_plugins()[editor_owner_id].erase(editor_plugin_screen);
				// Update screen main_plugin.
				get_editor_main_screen()->select(plugin_index);
				main_plugin->edit(current_obj);
			} else {
				editor_plugin_screen->edit(current_obj);
			}
			EditorPluginManager::get_singleton()->set_main_screen_editing(true);
		} else if (!main_plugin && editor_plugin_screen && EditorPluginManager::get_singleton()->is_main_screen_editing_flag()) {
			editor_plugin_screen->edit(nullptr);
			EditorPluginManager::get_singleton()->set_main_screen_editing(false);
		}

		EditorSceneManager::get_singleton()->edit_item(current_obj, editor_owner);
	}

	InspectorDock::get_singleton()->update(current_obj);
}

void EditorShell::_menu_option_confirm(int p_option, bool p_confirmed) {
	if (!p_confirmed) { /// @todo FIXME: this may be a hack.
		current_menu_option = (MenuOptions)p_option;
	}

	switch (p_option) {
		case SCENE_NEW_SCENE: {
			EditorSceneManager::get_singleton()->new_scene();

		} break;
		case SCENE_NEW_INHERITED_SCENE:
		case SCENE_OPEN_SCENE: {
			get_file_dialog()->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);
			get_file_dialog()->clear_filters();
			for (const String &extension : extensions) {
				get_file_dialog()->add_filter("*." + extension, extension.to_upper());
			}

			Node *scene = get_editor_data().get_edited_scene_root();
			if (scene) {
				get_file_dialog()->set_current_path(scene->get_scene_file_path());
			};
			get_file_dialog()->set_title(p_option == SCENE_OPEN_SCENE ? TTR("Open Scene") : TTR("Open Base Scene"));
			get_file_dialog()->popup_file_dialog();

		} break;
		case SCENE_QUICK_OPEN: {
			EditorCore::get_singleton()->get_quick_open_dialog()->popup_dialog({ "Resource" }, callable_mp(this, &EditorShell::_quick_opened));
		} break;
		case SCENE_QUICK_OPEN_SCENE: {
			EditorCore::get_singleton()->get_quick_open_dialog()->popup_dialog({ "PackedScene" }, callable_mp(this, &EditorShell::_quick_opened));
		} break;
		case SCENE_QUICK_OPEN_SCRIPT: {
			EditorCore::get_singleton()->get_quick_open_dialog()->popup_dialog({ "Script" }, callable_mp(this, &EditorShell::_quick_opened));
		} break;
		case SCENE_OPEN_PREV: {
			const String last_closed_scene = EditorSceneManager::get_singleton()->get_last_closed_scene();
			if (!last_closed_scene.is_empty()) {
				EditorSceneManager::get_singleton()->load_scene(last_closed_scene);
			}
		} break;
		case SCENE_CLOSE_OTHERS: {
			EditorSceneManager::get_singleton()->set_tab_closing_menu_option(-1);
			for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
				if (i == get_editor_data().get_edited_scene()) {
					continue;
				}
				EditorSceneManager::get_singleton()->queue_tab_close(get_editor_data().get_scene_path(i));
			}
			EditorSceneManager::get_singleton()->_proceed_closing_scene_tabs();
		} break;
		case SCENE_CLOSE_RIGHT: {
			EditorSceneManager::get_singleton()->set_tab_closing_menu_option(-1);
			for (int i = get_editor_data().get_edited_scene() + 1; i < get_editor_data().get_edited_scene_count(); i++) {
				EditorSceneManager::get_singleton()->queue_tab_close(get_editor_data().get_scene_path(i));
			}
			EditorSceneManager::get_singleton()->_proceed_closing_scene_tabs();
		} break;
		case SCENE_CLOSE_ALL: {
			EditorSceneManager::get_singleton()->set_tab_closing_menu_option(-1);
			for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
				EditorSceneManager::get_singleton()->queue_tab_close(get_editor_data().get_scene_path(i));
			}
			EditorSceneManager::get_singleton()->_proceed_closing_scene_tabs();
		} break;
		case SCENE_CLOSE: {
			EditorSceneManager::get_singleton()->_scene_tab_closed(get_editor_data().get_edited_scene());
		} break;
		case SCENE_TAB_CLOSE:
		case SCENE_SAVE_SCENE: {
			int scene_idx = (p_option == SCENE_SAVE_SCENE) ? -1 : EditorSceneManager::get_singleton()->get_tab_closing_idx();
			Node *scene = get_editor_data().get_edited_scene_root(scene_idx);
			if (scene && !scene->get_scene_file_path().is_empty()) {
				if (DirAccess::exists(scene->get_scene_file_path().get_base_dir())) {
					if (scene_idx != get_editor_data().get_edited_scene()) {
						_save_scene_with_preview(scene->get_scene_file_path(), scene_idx);
					} else {
						_save_scene_with_preview(scene->get_scene_file_path());
					}

					if (scene_idx != -1) {
						EditorSceneManager::get_singleton()->_discard_changes();
					}
					save_editor_layout_delayed();
				} else {
					show_save_accept(vformat(TTR("%s no longer exists! Please specify a new save location."), scene->get_scene_file_path().get_base_dir()), TTR("OK"));
				}
				break;
			}
			[[fallthrough]];
		}
		case SCENE_MULTI_SAVE_AS_SCENE:
		case SCENE_SAVE_AS_SCENE: {
			int scene_idx = (p_option == SCENE_SAVE_SCENE ||
									p_option == SCENE_SAVE_AS_SCENE ||
									p_option == SCENE_MULTI_SAVE_AS_SCENE)
					? -1
					: EditorSceneManager::get_singleton()->get_tab_closing_idx();

			Node *scene = get_editor_data().get_edited_scene_root(scene_idx);

			if (!scene) {
				if (p_option == SCENE_SAVE_SCENE) {
					// Pressing Ctrl + S saves the current script if a scene is currently open, but it won't if the scene has no root node.
					// Work around this by explicitly saving the script in this case (similar to pressing Ctrl + Alt + S).
					ScriptEditor::get_singleton()->save_current_script();
				}

				const int saved = EditorSceneManager::get_singleton()->_save_external_resources(true);
				if (saved > 0) {
					EditorToaster::get_singleton()->popup_str(vformat(TTR("The current scene has no root node, but %d modified external resource(s) and/or plugin data were saved anyway."), saved), EditorToaster::SEVERITY_INFO);
				} else if (p_option == SCENE_SAVE_AS_SCENE) {
					// Don't show this dialog when pressing Ctrl + S to avoid interfering with script saving.
					show_accept(
							TTR("A root node is required to save the scene. You can add a root node using the Scene tree dock."),
							TTR("OK"));
				}

				break;
			}

			get_file_dialog()->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);

			List<String> extensions;
			Ref<PackedScene> sd = memnew(PackedScene);
			ResourceSaver::get_recognized_extensions(sd, &extensions);
			get_file_dialog()->clear_filters();
			for (const String &extension : extensions) {
				get_file_dialog()->add_filter("*." + extension, extension.to_upper());
			}

			if (!scene->get_scene_file_path().is_empty()) {
				String path = scene->get_scene_file_path();
				String root_name = EditorCore::adjust_scene_name_casing(scene->get_name());
				String ext = path.get_extension().to_lower();
				path = path.get_base_dir().path_join(root_name + "." + ext);

				get_file_dialog()->set_current_path(path);
				if (extensions.size()) {
					if (extensions.find(ext) == nullptr) {
						get_file_dialog()->set_current_path(path.replacen("." + ext, "." + extensions.front()->get()));
					}
				}
			} else if (extensions.size()) {
				String root_name = scene->get_name();
				root_name = EditorCore::adjust_scene_name_casing(root_name);
				get_file_dialog()->set_current_path(root_name + "." + extensions.front()->get().to_lower());
			}
			get_file_dialog()->set_title(TTR("Save Scene As..."));
			get_file_dialog()->popup_file_dialog();

		} break;

		case SCENE_SAVE_ALL_SCENES: {
			EditorSceneManager::get_singleton()->_save_all_scenes();
		} break;

		case SCENE_RUN: {
			get_project_run_bar()->play_current_scene();
		} break;

		case PROJECT_EXPORT: {
			EditorCore::get_singleton()->get_project_export()->popup_export();
		} break;

		case PROJECT_PACK_AS_ZIP: {
			EditorFileDialog *file_pack_zip = get_pack_zip_file_dialog();
			ERR_FAIL_NULL(file_pack_zip);
			String resource_path = ProjectSettings::get_singleton()->get_resource_path();
			const String base_path = resource_path.substr(0, resource_path.rfind_char('/')) + "/";

			file_pack_zip->set_current_path(base_path);
			file_pack_zip->set_current_file(ProjectZIPPacker::get_project_zip_safe_name());
			file_pack_zip->popup_file_dialog();
		} break;

		case SCENE_UNDO: {
			EditorLog *undo_log = EditorCore::get_singleton()->get_log();
			if ((int)Input::get_singleton()->get_mouse_button_mask() & 0x7) {
				undo_log->add_message(TTR("Can't undo while mouse buttons are pressed."), EditorLog::MSG_TYPE_EDITOR);
			} else {
				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
				String action = undo_redo->get_current_action_name();
				int id = undo_redo->get_current_action_history_id();
				if (!undo_redo->undo()) {
					undo_log->add_message(TTR("Nothing to undo."), EditorLog::MSG_TYPE_EDITOR);
				} else if (!action.is_empty()) {
					switch (id) {
						case EditorUndoRedoManager::GLOBAL_HISTORY:
							undo_log->add_message(vformat(TTR("Global Undo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						case EditorUndoRedoManager::REMOTE_HISTORY:
							undo_log->add_message(vformat(TTR("Remote Undo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						default:
							undo_log->add_message(vformat(TTR("Scene Undo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
					}
				}
			}
			EditorSceneManager::get_singleton()->_update_unsaved_cache();
		} break;
		case SCENE_REDO: {
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			EditorLog *redo_log = EditorCore::get_singleton()->get_log();
			if ((int)Input::get_singleton()->get_mouse_button_mask() & 0x7) {
				redo_log->add_message(TTR("Can't redo while mouse buttons are pressed."), EditorLog::MSG_TYPE_EDITOR);
			} else {
				if (!undo_redo->redo()) {
					redo_log->add_message(TTR("Nothing to redo."), EditorLog::MSG_TYPE_EDITOR);
				} else {
					String action = undo_redo->get_current_action_name();
					if (action.is_empty()) {
						break;
					}

					switch (undo_redo->get_current_action_history_id()) {
						case EditorUndoRedoManager::GLOBAL_HISTORY:
							redo_log->add_message(vformat(TTR("Global Redo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						case EditorUndoRedoManager::REMOTE_HISTORY:
							redo_log->add_message(vformat(TTR("Remote Redo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						default:
							redo_log->add_message(vformat(TTR("Scene Redo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
					}
				}
			}
			EditorSceneManager::get_singleton()->_update_unsaved_cache();
		} break;

		case SCENE_RELOAD_SAVED_SCENE: {
			Node *scene = EditorSceneManager::get_singleton()->get_edited_scene();

			if (!scene) {
				break;
			}

			String scene_filename = scene->get_scene_file_path();
			String unsaved_message;

			if (scene_filename.is_empty()) {
				show_warning(TTR("Can't reload a scene that was never saved."));
				break;
			}

			if (EditorSceneManager::get_singleton()->has_unsaved_changes()) {
				if (!p_confirmed) {
					get_confirmation_dialog()->set_ok_button_text(TTRC("Save & Reload"));
					unsaved_message = EditorSceneManager::get_singleton()->_get_unsaved_scene_dialog_text(scene_filename, EditorCore::get_singleton()->get_started_timestamp());
					get_confirmation_dialog()->set_text(unsaved_message + "\n\n" + TTR("Save before reloading the scene?"));
					get_confirmation_dialog()->popup_centered();
					get_confirmation_button()->show();
					get_confirmation_button()->grab_focus();
					break;
				} else {
					_save_scene_with_preview(scene_filename);
				}
			}

			EditorSceneManager::get_singleton()->_discard_changes();
		} break;

		case SCENE_SHOW_IN_FILESYSTEM: {
			String path = get_editor_data().get_scene_path(get_editor_data().get_edited_scene());
			if (!path.is_empty()) {
				FileSystemDock::get_singleton()->navigate_to_path(path);
			}
		} break;

		case PROJECT_OPEN_SETTINGS: {
			EditorCore::get_singleton()->get_project_settings_editor()->popup_project_settings();
		} break;

		case PROJECT_FIND_IN_FILES: {
			ScriptEditor::get_singleton()->open_find_in_files_dialog("");
		} break;

		case PROJECT_INSTALL_ANDROID_SOURCE: {
			ExportTemplateManager *export_template_manager = get_export_template_manager();
			OptionButton *choose_android_export_profile = get_choose_android_export_profile();
			ConfirmationDialog *install_android_build_template = get_install_android_build_template_dialog();
			ConfirmationDialog *remove_android_build_template = get_remove_android_build_template_dialog();
			ConfirmationDialog *gradle_build_manage_templates = get_gradle_build_manage_templates_dialog();
			if (export_template_manager && choose_android_export_profile && install_android_build_template && remove_android_build_template && gradle_build_manage_templates) {
				if (p_confirmed) {
					if (export_template_manager->is_android_template_installed(android_export_preset)) {
						remove_android_build_template->set_text(vformat(TTR(REMOVE_ANDROID_BUILD_TEMPLATE_MESSAGE), export_template_manager->get_android_build_directory(android_export_preset)));
						remove_android_build_template->popup_centered();
					} else if (!export_template_manager->can_install_android_template(android_export_preset)) {
						gradle_build_manage_templates->popup_centered();
					} else {
						export_template_manager->install_android_template(android_export_preset);
					}
				} else {
					bool has_custom_gradle_build = false;
					choose_android_export_profile->clear();
					for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
						Ref<EditorExportPreset> export_preset = EditorExport::get_singleton()->get_export_preset(i);
						if (export_preset->get_platform()->get_class_name() == "EditorExportPlatformAndroid" && (bool)export_preset->get("gradle_build/use_gradle_build")) {
							choose_android_export_profile->add_item(export_preset->get_name(), i);
							String gradle_build_directory = export_preset->get("gradle_build/gradle_build_directory");
							String android_source_template = export_preset->get("gradle_build/android_source_template");
							if (!android_source_template.is_empty() || (gradle_build_directory != "" && gradle_build_directory != "res://android")) {
								has_custom_gradle_build = true;
							}
						}
					}
					_android_export_preset_selected(choose_android_export_profile->get_item_count() >= 1 ? 0 : -1);

					if (choose_android_export_profile->get_item_count() > 1 && has_custom_gradle_build) {
						// If there's multiple options and at least one of them uses a custom gradle build then prompt the user to choose.
						choose_android_export_profile->show();
						install_android_build_template->popup_centered();
					} else {
						choose_android_export_profile->hide();

						if (export_template_manager->is_android_template_installed(android_export_preset)) {
							remove_android_build_template->set_text(vformat(TTR(REMOVE_ANDROID_BUILD_TEMPLATE_MESSAGE), export_template_manager->get_android_build_directory(android_export_preset)));
							remove_android_build_template->popup_centered();
						} else if (export_template_manager->can_install_android_template(android_export_preset)) {
							install_android_build_template->popup_centered();
						} else {
							gradle_build_manage_templates->popup_centered();
						}
					}
				}
			}
		} break;
		case PROJECT_OPEN_USER_DATA_FOLDER: {
			// Ensure_user_data_dir() to prevent the edge case: "Open User Data Folder" won't work after the project was renamed in ProjectSettingsEditor unless the project is saved.
			OS::get_singleton()->ensure_user_data_dir();
			OS::get_singleton()->shell_show_in_file_manager(OS::get_singleton()->get_user_data_dir(), true);
		} break;
		case SCENE_QUIT:
		case PROJECT_QUIT_TO_PROJECT_MANAGER:
		case PROJECT_RELOAD_CURRENT_PROJECT: {
			if (p_confirmed && plugin_to_save) {
				plugin_to_save->save_external_data();
				p_confirmed = false;
			}

			if (p_confirmed && stop_project_confirmation && get_project_run_bar()->is_playing()) {
				get_project_run_bar()->stop_playing();
				stop_project_confirmation = false;
				p_confirmed = false;
			}

			if (!p_confirmed) {
				if (!stop_project_confirmation && get_project_run_bar()->is_playing()) {
					if (p_option == PROJECT_RELOAD_CURRENT_PROJECT) {
						get_confirmation_dialog()->set_text(TTR("Stop running project before reloading the current project?"));
						get_confirmation_dialog()->set_ok_button_text(TTR("Stop & Reload"));
					} else {
						get_confirmation_dialog()->set_text(TTR("Stop running project before exiting the editor?"));
						get_confirmation_dialog()->set_ok_button_text(TTR("Stop & Quit"));
					}
					get_confirmation_dialog()->reset_size();
					get_confirmation_dialog()->popup_centered();
					get_confirmation_button()->hide();
					stop_project_confirmation = true;
					break;
				}

				bool save_each = EDITOR_GET("interface/editor/save_each_scene_on_quit");
				if (EditorSceneManager::get_singleton()->_next_unsaved_scene(!save_each) == -1) {
					if (EditorUndoRedoManager::get_singleton()->is_history_unsaved(EditorUndoRedoManager::GLOBAL_HISTORY)) {
						if (p_option == PROJECT_RELOAD_CURRENT_PROJECT) {
							get_save_confirmation_dialog()->set_ok_button_text(TTR("Save & Reload"));
							get_save_confirmation_dialog()->set_text(TTR("Save modified resources before reloading?"));
						} else {
							get_save_confirmation_dialog()->set_ok_button_text(TTR("Save & Quit"));
							get_save_confirmation_dialog()->set_text(TTR("Save modified resources before closing?"));
						}
						get_save_confirmation_dialog()->reset_size();
						get_save_confirmation_dialog()->popup_centered();
						break;
					}

					plugin_to_save = nullptr;
					for (int i = 0; i < get_editor_data().get_editor_plugin_count(); i++) {
						const String unsaved_status = get_editor_data().get_editor_plugin(i)->get_unsaved_status();
						if (!unsaved_status.is_empty()) {
							if (p_option == PROJECT_RELOAD_CURRENT_PROJECT) {
								get_save_confirmation_dialog()->set_ok_button_text(TTR("Save & Reload"));
								get_save_confirmation_dialog()->set_text(unsaved_status);
							} else {
								get_save_confirmation_dialog()->set_ok_button_text(TTR("Save & Quit"));
								get_save_confirmation_dialog()->set_text(unsaved_status);
							}
							get_save_confirmation_dialog()->reset_size();
							get_save_confirmation_dialog()->popup_centered();
							plugin_to_save = get_editor_data().get_editor_plugin(i);
							break;
						}
					}

					if (plugin_to_save) {
						break;
					}

					EditorSceneManager::get_singleton()->_discard_changes();
					break;
				}

				if (save_each) {
					EditorSceneManager::get_singleton()->set_tab_closing_menu_option(current_menu_option);
					for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
						EditorSceneManager::get_singleton()->queue_tab_close(get_editor_data().get_scene_path(i));
					}
					EditorSceneManager::get_singleton()->_proceed_closing_scene_tabs();
				} else {
					String unsaved_scenes;
					int i = EditorSceneManager::get_singleton()->_next_unsaved_scene(true, 0);
					while (i != -1) {
						unsaved_scenes += "\n            " + get_editor_data().get_edited_scene_root(i)->get_scene_file_path();
						i = EditorSceneManager::get_singleton()->_next_unsaved_scene(true, ++i);
					}
					if (p_option == PROJECT_RELOAD_CURRENT_PROJECT) {
						get_save_confirmation_dialog()->set_ok_button_text(TTR("Save & Reload"));
						get_save_confirmation_dialog()->set_text(TTR("Save changes to the following scene(s) before reloading?") + unsaved_scenes);
					} else {
						get_save_confirmation_dialog()->set_ok_button_text(TTR("Save & Quit"));
						get_save_confirmation_dialog()->set_text((p_option == SCENE_QUIT ? TTR("Save changes to the following scene(s) before quitting?") : TTR("Save changes to the following scene(s) before opening Project Manager?")) + unsaved_scenes);
					}
					get_save_confirmation_dialog()->reset_size();
					get_save_confirmation_dialog()->popup_centered();
				}

				DisplayServer::get_singleton()->window_request_attention();
				break;
			}
			EditorSceneManager::get_singleton()->_save_external_resources();
			EditorSceneManager::get_singleton()->_discard_changes();
		} break;
		case SPINNER_UPDATE_CONTINUOUSLY: {
			EditorSettings::get_singleton()->set("interface/editor/update_continuously", true);
			_update_update_spinner();
			show_accept(TTR("This option is deprecated. Situations where refresh must be forced are now considered a bug. Please report."), TTR("OK"));
		} break;
		case SPINNER_UPDATE_WHEN_CHANGED: {
			EditorSettings::get_singleton()->set("interface/editor/update_continuously", false);
			_update_update_spinner();
		} break;
		case SPINNER_UPDATE_SPINNER_HIDE: {
			EditorSettings::get_singleton()->set("interface/editor/show_update_spinner", 2); // Disabled
			_update_update_spinner();
		} break;
		case EDITOR_OPEN_SETTINGS: {
			EditorCore::get_singleton()->get_editor_settings_dialog()->popup_edit_settings();
		} break;
		case EDITOR_OPEN_DATA_FOLDER: {
			OS::get_singleton()->shell_show_in_file_manager(EditorPaths::get_singleton()->get_data_dir(), true);
		} break;
		case EDITOR_OPEN_CONFIG_FOLDER: {
			OS::get_singleton()->shell_show_in_file_manager(EditorPaths::get_singleton()->get_config_dir(), true);
		} break;
		case EDITOR_MANAGE_EXPORT_TEMPLATES: {
			if (ExportTemplateManager *export_template_manager = get_export_template_manager()) {
				export_template_manager->popup_manager();
			}
		} break;
		case EDITOR_CONFIGURE_FBX_IMPORTER: {
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
			EditorCore::get_singleton()->get_fbx_importer_manager()->show_dialog();
#endif
		} break;
		case EDITOR_MANAGE_FEATURE_PROFILES: {
			EditorFeatureProfileManager::get_singleton()->popup_centered_clamped(Size2(900, 800) * EDSCALE, 0.8);
		} break;
		case EDITOR_TOGGLE_FULLSCREEN: {
			DisplayServer::WindowMode mode = DisplayServer::get_singleton()->window_get_mode();
			if (mode == DisplayServer::WINDOW_MODE_FULLSCREEN || mode == DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
				DisplayServer::get_singleton()->window_set_mode(prev_mode);
			} else {
				prev_mode = mode;
				DisplayServer::get_singleton()->window_set_mode(DisplayServer::WINDOW_MODE_FULLSCREEN);
			}
		} break;
		case EDITOR_TAKE_SCREENSHOT: {
			_request_screenshot();
		} break;
		case SETTINGS_PICK_MAIN_SCENE: {
			get_file_dialog()->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);
			get_file_dialog()->clear_filters();
			for (const String &extension : extensions) {
				get_file_dialog()->add_filter("*." + extension, extension.to_upper());
			}

			Node *scene = get_editor_data().get_edited_scene_root();
			if (scene) {
				get_file_dialog()->set_current_path(scene->get_scene_file_path());
			}
			get_file_dialog()->set_title(TTR("Pick a Main Scene"));
			get_file_dialog()->popup_file_dialog();

		} break;
		case HELP_SEARCH: {
			emit_signal(SNAME("request_help_search"), "");
		} break;
		case EDITOR_COMMAND_PALETTE: {
			EditorCore::get_singleton()->get_command_palette()->open_popup();
		} break;
		case HELP_DOCS: {
			OS::get_singleton()->shell_open(REDOT_VERSION_DOCS_URL "/");
		} break;
		case HELP_FORUM: {
			OS::get_singleton()->shell_open("https://forum.redotengine.org/");
		} break;
		case HELP_REPORT_A_BUG: {
			OS::get_singleton()->shell_open("https://github.com/Redot-Engine/redot-engine/issues");
		} break;
		case HELP_COPY_SYSTEM_INFO: {
			String info = EditorCore::get_singleton()->_get_system_info();
			DisplayServer::get_singleton()->clipboard_set(info);
		} break;
		case HELP_SUGGEST_A_FEATURE: {
			OS::get_singleton()->shell_open("https://github.com/Redot-Engine/redot-proposals#readme");
		} break;
		case HELP_SEND_DOCS_FEEDBACK: {
			OS::get_singleton()->shell_open("https://github.com/Redot-Engine/Redot-Documentation/issues");
		} break;
		case HELP_COMMUNITY: {
			OS::get_singleton()->shell_open("https://redotengine.org/community");
		} break;
		case HELP_ABOUT: {
			get_about_dialog()->popup_centered(Size2(780, 500) * EDSCALE);
		} break;
		case HELP_SUPPORT_REDOT_DEVELOPMENT: {
			OS::get_singleton()->shell_open("https://redotfoundation.org/support");
		} break;
	}
}

void EditorShell::_android_build_source_selected(const String &p_file) {
	ExportTemplateManager *export_template_manager = get_export_template_manager();
	ERR_FAIL_NULL(export_template_manager);
	export_template_manager->install_android_template_from_file(p_file, android_export_preset);
}

void EditorShell::_android_export_preset_selected(int p_index) {
	ExportTemplateManager *export_template_manager = get_export_template_manager();
	OptionButton *choose_android_export_profile = get_choose_android_export_profile();
	Label *install_android_build_template_message = get_install_android_build_template_message();
	ERR_FAIL_NULL(export_template_manager);
	ERR_FAIL_NULL(choose_android_export_profile);
	ERR_FAIL_NULL(install_android_build_template_message);
	if (p_index >= 0) {
		android_export_preset = EditorExport::get_singleton()->get_export_preset(choose_android_export_profile->get_item_id(p_index));
	} else {
		android_export_preset.unref();
	}
	install_android_build_template_message->set_text(vformat(TTR(INSTALL_ANDROID_BUILD_TEMPLATE_MESSAGE), export_template_manager->get_android_build_directory(android_export_preset)));
}

void EditorShell::_android_install_build_template() {
	if (ConfirmationDialog *install_android_build_template = get_install_android_build_template_dialog()) {
		install_android_build_template->hide();
	}
	if (EditorFileDialog *file_android_build_source = get_file_android_build_source_dialog()) {
		file_android_build_source->popup_centered_ratio();
	}
}

void EditorShell::_android_explore_build_templates() {
	ExportTemplateManager *export_template_manager = get_export_template_manager();
	ERR_FAIL_NULL(export_template_manager);
	OS::get_singleton()->shell_show_in_file_manager(ProjectSettings::get_singleton()->globalize_path(export_template_manager->get_android_build_directory(android_export_preset).get_base_dir()), true);
}

void EditorShell::_tool_menu_option(int p_idx) {
	PopupMenu *tool_menu = get_tool_menu();
	switch (tool_menu->get_item_id(p_idx)) {
		case TOOLS_ORPHAN_RESOURCES: {
			if (OrphanResourcesDialog *orphan_resources = get_orphan_resources_dialog()) {
				orphan_resources->show();
			}
		} break;
		case TOOLS_BUILD_PROFILE_MANAGER: {
			if (EditorBuildProfileManager *build_profile_manager = get_build_profile_manager()) {
				build_profile_manager->popup_centered_clamped(Size2(700, 800) * EDSCALE, 0.8);
			}
		} break;
		case TOOLS_PROJECT_UPGRADE: {
			if (ProjectUpgradeTool *project_upgrade_tool = get_project_upgrade_tool()) {
				project_upgrade_tool->popup_dialog();
			}
		} break;
		case TOOLS_UID_VIEWER: {
			if (UIDViewer *uid_viewer = get_uid_viewer()) {
				uid_viewer->_open_tool();
			}
		} break;
		case TOOLS_CUSTOM: {
			if (tool_menu->get_item_submenu(p_idx) == "") {
				Callable callback = tool_menu->get_item_metadata(p_idx);
				Callable::CallError ce;
				Variant result;
				callback.callp(nullptr, 0, result, ce);

				if (ce.error != Callable::CallError::CALL_OK) {
					String err = Variant::get_callable_error_text(callback, nullptr, 0, ce);
					ERR_PRINT("Error calling function from tool menu: " + err);
				}
			} // Else it's a submenu so don't do anything.
		} break;
	}
}

void EditorShell::_exit_editor(int p_exit_code) {
	exiting = true;
	waiting_for_first_scan = false;
	EditorCore::get_singleton()->get_resource_preview()->stop(); // Stop early to avoid crashes.
	_save_editor_layout_on_exit();

	// Dim the editor window while it's quitting to make it clearer that it's busy.
	dim_editor(true);

	// Unload addons before quitting to allow cleanup.
	EditorPluginManager::get_singleton()->unload_editor_addons();

	get_tree()->quit(p_exit_code);
}

void EditorShell::_update_file_menu_opened() {
	bool has_unsaved = false;
	for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
		if (EditorSceneManager::get_singleton()->_is_scene_unsaved(i)) {
			has_unsaved = true;
			break;
		}
	}
	if (has_unsaved) {
		get_file_menu()->set_item_disabled(get_file_menu()->get_item_index(SCENE_SAVE_ALL_SCENES), false);
		get_file_menu()->set_item_tooltip(get_file_menu()->get_item_index(SCENE_SAVE_ALL_SCENES), String());
	} else {
		get_file_menu()->set_item_disabled(get_file_menu()->get_item_index(SCENE_SAVE_ALL_SCENES), true);
		get_file_menu()->set_item_tooltip(get_file_menu()->get_item_index(SCENE_SAVE_ALL_SCENES), TTR("All scenes are already saved."));
	}
	EditorSceneManager::get_singleton()->_update_undo_redo_allowed();
}

void EditorShell::edit_foreign_resource(const Ref<Resource> &p_resource) {
	EditorSceneManager::get_singleton()->load_scene(p_resource->get_path().get_slice("::", 0));
	callable_mp(InspectorDock::get_singleton(), &InspectorDock::edit_resource).call_deferred(p_resource);
}

void EditorShell::_inherit_request(String p_file) {
	current_menu_option = SCENE_NEW_INHERITED_SCENE;
	_dialog_action(p_file);
}

void EditorShell::_instantiate_request(const Vector<String> &p_files) {
	EditorSceneManager::get_singleton()->request_instantiate_scenes(p_files);
}

void EditorShell::_add_to_recent_scenes(const String &p_scene) {
	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scenes", Array());
	if (rc.has(p_scene)) {
		rc.erase(p_scene);
	}
	rc.push_front(p_scene);
	if (rc.size() > 10) {
		rc.resize(10);
	}

	EditorSettings::get_singleton()->set_project_metadata("recent_files", "scenes", rc);
	_update_recent_scenes();
}

void EditorShell::_open_recent_scene(int p_idx) {
	PopupMenu *recent_scenes = get_recent_scenes_menu();
	if (p_idx == recent_scenes->get_item_count() - 1) {
		EditorSettings::get_singleton()->set_project_metadata("recent_files", "scenes", Array());
		callable_mp(this, &EditorShell::_update_recent_scenes).call_deferred();
	} else {
		Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scenes", Array());
		ERR_FAIL_INDEX(p_idx, rc.size());

		if (EditorSceneManager::get_singleton()->load_scene(rc[p_idx]) != OK) {
			rc.remove_at(p_idx);
			EditorSettings::get_singleton()->set_project_metadata("recent_files", "scenes", rc);
			_update_recent_scenes();
		}
	}
}

void EditorShell::_update_recent_scenes() {
	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scenes", Array());
	PopupMenu *recent_scenes = get_recent_scenes_menu();
	recent_scenes->clear();

	if (rc.size() == 0) {
		recent_scenes->add_item(TTRC("No Recent Scenes"), -1);
		recent_scenes->set_item_disabled(-1, true);
	} else {
		String path;
		for (int i = 0; i < rc.size(); i++) {
			path = rc[i];
			recent_scenes->add_item(path.replace("res://", ""), i);
		}

		recent_scenes->add_separator();
		recent_scenes->add_shortcut(ED_SHORTCUT("editor/clear_recent", TTRC("Clear Recent Scenes")));
	}
	recent_scenes->set_item_auto_translate_mode(-1, AUTO_TRANSLATE_MODE_ALWAYS);
	recent_scenes->reset_size();
}

void EditorShell::_quick_opened(const String &p_file_path) {
	EditorCore::get_singleton()->load_scene_or_resource(p_file_path);
}

void EditorShell::_project_run_started() {
	if (bool(EDITOR_GET("run/output/always_clear_output_on_play"))) {
		EditorCore::get_singleton()->log->clear();
	}

	int action_on_play = EDITOR_GET("run/bottom_panel/action_on_play");
	if (action_on_play == ACTION_ON_PLAY_OPEN_OUTPUT) {
		get_bottom_panel()->make_item_visible(EditorCore::get_singleton()->log);
	} else if (action_on_play == ACTION_ON_PLAY_OPEN_DEBUGGER) {
		get_bottom_panel()->make_item_visible(EditorDebuggerNode::get_singleton());
	}
}

void EditorShell::_project_run_stopped() {
	int action_on_stop = EDITOR_GET("run/bottom_panel/action_on_stop");
	if (action_on_stop == ACTION_ON_STOP_CLOSE_BUTTOM_PANEL) {
		get_bottom_panel()->hide_bottom_panel();
	}
}

void EditorShell::notify_all_debug_sessions_exited() {
	get_project_run_bar()->stop_playing();
}

void EditorShell::add_io_error(const String &p_error) {
	show_warning(p_error, TTR("IO Error"));
}

void EditorShell::add_io_warning(const String &p_warning) {
	show_warning(p_warning, TTR("IO Warning"));
}

bool EditorShell::close_scene() {
	int tab_index = get_editor_data().get_edited_scene();
	if (tab_index == 0 && EditorSceneManager::get_singleton()->get_edited_scene() == nullptr && get_editor_data().get_scene_path(tab_index).is_empty()) {
		return false;
	}

	EditorSceneManager::get_singleton()->set_tab_closing_idx(tab_index);
	current_menu_option = SCENE_CLOSE;
	EditorSceneManager::get_singleton()->_discard_changes();
	EditorSceneManager::get_singleton()->set_changing_scene(false);
	return true;
}

OS::ProcessID EditorShell::has_child_process(OS::ProcessID p_pid) const {
	return get_project_run_bar()->has_child_process(p_pid);
}

void EditorShell::stop_child_process(OS::ProcessID p_pid) {
	get_project_run_bar()->stop_child_process(p_pid);
}

void EditorShell::_pick_main_scene_custom_action(const String &p_custom_action_name) {
	if (p_custom_action_name == "select_current") {
		Node *scene = get_editor_data().get_edited_scene_root();

		if (!scene) {
			show_accept(TTR("There is no defined scene to run."), TTR("OK"));
			return;
		}

		get_pick_main_scene_dialog()->hide();

		if (!FileAccess::exists(scene->get_scene_file_path())) {
			current_menu_option = SAVE_AND_RUN_MAIN_SCENE;
			_menu_option_confirm(SCENE_SAVE_AS_SCENE, true);
			get_file_dialog()->set_title(TTR("Save scene before running..."));
		} else {
			current_menu_option = SETTINGS_PICK_MAIN_SCENE;
			_dialog_action(scene->get_scene_file_path());
		}
	}
}

AcceptDialog *EditorShell::get_accept_dialog() {
	if (!accept) {
		accept = memnew(AcceptDialog);
		accept->set_unparent_when_invisible(true);
		accept->set_autowrap(true);
		accept->set_min_size(Vector2i(600, 0));
		get_gui_base()->add_child(accept);
	}
	return accept;
}

AcceptDialog *EditorShell::get_save_accept_dialog() {
	if (!save_accept) {
		save_accept = memnew(AcceptDialog);
		save_accept->set_unparent_when_invisible(true);
		get_gui_base()->add_child(save_accept);
	}
	return save_accept;
}

AcceptDialog *EditorShell::get_warning_dialog() {
	if (!warning) {
		warning = memnew(AcceptDialog);
		warning->set_unparent_when_invisible(true);
		get_gui_base()->add_child(warning);
	}
	return warning;
}

void EditorShell::show_accept(const String &p_text, const String &p_title) {
	current_menu_option = -1;
	_close_save_scene_progress();
	AcceptDialog *accept_dialog = get_accept_dialog();
	accept_dialog->set_ok_button_text(p_title);
	accept_dialog->set_text(p_text);
	accept_dialog->reset_size();
	if (accept_dialog->is_inside_tree()) {
		accept_dialog->popup_centered_clamped(Size2i(), 0.0);
	} else {
		EditorInterface::get_singleton()->popup_dialog_centered_clamped(accept_dialog, Size2i(), 0.0);
	}
}

void EditorShell::show_save_accept(const String &p_text, const String &p_title) {
	current_menu_option = -1;
	_close_save_scene_progress();
	AcceptDialog *save_accept_dialog = get_save_accept_dialog();
	save_accept_dialog->set_ok_button_text(p_title);
	save_accept_dialog->set_text(p_text);
	save_accept_dialog->reset_size();
	if (save_accept_dialog->is_inside_tree()) {
		save_accept_dialog->popup_centered_clamped(Size2i(), 0.0);
	} else {
		EditorInterface::get_singleton()->popup_dialog_centered_clamped(save_accept_dialog, Size2i(), 0.0);
	}
}

void EditorShell::show_warning(const String &p_text, const String &p_title) {
	_close_save_scene_progress();
	AcceptDialog *warning_dialog = get_warning_dialog();
	warning_dialog->set_text(p_text);
	warning_dialog->set_title(p_title);
	warning_dialog->reset_size();
	if (warning_dialog->is_inside_tree()) {
		warning_dialog->popup_centered_clamped(Size2i(), 0.0);
	} else {
		EditorInterface::get_singleton()->popup_dialog_centered_clamped(warning_dialog, Size2i(), 0.0);
	}
}

bool EditorShell::ensure_main_scene(bool p_from_native) {
	get_pick_main_scene_dialog()->set_meta("from_native", p_from_native); // Whether from play button or native run.
	String main_scene = GLOBAL_GET("application/run/main_scene");

	if (main_scene.is_empty()) {
		current_menu_option = -1;
		get_pick_main_scene_dialog()->set_text(TTR("No main scene has ever been defined. Select one?\nYou can change it later in \"Project Settings\" under the 'application' category."));
		get_pick_main_scene_dialog()->popup_centered();

		if (get_editor_data().get_edited_scene_root()) {
			get_select_current_scene_button()->set_disabled(false);
			get_select_current_scene_button()->grab_focus();
		} else {
			get_select_current_scene_button()->set_disabled(true);
		}

		return false;
	}

	if (!FileAccess::exists(main_scene)) {
		current_menu_option = -1;
		get_pick_main_scene_dialog()->set_text(vformat(TTR("Selected scene '%s' does not exist. Select a valid one?\nYou can change it later in \"Project Settings\" under the 'application' category."), main_scene));
		get_pick_main_scene_dialog()->popup_centered();
		return false;
	}

	if (ResourceLoader::get_resource_type(main_scene) != "PackedScene") {
		current_menu_option = -1;
		get_pick_main_scene_dialog()->set_text(vformat(TTR("Selected scene '%s' is not a scene file. Select a valid one?\nYou can change it later in \"Project Settings\" under the 'application' category."), main_scene));
		get_pick_main_scene_dialog()->popup_centered();
		return false;
	}

	return true;
}

void EditorShell::_immediate_dialog_confirmed() {
	immediate_dialog_confirmed = true;
}

bool EditorShell::immediate_confirmation_dialog(const String &p_text, const String &p_ok_text, const String &p_cancel_text, uint32_t p_wrap_width) {
	ConfirmationDialog *cd = memnew(ConfirmationDialog);
	cd->set_text(p_text);
	cd->set_ok_button_text(p_ok_text);
	cd->set_cancel_button_text(p_cancel_text);
	if (p_wrap_width > 0) {
		cd->set_autowrap(true);
		cd->get_label()->set_custom_minimum_size(Size2(p_wrap_width, 0) * EDSCALE);
	}

	cd->connect(SceneStringName(confirmed), callable_mp(this, &EditorShell::_immediate_dialog_confirmed));
	get_gui_base()->add_child(cd);

	cd->popup_centered();

	while (true) {
		DisplayServer::get_singleton()->process_events();
		Main::iteration();
		if (immediate_dialog_confirmed || !cd->is_visible()) {
			break;
		}
	}

	memdelete(cd);
	return immediate_dialog_confirmed;
}

bool EditorShell::is_cmdline_mode() {
	return cmdline_mode;
}

bool EditorShell::is_exiting() const {
	return exiting;
}

bool EditorShell::is_editor_ready() const {
	return is_inside_tree() && !waiting_for_first_scan;
}

bool EditorShell::is_editor_dimmed() const {
	return dimmed;
}

void EditorShell::set_current_menu_option(int p_option) {
	current_menu_option = p_option;
}

int EditorShell::get_current_menu_option() {
	return current_menu_option;
}

bool EditorShell::_is_closing_editor() const {
	return EditorSceneManager::get_singleton()->get_tab_closing_menu_option() == SCENE_QUIT ||
			EditorSceneManager::get_singleton()->get_tab_closing_menu_option() == PROJECT_QUIT_TO_PROJECT_MANAGER ||
			EditorSceneManager::get_singleton()->get_tab_closing_menu_option() == PROJECT_RELOAD_CURRENT_PROJECT;
}

void EditorShell::_restart_editor(bool p_goto_project_manager) {
	exiting = true;

	if (get_project_run_bar()->is_playing()) {
		get_project_run_bar()->stop_playing();
	}

	String to_reopen;
	if (!p_goto_project_manager && get_tree()->get_edited_scene_root()) {
		to_reopen = get_tree()->get_edited_scene_root()->get_scene_file_path();
	}

	_exit_editor(EXIT_SUCCESS);

	List<String> args;
	for (const String &a : Main::get_forwardable_cli_arguments(Main::CLI_SCOPE_TOOL)) {
		args.push_back(a);
	}

	if (p_goto_project_manager) {
		args.push_back("--project-manager");

		// Setup working directory.
		const String exec_dir = OS::get_singleton()->get_executable_path().get_base_dir();
		if (!exec_dir.is_empty()) {
			args.push_back("--path");
			args.push_back(exec_dir);
		}
	} else {
		args.push_back("--path");
		args.push_back(ProjectSettings::get_singleton()->get_resource_path());

		args.push_back("-e");
	}

	if (!to_reopen.is_empty()) {
		args.push_back(to_reopen);
	}

	OS::get_singleton()->set_restart_on_exit(true, args);
}

void EditorShell::restart_editor(bool p_goto_project_manager) {
	_menu_option_confirm(p_goto_project_manager ? PROJECT_QUIT_TO_PROJECT_MANAGER : PROJECT_RELOAD_CURRENT_PROJECT, false);
}

void EditorShell::_cancel_confirmation() {
	stop_project_confirmation = false;
}

void EditorShell::_prepare_save_confirmation_popup() {
	ConfirmationDialog *sc = get_save_confirmation_dialog();
	if (sc->get_window() != get_last_exclusive_window()) {
		sc->reparent(get_last_exclusive_window());
	}
}

Dictionary EditorShell::drag_resource(const Ref<Resource> &p_res, Control *p_from) {
	Control *drag_control = memnew(Control);
	TextureRect *drag_preview = memnew(TextureRect);
	Label *label = memnew(Label);
	label->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

	Ref<Texture2D> preview;

	{
		/// @todo Make proper previews
		Ref<Texture2D> texture = EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("FileBigThumb"), EditorStringName(EditorIcons));
		if (texture.is_valid()) {
			Ref<Image> img = texture->get_image();
			img = img->duplicate();
			img->resize(48, 48); // meh
			preview = ImageTexture::create_from_image(img);
		}
	}

	drag_preview->set_texture(preview);
	drag_control->add_child(drag_preview);
	if (p_res->get_path().is_resource_file()) {
		label->set_text(p_res->get_path().get_file());
	} else if (!p_res->get_name().is_empty()) {
		label->set_text(p_res->get_name());
	} else {
		label->set_text(p_res->get_class());
	}

	drag_control->add_child(label);

	p_from->set_drag_preview(drag_control); // Wait until it enters scene.

	label->set_position(Point2((preview->get_width() - label->get_minimum_size().width) / 2, preview->get_height()));

	Dictionary drag_data;
	drag_data["type"] = "resource";
	drag_data["resource"] = p_res;
	drag_data["from"] = p_from;

	return drag_data;
}

Dictionary EditorShell::drag_files_and_dirs(const Vector<String> &p_paths, Control *p_from) {
	bool has_folder = false;
	bool has_file = false;
	for (int i = 0; i < p_paths.size(); i++) {
		bool is_folder = p_paths[i].ends_with("/");
		has_folder |= is_folder;
		has_file |= !is_folder;
	}

	int max_rows = 6;
	int num_rows = p_paths.size() > max_rows ? max_rows - 1 : p_paths.size(); // Don't waste a row to say "1 more file" - list it instead.
	VBoxContainer *vbox = memnew(VBoxContainer);
	for (int i = 0; i < num_rows; i++) {
		HBoxContainer *hbox = memnew(HBoxContainer);
		TextureRect *icon = memnew(TextureRect);
		Label *label = memnew(Label);
		label->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
		label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

		if (p_paths[i].ends_with("/")) {
			label->set_text(p_paths[i].substr(0, p_paths[i].length() - 1).get_file());
			icon->set_texture(EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("Folder"), EditorStringName(EditorIcons)));
		} else {
			label->set_text(p_paths[i].get_file());
			icon->set_texture(EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("File"), EditorStringName(EditorIcons)));
		}
		icon->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
		icon->set_size(Size2(16, 16));
		hbox->add_child(icon);
		hbox->add_child(label);
		vbox->add_child(hbox);
	}

	if (p_paths.size() > num_rows) {
		Label *label = memnew(Label);
		label->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
		if (has_file && has_folder) {
			label->set_text(vformat(TTR("%d more files or folders"), p_paths.size() - num_rows));
		} else if (has_folder) {
			label->set_text(vformat(TTR("%d more folders"), p_paths.size() - num_rows));
		} else {
			label->set_text(vformat(TTR("%d more files"), p_paths.size() - num_rows));
		}
		vbox->add_child(label);
	}
	p_from->set_drag_preview(vbox); // Wait until it enters scene.

	Dictionary drag_data;
	drag_data["type"] = has_folder ? "files_and_dirs" : "files";
	drag_data["files"] = p_paths;
	drag_data["from"] = p_from;
	return drag_data;
}

void EditorShell::add_tool_menu_item(const String &p_name, const Callable &p_callback) {
	PopupMenu *tool_menu = get_tool_menu();
	int idx = tool_menu->get_item_count();
	tool_menu->add_item(p_name, TOOLS_CUSTOM);
	tool_menu->set_item_metadata(idx, p_callback);
}

void EditorShell::add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu) {
	ERR_FAIL_NULL(p_submenu);
	ERR_FAIL_COND(p_submenu->get_parent() != nullptr);
	get_tool_menu()->add_submenu_node_item(p_name, p_submenu, TOOLS_CUSTOM);
}

void EditorShell::remove_tool_menu_item(const String &p_name) {
	PopupMenu *tool_menu = get_tool_menu();
	for (int i = 0; i < tool_menu->get_item_count(); i++) {
		if (tool_menu->get_item_id(i) != TOOLS_CUSTOM) {
			continue;
		}

		if (tool_menu->get_item_text(i) == p_name) {
			if (tool_menu->get_item_submenu(i) != "") {
				Node *n = tool_menu->get_node(tool_menu->get_item_submenu(i));
				tool_menu->remove_child(n);
				memdelete(n);
			}
			tool_menu->remove_item(i);
			tool_menu->reset_size();
			return;
		}
	}
}

void EditorShell::_inherit_imported(const String &p_action) {
	get_open_imported_dialog()->hide();
	EditorSceneManager::get_singleton()->load_scene(EditorSceneManager::get_singleton()->get_open_import_request(), true, true);
}

void EditorShell::_open_imported() {
	EditorSceneManager::get_singleton()->load_scene(EditorSceneManager::get_singleton()->get_open_import_request(), true, false, true);
}

void EditorShell::dim_editor(bool p_dimming) {
	dimmed = p_dimming;
	get_gui_base()->set_modulate(p_dimming ? Color(0.5, 0.5, 0.5) : Color(1, 1, 1));
}

void EditorShell::open_export_template_manager() {
	if (ExportTemplateManager *export_template_manager = get_export_template_manager()) {
		export_template_manager->popup_manager();
	}
}

void EditorShell::open_setting_override(const String &p_property) {
	EditorCore::get_singleton()->get_editor_settings_dialog()->hide();
	EditorCore::get_singleton()->get_project_settings_editor()->popup_for_override(p_property);
}

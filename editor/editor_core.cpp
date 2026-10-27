/**************************************************************************/
/*  editor_core.cpp                                                       */
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

#include "editor_core.h"
#include "editor/editor_scene_manager.h"
#include "editor/editor_shell.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/editor_plugin_manager.h"

#include "editor/animation/animation_player_editor_plugin.h"
#include "editor/asset_library/asset_library_editor_plugin.h"
#include "editor/audio/editor_audio_buses.h"
#include "editor/run/game_view_plugin.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "editor/script/script_text_editor.h"
#include "editor/version_control/version_control_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/time.h"
#include "core/string/translation_server.h"
#include "core/version.h"
#include "editor/debugger/editor_debugger_inspector.h"
#include "editor/doc/editor_help.h"
#include "editor/docks/inspector_dock.h"
#include "editor/editor_log.h"
#include "editor/editor_string_names.h"
#include "editor/export/editor_export.h"
#include "editor/export/project_export.h"
#include "editor/file_system/dependency_editor.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_quick_open_dialog.h"
#include "editor/gui/editor_toaster.h"
#include "editor/import/fbx_importer_manager.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/inspector/editor_property_name_processor.h"
#include "editor/inspector/editor_resource_preview.h"
#include "editor/inspector/multi_node_edit.h"
#include "editor/scene/canvas_item_editor_plugin.h"
#include "editor/script/editor_script.h"
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_feature_profile.h"
#include "editor/settings/editor_settings.h"
#include "editor/settings/editor_settings_dialog.h"
#include "editor/settings/project_settings_editor.h"
#include "editor/shader/editor_native_shader_source_visualizer.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/file_dialog.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/property_utils.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/portable_compressed_texture.h"
#include "scene/theme/theme_db.h"
#include "servers/audio/audio_server.h"
#include "servers/display/display_server.h"
#include "servers/navigation_2d/navigation_server_2d.h"
#include "servers/navigation_3d/navigation_server_3d.h"
#include "servers/physics_2d/physics_server_2d.h"
#include "servers/physics_3d/physics_server_3d.h"
#include "servers/rendering/rendering_server.h"

EditorCore *EditorCore::singleton = nullptr;

static Node *_resource_get_edited_scene() {
	return EditorSceneManager::get_singleton()->get_edited_scene();
}

void EditorCore::_initialize_engine() {
	Resource::_get_local_scene_func = _resource_get_edited_scene;

	PortableCompressedTexture2D::set_keep_all_compressed_buffers(true);
	RenderingServer::get_singleton()->set_debug_generate_wireframes(true);
	AudioServer::get_singleton()->set_enable_tagging_used_audio_streams(true);

	// No navigation by default if in editor.
	if (NavigationServer3D::get_singleton()->get_debug_enabled()) {
		NavigationServer3D::get_singleton()->set_active(true);
	} else {
		NavigationServer3D::get_singleton()->set_active(false);
	}

#ifndef PHYSICS_3D_DISABLED
	PhysicsServer3D::get_singleton()->set_active(false);
#endif
#ifndef PHYSICS_2D_DISABLED
	PhysicsServer2D::get_singleton()->set_active(false);
#endif

	// No scripting by default if in editor, except for tool scripts.
	ScriptServer::set_scripting_enabled(false);

	if (!DisplayServer::get_singleton()->is_touchscreen_available()) {
		Input::get_singleton()->set_emulate_touch_from_mouse(false);
	}
	if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CUSTOM_CURSOR_SHAPE)) {
		DisplayServer::get_singleton()->cursor_set_custom_image(Ref<Resource>());
	}

	SceneState::set_disable_placeholders(true);
	ResourceLoader::clear_translation_remaps();
	ResourceLoader::set_create_missing_resources_if_class_unavailable(true);

	TranslationServer::get_singleton()->get_main_domain()->set_enabled(false);

	if (!EditorSettings::get_singleton()) {
		EditorSettings::create();
	}

	// Exporters might need the theme.
	EditorThemeManager::initialize();
	theme = EditorThemeManager::generate_theme();
	DisplayServer::set_early_window_clear_color_override(true, theme->get_color(SNAME("background"), EditorStringName(Editor)));

	const Vector<String> textfile_ext = ((String)(EDITOR_GET("docks/filesystem/textfile_extensions"))).split(",", false);
	for (const String &E : textfile_ext) {
		textfile_extensions.insert(E);
	}
	const Vector<String> other_file_ext = ((String)(EDITOR_GET("docks/filesystem/other_file_extensions"))).split(",", false);
	for (const String &E : other_file_ext) {
		other_file_extensions.insert(E);
	}
}

void EditorCore::_create_builtin_plugins() {
	// Register built-in editor plugins with the plugin manager.
	// This must happen after editor_plugin_manager is created and before
	// any code calls get_editor_by_name() or get_spatial_editor().

	// Animation editors
	editor_plugin_manager->add_editor_plugin(memnew(AnimationPlayerEditorPlugin));
	editor_plugin_manager->add_editor_plugin(memnew(AnimationTrackKeyEditEditorPlugin));
	editor_plugin_manager->add_editor_plugin(memnew(AnimationMarkerKeyEditEditorPlugin));

	// Scene editors
	editor_plugin_manager->add_editor_plugin(memnew(CanvasItemEditorPlugin));
	editor_plugin_manager->add_editor_plugin(memnew(Node3DEditorPlugin));

	// Script editor
	editor_plugin_manager->add_editor_plugin(memnew(ScriptEditorPlugin));

	// Game view (not in recovery mode)
	if (!Engine::get_singleton()->is_recovery_mode_hint()) {
		editor_plugin_manager->add_editor_plugin(memnew(GameViewPlugin));
	}

	// Audio buses editor
	EditorAudioBuses *audio_bus_editor = EditorAudioBuses::register_editor();
	editor_plugin_manager->add_editor_plugin(memnew(AudioBusesEditorPlugin(audio_bus_editor)));

	// Script text editors
	ScriptTextEditor::register_editor(); // Register one for text scripts.

	// Asset library
	if (AssetLibraryEditorPlugin::is_available()) {
		editor_plugin_manager->add_editor_plugin(memnew(AssetLibraryEditorPlugin));
	} else {
		print_verbose("Asset Library not available (due to using Web editor, or SSL support disabled).");
	}

	// Version control
	editor_plugin_manager->add_editor_plugin(VersionControlEditorPlugin::get_singleton());
}

void EditorCore::_initialize_services() {
	add_child(memnew(EditorPropertyNameProcessor));

	editor_scene_manager = memnew(EditorSceneManager);
	add_child(editor_scene_manager);

	// Create EditorFeatureProfileManager early - many components
	// (inspector, create dialogs, docks) call get_singleton() on it.
	memnew(EditorFeatureProfileManager);
	add_child(EditorFeatureProfileManager::get_singleton());

	editor_plugin_manager = memnew(EditorPluginManager);
	editor_plugin_manager->set_core(this);
	if (shell) {
		editor_plugin_manager->set_shell(shell);
	}
	editor_scene_manager->set_plugin_manager(editor_plugin_manager);

	// Create built-in editor plugins before creating any docks or other
	// components that look them up via get_editor_by_name().
	_create_builtin_plugins();

	editor_selection = memnew(EditorSelection);

	editor_file_system = memnew(EditorFileSystem);
	add_child(editor_file_system);

	editor_file_system->connect("resources_reimporting", callable_mp(editor_scene_manager, &EditorSceneManager::_resources_reimporting));
	editor_file_system->connect("resources_reimported", callable_mp(editor_scene_manager, &EditorSceneManager::_resources_reimported));

	editor_export = memnew(EditorExport);
	add_child(editor_export);

	resource_preview = memnew(EditorResourcePreview);
	add_child(resource_preview);

	log = memnew(EditorLog);
	// Note: log is not added as a child here. The shell will parent it
	// into the bottom panel. We still own it and delete it in our destructor.

	command_palette = EditorCommandPalette::get_singleton();

	_build_icon_type_cache();

	_register_global_callbacks();
}

void EditorCore::initialize_dialogs() {
	project_export = memnew(ProjectExportDialog);
	add_child(project_export);

	editor_settings_dialog = memnew(EditorSettingsDialog);
	add_child(editor_settings_dialog);

	project_settings_editor = memnew(ProjectSettingsEditor(&editor_data));
	add_child(project_settings_editor);

	quick_open_dialog = memnew(EditorQuickOpenDialog);
	// Note: not added as a child. The shell will parent it.

	quick_open_color_palette = memnew(EditorQuickOpenDialog);
	// Note: not added as a child. The shell will parent it.

	native_shader_source_visualizer = memnew(EditorNativeShaderSourceVisualizer);
	add_child(native_shader_source_visualizer);

#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	fbx_importer_manager = memnew(FBXImporterManager);
	add_child(fbx_importer_manager);
#endif
}

void EditorCore::unparent_quick_open_dialogs() {
	for (EditorQuickOpenDialog *dialog : { quick_open_dialog, quick_open_color_palette }) {
		if (dialog && dialog->get_parent()) {
			dialog->get_parent()->remove_child(dialog);
		}
	}
}

void EditorCore::initialize() {
	ERR_FAIL_COND_MSG(editor_selection != nullptr, "EditorCore::initialize() was called more than once.");
	_initialize_engine();
	_initialize_services();
}

void EditorCore::set_shell(EditorShell *p_shell) {
	shell = p_shell;
	if (p_shell) {
		p_shell->set_editor_core(this);
		p_shell->set_scene_manager(editor_scene_manager);
		p_shell->set_editor_plugin_manager(editor_plugin_manager);
	}
	if (editor_plugin_manager) {
		editor_plugin_manager->set_shell(p_shell);
	}
	if (editor_scene_manager) {
		editor_scene_manager->set_shell(p_shell);
	}
}

EditorCore::EditorCore() {
	DEV_ASSERT(!singleton);
	singleton = this;
}

void EditorCore::remove_print_handler_early() {
	remove_print_handler(&print_handler);
}

EditorCore::~EditorCore() {
	_unregister_global_callbacks();
	EditorSettings::destroy();
	EditorThemeManager::finalize();
	memdelete_notnull(editor_selection);
	memdelete_notnull(quick_open_dialog);
	memdelete_notnull(quick_open_color_palette);
	memdelete_notnull(editor_plugin_manager);
	singleton = nullptr;
}

void EditorCore::_register_global_callbacks() {
	// Used for previews.
	FileDialog::get_icon_func = _file_dialog_get_icon;
	FileDialog::register_func = _file_dialog_register;
	FileDialog::unregister_func = _file_dialog_unregister;

	EditorFileDialog::get_icon_func = _file_dialog_get_icon;
	EditorFileDialog::register_func = _editor_file_dialog_register;
	EditorFileDialog::unregister_func = _editor_file_dialog_unregister;

	FileAccess::set_file_close_fail_notify_callback(_file_access_close_error_notify);
	ResourceSaver::set_save_callback(_resource_saved);
	ResourceLoader::set_load_callback(_resource_loaded);
	ResourceLoader::set_dependency_error_notify_func(&EditorCore::_dependency_error_report);
	ResourceLoader::set_error_notify_func(_resource_load_error_notify);
	SceneState::set_instantiation_warning_notify_func(_instantiation_warning_notify);

	// Keep the scene manager's undo/redo-dependent UI state in sync with the
	// global undo history.
	EditorUndoRedoManager::get_singleton()->connect("version_changed", callable_mp(editor_scene_manager, &EditorSceneManager::_update_undo_redo_allowed));
	EditorUndoRedoManager::get_singleton()->connect("version_changed", callable_mp(editor_scene_manager, &EditorSceneManager::_update_unsaved_cache));
	EditorUndoRedoManager::get_singleton()->connect("history_changed", callable_mp(editor_scene_manager, &EditorSceneManager::_update_undo_redo_allowed));
	EditorUndoRedoManager::get_singleton()->connect("history_changed", callable_mp(editor_scene_manager, &EditorSceneManager::_update_unsaved_cache));

	print_handler.printfunc = _print_handler;
	print_handler.userdata = this;
	add_print_handler(&print_handler);
}

void EditorCore::_unregister_global_callbacks() {
	// Callable update_undo_redo_allowed = callable_mp(editor_scene_manager, &EditorSceneManager::_update_undo_redo_allowed);
	// Callable update_unsaved_cache = callable_mp(editor_scene_manager, &EditorSceneManager::_update_unsaved_cache);

	/// @note The EditorUndoRedoManager signals are implicitly disconnected in the class's destructor.
	/// EditorUndorRedoManager is a member of EditorData, which gets deleted on close.
	/// So we do not disconnect the signals here, which were connected in _register_global_callbacks

	FileDialog::get_icon_func = nullptr;
	FileDialog::register_func = nullptr;
	FileDialog::unregister_func = nullptr;

	EditorFileDialog::get_icon_func = nullptr;
	EditorFileDialog::register_func = nullptr;
	EditorFileDialog::unregister_func = nullptr;

	FileAccess::set_file_close_fail_notify_callback(nullptr);
	ResourceSaver::set_save_callback(nullptr);
	ResourceLoader::set_load_callback(nullptr);
	ResourceLoader::set_dependency_error_notify_func(nullptr);
	ResourceLoader::set_error_notify_func(nullptr);
	SceneState::set_instantiation_warning_notify_func(nullptr);

	file_dialogs.clear();
	editor_file_dialogs.clear();
}

void EditorCore::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			started_timestamp = Time::get_singleton()->get_unix_time_from_system();
		} break;
	}
}

void EditorCore::_bind_methods() {
	ADD_SIGNAL(MethodInfo("resource_saved", PropertyInfo(Variant::OBJECT, "obj")));
	ADD_SIGNAL(MethodInfo("preview_locale_changed"));
}

// ── Resource operations ─────────────────────────────────────────────────────

void EditorCore::disambiguate_filenames(const Vector<String> p_full_paths, Vector<String> &r_filenames) {
	ERR_FAIL_COND_MSG(p_full_paths.size() != r_filenames.size(), vformat("disambiguate_filenames requires two string vectors of same length (%d != %d).", p_full_paths.size(), r_filenames.size()));

	// Keep track of a list of "index sets," i.e. sets of indices
	// within disambiguated_scene_names which contain the same name.
	Vector<RBSet<int>> index_sets;
	HashMap<String, int> scene_name_to_set_index;
	for (int i = 0; i < r_filenames.size(); i++) {
		const String &scene_name = r_filenames[i];
		if (!scene_name_to_set_index.has(scene_name)) {
			index_sets.append(RBSet<int>());
			scene_name_to_set_index.insert(r_filenames[i], index_sets.size() - 1);
		}
		index_sets.write[scene_name_to_set_index[scene_name]].insert(i);
	}

	// For each index set with a size > 1, we need to disambiguate.
	for (int i = 0; i < index_sets.size(); i++) {
		RBSet<int> iset = index_sets[i];
		while (iset.size() > 1) {
			// Append the parent folder to each scene name.
			for (const int &E : iset) {
				int set_idx = E;
				String scene_name = r_filenames[set_idx];
				String full_path = p_full_paths[set_idx];

				// Get rid of file extensions and res:// prefixes.
				scene_name = scene_name.get_basename();
				if (full_path.begins_with("res://")) {
					full_path = full_path.substr(6);
				}
				full_path = full_path.get_basename();

				// Normalize trailing slashes when normalizing directory names.
				scene_name = scene_name.trim_suffix("/");
				full_path = full_path.trim_suffix("/");

				int scene_name_size = scene_name.size();
				int full_path_size = full_path.size();
				int difference = full_path_size - scene_name_size;

				// Find just the parent folder of the current path and append it.
				// If the current name is foo.tscn, and the full path is /some/folder/foo.tscn
				// then slash_idx is the second '/', so that we select just "folder", and
				// append that to yield "folder/foo.tscn".
				if (difference > 0) {
					String parent = full_path.substr(0, difference);
					int slash_idx = parent.rfind_char('/');
					slash_idx = parent.rfind_char('/', slash_idx - 1);
					parent = (slash_idx >= 0 && parent.length() > 1) ? parent.substr(slash_idx + 1) : parent;
					r_filenames.write[set_idx] = parent + r_filenames[set_idx];
				}
			}

			// Loop back through scene names and remove non-ambiguous names.
			bool can_proceed = false;
			RBSet<int>::Element *E = iset.front();
			while (E) {
				String scene_name = r_filenames[E->get()];
				bool duplicate_found = false;
				for (const int &F : iset) {
					if (E->get() == F) {
						continue;
					}
					const String &other_scene_name = r_filenames[F];
					if (other_scene_name == scene_name) {
						duplicate_found = true;
						break;
					}
				}

				RBSet<int>::Element *to_erase = duplicate_found ? nullptr : E;

				// We need to check that we could actually append anymore names
				// if we wanted to for disambiguation. If we can't, then we have
				// to abort even with ambiguous names. We clean the full path
				// and the scene name first to remove extensions so that this
				// comparison actually works.
				String path = p_full_paths[E->get()];

				// Get rid of file extensions and res:// prefixes.
				scene_name = scene_name.get_basename();
				if (path.begins_with("res://")) {
					path = path.substr(6);
				}
				path = path.get_basename();

				// Normalize trailing slashes when normalizing directory names.
				scene_name = scene_name.trim_suffix("/");
				path = path.trim_suffix("/");

				// We can proceed if the full path is longer than the scene name,
				// meaning that there is at least one more parent folder we can
				// tack onto the name.
				can_proceed = can_proceed || (path.size() - scene_name.size()) >= 1;

				E = E->next();
				if (to_erase) {
					iset.erase(to_erase);
				}
			}

			if (!can_proceed) {
				break;
			}
		}
	}
}

String EditorCore::adjust_scene_name_casing(const String &p_root_name) {
	switch (GLOBAL_GET("editor/naming/scene_name_casing").operator int()) {
		case SCENE_NAME_CASING_AUTO:
			// Use casing of the root node.
			break;
		case SCENE_NAME_CASING_PASCAL_CASE:
			return p_root_name.to_pascal_case();
		case SCENE_NAME_CASING_SNAKE_CASE:
			return p_root_name.to_snake_case();
		case SCENE_NAME_CASING_KEBAB_CASE:
			return p_root_name.to_kebab_case();
		case SCENE_NAME_CASING_CAMEL_CASE:
			return p_root_name.to_camel_case();
	}
	return p_root_name;
}

String EditorCore::adjust_script_name_casing(const String &p_file_name, ScriptLanguage::ScriptNameCasing p_auto_casing) {
	int editor_casing = GLOBAL_GET("editor/naming/script_name_casing");
	if (editor_casing == ScriptLanguage::SCRIPT_NAME_CASING_AUTO) {
		// Use the script language's preferred casing.
		editor_casing = p_auto_casing;
	}

	switch (editor_casing) {
		case ScriptLanguage::SCRIPT_NAME_CASING_AUTO:
			// The script language has no preference, so do not adjust.
			break;
		case ScriptLanguage::SCRIPT_NAME_CASING_PASCAL_CASE:
			return p_file_name.to_pascal_case();
		case ScriptLanguage::SCRIPT_NAME_CASING_SNAKE_CASE:
			return p_file_name.to_snake_case();
		case ScriptLanguage::SCRIPT_NAME_CASING_KEBAB_CASE:
			return p_file_name.to_kebab_case();
		case ScriptLanguage::SCRIPT_NAME_CASING_CAMEL_CASE:
			return p_file_name.to_camel_case();
	}
	return p_file_name;
}

// ── Translation / preview locale ────────────────────────────────────────────

void EditorCore::_gdextensions_reloaded() {
	// In case the developer is inspecting an object that will be changed by the reload.
	InspectorDock::get_inspector_singleton()->update_tree();

	// Reload script editor to revalidate GDScript if classes are added or removed.
	ScriptEditor::get_singleton()->reload_scripts(true);

	// Regenerate documentation without using script documentation cache since that would
	// revert doc changes during this session.
	EditorHelp::generate_doc(true, false);
}

void EditorCore::_update_translations() {
	Ref<TranslationDomain> main = TranslationServer::get_singleton()->get_main_domain();

	main->clear();
	TranslationServer::get_singleton()->load_translations();

	if (main->is_enabled()) {
		// Check for the exact locale.
		// `get_potential_translations("zh_CN")` could return translations for "zh".
		if (main->get_loaded_locales().has(main->get_locale_override())) {
			// The set of translation resources for the current locale changed.
			const HashSet<Ref<Translation>> translations = main->get_potential_translations(main->get_locale_override());
			if (translations != tracked_translations) {
				_translation_resources_changed();
			}
		} else {
			// Translations for the current preview locale is removed.
			main->set_enabled(false);
			main->set_locale_override(String());
			_translation_resources_changed();
		}
	}
}

void EditorCore::_translation_resources_changed() {
	for (const Ref<Translation> &E : tracked_translations) {
		E->disconnect_changed(callable_mp(this, &EditorCore::_queue_translation_notification));
	}
	tracked_translations.clear();

	const Ref<TranslationDomain> main = TranslationServer::get_singleton()->get_main_domain();
	if (main->is_enabled()) {
		const HashSet<Ref<Translation>> translations = main->get_potential_translations(main->get_locale_override());
		tracked_translations.reserve(translations.size());
		for (const Ref<Translation> &translation : translations) {
			translation->connect_changed(callable_mp(this, &EditorCore::_queue_translation_notification));
			tracked_translations.insert(translation);
		}
	}

	_queue_translation_notification();
	emit_signal(SNAME("preview_locale_changed"));
}

void EditorCore::_queue_translation_notification() {
	if (pending_translation_notification) {
		return;
	}
	pending_translation_notification = true;
	callable_mp(this, &EditorCore::_propagate_translation_notification).call_deferred();
}

void EditorCore::_propagate_translation_notification() {
	pending_translation_notification = false;
	EditorShell *editor_shell = get_shell();
	ERR_FAIL_NULL(editor_shell);
	editor_shell->get_scene_root()->propagate_notification(NOTIFICATION_TRANSLATION_CHANGED);
}

String EditorCore::get_preview_locale() const {
	const Ref<TranslationDomain> &main_domain = TranslationServer::get_singleton()->get_main_domain();
	return main_domain->is_enabled() ? main_domain->get_locale_override() : String();
}

void EditorCore::set_preview_locale(const String &p_locale) {
	const String &prev_locale = get_preview_locale();
	if (prev_locale == p_locale) {
		return;
	}

	// Texts set in the editor could be identifiers that should never be translated.
	// So we need to disable translation entirely.
	Ref<TranslationDomain> main_domain = TranslationServer::get_singleton()->get_main_domain();
	main_domain->set_enabled(!p_locale.is_empty());
	main_domain->set_locale_override(p_locale);

	_translation_resources_changed();
}

// ── Theme / icons ───────────────────────────────────────────────────────────

Ref<Texture2D> EditorCore::_get_editor_theme_native_menu_icon(const StringName &p_name, bool p_global_menu, bool p_dark_mode) const {
	if (!p_global_menu) {
		return theme->get_icon(p_name, SNAME("EditorIcons"));
	}
	if (p_dark_mode && theme->has_icon(String(p_name) + "Dark", SNAME("EditorIcons"))) {
		return theme->get_icon(String(p_name) + "Dark", SNAME("EditorIcons"));
	} else if (!p_dark_mode && theme->has_icon(String(p_name) + "Light", SNAME("EditorIcons"))) {
		return theme->get_icon(String(p_name) + "Light", SNAME("EditorIcons"));
	}
	return theme->get_icon(p_name, SNAME("EditorIcons"));
}

void EditorCore::update_preview_themes(int p_mode) {
	EditorShell *editor_shell = get_shell();
	ERR_FAIL_NULL(editor_shell);
	SubViewport *scene_root = editor_shell->get_scene_root();
	if (!scene_root->is_inside_tree()) {
		return; // Too early.
	}

	Vector<Ref<Theme>> preview_themes;

	switch (p_mode) {
		case CanvasItemEditor::THEME_PREVIEW_PROJECT:
			preview_themes.push_back(ThemeDB::get_singleton()->get_project_theme());
			break;

		case CanvasItemEditor::THEME_PREVIEW_EDITOR:
			preview_themes.push_back(get_editor_theme());
			break;

		default:
			break;
	}

	preview_themes.push_back(ThemeDB::get_singleton()->get_default_theme());

	ThemeContext *preview_context = ThemeDB::get_singleton()->get_theme_context(scene_root);
	if (preview_context) {
		preview_context->set_themes(preview_themes);
	} else {
		ThemeDB::get_singleton()->create_theme_context(scene_root, preview_themes);
	}
}

Ref<Texture2D> EditorCore::_get_class_or_script_icon(const String &p_class, const String &p_script_path, const String &p_fallback, bool p_fallback_script_to_theme) {
	ERR_FAIL_COND_V_MSG(p_class.is_empty(), nullptr, "Class name cannot be empty.");
	EditorData &ed = editor_data;

	// Check for a script icon first.
	if (!p_script_path.is_empty()) {
		Ref<Texture2D> script_icon = ed.get_script_icon(p_script_path);
		if (script_icon.is_valid()) {
			return script_icon;
		}

		if (p_fallback_script_to_theme) {
			// Look for the native base type in the editor theme. This is relevant for
			// scripts extending other scripts and for built-in classes.
			String base_type;
			if (ScriptServer::is_global_class(p_class)) {
				base_type = ScriptServer::get_global_class_native_base(p_class);
			} else {
				Ref<Script> scr = ResourceLoader::load(p_script_path, "Script");
				if (scr.is_valid()) {
					base_type = scr->get_instance_base_type();
				}
			}
			if (theme.is_valid() && theme->has_icon(base_type, EditorStringName(EditorIcons))) {
				return theme->get_icon(base_type, EditorStringName(EditorIcons));
			}
		}
	}

	// Script was not valid or didn't yield any useful values, try the class name
	// directly.

	// Check if the class name is an extension-defined type.
	Ref<Texture2D> ext_icon = ed.extension_class_get_icon(p_class);
	if (ext_icon.is_valid()) {
		return ext_icon;
	}

	/// Check if the class name is a custom type.
	/// @todo Should probably be deprecated in 4.x
	const EditorData::CustomType *ctype = ed.get_custom_type_by_name(p_class);
	if (ctype && ctype->icon.is_valid()) {
		return ctype->icon;
	}

	// Look up the class name or the fallback name in the editor theme.
	// This is only relevant for built-in classes.
	if (theme.is_valid()) {
		if (theme->has_icon(p_class, EditorStringName(EditorIcons))) {
			return theme->get_icon(p_class, EditorStringName(EditorIcons));
		}

		if (!p_fallback.is_empty() && theme->has_icon(p_fallback, EditorStringName(EditorIcons))) {
			return theme->get_icon(p_fallback, EditorStringName(EditorIcons));
		}

		// If the fallback is empty or wasn't found, use the default fallback.
		if (ClassDB::class_exists(p_class)) {
			bool instantiable = !ClassDB::is_virtual(p_class) && ClassDB::can_instantiate(p_class);
			if (ClassDB::is_parent_class(p_class, SNAME("Node"))) {
				return theme->get_icon(instantiable ? "Node" : "NodeDisabled", EditorStringName(EditorIcons));
			} else {
				return theme->get_icon(instantiable ? "Object" : "ObjectDisabled", EditorStringName(EditorIcons));
			}
		}
	}

	return nullptr;
}

Ref<Texture2D> EditorCore::get_object_icon(const Object *p_object, const String &p_fallback) {
	ERR_FAIL_NULL_V_MSG(p_object, nullptr, "Object cannot be null.");

	Ref<Script> scr = p_object->get_script();

	const EditorDebuggerRemoteObjects *robjs = Object::cast_to<EditorDebuggerRemoteObjects>(p_object);
	if (robjs) {
		String class_name;
		if (scr.is_valid()) {
			class_name = scr->get_global_name();

			if (class_name.is_empty()) {
				// If there is no class_name in this script we just take the script path.
				class_name = scr->get_path();
			}
		}

		if (class_name.is_empty()) {
			return get_class_icon(robjs->type_name, p_fallback);
		}

		return get_class_icon(class_name, p_fallback);
	}

	if (scr.is_null() && p_object->is_class("Script")) {
		scr = p_object;
	}

	if (Object::cast_to<MultiNodeEdit>(p_object)) {
		return get_class_icon(Object::cast_to<MultiNodeEdit>(p_object)->get_edited_class_name(), p_fallback);
	} else {
		return _get_class_or_script_icon(p_object->get_class(), scr.is_valid() ? scr->get_path() : String(), p_fallback);
	}
}

Ref<Texture2D> EditorCore::get_class_icon(const String &p_class, const String &p_fallback) {
	ERR_FAIL_COND_V_MSG(p_class.is_empty(), nullptr, "Class name cannot be empty.");
	const Pair<String, String> key(p_class, p_fallback);

	// Take from the local cache, if available.
	{
		Ref<Texture2D> *icon = class_icon_cache.getptr(key);
		if (icon) {
			return *icon;
		}
	}

	String script_path;
	if (ScriptServer::is_global_class(p_class)) {
		script_path = ScriptServer::get_global_class_path(p_class);
	} else if (!p_class.get_extension().is_empty() && ResourceLoader::exists(p_class)) { // If the script is not a class_name we check if the script resource exists.
		script_path = p_class;
	}

	Ref<Texture2D> icon = _get_class_or_script_icon(p_class, script_path, p_fallback, true);
	class_icon_cache[key] = icon;
	return icon;
}

Ref<Script> EditorCore::get_object_custom_type_base(const Object *p_object) const {
	ERR_FAIL_NULL_V(p_object, nullptr);

	const Node *node = Object::cast_to<const Node>(p_object);
	if (node && node->has_meta(SceneStringName(_custom_type_script))) {
		return PropertyUtils::get_custom_type_script(node);
	}

	Ref<Script> scr = p_object->get_script();

	if (scr.is_valid()) {
		/// @todo Should probably be deprecated in 4.x
		StringName base = scr->get_instance_base_type();
		if (base != StringName() && editor_data.get_custom_types().has(base)) {
			const Vector<EditorData::CustomType> &types = editor_data.get_custom_types()[base];

			Ref<Script> base_scr = scr;
			while (base_scr.is_valid()) {
				for (int i = 0; i < types.size(); ++i) {
					if (types[i].script == base_scr) {
						return types[i].script;
					}
				}
				base_scr = base_scr->get_base_script();
			}
		}
	}

	return nullptr;
}

StringName EditorCore::get_object_custom_type_name(const Object *p_object) const {
	ERR_FAIL_NULL_V(p_object, StringName());

	Ref<Script> scr = p_object->get_script();
	if (scr.is_null() && Object::cast_to<Script>(p_object)) {
		scr = p_object;
	}

	if (scr.is_valid()) {
		Ref<Script> base_scr = scr;
		while (base_scr.is_valid()) {
			StringName name = editor_data.script_class_get_name(base_scr->get_path());
			if (name != StringName()) {
				return name;
			}

			/// @todo Should probably be deprecated in 4.x.
			StringName base = base_scr->get_instance_base_type();
			if (base != StringName() && editor_data.get_custom_types().has(base)) {
				const Vector<EditorData::CustomType> &types = editor_data.get_custom_types()[base];
				for (int i = 0; i < types.size(); ++i) {
					if (types[i].script == base_scr) {
						return types[i].name;
					}
				}
			}
			base_scr = base_scr->get_base_script();
		}
	}

	return StringName();
}

bool EditorCore::is_object_of_custom_type(const Object *p_object, const StringName &p_class) {
	ERR_FAIL_NULL_V(p_object, false);

	Ref<Script> scr = p_object->get_script();
	if (scr.is_null() && Object::cast_to<Script>(p_object)) {
		scr = p_object;
	}

	if (scr.is_valid()) {
		if (scr->has_trait(p_class)) {
			return true;
		}
		Ref<Script> base_script = scr;
		while (base_script.is_valid()) {
			StringName name = editor_data.script_class_get_name(base_script->get_path());
			if (name == p_class) {
				return true;
			}
			base_script = base_script->get_base_script();
		}
	}
	return false;
}

// ── Resource save / load ────────────────────────────────────────────────────

Error EditorCore::load_resource(const String &p_resource, bool p_ignore_broken_deps) {
	dependency_errors.clear();

	Error err;

	Ref<Resource> res;
	if (ResourceLoader::exists(p_resource, "")) {
		res = ResourceLoader::load(p_resource, "", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	} else if (textfile_extensions.has(p_resource.get_extension())) {
		res = ScriptEditor::get_singleton()->open_file(p_resource);
	} else if (other_file_extensions.has(p_resource.get_extension())) {
		OS::get_singleton()->shell_open(ProjectSettings::get_singleton()->globalize_path(p_resource));
		return OK;
	}
	ERR_FAIL_COND_V(res.is_null(), ERR_CANT_OPEN);

	if (!p_ignore_broken_deps && dependency_errors.has(p_resource)) {
		Vector<String> errors;
		for (const String &E : dependency_errors[p_resource]) {
			errors.push_back(E);
		}
		EditorShell *editor_shell = get_shell();
		ERR_FAIL_NULL_V_MSG(editor_shell, ERR_FILE_MISSING_DEPENDENCIES, "Load Resource: EditorShell is null!");
		editor_shell->get_dependency_error_dialog()->show(p_resource, errors);
		dependency_errors.erase(p_resource);

		return ERR_FILE_MISSING_DEPENDENCIES;
	}

	InspectorDock::get_singleton()->edit_resource(res);
	return OK;
}

Error EditorCore::load_scene_or_resource(const String &p_path, bool p_ignore_broken_deps, bool p_change_scene_tab_if_already_open) {
	if (ClassDB::is_parent_class(ResourceLoader::get_resource_type(p_path), "PackedScene")) {
		if (!p_change_scene_tab_if_already_open && get_shell()->is_scene_open(p_path)) {
			return OK;
		}
		return EditorSceneManager::get_singleton()->load_scene(p_path, p_ignore_broken_deps);
	}
	return load_resource(p_path, p_ignore_broken_deps);
}

void EditorCore::save_resource_in_path(const Ref<Resource> &p_resource, const String &p_path) {
	editor_data.apply_changes_in_editors();

	if (saving_resources_in_path.has(p_resource)) {
		return;
	}
	saving_resources_in_path.insert(p_resource);

	int flg = 0;
	if (EDITOR_GET("filesystem/on_save/compress_binary_resources")) {
		flg |= ResourceSaver::FLAG_COMPRESS;
	}

	String path = ProjectSettings::get_singleton()->localize_path(p_path);
	Error err = ResourceSaver::save(p_resource, path, flg | ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS);

	if (err != OK) {
		if (ResourceLoader::is_imported(p_resource->get_path())) {
			get_shell()->show_accept(TTR("Imported resources can't be saved."), TTR("OK"));
		} else {
			get_shell()->show_accept(TTR("Error saving resource!"), TTR("OK"));
		}

		saving_resources_in_path.erase(p_resource);
		return;
	}

	Ref<Resource> prev_resource = ResourceCache::get_ref(p_path);
	if (prev_resource.is_null() || prev_resource != p_resource) {
		p_resource->set_path(path, true);
	}
	if (prev_resource.is_valid() && prev_resource != p_resource) {
		replace_resources_in_scenes({ prev_resource }, { p_resource });
	}
	saving_resources_in_path.erase(p_resource);

	_resource_saved(p_resource, path);

	emit_signal(SNAME("resource_saved"), p_resource);
	editor_data.notify_resource_saved(p_resource);
}

void EditorCore::save_resource(const Ref<Resource> &p_resource) {
	// If built-in resource, save the scene instead.
	if (p_resource->is_built_in()) {
		const String scene_path = p_resource->get_path().get_slice("::", 0);
		if (!scene_path.is_empty()) {
			if (ResourceLoader::exists(scene_path) && ResourceLoader::get_resource_type(scene_path) == "PackedScene") {
				get_shell()->save_scene_if_open(scene_path);
			} else {
				// Not a packed scene, so save it as regular resource.
				Ref<Resource> parent_resource = ResourceCache::get_ref(scene_path);
				ERR_FAIL_COND_MSG(parent_resource.is_null(), "Parent resource not loaded, can't save.");
				save_resource(parent_resource);
			}
			return;
		}
	}

	// If the resource has been imported, ask the user to use a different path in order to save it.
	String path = p_resource->get_path();
	if (path.is_resource_file() && !FileAccess::exists(path + ".import")) {
		save_resource_in_path(p_resource, p_resource->get_path());
	} else {
		save_resource_as(p_resource);
	}
}

void EditorCore::save_resource_as(const Ref<Resource> &p_resource, const String &p_at_path) {
	{
		String path = p_resource->get_path();
		if (!path.is_resource_file()) {
			int srpos = path.find("::");
			if (srpos != -1) {
				String base = path.substr(0, srpos);
				if (!EditorSceneManager::get_singleton()->get_edited_scene() || EditorSceneManager::get_singleton()->get_edited_scene()->get_scene_file_path() != base) {
					get_shell()->show_warning(TTR("This resource can't be saved because it does not belong to the edited scene. Make it unique first."));
					return;
				}
			}
		} else if (FileAccess::exists(path + ".import")) {
			get_shell()->show_warning(TTR("This resource can't be saved because it was imported from another file. Make it unique first."));
			return;
		}
	}

	EditorShell *editor_shell = get_shell();
	EditorFileDialog *file = editor_shell->get_file_dialog();
	file->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	saving_resource = p_resource;

	editor_shell->set_current_menu_option(EditorShell::RESOURCE_SAVE_AS);
	List<String> extensions;
	Ref<PackedScene> sd = memnew(PackedScene);
	ResourceSaver::get_recognized_extensions(p_resource, &extensions);
	file->clear_filters();

	List<String> preferred;
	for (const String &E : extensions) {
		if (p_resource->is_class("Script") && (E == "tres" || E == "res")) {
			// This serves no purpose and confused people.
			continue;
		}
		file->add_filter("*." + E, E.to_upper());
		preferred.push_back(E);
	}
	// Lowest provided extension priority.
	List<String>::Element *res_element = preferred.find("res");
	if (res_element) {
		preferred.move_to_back(res_element);
	}

	if (!p_at_path.is_empty()) {
		file->set_current_dir(p_at_path);
		if (p_resource->get_path().is_resource_file()) {
			file->set_current_file(p_resource->get_path().get_file());
		} else {
			if (!preferred.is_empty()) {
				String resource_name_snake_case = p_resource->get_class().to_snake_case();
				file->set_current_file("new_" + resource_name_snake_case + "." + preferred.front()->get().to_lower());
			} else {
				file->set_current_file(String());
			}
		}
	} else if (!p_resource->get_path().get_base_dir().is_empty()) {
		file->set_current_path(p_resource->get_path());
		if (!extensions.is_empty()) {
			const String ext = p_resource->get_path().get_extension().to_lower();
			if (extensions.find(ext) == nullptr) {
				file->set_current_path(p_resource->get_path().replacen("." + ext, "." + extensions.front()->get()));
			}
		}
	} else if (!preferred.is_empty()) {
		const String resource_name_snake_case = p_resource->get_class().to_snake_case();
		const String existing = "new_" + resource_name_snake_case + "." + preferred.front()->get().to_lower();
		file->set_current_path(existing);
	}
	file->set_title(TTR("Save Resource As..."));
	file->popup_file_dialog();
}

void EditorCore::save_default_environment() {
	Ref<Environment> fallback = get_tree()->get_root()->get_world_3d()->get_fallback_environment();

	if (fallback.is_valid() && fallback->get_path().is_resource_file()) {
		HashMap<Ref<Resource>, bool> processed;
		_find_and_save_edited_subresources(fallback.ptr(), processed, 0);
		save_resource_in_path(fallback, fallback->get_path());
	}
}

bool EditorCore::_find_and_save_resource(Ref<Resource> p_res, HashMap<Ref<Resource>, bool> &processed, int32_t flags) {
	if (p_res.is_null()) {
		return false;
	}

	if (processed.has(p_res)) {
		return processed[p_res];
	}

	bool changed = p_res->is_edited();
	p_res->set_edited(false);

	bool subchanged = _find_and_save_edited_subresources(p_res.ptr(), processed, flags);

	if (p_res->get_path().is_resource_file()) {
		if (changed || subchanged) {
			ResourceSaver::save(p_res, p_res->get_path(), flags);
		}
		processed[p_res] = false; // Because it's a file.
		return false;
	} else {
		processed[p_res] = changed;
		return changed;
	}
}

bool EditorCore::_find_and_save_edited_subresources(Object *obj, HashMap<Ref<Resource>, bool> &processed, int32_t flags) {
	bool ret_changed = false;
	List<PropertyInfo> pi;
	obj->get_property_list(&pi);
	for (const PropertyInfo &E : pi) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		switch (E.type) {
			case Variant::OBJECT: {
				Ref<Resource> res = obj->get(E.name);

				if (_find_and_save_resource(res, processed, flags)) {
					ret_changed = true;
				}

			} break;
			case Variant::ARRAY: {
				Array varray = obj->get(E.name);
				int len = varray.size();
				for (int i = 0; i < len; i++) {
					const Variant &v = varray.get(i);
					Ref<Resource> res = v;
					if (_find_and_save_resource(res, processed, flags)) {
						ret_changed = true;
					}
				}

			} break;
			case Variant::DICTIONARY: {
				Dictionary d = obj->get(E.name);
				for (const KeyValue<Variant, Variant> &kv : d) {
					Ref<Resource> res = kv.value;
					if (_find_and_save_resource(res, processed, flags)) {
						ret_changed = true;
					}
				}
			} break;
			default: {
			}
		}
	}

	return ret_changed;
}

void EditorCore::_save_edited_subresources(Node *scene, HashMap<Ref<Resource>, bool> &processed, int32_t flags) {
	_find_and_save_edited_subresources(scene, processed, flags);

	for (int i = 0; i < scene->get_child_count(); i++) {
		Node *n = scene->get_child(i);
		if (n->get_owner() != editor_data.get_edited_scene_root()) {
			continue;
		}
		_save_edited_subresources(n, processed, flags);
	}
}

bool EditorCore::is_resource_read_only(Ref<Resource> p_resource, bool p_foreign_resources_are_writable) {
	ERR_FAIL_COND_V(p_resource.is_null(), false);

	String path = p_resource->get_path();
	if (!path.is_resource_file()) {
		// If the resource name contains '::', that means it is a subresource embedded in another resource.
		int srpos = path.find("::");
		if (srpos != -1) {
			String base = path.substr(0, srpos);
			// If the base resource is a packed scene, we treat it as read-only if it is not the currently edited scene.
			if (ResourceLoader::get_resource_type(base) == "PackedScene") {
				if (!get_tree()->get_edited_scene_root() || get_tree()->get_edited_scene_root()->get_scene_file_path() != base) {
					// If we have not flagged foreign resources as writable or the base scene the resource is
					// part was imported, it can be considered read-only.
					if (!p_foreign_resources_are_writable || FileAccess::exists(base + ".import")) {
						return true;
					}
				}
			} else {
				// If a corresponding .import file exists for the base file, we assume it to be imported and should therefore treated as read-only.
				if (FileAccess::exists(base + ".import")) {
					return true;
				}
			}
		}
	} else if (FileAccess::exists(path + ".import")) {
		// The resource is not a subresource, but if it has an .import file, it's imported so treat it as read only.
		return true;
	}

	return false;
}

bool EditorCore::update_file_extension_caches() {
	HashSet<String> textfile_ext_new;
	const Vector<String> textfile_ext = ((String)(EDITOR_GET("docks/filesystem/textfile_extensions"))).split(",", false);
	for (const String &E : textfile_ext) {
		textfile_ext_new.insert(E);
	}
	HashSet<String> other_file_ext_new;
	const Vector<String> other_file_ext = ((String)(EDITOR_GET("docks/filesystem/other_file_extensions"))).split(",", false);
	for (const String &E : other_file_ext) {
		other_file_ext_new.insert(E);
	}

	bool changed = false;
	if (textfile_extensions != textfile_ext_new) {
		textfile_extensions = textfile_ext_new;
		changed = true;
	}
	if (other_file_extensions != other_file_ext_new) {
		other_file_extensions = other_file_ext_new;
		changed = true;
	}
	return changed;
}

void EditorCore::replace_resources_in_object(Object *p_object, const Vector<Ref<Resource>> &p_source_resources, const Vector<Ref<Resource>> &p_target_resource) {
	List<PropertyInfo> pi;
	p_object->get_property_list(&pi);

	for (const PropertyInfo &E : pi) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		switch (E.type) {
			case Variant::OBJECT: {
				if (E.hint == PROPERTY_HINT_RESOURCE_TYPE) {
					const Variant &v = p_object->get(E.name);
					Ref<Resource> res = v;

					if (res.is_valid()) {
						int res_idx = p_source_resources.find(res);
						if (res_idx != -1) {
							p_object->set(E.name, p_target_resource.get(res_idx));
						} else {
							replace_resources_in_object(v, p_source_resources, p_target_resource);
						}
					}
				}
			} break;
			case Variant::ARRAY: {
				Array varray = p_object->get(E.name);
				int len = varray.size();
				bool array_requires_updating = false;
				for (int i = 0; i < len; i++) {
					const Variant &v = varray.get(i);
					Ref<Resource> res = v;

					if (res.is_valid()) {
						int res_idx = p_source_resources.find(res);
						if (res_idx != -1) {
							varray.set(i, p_target_resource.get(res_idx));
							array_requires_updating = true;
						} else {
							replace_resources_in_object(v, p_source_resources, p_target_resource);
						}
					}
				}
				if (array_requires_updating) {
					p_object->set(E.name, varray);
				}
			} break;
			case Variant::DICTIONARY: {
				Dictionary d = p_object->get(E.name);
				bool dictionary_requires_updating = false;
				for (const Variant &F : d.get_key_list()) {
					Variant v = d[F];
					Ref<Resource> res = v;

					if (res.is_valid()) {
						int res_idx = p_source_resources.find(res);
						if (res_idx != -1) {
							d[F] = p_target_resource.get(res_idx);
							dictionary_requires_updating = true;
						} else {
							replace_resources_in_object(v, p_source_resources, p_target_resource);
						}
					}
				}
				if (dictionary_requires_updating) {
					p_object->set(E.name, d);
				}
			} break;
			default: {
			}
		}
	}

	Node *n = Object::cast_to<Node>(p_object);
	if (n) {
		for (int i = 0; i < n->get_child_count(); i++) {
			replace_resources_in_object(n->get_child(i), p_source_resources, p_target_resource);
		}
	}
}

void EditorCore::replace_resources_in_scenes(const Vector<Ref<Resource>> &p_source_resources, const Vector<Ref<Resource>> &p_target_resource) {
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Node *edited_scene_root = editor_data.get_edited_scene_root(i);
		if (edited_scene_root) {
			replace_resources_in_object(edited_scene_root, p_source_resources, p_target_resource);
		}
	}
}

void EditorCore::fix_dependencies(const String &p_for_file) {
	get_shell()->get_dependency_fixer()->edit(p_for_file);
}

void EditorCore::run_editor_script(const Ref<Script> &p_script) {
	Error err = p_script->reload(true); // Always hard reload the script before running.
	if (err != OK || !p_script->is_valid()) {
		EditorToaster::get_singleton()->popup_str(TTR("Cannot run the script because it contains errors, check the output log."), EditorToaster::SEVERITY_WARNING);
		return;
	}

	// Perform additional checks on the script to evaluate if it's runnable.

	bool is_runnable = true;
	if (!ClassDB::is_parent_class(p_script->get_instance_base_type(), "EditorScript")) {
		is_runnable = false;

		EditorToaster::get_singleton()->popup_str(TTR("Cannot run the script because it doesn't extend EditorScript."), EditorToaster::SEVERITY_WARNING);
	}
	if (!p_script->is_tool()) {
		is_runnable = false;

		if (p_script->get_class() == "GDScript") {
			EditorToaster::get_singleton()->popup_str(TTR("Cannot run the script because it's not a tool script (add the @tool annotation at the top)."), EditorToaster::SEVERITY_WARNING);
		} else if (p_script->get_class() == "CSharpScript") {
			EditorToaster::get_singleton()->popup_str(TTR("Cannot run the script because it's not a tool script (add the [Tool] attribute above the class definition)."), EditorToaster::SEVERITY_WARNING);
		} else {
			EditorToaster::get_singleton()->popup_str(TTR("Cannot run the script because it's not a tool script."), EditorToaster::SEVERITY_WARNING);
		}
	}
	if (!is_runnable) {
		return;
	}

	Ref<EditorScript> es = memnew(EditorScript);
	es->set_script(p_script);
	es->run();
}

// ── Multi-window / color picker / settings overrides ────────────────────────

bool EditorCore::is_multi_window_enabled() const {
	return !SceneTree::get_singleton()->get_root()->is_embedding_subwindows() && !EDITOR_GET("interface/editor/single_window_mode") && EDITOR_GET("interface/multi_window/enable");
}

String EditorCore::get_multiwindow_support_tooltip_text() const {
	if (SceneTree::get_singleton()->get_root()->is_embedding_subwindows()) {
		if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_SUBWINDOWS)) {
			return TTR("Multi-window support is not available because the `--single-window` command line argument was used to start the editor.");
		} else {
			return TTR("Multi-window support is not available because the current platform doesn't support multiple windows.");
		}
	} else if (EDITOR_GET("interface/editor/single_window_mode")) {
		return TTR("Multi-window support is not available because Interface > Editor > Single Window Mode is enabled in the editor settings.");
	}

	return TTR("Multi-window support is not available because Interface > Multi Window > Enable is disabled in the editor settings.");
}

void EditorCore::setup_color_picker(ColorPicker *p_picker) {
	p_picker->set_editor_settings(EditorSettings::get_singleton());
	int default_color_mode = EditorSettings::get_singleton()->get_project_metadata("color_picker", "color_mode", EDITOR_GET("interface/inspector/default_color_picker_mode"));
	int picker_shape = EditorSettings::get_singleton()->get_project_metadata("color_picker", "picker_shape", EDITOR_GET("interface/inspector/default_color_picker_shape"));
	bool show_intensity = EDITOR_GET("interface/inspector/color_picker_show_intensity");

	p_picker->set_color_mode((ColorPicker::ColorModeType)default_color_mode);
	p_picker->set_picker_shape((ColorPicker::PickerShapeType)picker_shape);
	p_picker->set_edit_intensity(show_intensity);

	p_picker->set_quick_open_callback(callable_mp(this, &EditorCore::_quick_open_color_palette));
	p_picker->set_palette_saved_callback(callable_mp(EditorFileSystem::get_singleton(), &EditorFileSystem::update_file));
	palette_file_selected_callback = callable_mp(p_picker, &ColorPicker::_quick_open_palette_file_selected);
}

void EditorCore::_quick_open_color_palette() {
	quick_open_color_palette->popup_dialog({ "ColorPalette" }, palette_file_selected_callback);
	quick_open_color_palette->set_title(TTRC("Quick Open Color Palette..."));
}

void EditorCore::notify_settings_overrides_changed() {
	settings_overrides_changed = true;
}

// ── Global callbacks ────────────────────────────────────────────────────────

Ref<Texture2D> EditorCore::_file_dialog_get_icon(const String &p_path) {
	EditorFileSystemDirectory *efsd = EditorFileSystem::get_singleton()->get_filesystem_path(p_path.get_base_dir());
	if (efsd) {
		String file = p_path.get_file();
		for (int i = 0; i < efsd->get_file_count(); i++) {
			if (efsd->get_file(i) == file) {
				String type = efsd->get_file_type(i);

				if (singleton->icon_type_cache.has(type)) {
					return singleton->icon_type_cache[type];
				} else {
					return singleton->icon_type_cache["Object"];
				}
			}
		}
	}

	return singleton->icon_type_cache["Object"];
}

void EditorCore::_build_icon_type_cache() {
	if (theme.is_null()) {
		return;
	}
	List<StringName> tl;
	theme->get_icon_list(EditorStringName(EditorIcons), &tl);
	for (const StringName &E : tl) {
		if (!ClassDB::class_exists(E)) {
			continue;
		}
		icon_type_cache[E] = theme->get_icon(E, EditorStringName(EditorIcons));
	}
}

void EditorCore::_file_dialog_register(FileDialog *p_dialog) {
	singleton->file_dialogs.insert(p_dialog);
}

void EditorCore::_file_dialog_unregister(FileDialog *p_dialog) {
	singleton->file_dialogs.erase(p_dialog);
}

void EditorCore::_editor_file_dialog_register(EditorFileDialog *p_dialog) {
	singleton->editor_file_dialogs.insert(p_dialog);
}

void EditorCore::_editor_file_dialog_unregister(EditorFileDialog *p_dialog) {
	singleton->editor_file_dialogs.erase(p_dialog);
}

void EditorCore::_file_access_close_error_notify(const String &p_str) {
	callable_mp_static(&EditorCore::_file_access_close_error_notify_impl).call_deferred(p_str);
}

void EditorCore::_file_access_close_error_notify_impl(const String &p_str) {
	ERR_FAIL_NULL(singleton);
	singleton->get_shell()->add_io_error(vformat(TTR("Unable to write to file '%s', file in use, locked or lacking permissions."), p_str));
}

void EditorCore::_resource_load_error_notify(const String &p_error) {
	callable_mp_static(&EditorCore::_resource_load_error_notify_impl).call_deferred(p_error);
}

void EditorCore::_resource_load_error_notify_impl(const String &p_error) {
	ERR_FAIL_NULL(singleton);
	singleton->get_shell()->add_io_error(p_error);
}

void EditorCore::_instantiation_warning_notify(const String &p_warning) {
	callable_mp_static(&EditorCore::_instantiation_warning_notify_impl).call_deferred(p_warning);
}

void EditorCore::_instantiation_warning_notify_impl(const String &p_warning) {
	ERR_FAIL_NULL(singleton);
	singleton->get_shell()->add_io_warning(p_warning);
	singleton->get_scene_manager()->mark_scene_as_unsaved();
}

void EditorCore::_print_handler(void *p_this, const String &p_string, bool p_error, bool p_rich) {
	if (!Thread::is_main_thread()) {
		callable_mp_static(&EditorCore::_print_handler_impl).call_deferred(p_string, p_error, p_rich);
	} else {
		_print_handler_impl(p_string, p_error, p_rich);
	}
}

void EditorCore::_print_handler_impl(const String &p_string, bool p_error, bool p_rich) {
	if (!singleton) {
		return;
	}
	EditorLog *editor_log = EditorCore::get_singleton()->get_log();
	if (!editor_log) {
		return;
	}
	if (p_error) {
		editor_log->add_message(p_string, EditorLog::MSG_TYPE_ERROR);
	} else if (p_rich) {
		editor_log->add_message(p_string, EditorLog::MSG_TYPE_STD_RICH);
	} else {
		editor_log->add_message(p_string, EditorLog::MSG_TYPE_STD);
	}
}

void EditorCore::_resource_saved(Ref<Resource> p_resource, const String &p_path) {
	if (singleton->saving_resources_in_path.has(p_resource)) {
		// This is going to be handled by save_resource_in_path when the time is right.
		return;
	}

	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(p_path);
	}

	singleton->editor_folding.save_resource_folding(p_resource, p_path);
}

void EditorCore::_resource_loaded(Ref<Resource> p_resource, const String &p_path) {
	singleton->editor_folding.load_resource_folding(p_resource, p_path);
}

void EditorCore::_dependency_error_report(const String &p_path, const String &p_dep, const String &p_type) {
	DEV_ASSERT(Thread::get_caller_id() == Thread::get_main_id());
	if (!singleton->dependency_errors.has(p_path)) {
		singleton->dependency_errors[p_path] = HashSet<String>();
	}
	singleton->dependency_errors[p_path].insert(p_dep + "::" + p_type);
}

// ── System info ─────────────────────────────────────────────────────────────

String EditorCore::_get_system_info() const {
	String distribution_name = OS::get_singleton()->get_distribution_name();
	if (distribution_name.is_empty()) {
		distribution_name = OS::get_singleton()->get_name();
	}
	if (distribution_name.is_empty()) {
		distribution_name = "Other";
	}
	const String distribution_version = OS::get_singleton()->get_version_alias();

	String godot_version = "Redot v" + String(REDOT_VERSION_FULL_CONFIG);
	if (String(REDOT_VERSION_BUILD) != "official") {
		String hash = String(REDOT_VERSION_HASH);
		hash = hash.is_empty() ? String("unknown") : vformat("(%s)", hash.left(9));
		godot_version += " " + hash;
	}

	String display_session_type;
#ifdef LINUXBSD_ENABLED
	// `remove_char` is necessary, because `capitalize` introduces a whitespace between "x" and "11".
	display_session_type = OS::get_singleton()->get_environment("XDG_SESSION_TYPE").capitalize().remove_char(' ');
#endif // LINUXBSD_ENABLED
	String driver_name = OS::get_singleton()->get_current_rendering_driver_name().to_lower();
	String rendering_method = OS::get_singleton()->get_current_rendering_method().to_lower();

	const String rendering_device_name = RenderingServer::get_singleton()->get_video_adapter_name();

	RenderingDevice::DeviceType device_type = RenderingServer::get_singleton()->get_video_adapter_type();
	String device_type_string;
	switch (device_type) {
		case RenderingDevice::DeviceType::DEVICE_TYPE_INTEGRATED_GPU:
			device_type_string = "integrated";
			break;
		case RenderingDevice::DeviceType::DEVICE_TYPE_DISCRETE_GPU:
			device_type_string = "dedicated";
			break;
		case RenderingDevice::DeviceType::DEVICE_TYPE_VIRTUAL_GPU:
			device_type_string = "virtual";
			break;
		case RenderingDevice::DeviceType::DEVICE_TYPE_CPU:
			device_type_string = "(software emulation on CPU)";
			break;
		case RenderingDevice::DeviceType::DEVICE_TYPE_OTHER:
		case RenderingDevice::DeviceType::DEVICE_TYPE_MAX:
			break; // Can't happen, but silences warning for DEVICE_TYPE_MAX
	}

	const Vector<String> video_adapter_driver_info = OS::get_singleton()->get_video_adapter_driver_info();

	const String processor_name = OS::get_singleton()->get_processor_name();
	const int processor_count = OS::get_singleton()->get_processor_count();

	// Prettify
	if (rendering_method == "forward_plus") {
		rendering_method = "Forward+";
	} else if (rendering_method == "mobile") {
		rendering_method = "Mobile";
	} else if (rendering_method == "gl_compatibility") {
		rendering_method = "Compatibility";
	}
	if (driver_name == "vulkan") {
		driver_name = "Vulkan";
	} else if (driver_name == "d3d12") {
		driver_name = "Direct3D 12";
	} else if (driver_name == "opengl3_angle") {
		driver_name = "OpenGL ES 3/ANGLE";
	} else if (driver_name == "opengl3_es") {
		driver_name = "OpenGL ES 3";
	} else if (driver_name == "opengl3") {
		if (OS::get_singleton()->get_gles_over_gl()) {
			driver_name = "OpenGL 3";
		} else {
			driver_name = "OpenGL ES 3";
		}
	} else if (driver_name == "metal") {
		driver_name = "Metal";
	}

	// Join info.
	Vector<String> info;
	info.push_back(godot_version);
	String distribution_display_session_type = distribution_name;
	if (!distribution_version.is_empty()) {
		distribution_display_session_type += " " + distribution_version;
	}
	if (!display_session_type.is_empty()) {
		distribution_display_session_type += " on " + display_session_type;
	}
	info.push_back(distribution_display_session_type);

	String display_driver_window_mode;
#ifdef LINUXBSD_ENABLED
	// `remove_char` is necessary, because `capitalize` introduces a whitespace between "x" and "11".
	display_driver_window_mode = DisplayServer::get_singleton()->get_name().capitalize().remove_char(' ') + " display driver";
#endif // LINUXBSD_ENABLED
	if (!display_driver_window_mode.is_empty()) {
		display_driver_window_mode += ", ";
	}
	display_driver_window_mode += get_viewport()->is_embedding_subwindows() ? "Single-window" : "Multi-window";

	if (DisplayServer::get_singleton()->get_screen_count() == 1) {
		display_driver_window_mode += ", " + itos(DisplayServer::get_singleton()->get_screen_count()) + " monitor";
	} else {
		display_driver_window_mode += ", " + itos(DisplayServer::get_singleton()->get_screen_count()) + " monitors";
	}

	info.push_back(display_driver_window_mode);

	info.push_back(vformat("%s (%s)", driver_name, rendering_method));

	String graphics;
	if (!device_type_string.is_empty()) {
		graphics = device_type_string + " ";
	}
	graphics += rendering_device_name;
	if (video_adapter_driver_info.size() == 2) { // This vector is always either of length 0 or 2.
		const String &vad_name = video_adapter_driver_info[0];
		const String &vad_version = video_adapter_driver_info[1]; // Version could be potentially empty on Linux/BSD.
		if (!vad_version.is_empty()) {
			graphics += vformat(" (%s; %s)", vad_name, vad_version);
		} else if (!vad_name.is_empty()) {
			graphics += vformat(" (%s)", vad_name);
		}
	}
	info.push_back(graphics);

	info.push_back(vformat("%s (%d threads)", processor_name, processor_count));

	const int64_t system_ram = OS::get_singleton()->get_memory_info()["physical"];
	if (system_ram > 0) {
		// If the memory info is available, display it.
		info.push_back(vformat("%s memory", String::humanize_size(system_ram)));
	}

	return String(" - ").join(info);
}

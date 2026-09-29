/**************************************************************************/
/*  editor_shell.h                                                        */
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

#include "core/io/config_file.h"
#include "core/object/object_id.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

#include "editor/editor_core.h"
#include "editor/editor_scene_manager.h"
#include "editor/plugins/editor_plugin_manager.h"

class Button;
class ColorPicker;
class ConfirmationDialog;
class Control;
class DependencyEditor;
class DependencyErrorDialog;
class EditorBottomPanel;
class EditorData;
class EditorFileDialog;
class EditorFolding;
class EditorMainScreen;
class EditorPlugin;
class EditorSceneTabs;
class EditorPluginList;
class EditorResourceConversionPlugin;
class EditorSelection;
class EditorSelectionHistory;
class EditorTitleBar;
class EditorRunBar;
class Node;
class Object;
class PopupMenu;
class ProjectSettingsEditor;
class Resource;
class SubViewport;
class VSplitContainer;

/**
 * Concrete base class for the editor's visual shell (the UI layer that sits
 * on top of `EditorCore`).
 *
 * `EditorShell` aggregates the editor's decoupled functional components —
 * `EditorCore`, `EditorSceneManager`, and `EditorPluginManager` — and exposes
 * their functional operations as concrete delegates. A shell therefore only
 * has to implement the genuinely visual parts (the pure virtuals below):
 * containers, user feedback, menus, window state, drag-and-drop, and the
 * scene-session UI hooks.
 *
 * It is a plain C++ base class: NOT a `Node`, NOT a `RefCounted`, and NOT a
 * singleton. The three component pointers are non-owning; they are wired by
 * `EditorCore::set_shell()` when a concrete shell (e.g. `EditorNode`)
 * registers itself, and are `nullptr` until then.
 */
class EditorShell : public Node {
	GDCLASS(EditorShell, Node);

protected:
	/// `EditorShell` is an abstract base class; it declares no script-visible
	/// properties or methods of its own, so there is nothing to bind.
	static void _bind_methods() {}

public:
	virtual ~EditorShell() = default;

	/// The quick-open dialogs are owned by `EditorCore` but parented into the
	/// shell's GUI tree. Unparent them on predelete — which runs before the
	/// shell's subtree (and thus the dialogs) is freed — so that `EditorCore`'s
	/// destructor remains the single deletion point and does not double-free
	/// them during SceneTree teardown.
	void _notification(int p_what) {
		if (p_what == NOTIFICATION_PREDELETE && editor_core) {
			editor_core->unparent_quick_open_dialogs();
		}
	}

	/// The menu options dispatched by a shell through `trigger_menu_option`
	/// and the pending file-dialog option (`set_current_menu_option`).
	enum MenuOptions {
		// Scene menu.
		SCENE_NEW_SCENE,
		SCENE_NEW_INHERITED_SCENE,
		SCENE_OPEN_SCENE,
		SCENE_OPEN_PREV,
		SCENE_OPEN_RECENT,
		SCENE_SAVE_SCENE,
		SCENE_SAVE_AS_SCENE,
		SCENE_SAVE_ALL_SCENES,
		SCENE_MULTI_SAVE_AS_SCENE,
		SCENE_QUICK_OPEN,
		SCENE_QUICK_OPEN_SCENE,
		SCENE_QUICK_OPEN_SCRIPT,
		SCENE_UNDO,
		SCENE_REDO,
		SCENE_RELOAD_SAVED_SCENE,
		SCENE_CLOSE,
		SCENE_QUIT,

		FILE_EXPORT_MESH_LIBRARY,

		// Project menu.
		PROJECT_OPEN_SETTINGS,
		PROJECT_FIND_IN_FILES,
		PROJECT_VERSION_CONTROL,
		PROJECT_EXPORT,
		PROJECT_PACK_AS_ZIP,
		PROJECT_INSTALL_ANDROID_SOURCE,
		PROJECT_OPEN_USER_DATA_FOLDER,
		PROJECT_RELOAD_CURRENT_PROJECT,
		PROJECT_QUIT_TO_PROJECT_MANAGER,

		TOOLS_ORPHAN_RESOURCES,
		TOOLS_BUILD_PROFILE_MANAGER,
		TOOLS_PROJECT_UPGRADE,
		TOOLS_UID_VIEWER,
		TOOLS_CUSTOM,

		VCS_METADATA,
		VCS_SETTINGS,

		// Editor menu.
		EDITOR_OPEN_SETTINGS,
		EDITOR_COMMAND_PALETTE,
		EDITOR_TAKE_SCREENSHOT,
		EDITOR_TOGGLE_FULLSCREEN,
		EDITOR_OPEN_DATA_FOLDER,
		EDITOR_OPEN_CONFIG_FOLDER,
		EDITOR_MANAGE_FEATURE_PROFILES,
		EDITOR_MANAGE_EXPORT_TEMPLATES,
		EDITOR_CONFIGURE_FBX_IMPORTER,

		LAYOUT_SAVE,
		LAYOUT_DELETE,
		LAYOUT_DEFAULT,

		// Help menu.
		HELP_SEARCH,
		HELP_DOCS,
		HELP_FORUM,
		HELP_COMMUNITY,
		HELP_COPY_SYSTEM_INFO,
		HELP_REPORT_A_BUG,
		HELP_SUGGEST_A_FEATURE,
		HELP_SEND_DOCS_FEEDBACK,
		HELP_ABOUT,
		HELP_SUPPORT_REDOT_DEVELOPMENT,

		// Update spinner menu.
		SPINNER_UPDATE_CONTINUOUSLY,
		SPINNER_UPDATE_WHEN_CHANGED,
		SPINNER_UPDATE_SPINNER_HIDE,

		// Non-menu options.
		SCENE_TAB_CLOSE,
		SAVE_AND_RUN,
		SAVE_AND_RUN_MAIN_SCENE,
		RESOURCE_SAVE,
		RESOURCE_SAVE_AS,
		SETTINGS_PICK_MAIN_SCENE,
	};

	enum ActionOnPlay {
		ACTION_ON_PLAY_DO_NOTHING,
		ACTION_ON_PLAY_OPEN_OUTPUT,
		ACTION_ON_PLAY_OPEN_DEBUGGER,
	};

	enum ActionOnStop {
		ACTION_ON_STOP_DO_NOTHING,
		ACTION_ON_STOP_CLOSE_BUTTOM_PANEL,
	};

	// ── Aggregated functional components ────────────────────────────────
	// Non-owning pointers, wired by `EditorCore::set_shell()`.

	/// The core service container.
	EditorCore *editor_core = nullptr;
	/// The open-scene session manager.
	EditorSceneManager *scene_manager = nullptr;
	/// The plugin registry.
	EditorPluginManager *plugin_manager = nullptr;

	void set_editor_core(EditorCore *p_editor_core) { editor_core = p_editor_core; }
	void set_scene_manager(EditorSceneManager *p_scene_manager) { scene_manager = p_scene_manager; }
	void set_editor_plugin_manager(EditorPluginManager *p_plugin_manager) { plugin_manager = p_plugin_manager; }

	/// @returns The core service container.
	EditorCore *get_editor_core() const { return editor_core; }
	/// @returns The open-scene session manager.
	EditorSceneManager *get_scene_manager() const { return scene_manager; }
	/// @returns The plugin registry.
	EditorPluginManager *get_editor_plugin_manager() const { return plugin_manager; }

	// ── Core service accessors (concrete delegates) ─────────────────────

	/// @returns The current editor selection.
	EditorSelection *get_editor_selection() { return editor_core->get_editor_selection(); }
	/// @returns The editor's data store (open scenes, edited object, undo history, etc.).
	EditorData &get_editor_data() { return editor_core->get_editor_data(); }
	/// @returns The helper that saves/restores scene folding and collapse state.
	EditorFolding &get_editor_folding() { return editor_core->get_editor_folding(); }
	/// @returns The history of previously selected/edited items (for back/forward).
	EditorSelectionHistory *get_editor_selection_history() { return editor_core->get_editor_selection_history(); }
	/// @returns The Project Settings editor window object.
	ProjectSettingsEditor *get_project_settings() { return editor_core->get_project_settings_editor(); }

	// ── Scene-session operations (concrete delegates) ───────────────────

	/// @returns `true` if the given scene path is currently open in a tab.
	bool is_scene_open(const String &p_path) { return scene_manager->is_scene_open(p_path); }
	/// @returns `true` if the scene file is currently open in an editor tab.
	bool is_scene_in_use(const String &p_path) { return scene_manager->is_scene_in_use(p_path); }
	/// @return `true` if any node in the subtree instances the given scene path.
	bool _find_scene_in_use(Node *p_node, const String &p_path) const { return scene_manager->_find_scene_in_use(p_node, p_path); }
	/// @returns `true` if any scenes are open in this editor session.
	bool has_scenes_in_session() { return scene_manager->has_scenes_in_session(); }
	/// @returns `true` if any open scene or resource has unsaved modifications.
	bool has_unsaved_changes() const { return scene_manager->has_unsaved_changes(); }

	/// Undoes the last edit.
	void undo() { scene_manager->undo(); }
	/// Redoes the last undo.
	void redo() { scene_manager->redo(); }

	/// Saves every open scene.
	void save_all_open_scenes() { scene_manager->save_all_open_scenes(); }
	/// Saves a scene if it is open.
	void save_scene_if_open(const String &p_scene_path) { scene_manager->save_scene_if_open(p_scene_path); }
	/// Saves every scene whose path is in the given set.
	void save_scene_list(const HashSet<String> &p_scene_paths) { scene_manager->save_scene_list(p_scene_paths); }
	/// Saves everything that needs saving before running the project.
	void save_before_run() { scene_manager->save_before_run(); }
	/// Triggers the autosave routine if configured.
	void try_autosave() { scene_manager->try_autosave(); }
	/// Saves external resources and modified subresources.
	/// @return How many were saved (or error count semantics used by callers).
	int _save_external_resources(bool p_also_save_external_data = false) { return scene_manager->_save_external_resources(p_also_save_external_data); }

	// ── Plugin registry (concrete delegates) ────────────────────────────

	/// Registers an `EditorPlugin` instance with the editor (makes it active).
	void add_editor_plugin(EditorPlugin *p_editor, bool p_config_changed = false) { plugin_manager->add_editor_plugin(p_editor, p_config_changed); }
	/// Unregisters / deactivates an `EditorPlugin` instance.
	void remove_editor_plugin(EditorPlugin *p_editor, bool p_config_changed = false) { plugin_manager->remove_editor_plugin(p_editor, p_config_changed); }

	/// @returns The plugin list that receives input before everything else.
	EditorPluginList *get_editor_plugins_force_input_forwarding() { return plugin_manager->get_editor_plugins_force_input_forwarding(); }
	/// @returns The list of plugins that have opted in to draw over the 2D/3D viewport.
	EditorPluginList *get_editor_plugins_force_over() { return plugin_manager->get_editor_plugins_force_over(); }
	/// @returns The plugin list drawn over the viewport normally.
	EditorPluginList *get_editor_plugins_over() { return plugin_manager->get_editor_plugins_over(); }

	/// Registers a plugin that can convert one resource type to another.
	void add_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin) { plugin_manager->add_resource_conversion_plugin(p_plugin); }
	/// Unregisters a resource conversion plugin.
	void remove_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin) { plugin_manager->remove_resource_conversion_plugin(p_plugin); }

	/// Restores plugin-specific per-scene state from a scene's config data.
	void _load_editor_plugin_states_from_config(const Ref<ConfigFile> &p_config_file) { plugin_manager->_load_editor_plugin_states_from_config(p_config_file); }
	/// Handles a plugin starting to edit an object.
	void _plugin_over_edit(EditorPlugin *p_plugin, Object *p_object) { plugin_manager->_plugin_over_edit(p_plugin, p_object); }
	/// Registers a plugin as the active screen plugin when it owns the main screen.
	void _plugin_over_self_own(EditorPlugin *p_plugin) { plugin_manager->_plugin_over_self_own(p_plugin); }

	/// Registers editor plugins and runs their initialization routines.
	void init_plugins() { plugin_manager->init_plugins(); }
	/// Registers a callback invoked once when the editor starts up.
	void add_init_callback(EditorNodeInitCallback p_callback) { EditorPluginManager::add_init_callback(p_callback); }
	/// Registers a callback invoked to check whether the project "builds" before running.
	void add_build_callback(EditorBuildCallback p_callback) { EditorPluginManager::add_build_callback(p_callback); }
	/// Registers a callback invoked when each plugin is initialized.
	void add_plugin_init_callback(EditorPluginInitializeCallback p_callback) { EditorPluginManager::add_plugin_init_callback(p_callback); }
	/// Instantiates and registers an editor plugin class provided by an extension by class name.
	void add_extension_editor_plugin(const StringName &p_class_name) { plugin_manager->add_extension_editor_plugin(p_class_name); }
	/// Removes an extension-provided editor plugin by class name.
	void remove_extension_editor_plugin(const StringName &p_class_name) { plugin_manager->remove_extension_editor_plugin(p_class_name); }
	/// Enables or disables a project addon plugin by name.
	void set_addon_plugin_enabled(const String &p_addon, bool p_enabled, bool p_config_changed = false) { plugin_manager->set_addon_plugin_enabled(p_addon, p_enabled, p_config_changed); }
	/// @returns `true` if the named addon plugin is currently enabled.
	bool is_addon_plugin_enabled(const String &p_addon) const { return plugin_manager->is_addon_plugin_enabled(p_addon); }
	/// Runs all registered build callbacks; @return `false` if any of them fail.
	bool call_build() { return plugin_manager->call_build(); }
	/// Finds conversion plugins capable of converting the given resource.
	Vector<Ref<EditorResourceConversionPlugin>> find_resource_conversion_plugin_for_resource(const Ref<Resource> &p_for_resource) { return plugin_manager->find_resource_conversion_plugin_for_resource(p_for_resource); }
	/// Finds conversion plugins that target the named resource type.
	Vector<Ref<EditorResourceConversionPlugin>> find_resource_conversion_plugin_for_type_name(const String &p_type) { return plugin_manager->find_resource_conversion_plugin_for_type_name(p_type); }
	/// Performs editor shutdown cleanup (clears registered init callbacks).
	void cleanup() { plugin_manager->cleanup(); }

	// ── Container access ──────────────────────────────────────────────────

	/// @returns The root `Control` of the editor's GUI.
	virtual Control *get_gui_base() = 0;

	/// @returns The viewport that is the root of the scene being edited.
	virtual SubViewport *get_scene_root() = 0;

	/// @returns The widget managing the main-screen buttons (2D, 3D, Script, …).
	virtual EditorMainScreen *get_editor_main_screen() = 0;

	/// @returns The bottom panel container (Output, Debugger, …).
	virtual EditorBottomPanel *get_bottom_panel() = 0;

	/// @returns The editor's top title-bar widget.
	virtual EditorTitleBar *get_title_bar() = 0;

	/// @returns The widget managing the open scene tabs.
	virtual EditorSceneTabs *get_scene_tabs() = 0;

	/// @returns The run bar (play/stop controls).
	virtual EditorRunBar *get_project_run_bar() = 0;

	/// @returns The vertical split containing the main editor area and bottom panel.
	virtual VSplitContainer *get_top_split() = 0;

	// ── User feedback ─────────────────────────────────────────────────────

	/// Shows a warning dialog with the given text and title.
	virtual void show_warning(const String &p_text, const String &p_title = TTR("Warning!")) = 0;

	/// Shows a simple informational "accept" dialog with the given text and title.
	virtual void show_accept(const String &p_text, const String &p_title) = 0;

	/// Queues an IO error message to be shown to the user in a dialog.
	virtual void add_io_error(const String &p_error) = 0;

	/// Queues an IO warning message to be shown to the user in a dialog.
	virtual void add_io_warning(const String &p_warning) = 0;

	/// Shows a modal OK/Cancel dialog and blocks until the user answers.
	/// @return `true` if the user accepted.
	virtual bool immediate_confirmation_dialog(const String &p_text, const String &p_ok_text = TTR("Ok"), const String &p_cancel_text = TTR("Cancel"), uint32_t p_wrap_width = 0) = 0;

	/// Shows a dialog explaining why loading the given file failed.
	virtual void _dialog_display_load_error(String p_file, Error p_error) = 0;

	/// Shows a dialog explaining why saving the given file failed.
	virtual void _dialog_display_save_error(String p_file, Error p_error) = 0;

	// ── Menu integration ──────────────────────────────────────────────────

	/// Adds a custom item to the Project > Tools menu that calls the given callable.
	virtual void add_tool_menu_item(const String &p_name, const Callable &p_callback) = 0;

	/// Adds a custom submenu item to the Project > Tools menu.
	virtual void add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu) = 0;

	/// Removes an item previously added to the Tools menu.
	virtual void remove_tool_menu_item(const String &p_name) = 0;

	/// @returns The "Export As…" popup menu (for mesh libraries, etc.).
	virtual PopupMenu *get_export_as_menu() = 0;

	/// Triggers a menu action programmatically, as if the user clicked it.
	virtual void trigger_menu_option(int p_option, bool p_confirmed) = 0;

	// ── Shell state / queries ─────────────────────────────────────────────

	/// @returns `true` if the editor is in the process of quitting.
	virtual bool is_exiting() const = 0;

	/// @returns `true` once the editor has finished its initial filesystem scan and is ready for use.
	virtual bool is_editor_ready() const = 0;

	/// Dims or undims the editor UI (e.g. while a modal is open).
	virtual void dim_editor(bool p_dimming) = 0;

	/// @returns `true` if the editor UI is currently dimmed.
	virtual bool is_editor_dimmed() const = 0;

	/// Enters or exits distraction-free mode.
	virtual void set_distraction_free_mode(bool p_enter) = 0;

	/// Refreshes the UI according to the current distraction-free mode state.
	virtual void update_distraction_free_mode() = 0;

	/// @returns `true` if distraction-free mode is currently active.
	virtual bool is_distraction_free_mode_enabled() const = 0;

	/// @returns `true` if the editor is running in headless / command-line mode.
	virtual bool is_cmdline_mode() = 0;

	/// Notifies the editor that all running debug sessions have ended.
	virtual void notify_all_debug_sessions_exited() = 0;

	/// @returns The process ID if the given PID is a child process spawned by the editor, or an invalid ID otherwise.
	virtual OS::ProcessID has_child_process(OS::ProcessID p_pid) const = 0;

	/// Stops a child process previously spawned by the editor.
	virtual void stop_child_process(OS::ProcessID p_pid) = 0;

	// ── Shell state accessors used by the core ───────────────────────────────

	/// @returns The general-purpose file dialog (open/save) driven by core
	/// resource operations such as `save_resource_as`.
	virtual EditorFileDialog *get_file_dialog() = 0;

	/// Sets the menu option that will be executed when the pending file dialog
	/// is confirmed.
	virtual void set_current_menu_option(int p_option) = 0;

	/// @returns The menu option currently in progress (or -1 if none).
	virtual int get_current_menu_option() = 0;

	/// @returns The dialog that displays broken-dependency errors for a loaded resource.
	virtual DependencyErrorDialog *get_dependency_error_dialog() = 0;

	/// @returns The dependency editor (fixer) dialog.
	virtual DependencyEditor *get_dependency_fixer() = 0;

	// ── Shell state accessors used by the scene manager ───────────────────

	/// @returns The Scene/File menu (for enabling/disabling items).
	virtual PopupMenu *get_file_menu() = 0;

	/// @returns The "save before closing" confirmation dialog.
	virtual ConfirmationDialog *get_save_confirmation_dialog() = 0;

	/// @returns The generic confirmation dialog.
	virtual ConfirmationDialog *get_confirmation_dialog() = 0;

	/// @returns The "open imported scene" confirmation dialog.
	virtual ConfirmationDialog *get_open_imported_dialog() = 0;

	/// @returns The "new inherited scene" button in the open-imported dialog.
	virtual Button *get_new_inherited_button() = 0;

	// ── Run / debug ───────────────────────────────────────────────────────

	/// Runs an external command in a background thread, displaying its output
	/// in a dialog until it completes.
	/// @return The process exit code (or an error code if it could not be launched).
	virtual int execute_and_show_output(const String &p_title, const String &p_path, const List<String> &p_arguments, bool p_close_on_ok, bool p_close_on_errors, String *r_output) = 0;

	/// Restarts the editor, optionally landing in the project manager.
	virtual void restart_editor(bool p_goto_project_manager = false) = 0;

	/// Internal restart implementation, optionally landing in the project manager.
	virtual void _restart_editor(bool p_goto_project_manager = false) = 0;

	/// Shuts the editor down with the given exit code.
	virtual void _exit_editor(int p_exit_code) = 0;

	/// Opens the export template management dialog.
	virtual void open_export_template_manager() = 0;

	/// Validates the project's custom user-data directory, prompting the user
	/// to fix it if invalid.
	/// @return `true` if the directory is valid (or no custom directory is in use).
	virtual bool validate_custom_directory() = 0;

	// ── Drag / drop ───────────────────────────────────────────────────────

	/// Builds drag-and-drop data for dragging a resource from a control.
	virtual Dictionary drag_resource(const Ref<Resource> &p_res, Control *p_from) = 0;

	/// Builds drag-and-drop data for dragging files/folders from a control.
	virtual Dictionary drag_files_and_dirs(const Vector<String> &p_paths, Control *p_from) = 0;

	/// Schedules the editor dock layout to be saved shortly (debounced).
	virtual void save_editor_layout_delayed() = 0;

	// ── Scene-session UI hooks ────────────────────────────────────────────
	// These are the cross-boundary operations that the scene manager (or other
	// core services) dispatch to the shell. They exist on the concrete shell;
	// the shell exposes them so core components can reach them without a
	// concrete shell type.

	/// Handles a request to create a new scene inheriting from the given file.
	virtual void _inherit_request(String p_file) = 0;

	/// Handles a request to instantiate scene files into the current scene.
	virtual void _instantiate_request(const Vector<String> &p_files) = 0;

	/// Handles "new inherited scene" requests triggered from imported files.
	virtual void _inherit_imported(const String &p_action) = 0;

	/// Handles the open-imported-file confirmation dialog being accepted.
	virtual void _open_imported() = 0;

	/// @returns `true` if the feature profile disables editors for the given class.
	virtual bool _is_class_editor_disabled_by_feature_profile(const StringName &p_class) = 0;

	/// Saves a scene while rendering a preview thumbnail for it.
	virtual void _save_scene_with_preview(String p_file, int p_idx = -1) = 0;

	/// Re-triggers editing of the current object (pushes it to inspector and plugins).
	virtual void _edit_current(bool p_skip_foreign = false, bool p_skip_inspector_update = false) = 0;

	/// Opens a resource that belongs to another scene/instance for editing in its foreign context.
	virtual void edit_foreign_resource(const Ref<Resource> &p_resource) = 0;

	/// Opens the Project Settings dialog focused on the given override property.
	virtual void open_setting_override(const String &p_property) = 0;

	/// Ensures a main scene is set (prompting the user if needed).
	/// @return `true` if the project can run.
	virtual bool ensure_main_scene(bool p_from_native) = 0;

	/// Closes the current scene tab; returns `false` if the user cancelled.
	virtual bool close_scene() = 0;

	// ── Shell UI bookkeeping used by the scene manager ────────────────────

	/// Refreshes the window title (project name, current scene, unsaved marker).
	virtual void _update_title() = 0;

	/// Entry point for a menu option. If it needs confirmation, shows a dialog
	/// first and defers to the shell's `_menu_option` once confirmed.
	virtual void _menu_option_confirm(int p_option, bool p_confirmed) = 0;

	/// @returns `true` while the editor is mid-shutdown/quit.
	virtual bool _is_closing_editor() const = 0;

	/// Adds a path to the recent-scenes list.
	virtual void _add_to_recent_scenes(const String &p_scene) = 0;

	/// Records an edited object in the selection/edit history for back/forward nav.
	virtual void _add_to_history(const Object *p_object, const String &p_property, bool p_inspector_only) = 0;
};

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
#include "servers/display/display_server.h"

#include "editor/editor_core.h"
#include "editor/editor_scene_manager.h"
#include "editor/export/editor_export.h"
#include "editor/plugins/editor_plugin_manager.h"

class AcceptDialog;
class Button;
class ColorPicker;
class ConfirmationDialog;
class Control;
class DependencyEditor;
class DependencyErrorDialog;
class EditorAbout;
class EditorBuildProfileManager;
class EditorBottomPanel;
class EditorData;
class EditorFileDialog;
class ExportTemplateManager;
class Label;
class OptionButton;
class OrphanResourcesDialog;
class ProjectUpgradeTool;
class UIDViewer;
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
class Theme;
class VSplitContainer;

/**
 * Abstract base class for the editor's functional logic and visual shell.
 *
 * `EditorShell` aggregates the editor's decoupled functional components —
 * `EditorCore`, `EditorSceneManager`, and `EditorPluginManager` — and owns
 * the editor's FUNCTIONAL behavior: menu dispatch, save/open flows,
 * filesystem-scan reactions, title updates, dialogs, and the quit/restart
 * flow. A concrete shell (e.g. `EditorNode`, `AreaEditorNode`) therefore
 * only has to provide the genuinely visual parts (the pure virtuals
 * below): containers, menus, dialogs, window state, and the
 * scene-session UI hooks specific to its layout.
 *
 * It is a `Node`-based class (NOT a singleton). The three component
 * pointers are non-owning; they are wired by `EditorCore::set_shell()`
 * when a concrete shell registers itself, and are `nullptr` until then.
 */
class EditorShell : public Node {
	GDCLASS(EditorShell, Node);

protected:
	/// Signals shared by all shells. Plugins connect to these through the
	/// active shell (`EditorCore::get_singleton()->get_shell()`) instead of a
	/// concrete editor type.
	static void _bind_methods() {
		ADD_SIGNAL(MethodInfo("request_help_search"));
		ADD_SIGNAL(MethodInfo("script_add_function_request", PropertyInfo(Variant::OBJECT, "obj"), PropertyInfo(Variant::STRING, "function"), PropertyInfo(Variant::PACKED_STRING_ARRAY, "args")));
	}

public:
	virtual ~EditorShell() = default;

	/// Default shell notification handler: unparents the core-owned
	/// quick-open dialogs on predelete — which runs before the shell's
	/// subtree (and thus the dialogs) is freed — so that `EditorCore`'s
	/// destructor remains the single deletion point and does not
	/// double-free them during SceneTree teardown; kicks the deferred
	/// first filesystem scan (`_begin_first_scan`) once the shell is
	/// processing; and routes window close requests into the quit flow.
	/// Concrete shells may override and must call `EditorShell::_notification()`
	/// first to keep this behavior.
	virtual void _notification(int p_what);

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

		// Scene tab context menu.
		SCENE_SHOW_IN_FILESYSTEM = 1000, ///< Prevents conflicts with other menu option IDs.
		SCENE_RUN,
		SCENE_CLOSE_OTHERS,
		SCENE_CLOSE_RIGHT,
		SCENE_CLOSE_ALL,
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

	// ── Shared shell state ──────────────────────────────────────────────
	// Functional state shared by every shell (menu flow, quit flow, first-scan
	// state). Concrete shells keep their own additional visual state.

	/// The menu option currently in progress (or -1 if none).
	int current_menu_option = 0;
	/// `true` while a "stop running project before quitting?" confirmation is pending.
	bool stop_project_confirmation = false;
	/// Plugin whose external data should be saved on quit, if any.
	EditorPlugin *plugin_to_save = nullptr;
	/// Window mode to restore when toggling fullscreen off.
	DisplayServer::WindowMode prev_mode = DisplayServer::WINDOW_MODE_MAXIMIZED;
	/// `true` if the editor is in the process of quitting.
	bool exiting = false;
	/// `true` if the editor UI is currently dimmed.
	bool dimmed = false;
	/// `true` if the editor is running in headless / command-line mode.
	bool cmdline_mode = false;
	/// `true` while the editor waits for the initial filesystem scan to finish.
	bool waiting_for_first_scan = true;
	/// `true` once a shell has requested the first filesystem scan (fired from processing).
	bool requested_first_scan = false;
	/// Result of the most recent `immediate_confirmation_dialog()` poll.
	bool immediate_dialog_confirmed = false;
	/// Export preset selected for installing the Android build template from file.
	Ref<EditorExportPreset> android_export_preset;

	/// Backs the default `get_accept_dialog()` implementation (created lazily
	/// by it, parented to `get_gui_base()`). Replaced by a shell's own dialog
	/// when that shell overrides the accessor (e.g. `EditorNode`).
	AcceptDialog *accept = nullptr;
	/// Backs the default `get_save_accept_dialog()` implementation (created
	/// lazily by it, parented to `get_gui_base()`). Replaced by a shell's own
	/// dialog when that shell overrides the accessor.
	AcceptDialog *save_accept = nullptr;
	/// Backs the default `get_warning_dialog()` implementation (created lazily
	/// by it, parented to `get_gui_base()`). Replaced by a shell's own dialog
	/// when that shell overrides the accessor.
	AcceptDialog *warning = nullptr;

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

	// ── Container access (shell-specific UI, provided by each shell) ───

	/// @returns The root `Control` of the editor's GUI.
	virtual Control *get_gui_base() const = 0;

	/// @returns The viewport that is the root of the scene being edited.
	virtual SubViewport *get_scene_root() const = 0;

	/// @returns The widget managing the main-screen buttons (2D, 3D, Script, …).
	virtual EditorMainScreen *get_editor_main_screen() const = 0;

	/// @returns The bottom panel container (Output, Debugger, …).
	virtual EditorBottomPanel *get_bottom_panel() const = 0;

	/// @returns The editor's top title-bar widget.
	virtual EditorTitleBar *get_title_bar() const = 0;

	/// @returns The widget managing the open scene tabs.
	virtual EditorSceneTabs *get_scene_tabs() const = 0;

	/// @returns The run bar (play/stop controls).
	virtual EditorRunBar *get_project_run_bar() const = 0;

	/// @returns The vertical split containing the main editor area and bottom panel.
	virtual VSplitContainer *get_top_split() const = 0;

	// ── Dialog / menu accessors used by the functional logic ────────────

	/// @returns The general-purpose file dialog (open/save) driven by core
	/// resource operations such as `save_resource_as`.
	virtual EditorFileDialog *get_file_dialog() = 0;

	/// @returns The generic "accept" dialog. By default this lazily creates a
	/// plain `AcceptDialog` (autowrap, 600px min width) parented to
	/// `get_gui_base()`; a concrete shell may override to supply its own
	/// styled dialog (e.g. `EditorNode` pre-creates and customizes one).
	virtual AcceptDialog *get_accept_dialog();

	/// @returns The "accept" dialog styled for save-related messages. By
	/// default this lazily creates a plain `AcceptDialog` parented to
	/// `get_gui_base()`; a concrete shell may override to supply its own.
	virtual AcceptDialog *get_save_accept_dialog();

	/// @returns The warning dialog. By default this lazily creates a plain
	/// `AcceptDialog` parented to `get_gui_base()`; a concrete shell may
	/// override to supply its own styled dialog (e.g. `EditorNode` adds a
	/// "Copy Text" button).
	virtual AcceptDialog *get_warning_dialog();

	/// @returns The "OK" button shown next to the generic confirmation dialog.
	virtual Button *get_confirmation_button() = 0;

	/// @returns The "pick a main scene" confirmation dialog.
	virtual ConfirmationDialog *get_pick_main_scene_dialog() = 0;

	/// @returns The "Select Current" button in the pick-main-scene dialog.
	virtual Button *get_select_current_scene_button() = 0;

	/// @returns The "About" dialog.
	virtual EditorAbout *get_about_dialog() = 0;

	/// @returns The "Export Template Manager" dialog (nullable if the shell has none).
	virtual ExportTemplateManager *get_export_template_manager() { return nullptr; }

	/// @returns The "Export Project as ZIP" file dialog (nullable if the shell has none).
	virtual EditorFileDialog *get_pack_zip_file_dialog() { return nullptr; }

	/// @returns The "Export Mesh Library" file dialog (nullable if the shell has none).
	virtual EditorFileDialog *get_export_lib_file_dialog() { return nullptr; }

	/// @returns The Orphan Resources tool dialog (nullable if the shell has none).
	virtual OrphanResourcesDialog *get_orphan_resources_dialog() { return nullptr; }

	/// @returns The Build Profile Manager dialog (nullable if the shell has none).
	virtual EditorBuildProfileManager *get_build_profile_manager() { return nullptr; }

	/// @returns The Project Upgrade tool (nullable if the shell has none).
	virtual ProjectUpgradeTool *get_project_upgrade_tool() { return nullptr; }

	/// @returns The UID Viewer window (nullable if the shell has none).
	virtual UIDViewer *get_uid_viewer() { return nullptr; }

	/// @returns The "choose Android export preset" option button (nullable if the shell has none).
	virtual OptionButton *get_choose_android_export_profile() { return nullptr; }

	/// @returns The "Android template already installed" warning dialog (nullable if the shell has none).
	virtual ConfirmationDialog *get_remove_android_build_template_dialog() { return nullptr; }

	/// @returns The "manage Android build templates" dialog (nullable if the shell has none).
	virtual ConfirmationDialog *get_gradle_build_manage_templates_dialog() { return nullptr; }

	/// @returns The "install Android build template" dialog (nullable if the shell has none).
	virtual ConfirmationDialog *get_install_android_build_template_dialog() { return nullptr; }

	/// @returns The "select Android sources file" dialog (nullable if the shell has none).
	virtual EditorFileDialog *get_file_android_build_source_dialog() { return nullptr; }

	/// @returns The info label inside the install-Android-build-template dialog (nullable if the shell has none).
	virtual Label *get_install_android_build_template_message() { return nullptr; }

	/// @returns The "Recent Scenes" popup menu (each shell provides one).
	virtual PopupMenu *get_recent_scenes_menu() = 0;

	/// @returns The Project > Tools popup menu (each shell provides one).
	virtual PopupMenu *get_tool_menu() = 0;

	// ── User feedback ─────────────────────────────────────────────────────

	/// Shows a simple informational "accept" dialog with the given text and title.
	/// The dialog is lazily created (and parented to `get_gui_base()`) if the
	/// shell did not create it.
	virtual void show_accept(const String &p_text, const String &p_title);

	/// Shows an "accept" dialog styled for save-related messages.
	virtual void show_save_accept(const String &p_text, const String &p_title);

	/// Shows a warning dialog with the given text and title.
	virtual void show_warning(const String &p_text, const String &p_title = TTR("Warning!"));

	/// Queues an IO error message to be shown to the user in a dialog.
	/// The default surfaces it through `show_warning`; shells may override
	/// with a richer presentation.
	virtual void add_io_error(const String &p_error);

	/// Queues an IO warning message to be shown to the user in a dialog.
	virtual void add_io_warning(const String &p_warning);

	/// Shows a modal OK/Cancel dialog and blocks until the user answers.
	/// @return `true` if the user accepted.
	virtual bool immediate_confirmation_dialog(const String &p_text, const String &p_ok_text = TTR("Ok"), const String &p_cancel_text = TTR("Cancel"), uint32_t p_wrap_width = 0);

	/// Shows a dialog explaining why loading the given file failed.
	virtual void _dialog_display_load_error(String p_file, Error p_error);

	/// Shows a dialog explaining why saving the given file failed.
	virtual void _dialog_display_save_error(String p_file, Error p_error);

	// ── Menu integration ──────────────────────────────────────────────────

	/// Adds a custom item to the Project > Tools menu that calls the given callable.
	virtual void add_tool_menu_item(const String &p_name, const Callable &p_callback);

	/// Adds a custom submenu item to the Project > Tools menu.
	virtual void add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu);

	/// Removes an item previously added to the Tools menu.
	virtual void remove_tool_menu_item(const String &p_name);

	/// @returns The "Export As…" popup menu (for mesh libraries, etc.).
	virtual PopupMenu *get_export_as_menu() = 0;

	/// Triggers a menu action programmatically, as if the user clicked it.
	virtual void trigger_menu_option(int p_option, bool p_confirmed);

	// ── Theme / title ─────────────────────────────────────────────────────

	/// Refreshes the window title (project name, current scene, unsaved marker).
	virtual void _update_title();

	/// Applies the configured V-Sync mode to the editor window.
	virtual void _update_vsync_mode();

	/// Rebuilds (or refreshes, with `p_skip_creation`) the editor theme and
	/// applies it to the shell and its window, then delegates shell-specific
	/// style application to `_apply_theme_styles`.
	virtual void _update_theme(bool p_skip_creation = false);

	/// Hook for shell-specific theme style overrides (e.g. panel styleboxes,
	/// menu icons). The default does nothing.
	virtual void _apply_theme_styles(const Ref<Theme> &p_theme) {}

	/// Hook for updating a shell-specific title label. The default does
	/// nothing (the window title itself is set by `_update_title`).
	virtual void _update_title_label(const String &p_title) {}

	// ── Shell state / queries ─────────────────────────────────────────────

	/// @returns `true` if the editor is in the process of quitting.
	virtual bool is_exiting() const;

	/// @returns `true` once the editor has finished its initial filesystem scan and is ready for use.
	virtual bool is_editor_ready() const;

	/// Dims or undims the editor UI (e.g. while a modal is open).
	virtual void dim_editor(bool p_dimming);

	/// @returns `true` if the editor UI is currently dimmed.
	virtual bool is_editor_dimmed() const;

	/// Enters or exits distraction-free mode.
	virtual void set_distraction_free_mode(bool p_enter) = 0;

	/// Refreshes the UI according to the current distraction-free mode state.
	virtual void update_distraction_free_mode() = 0;

	/// @returns `true` if distraction-free mode is currently active.
	virtual bool is_distraction_free_mode_enabled() const = 0;

	/// @returns `true` if the editor is running in headless / command-line mode.
	virtual bool is_cmdline_mode();

	/// Notifies the editor that all running debug sessions have ended.
	virtual void notify_all_debug_sessions_exited();

	/// @returns The process ID if the given PID is a child process spawned by the editor, or an invalid ID otherwise.
	virtual OS::ProcessID has_child_process(OS::ProcessID p_pid) const;

	/// Stops a child process previously spawned by the editor.
	virtual void stop_child_process(OS::ProcessID p_pid);

	// ── Shell state accessors used by the core ───────────────────────────────

	/// Sets the menu option that will be executed when the pending file dialog
	/// is confirmed.
	virtual void set_current_menu_option(int p_option);

	/// @returns The menu option currently in progress (or -1 if none).
	virtual int get_current_menu_option();

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
	virtual void restart_editor(bool p_goto_project_manager = false);

	/// Internal restart implementation, optionally landing in the project manager.
	virtual void _restart_editor(bool p_goto_project_manager = false);

	/// Shuts the editor down with the given exit code.
	virtual void _exit_editor(int p_exit_code);

	/// Opens the export template management dialog.
	virtual void open_export_template_manager();

	/// Validates the project's custom user-data directory, prompting the user
	/// to fix it if invalid.
	/// @return `true` if the directory is valid (or no custom directory is in use).
	virtual bool validate_custom_directory() = 0;

	// ── Plugin dock integration ───────────────────────────────────────────

	/// Adds a plugin dock control to the given dock slot (see the dock slot
	/// enum of the shell's dock system).
	/// The default implementation does nothing; shells that support plugin
	/// docks (e.g. `EditorNode`) override it to route the dock to their dock
	/// system.
	virtual void add_control_to_dock(int p_slot, Control *p_control, const Ref<Shortcut> &p_shortcut) {
		ERR_PRINT(vformat("Cannot add dock: this shell does not support plugin docks (slot %d).", p_slot));
		(void)p_control;
		(void)p_shortcut;
	}
	/// Removes a plugin dock control from all dock slots.
	/// The default implementation does nothing; shells that support plugin
	/// docks (e.g. `EditorNode`) override it.
	virtual void remove_control_from_docks(Control *p_control) {
		(void)p_control;
	}
	/// Sets the tab icon of a plugin dock control.
	/// The default implementation does nothing; shells that support plugin
	/// docks (e.g. `EditorNode`) override it.
	virtual void set_dock_tab_icon(Control *p_control, const Ref<Texture2D> &p_icon) {
		(void)p_control;
		(void)p_icon;
	}

	/// Enables or disables the low-processor-usage mode applied while the
	/// editor window is unfocused. The default does nothing; `EditorNode`
	/// overrides it.
	virtual void set_unfocused_low_processor_usage_mode_enabled(bool p_enabled) { (void)p_enabled; }

	// ── Drag / drop ───────────────────────────────────────────────────────

	/// Builds drag-and-drop data for dragging a resource from a control.
	virtual Dictionary drag_resource(const Ref<Resource> &p_res, Control *p_from);

	/// Builds drag-and-drop data for dragging files/folders from a control.
	virtual Dictionary drag_files_and_dirs(const Vector<String> &p_paths, Control *p_from);

	/// Schedules the editor dock layout to be saved shortly (debounced).
	/// The default does nothing; shells with a dock layout override it.
	virtual void save_editor_layout_delayed() {}

	// ── Filesystem scan reactions ─────────────────────────────────────────

	/// Handles filesystem-changed notifications: invalidates core file
	/// dialogs and marks unsaved scenes. Shells with additional reactions
	/// (e.g. deferred CLI exports) override and call `EditorShell::_fs_changed()`.
	virtual void _fs_changed();

	/// Handles non-resource source files (e.g. scripts) appearing/disappearing;
	/// also completes the initial filesystem scan (layout load, deferred scene
	/// load, preview thread, lock-file removal).
	virtual void _sources_changed(bool p_exist);

	/// Handles resources changing on disk (reload prompts, live updates).
	virtual void _resources_changed(const Vector<String> &p_resources);

	/// Deletes the editor's lock file on clean shutdown.
	virtual void _remove_lock_file();

	/// Kicks off the initial filesystem scan at editor startup (deferred to
	/// the first `NOTIFICATION_PROCESS`).
	virtual void _begin_first_scan();

	/// Runs pending project-upgrade steps queued for this session, once the
	/// first filesystem scan completes. The default does nothing; shells
	/// owning an upgrade tool override it.
	virtual void _execute_upgrades() {}

	// ── Menu flow ──────────────────────────────────────────────────────────

	/// Entry point for a menu option; shows a confirmation dialog when needed
	/// and defers to `_menu_option_confirm` once confirmed.
	virtual void _menu_option(int p_option);

	/// Re-runs the currently pending menu option as if the user confirmed it.
	virtual void _menu_confirm_current();

	/// Entry point for a menu option. If it needs confirmation, shows a dialog
	/// first and defers to the shell's `_menu_option` once confirmed.
	virtual void _menu_option_confirm(int p_option, bool p_confirmed);

	/// Handles the user confirming a file dialog; dispatches on the pending action.
	virtual void _dialog_action(String p_file);

	/// Handles a custom item added to the Project > Tools menu being triggered.
	virtual void _tool_menu_option(int p_idx);

	/// Refreshes the Scene/File menu contents each time it's opened (enabled states, etc.).
	virtual void _update_file_menu_opened();

	/// Handles a file being chosen in the Quick Open dialog.
	virtual void _quick_opened(const String &p_file_path);

	/// Requests a screenshot of the editor window. The default does nothing;
	/// shells with a screenshot feature override it.
	virtual void _request_screenshot() {}

	/// Rebuilds the update spinner menu and applies its update mode. The
	/// default does nothing; shells with an update spinner override it.
	virtual void _update_update_spinner() {}

	// ── Scene-session UI hooks ────────────────────────────────────────────
	// These are the cross-boundary operations that the scene manager (or other
	// core services) dispatch to the shell. They exist on the concrete shell;
	// the shell exposes them so core components can reach them without a
	// concrete shell type.

	/// Handles a request to create a new scene inheriting from the given file.
	virtual void _inherit_request(String p_file);

	/// Handles a request to instantiate scene files into the current scene.
	virtual void _instantiate_request(const Vector<String> &p_files);

	/// Handles "new inherited scene" requests triggered from imported files.
	virtual void _inherit_imported(const String &p_action);

	/// Handles the open-imported-file confirmation dialog being accepted.
	virtual void _open_imported();

	/// @returns `true` if the feature profile disables editors for the given class.
	virtual bool _is_class_editor_disabled_by_feature_profile(const StringName &p_class);

	/// Saves a scene while rendering a preview thumbnail for it.
	virtual void _save_scene_with_preview(String p_file, int p_idx = -1) = 0;

	/// Tears down the scene-save progress indicator after saving completes.
	/// The default does nothing; shells with a save progress indicator override it.
	virtual void _close_save_scene_progress() {}

	/// Re-triggers editing of the current object (pushes it to inspector and plugins).
	virtual void _edit_current(bool p_skip_foreign = false, bool p_skip_inspector_update = false);

	/// Opens a resource that belongs to another scene/instance for editing in its foreign context.
	virtual void edit_foreign_resource(const Ref<Resource> &p_resource);

	/// Opens the Project Settings dialog focused on the given override property.
	virtual void open_setting_override(const String &p_property);

	/// Ensures a main scene is set (prompting the user if needed).
	/// @return `true` if the project can run.
	virtual bool ensure_main_scene(bool p_from_native);

	/// Closes the current scene tab; returns `false` if the user cancelled.
	virtual bool close_scene();

	/// Handles custom actions added to the main-scene picker dialog.
	virtual void _pick_main_scene_custom_action(const String &p_custom_action_name);

	// ── Recent scenes ─────────────────────────────────────────────────────

	/// Adds a path to the recent-scenes list.
	virtual void _add_to_recent_scenes(const String &p_scene);

	/// Opens the recent scene at the given menu index.
	virtual void _open_recent_scene(int p_idx);

	/// Rebuilds the Recent Scenes popup contents from the stored list.
	virtual void _update_recent_scenes();

	// ── Project run reactions ─────────────────────────────────────────────

	/// Handles clearing output, etc. when the project starts running.
	virtual void _project_run_started();

	/// Handles closing the bottom panel, etc. when the project stops.
	virtual void _project_run_stopped();

	// ── History ───────────────────────────────────────────────────────────

	/// Records an edited object in the selection/edit history for back/forward nav.
	virtual void _add_to_history(const Object *p_object, const String &p_property, bool p_inspector_only);

	// ── Quit / restart flow ───────────────────────────────────────────────

	/// @returns `true` while the editor is mid-shutdown/quit.
	virtual bool _is_closing_editor() const;

	/// Cancels the pending "stop running project" confirmation.
	virtual void _cancel_confirmation();

	/// Re-parents the save-confirmation popup so it stacks above other popups.
	virtual void _prepare_save_confirmation_popup();

	// ── Layout persistence hooks ──────────────────────────────────────────
	// The defaults do nothing; shells with a dock/panel layout system override
	// them to persist/restore their layout.

	/// Loads the editor layout once the first filesystem scan completes.
	virtual void _load_editor_layout_on_first_scan() {}

	/// Saves the editor layout when the editor exits.
	virtual void _save_editor_layout_on_exit() {}

	/// Saves a named editor layout to the given file.
	virtual void _save_layout(const String &p_file) {}

	/// Deletes a named editor layout.
	virtual void _delete_layout(const String &p_file) {}

	/// Applies the built-in default editor layout.
	virtual void _apply_default_layout() {}

	// ── Android build template flow ───────────────────────────────────────
	// Functional parts of the "Install Android build template" flow; the shell
	// provides the dialogs through the `get_*_android_*` accessors above.

	/// Handles the user picking an Android build source path in the file dialog.
	virtual void _android_build_source_selected(const String &p_file);

	/// Handles selection of which export preset the Android build template uses.
	virtual void _android_export_preset_selected(int p_index);

	/// Opens the "select Android sources file" dialog (from the templates dialog).
	virtual void _android_install_build_template();

	/// Opens the Android build templates directory in the OS file browser.
	virtual void _android_explore_build_templates();

private:
	/// Slot for the one-shot `filesystem_changed` connection made when the
	/// first scan starts; routes to the (virtual) `_execute_upgrades`.
	void _on_first_scan_completed();

	/// Slot for the ad-hoc `ConfirmationDialog` created by
	/// `immediate_confirmation_dialog()`: sets `immediate_dialog_confirmed`,
	/// which breaks that function's manual event loop and becomes its
	/// return value (true = OK pressed; false = dialog dismissed).
	void _immediate_dialog_confirmed();
};

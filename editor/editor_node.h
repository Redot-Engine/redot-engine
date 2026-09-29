/**************************************************************************/
/*  editor_node.h                                                         */
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
 * @file editor_node.h
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "core/object/script_language.h"
#include "core/templates/safe_refcount.h"
#include "editor/editor_core.h"
#include "editor/editor_data.h"
#include "editor/editor_progress.h"
#include "editor/editor_scene_manager.h"
#include "editor/editor_shell.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/editor_plugin_list.h"
#include "editor/plugins/editor_plugin_manager.h"
#include "editor/settings/editor_folding.h"

typedef void (*EditorNodeInitCallback)();
typedef void (*EditorPluginInitializeCallback)();
typedef bool (*EditorBuildCallback)();

class AcceptDialog;
class ColorPicker;
class ConfirmationDialog;
class Control;
class FileDialog;
class HBoxContainer;
class MenuBar;
class MenuButton;
class OptionButton;
class Panel;
class PanelContainer;
class RichTextLabel;
class SubViewport;
class TextureProgressBar;
class Translation;
class Tree;
class VBoxContainer;
class VSplitContainer;
class Window;

class AudioStreamImportSettingsDialog;
class AudioStreamPreviewGenerator;
class BackgroundProgress;
class DependencyEditor;
class DependencyErrorDialog;
class DockSplitContainer;
class DynamicFontImportSettingsDialog;
class EditorAbout;
class EditorBuildProfileManager;
class EditorBottomPanel;
class EditorDockManager;
class EditorExportPreset;
class EditorFeatureProfileManager;
class EditorFileDialog;
class EditorFolding;
class EditorLayoutsDialog;
class EditorLog;
class EditorMainScreen;
class EditorResourceConversionPlugin;
class EditorRunBar;
class EditorSceneTabs;
class EditorSelectionHistory;
class EditorTitleBar;
class ExportTemplateManager;
class FBXImporterManager;
class FileSystemDock;
class HistoryDock;
class OrphanResourcesDialog;
class ProgressDialog;
class ProjectExportDialog;
class ProjectSettingsEditor;
class SceneImportSettingsDialog;
class ProjectUpgradeTool;
class UIDViewer;
class EditorQuickOpenDialog;

#ifdef ANDROID_ENABLED
class TouchActionsPanel;
#endif

class EditorNode : public EditorShell {
	GDCLASS(EditorNode, EditorShell);

public:
	struct ExecuteThreadArgs {
		String path;
		List<String> args;
		String output;
		Thread execute_output_thread;
		Mutex execute_output_mutex;
		int exitcode = 0;
		SafeFlag done;
	};

private:
	friend class EditorSceneTabs;

	struct ExportDefer {
		String preset;
		String path;
		bool debug = false;
		bool pack_only = false;
		bool android_build_template = false;
		bool patch = false;
		Vector<String> patches;
	} export_defer;

	static EditorNode *singleton;

	HistoryDock *history_dock = nullptr;

	Control *gui_base = nullptr;
	VBoxContainer *main_vbox = nullptr;
	OptionButton *renderer = nullptr;

#ifdef ANDROID_ENABLED
	VBoxContainer *base_vbox = nullptr; ///< It only contains the title_bar and main_hbox.
	HBoxContainer *main_hbox = nullptr; ///< It only contains the touch_actions_panel and main_vbox.
	TouchActionsPanel *touch_actions_panel = nullptr;
	void _touch_actions_panel_mode_changed();
#endif

	ConfirmationDialog *video_restart_dialog = nullptr;

	int renderer_current = 0;
	String renderer_request;

	/// @name Split Containers
	/// @{
	DockSplitContainer *left_l_hsplit = nullptr;
	DockSplitContainer *left_l_vsplit = nullptr;
	DockSplitContainer *left_r_hsplit = nullptr;
	DockSplitContainer *left_r_vsplit = nullptr;
	DockSplitContainer *main_hsplit = nullptr;
	DockSplitContainer *right_hsplit = nullptr;
	DockSplitContainer *right_l_vsplit = nullptr;
	DockSplitContainer *right_r_vsplit = nullptr;
	DockSplitContainer *center_split = nullptr;
	/// @}
	/// @name Main Tabs
	/// @{
	EditorSceneTabs *scene_tabs = nullptr;
	/// @}

	bool exiting = false;
	bool dimmed = false;

	DisplayServer::WindowMode prev_mode = DisplayServer::WINDOW_MODE_MAXIMIZED;
	int old_split_ofs = 0;
	VSplitContainer *top_split = nullptr;
	Control *vp_base = nullptr;

	Label *project_title = nullptr;
	Control *left_menu_spacer = nullptr;
	Control *right_menu_spacer = nullptr;
	EditorTitleBar *title_bar = nullptr;
	EditorRunBar *project_run_bar = nullptr;
	HBoxContainer *right_menu_hb = nullptr;

	/// @name Spacers to center 2D / 3D / Script buttons.
	/// @{
	HBoxContainer *left_spacer = nullptr;
	Control *right_spacer = nullptr;
	/// @}

	Control *menu_btn_spacer = nullptr;
	MenuButton *main_menu_button = nullptr;
	MenuBar *main_menu_bar = nullptr;

	PopupMenu *apple_menu = nullptr;
	PopupMenu *file_menu = nullptr;
	PopupMenu *project_menu = nullptr;
	PopupMenu *debug_menu = nullptr;
	PopupMenu *settings_menu = nullptr;
	PopupMenu *help_menu = nullptr;
	PopupMenu *tool_menu = nullptr;
	PopupMenu *export_as_menu = nullptr;
	Button *export_button = nullptr;
	Button *search_button = nullptr;
	TextureProgressBar *audio_vu = nullptr;

	Timer *screenshot_timer = nullptr;

	RichTextLabel *load_errors = nullptr;
	AcceptDialog *load_error_dialog = nullptr;
	bool load_errors_queued_to_display = false;

	RichTextLabel *execute_outputs = nullptr;
	AcceptDialog *execute_output_dialog = nullptr;

	Timer *system_theme_timer = nullptr;
	bool follow_system_theme = false;
	bool use_system_accent_color = false;
	bool last_dark_mode_state = false;
	Color last_system_base_color = Color(0, 0, 0, 0);
	Color last_system_accent_color = Color(0, 0, 0, 0);

	PopupMenu *recent_scenes = nullptr;
	String _recent_scene;

	ConfirmationDialog *confirmation = nullptr;
	bool stop_project_confirmation = false;
	Button *confirmation_button = nullptr;
	ConfirmationDialog *save_confirmation = nullptr;
	ConfirmationDialog *import_confirmation = nullptr;
	ConfirmationDialog *pick_main_scene = nullptr;
	ConfirmationDialog *open_project_settings = nullptr;
	Button *select_current_scene_button = nullptr;
	AcceptDialog *accept = nullptr;
	AcceptDialog *save_accept = nullptr;
	EditorAbout *about = nullptr;
	AcceptDialog *warning = nullptr;
	EditorPlugin *plugin_to_save = nullptr;

	int overridden_default_layout = -1;
	Ref<ConfigFile> default_layout;
	PopupMenu *editor_layouts = nullptr;
	EditorLayoutsDialog *layout_dialog = nullptr;

	ConfirmationDialog *gradle_build_manage_templates = nullptr;
	ConfirmationDialog *install_android_build_template = nullptr;
	ConfirmationDialog *remove_android_build_template = nullptr;
	Label *install_android_build_template_message = nullptr;
	OptionButton *choose_android_export_profile = nullptr;
	Ref<EditorExportPreset> android_export_preset;

	PopupMenu *vcs_actions_menu = nullptr;
	EditorFileDialog *file = nullptr;
	ExportTemplateManager *export_template_manager = nullptr;
	EditorFeatureProfileManager *feature_profile_manager = nullptr;
	EditorBuildProfileManager *build_profile_manager = nullptr;
	EditorFileDialog *file_templates = nullptr;
	EditorFileDialog *file_export_lib = nullptr;
	EditorFileDialog *file_script = nullptr;
	EditorFileDialog *file_android_build_source = nullptr;
	EditorFileDialog *file_pack_zip = nullptr;
	String current_path;
	MenuButton *update_spinner = nullptr;

	EditorMainScreen *editor_main_screen = nullptr;

	AudioStreamPreviewGenerator *audio_preview_gen = nullptr;
	ProgressDialog *progress_dialog = nullptr;
	BackgroundProgress *progress_hb = nullptr;

	DependencyErrorDialog *dependency_error = nullptr;
	DependencyEditor *dependency_fixer = nullptr;
	OrphanResourcesDialog *orphan_resources = nullptr;
	ConfirmationDialog *open_imported = nullptr;
	Button *new_inherited_button = nullptr;

	EditorDockManager *editor_dock_manager = nullptr;
	Timer *editor_layout_save_delay_timer = nullptr;
	Timer *scan_changes_timer = nullptr;
	Button *distraction_free = nullptr;

	EditorBottomPanel *bottom_panel = nullptr;

	Tree *disk_changed_list = nullptr;
	LocalVector<String> disk_changed_scenes;
	bool disk_changed_project = false;
	ConfirmationDialog *disk_changed = nullptr;
	ConfirmationDialog *project_data_missing = nullptr;

	bool scene_distraction_free = false;
	bool script_distraction_free = false;

	bool cmdline_mode = false;
	bool immediate_dialog_confirmed = false;

	bool requested_first_scan = false;
	bool waiting_for_first_scan = true;
	bool load_editor_layout_done = false;

	HashSet<Ref<Translation>> tracked_translations;
	bool pending_translation_notification = false;

	int current_menu_option = 0;

	SubViewport *scene_root = nullptr; ///< Root of the scene being edited.

	uint64_t update_spinner_step_msec = 0;
	uint64_t update_spinner_step_frame = 0;
	int update_spinner_step = 0;

	EditorProgress *save_scene_progress = nullptr;

	DynamicFontImportSettingsDialog *fontdata_import_settings = nullptr;
	SceneImportSettingsDialog *scene_import_settings = nullptr;
	AudioStreamImportSettingsDialog *audio_stream_import_settings = nullptr;

	ProjectUpgradeTool *project_upgrade_tool = nullptr;
	bool run_project_upgrade_tool = false;

	UIDViewer *uid_viewer = nullptr;

	bool was_window_windowed_last = false;

	bool unfocused_low_processor_usage_mode_enabled = true;

	/// Returns true if the update spinner button should be shown (based on editor settings and whether the display mode uses it).
	bool _should_display_update_spinner() const;

	void _update_theme(bool p_skip_creation = false); ///< Rebuilds or refreshes the editor theme (e.g. after settings/system theme change).
	void _dialog_action(String p_file); ///< Handles the user confirming a file dialog; dispatches on the pending action.

	void _add_to_history(const Object *p_object, const String &p_property, bool p_inspector_only) override; ///< Records an edited object in the selection/edit history for back/forward nav.
	void _dialog_display_save_error(String p_file, Error p_error); ///< Shows a dialog explaining why saving the given file failed.
	void _dialog_display_load_error(String p_file, Error p_error); ///< Shows a dialog explaining why loading the given file failed.

	void _menu_option(int p_option); ///< Executes a menu option that has already passed any confirmation gate.
	void _menu_confirm_current(); ///< Re-runs the currently pending menu option as if the user confirmed it.
	void _menu_option_confirm(int p_option, bool p_confirmed) override; /// Entry point for a menu option. If it needs confirmation, shows a dialog first and defers to `_menu_option` once confirmed.

	void _android_build_source_selected(const String &p_file); ///< Handles the user picking an Android build source path in the file dialog.
	void _android_export_preset_selected(int p_index); /// Handles selection of which export preset the Android build template uses.
	void _android_install_build_template();
	void _android_explore_build_templates(); ///< Opens the Android build templates directory in the OS file browser.

	/// Schedules a screenshot of the editor window.  Simply calls `_screenshot()`, but this gets called from `_menu_option_confirm`
	/// when screenshot is selected, which starts a `Timer`, which has to have a 0-argument function.
	void _request_screenshot();
	void _screenshot(bool p_use_utc = false);
	void _save_screenshot(const String &p_path);
	/// Saves a screenshot captured from an embedded (sub-window) process region.
	void _save_screenshot_with_embedded_process(int64_t p_w, int64_t p_h, const String &p_emb_path, const Rect2i &p_rect, const String &p_path);

	void _check_system_theme_changed(); ///< Polls whether the OS light/dark theme or accent color changed, so the editor theme can follow it.

	void _tool_menu_option(int p_idx); ///< Handles a custom item added to the Project > Tools menu being triggered.
	void _export_as_menu_option(int p_idx); ///< Handles a selection in the "Export As..." menu (e.g. mesh library export).
	void _update_file_menu_opened(); ///< Refreshes the Scene/File menu contents each time it's opened (recents, enabled states, etc.).
	void _palette_quick_open_dialog(); ///< Opens the quick-open dialog bound to the command palette path.

	void _fs_changed(); ///< Handles filesystem-changed notifications; queues external-change scanning.

	void _sources_changed(bool p_exist); ///< Handles non-resource source files (e.g. scripts) appearing/disappearing.
	void _remove_lock_file(); ///< Deletes the editor's lock file on clean shutdown.  This is what's used to detect an improper shutdown and provide the option of recovery mode.

	void _node_renamed(); ///< Handles a scene tree node being renamed (updates titles/paths as needed).
	void _update_title() override; ///< Refreshes the window title (project name, current scene, unsaved marker).

	void _version_control_menu_option(int p_idx); ///< Handles a selection in the Version Control menu.
	/// Legacy: collapses the bottom panel by zeroing center_split's offset.
	/// Unused — bottom panel visibility is now handled by EditorBottomPanel.
	void _close_messages();
	void _show_messages(); ///< Legacy: restores the bottom panel to its previous split offset. Unused.
	void _titlebar_resized(); ///< Handles the title bar being resized (re-centers screen buttons, etc.).
	void _viewport_resized(); ///< Handles the main viewport area being resized.

	// void _save_all_scenes();

	void _cancel_confirmation(); ///< Cancels the pending generic confirmation dialog.
	void _prepare_save_confirmation_popup(); ///< Builds the "unsaved changes" confirmation dialog contents before showing it.

	void _inherit_request(String p_file); ///< Handles a request to create a new scene inheriting from the given file.
	void _instantiate_request(const Vector<String> &p_files); ///< Handles a request to instantiate scene files into the current scene.

	void _quick_opened(const String &p_file_path); ///< Handles a file being chosen in the Quick Open dialog.
	void _open_command_palette();

	void _project_run_started(); ///< Handles clearing output, etc... (optional) when running game
	void _project_run_stopped(); ///< Handles closing bottom panel, etc... when stopping game

	void _add_to_recent_scenes(const String &p_scene) override; ///< Adds a path to the recent-scenes list.
	void _update_recent_scenes(); ///< Rebuilds the Recent Scenes popup contents from the stored list.
	void _open_recent_scene(int p_idx); ///< Opens the recent scene at the given menu index.

	void _dropped_files(const Vector<String> &p_files); ///< Handles files being drag-and-dropped onto the editor window.
	void _add_dropped_files_recursive(const Vector<String> &p_files, String to_path); ///< Recursively imports/copies dropped files and folders to a target path.

	void _update_vsync_mode(); ///< Applies the configured V-Sync mode to the editor window.
	void _update_from_settings(); ///< Re-reads editor settings and applies anything that changed.

	void _renderer_selected(int); ///< Handles the renderer option button selection changing.
	void _update_renderer_color(); ///< Tints renderer menu entries (e.g. marks overridden renderers).
	void _add_renderer_entry(const String &p_renderer_name, bool p_mark_overridden); ///< Adds a renderer choice to the renderer dropdown.
	void _set_renderer_name_save_and_restart(); ///< Saves the chosen renderer and restarts the editor to apply it.

	void _exit_editor(int p_exit_code);

	/// Swallows all key events while the ProgressDialog is visible, so editor
	/// shortcuts can't fire during a blocking operation — faking modal-popup
	/// behavior. Input processing on this node is enabled by
	/// ProgressDialog::_popup and disabled when the dialog's last task ends;
	/// it does nothing otherwise. Mouse input is blocked separately by the dialog's full-screen container.
	virtual void input(const Ref<InputEvent> &p_event) override;
	virtual void shortcut_input(const Ref<InputEvent> &p_event) override; ///< Shortcut input hook (handles editor keyboard shortcuts).

	void _find_node_types(Node *p_node, int &count_2d, int &count_3d); ///< Counts 2D vs 3D node types under a node (used to pick the default main screen).
	void _save_scene_with_preview(String p_file, int p_idx = -1); ///< Saves a scene while rendering a preview thumbnail for it.
	void _close_save_scene_progress(); ///< Tears down the scene-save progress indicator after saving completes.

	bool _is_closing_editor() const override; ///< @return `true` while the editor is mid-shutdown/quit.
	void _restart_editor(bool p_goto_project_manager = false); ///< Internal restart implementation, optionally landing in the project manager.

	void _save_editor_layout_to_config();
	void _load_editor_layout_to_config();

	void _save_central_editor_layout_to_config(Ref<ConfigFile> p_config_file);
	void _load_central_editor_layout_from_config(Ref<ConfigFile> p_config_file);

	void _save_window_settings_to_config(Ref<ConfigFile> p_layout, const String &p_section);

	void _save_open_scenes_to_config(Ref<ConfigFile> p_layout);
	void _load_open_scenes_from_config(Ref<ConfigFile> p_layout);

	void _update_layouts_menu(); ///< Rebuilds the Layouts submenu with saved layout names.
	void _layout_menu_option(int p_id); ///< Handles a Layouts menu selection (save/delete/apply layout).

	void _toggle_distraction_free_mode(); ///< Toggles distraction-free mode (hides docks/panels).

	void _inherit_imported(const String &p_action); ///< Handles "new inherited scene" requests triggered from imported files.
	void _open_imported(); ///< Handles the open-imported-file confirmation dialog being accepted.

	void _update_update_spinner(); ///< Rebuilds the update spinner menu and applies its update mode.

	void _resources_changed(const Vector<String> &p_resources); ///< Handles resources changing on disk (reload prompts, live updates).
	void _scan_external_changes(); ///< Scans the filesystem for external modifications to open resources.
	void _reload_modified_scenes(); ///< Reloads open scenes that were modified outside the editor.
	void _reload_project_settings();
	void _resave_externally_modified_scenes(String p_str); ///< Re-saves scenes that were externally modified, after user confirmation.

	/// Applies the current feature profile to the UI: enables or disables the
	/// Node/FileSystem/Import/History docks and the 3D/Script/Game/AssetLib
	/// main-screen buttons based on which features the profile turns off.
	/// Called when the active profile changes; re-enables everything if unset.
	void _feature_profile_changed();
	/// @return `true` if the feature profile disables editors for the given class.
	bool _is_class_editor_disabled_by_feature_profile(const StringName &p_class);

	/// Handles custom actions added to the main-scene picker dialog.
	void _pick_main_scene_custom_action(const String &p_custom_action_name);

	/// Slot for the ad-hoc `ConfirmationDialog` created by `immediate_confirmation_dialog()`: sets `immediate_dialog_confirmed`,
	/// which breaks that function's manual event loop and becomes its
	/// return value (true = OK pressed; false = dialog dismissed).
	void _immediate_dialog_confirmed();

	void _begin_first_scan(); ///< Kicks off the initial filesystem scan at editor startup.

	/// Recursive function to inform nodes that an array of nodes have had their scene reimported.
	/// It will attempt to call a method named '_nodes_scene_reimported' on every node in the
	void _progress_dialog_visibility_changed(); ///< Open the io errors after the progress dialog is closed.
	void _load_error_dialog_visibility_changed(); ///< Clears load errors if changed to not visible.

	void _execute_upgrades(); ///< Runs pending project-upgrade steps queued for this session.

	bool _is_project_data_missing();

	void _update_main_menu_type(); ///< Updates the main menu's presentation (native menubar vs in-window).
	void _add_to_main_menu(const String &p_name, PopupMenu *p_menu); ///< Adds a PopupMenu to the main menu under a translated name.

protected:
	friend class FileSystemDock;

	static void _bind_methods();
	void _notification(int p_what);

public:
	/// @todo This is a very naive estimation, but we need something now. Will be reworked later.
	bool is_editor_ready() const override { return is_inside_tree() && !waiting_for_first_scan; } ///< @return `true` once the editor has finished its initial filesystem scan and is ready for use.

	static EditorNode *get_singleton() { return singleton; } ///< @return The global `EditorNode` singleton (the one running editor instance).

	EditorTitleBar *get_title_bar() override { return singleton->title_bar; } ///< @return The editor's top title bar widget.
	VSplitContainer *get_top_split() override { return singleton->top_split; } ///< @return The vertical split containing the main editor area and bottom panel.
	EditorBottomPanel *get_bottom_panel() override { return singleton->bottom_panel; } ///< @return The bottom panel container (Output, Debugger, etc.).
	EditorMainScreen *get_editor_main_screen() override { return singleton->editor_main_screen; } ///< @returns The widget managing the main screen buttons (2D, 3D, Script, ...).

	// `EditorShell` state accessors used by the core.
	EditorFileDialog *get_file_dialog() override { return file; } ///< @returns The general-purpose file dialog (open/save) driven by core resource operations.
	void set_current_menu_option(int p_option) override { current_menu_option = p_option; } ///< Sets the menu option executed when the pending file dialog is confirmed.
	int get_current_menu_option() override { return current_menu_option; } ///< @returns The menu option currently in progress (or -1 if none).
	DependencyErrorDialog *get_dependency_error_dialog() override { return dependency_error; } ///< @returns The dialog that displays broken-dependency errors for a loaded resource.
	DependencyEditor *get_dependency_fixer() override { return dependency_fixer; } ///< @returns The dependency editor (fixer) dialog.
	PopupMenu *get_file_menu() override { return file_menu; } ///< @returns The Scene/File menu (for enabling/disabling items).
	ConfirmationDialog *get_save_confirmation_dialog() override { return save_confirmation; } ///< @returns The "save before closing" confirmation dialog.
	ConfirmationDialog *get_confirmation_dialog() override { return confirmation; } ///< @returns The generic confirmation dialog.
	ConfirmationDialog *get_open_imported_dialog() override { return open_imported; } ///< @returns The "open imported scene" confirmation dialog.
	Button *get_new_inherited_button() override { return new_inherited_button; } ///< @returns The "new inherited scene" button in the open-imported dialog.

	void add_io_error(const String &p_error) override; ///< Queues an IO error message to be shown to the user in a dialog.
	void add_io_warning(const String &p_warning) override; ///< Queues an IO warning message to be shown to the user in a dialog.

	/// Shows a modal OK/Cancel dialog and blocks until the user answers;
	/// @return `true` if the user accepted.
	bool immediate_confirmation_dialog(const String &p_text, const String &p_ok_text = TTR("Ok"), const String &p_cancel_text = TTR("Cancel"), uint32_t p_wrap_width = 0) override;

	bool is_cmdline_mode() override; ///< @return `true` if the editor is running in headless/command-line mode.

	/// Triggers a menu action programmatically, as if the user clicked it;
	/// @param p_confirmed pretends a confirmation dialog was already accepted.
	void trigger_menu_option(int p_option, bool p_confirmed);

	void update_distraction_free_mode(); ///< Refreshes the UI according to the current distraction-free mode state.
	// NOTE - only applies to hopefully legacy UI - truly dynamic windows should supersede this.
	void set_distraction_free_mode(bool p_enter); /// Returns true if distraction-free mode is currently active.
	bool is_distraction_free_mode_enabled() const;

	void show_about() { _menu_option_confirm(HELP_ABOUT, false); } ///< Opens the About dialog.

	void edit_foreign_resource(const Ref<Resource> &p_resource) override; ///< Opens a resource that belongs to another scene/instance for editing in its foreign context.
	void _edit_current(bool p_skip_foreign = false, bool p_skip_inspector_update = false) override; ///< Re-triggers editing of the current object (pushes it to inspector and plugins).

	SubViewport *get_scene_root() override { return scene_root; } ///< @returns the viewport that is the root of the scene being edited.
	EditorSceneTabs *get_scene_tabs() override { return scene_tabs; } ///< @returns The widget managing the open scene tabs.
	EditorRunBar *get_project_run_bar() override { return project_run_bar; } ///< @returns The run bar (play/stop controls).

	/// Toggles low-processor-usage mode when the editor window loses focus, to save CPU/battery in the background.
	void set_unfocused_low_processor_usage_mode_enabled(bool p_enabled);

	int execute_and_show_output(const String &p_title, const String &p_path, const List<String> &p_arguments, bool p_close_on_ok, bool p_close_on_errors, String *r_output) override;

	void notify_all_debug_sessions_exited(); ///< Notifies the editor that all running debug sessions have ended.

	/// @return The process ID if the given PID is a child process spawned by the editor (e.g. a running game instance), or an invalid ID otherwise.
	OS::ProcessID has_child_process(OS::ProcessID p_pid) const;
	void stop_child_process(OS::ProcessID p_pid); ///< Stops a child process previously spawned by the editor.

	void show_accept(const String &p_text, const String &p_title); ///< Shows a simple informational "accept" dialog with the given text and title.
	void show_save_accept(const String &p_text, const String &p_title); ///< Shows an "accept" dialog styled for save-related messages.
	void show_warning(const String &p_text, const String &p_title = TTR("Warning!")); ///< Shows a warning dialog with the given text and title.

	void _copy_warning(const String &p_str); ///< Internal handler that surfaces a warning when a copy operation fails.

	/// Exports the project using the named export preset to the given path; supports debug builds, pack-only output, Android templates, and patches.
	Error export_preset(const String &p_preset, const String &p_path, bool p_debug, bool p_pack_only, bool p_android_build_template, bool p_patch, const Vector<String> &p_patches);

	Control *get_gui_base() override { return gui_base; } ///< @returns The root Control of the editor's GUI.

	bool close_scene(); ///< Closes the current scene tab; returns false if the user cancelled.

	void save_editor_layout_delayed(); ///< Schedules the editor dock layout to be saved shortly (debounced).

	void open_export_template_manager(); ///< Opens the export template management dialog.

	bool is_exiting() const { return exiting; } ///< @returns `true` if the editor is in the process of quitting.

	Dictionary drag_resource(const Ref<Resource> &p_res, Control *p_from); ///< Builds drag-and-drop data for dragging a resource from a control.
	Dictionary drag_files_and_dirs(const Vector<String> &p_paths, Control *p_from); ///< Builds drag-and-drop data for dragging files/folders from a control.

	void add_tool_menu_item(const String &p_name, const Callable &p_callback); ///< Adds a custom item to the Project > Tools menu that calls the given callable.
	void add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu); ///< Adds a custom item to the Project > Tools menu that calls the given callable.
	void remove_tool_menu_item(const String &p_name); ///< Removes an item previously added to the Tools menu.

	PopupMenu *get_export_as_menu(); ///< @return The "Export As..." popup menu (for mesh libraries, etc.).

	void restart_editor(bool p_goto_project_manager = false);

	void open_setting_override(const String &p_property) override; ///< Opens the Project Settings dialog focused on an override property.

	void dim_editor(bool p_dimming); ///< Dims or undims the editor UI (e.g. while a modal is open).
	bool is_editor_dimmed() const; ///< Returns true if the editor UI is currently dimmed.

	EditorNode();
	~EditorNode();

	bool ensure_main_scene(bool p_from_native); ///< Ensures a main scene is set (prompting the user if needed); @return `true` if the project can run.
	bool validate_custom_directory() override; ///< Checks the project's custom directory setting is valid.
};

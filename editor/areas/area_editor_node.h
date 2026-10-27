// editor/areas/area_editor_node.h
#pragma once

#include "core/templates/list.h"
#include "editor/editor_shell.h"

class AreaPane;
class AreaSplit;
class ColorRect;
class VBoxContainer;
class EditorMainScreen;
class EditorBottomPanel;
class EditorTitleBar;
class EditorSceneTabs;
class EditorRunBar;
class EditorFileDialog;
class DependencyErrorDialog;
class DependencyEditor;
class ConfirmationDialog;
class AcceptDialog;
class Button;
class PopupMenu;
class SubViewport;
class VSplitContainer;

/**
 * Area-based editor shell implementing the EditorShell interface.
 * Uses a drag-and-drop area layout system instead of the traditional
 * dock-based layout. Integrates with EditorCore through set_shell().
 */
class AreaEditorNode : public EditorShell {
	GDCLASS(AreaEditorNode, EditorShell);

	static AreaEditorNode *singleton;

	// ── Editor component members ──────────────────────────────────────

	VBoxContainer *main_vbox = nullptr;
	Control *layout_root = nullptr;
	EditorMainScreen *editor_main_screen = nullptr;
	EditorBottomPanel *bottom_panel = nullptr;
	EditorTitleBar *title_bar = nullptr;
	EditorSceneTabs *scene_tabs = nullptr;
	EditorRunBar *project_run_bar = nullptr;
	VSplitContainer *top_split = nullptr;
	SubViewport *scene_root = nullptr;
	AreaSplit *root_split = nullptr;

	// ── Dialogs and menus ─────────────────────────────────────────────

	AcceptDialog *load_error_dialog = nullptr;
	AcceptDialog *save_accept = nullptr;
	AcceptDialog *warning = nullptr;
	ConfirmationDialog *confirmation = nullptr;
	ConfirmationDialog *save_confirmation = nullptr;
	ConfirmationDialog *open_imported = nullptr;
	DependencyErrorDialog *dependency_error = nullptr;
	DependencyEditor *dependency_fixer = nullptr;
	EditorFileDialog *file_dialog = nullptr;
	Button *new_inherited_button = nullptr;
	PopupMenu *file_menu = nullptr;
	PopupMenu *export_as_menu = nullptr;
	PopupMenu *tool_menu = nullptr;

	// ── Drag and drop state ───────────────────────────────────────────

	AreaPane *drag_source = nullptr;
	Vector2 drag_press_pos;
	bool dragging = false;
	bool drag_is_move = false;

	// ── Hover/preview state ───────────────────────────────────────────

	AreaPane *hover_target = nullptr;
	bool hover_vertical = false;
	bool hover_before = false;
	ColorRect *preview_overlay = nullptr;

	// ── Shell state ───────────────────────────────────────────────────

	bool exiting = false;
	bool dimmed = false;
	bool cmdline_mode = false;
	bool is_ready = false;
	bool distraction_free = false;

	int current_menu_option = 0;

	AreaSplit *pending_offset_split = nullptr;
	int pending_offset_px = 0;

	/// Splits that need initial offset values set on first resize.
	List<AreaSplit *> splits_needing_offset;

	/// Test-harness counter used to title/color panes created by "+ Pane".
	int pane_count = 0;

protected:
	static void _bind_methods();
	void _notification(int p_what);

private:
	/// Collects every AreaPane descendant of p_node into r_list.
	void _collect_panes(Node *p_node, List<AreaPane *> &r_list);

	/// @return The deepest pane whose global rect contains p_global_pos.
	AreaPane *_find_hovered_pane(const Vector2 &p_global_pos);

	/// Determines drop zone based on mouse position relative to pane rect.
	static void _zone_for(const Rect2 &p_rect, const Vector2 &p_pos, bool &r_vertical, bool &r_before, bool &r_valid);

	/// Positions and shows preview overlay over the drop region.
	void _show_preview(AreaPane *p_target, bool p_vertical, bool p_before);

	/// Hides the preview overlay and clears hover target.
	void _hide_preview();

	/// Performs the actual drop, mutating the layout tree.
	void _commit_drop();

	/// Collects splits with fewer than two children for cleanup.
	void _collect_orphans(Node *p_node, List<AreaSplit *> &r_list);

	/// Removes orphaned splits after layout changes.
	void _cleanup_orphan_splits();

	/// Test harness: adds a new randomly-colored pane to the layout tree.
	void _add_test_pane();

public:
	static AreaEditorNode *get_singleton() { return singleton; }

	// ── Entry point for pane dragging ─────────────────────────────────

	void begin_pane_press(AreaPane *p_pane, const Vector2 &p_global_pos, bool p_is_header);
	void close_pane(AreaPane *p_pane);

	// ── EditorShell: Container access ─────────────────────────────────

	Control *get_gui_base() override;
	SubViewport *get_scene_root() override;
	EditorMainScreen *get_editor_main_screen() override;
	EditorBottomPanel *get_bottom_panel() override;
	EditorTitleBar *get_title_bar() override;
	EditorSceneTabs *get_scene_tabs() override;
	EditorRunBar *get_project_run_bar() override;
	VSplitContainer *get_top_split() override;

	// ── EditorShell: User feedback ────────────────────────────────────

	void show_warning(const String &p_text, const String &p_title = TTR("Warning!")) override;
	void show_accept(const String &p_text, const String &p_title) override;
	void add_io_error(const String &p_error) override;
	void add_io_warning(const String &p_warning) override;
	bool immediate_confirmation_dialog(const String &p_text, const String &p_ok_text = TTR("Ok"), const String &p_cancel_text = TTR("Cancel"), uint32_t p_wrap_width = 0) override;
	void _dialog_display_load_error(String p_file, Error p_error) override;
	void _dialog_display_save_error(String p_file, Error p_error) override;

	// ── EditorShell: Menu integration ─────────────────────────────────

	void add_tool_menu_item(const String &p_name, const Callable &p_callback) override;
	void add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu) override;
	void remove_tool_menu_item(const String &p_name) override;
	PopupMenu *get_export_as_menu() override;
	void trigger_menu_option(int p_option, bool p_confirmed) override;

	// ── EditorShell: State queries ────────────────────────────────────

	bool is_exiting() const override;
	bool is_editor_ready() const override;
	void dim_editor(bool p_dimming) override;
	bool is_editor_dimmed() const override;
	void set_distraction_free_mode(bool p_enter) override;
	void update_distraction_free_mode() override;
	bool is_distraction_free_mode_enabled() const override;
	bool is_cmdline_mode() override;
	void notify_all_debug_sessions_exited() override;
	OS::ProcessID has_child_process(OS::ProcessID p_pid) const override;
	void stop_child_process(OS::ProcessID p_pid) override;

	// ── EditorShell: State accessors used by core ─────────────────────

	EditorFileDialog *get_file_dialog() override;
	void set_current_menu_option(int p_option) override;
	int get_current_menu_option() override;
	DependencyErrorDialog *get_dependency_error_dialog() override;
	DependencyEditor *get_dependency_fixer() override;

	// ── EditorShell: State accessors used by scene manager ────────────

	PopupMenu *get_file_menu() override;
	ConfirmationDialog *get_save_confirmation_dialog() override;
	ConfirmationDialog *get_confirmation_dialog() override;
	ConfirmationDialog *get_open_imported_dialog() override;
	Button *get_new_inherited_button() override;

	// ── EditorShell: Run/debug ────────────────────────────────────────

	int execute_and_show_output(const String &p_title, const String &p_path, const List<String> &p_arguments, bool p_close_on_ok, bool p_close_on_errors, String *r_output) override;
	void restart_editor(bool p_goto_project_manager = false) override;
	void _restart_editor(bool p_goto_project_manager = false) override;
	void _exit_editor(int p_exit_code) override;
	void open_export_template_manager() override;
	bool validate_custom_directory() override;

	// ── EditorShell: Drag/drop ────────────────────────────────────────

	Dictionary drag_resource(const Ref<Resource> &p_res, Control *p_from) override;
	Dictionary drag_files_and_dirs(const Vector<String> &p_paths, Control *p_from) override;
	void save_editor_layout_delayed() override;

	// ── EditorShell: Scene-session UI hooks ───────────────────────────

	void _inherit_request(String p_file) override;
	void _instantiate_request(const Vector<String> &p_files) override;
	void _inherit_imported(const String &p_action) override;
	void _open_imported() override;
	bool _is_class_editor_disabled_by_feature_profile(const StringName &p_class) override;
	void _save_scene_with_preview(String p_file, int p_idx = -1) override;
	void _edit_current(bool p_skip_foreign = false, bool p_skip_inspector_update = false) override;
	void edit_foreign_resource(const Ref<Resource> &p_resource) override;
	void open_setting_override(const String &p_property) override;
	bool ensure_main_scene(bool p_from_native) override;
	bool close_scene() override;

	// ── EditorShell: Shell UI bookkeeping ─────────────────────────────

	void _update_title() override;
	void _menu_option_confirm(int p_option, bool p_confirmed) override;
	bool _is_closing_editor() const override;
	void _add_to_recent_scenes(const String &p_scene) override;
	void _add_to_history(const Object *p_object, const String &p_property, bool p_inspector_only) override;

	AreaEditorNode();
	~AreaEditorNode();
};
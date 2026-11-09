// editor/areas/area_editor_node.h
#pragma once

#include "core/templates/list.h"
#include "editor/editor_shell.h"

class AreaPane;
class AreaSplit;
class Button;
class ColorRect;
class ConfirmationDialog;
class Control;
class DependencyEditor;
class DependencyErrorDialog;
class EditorAbout;
class EditorBuildProfileManager;
class EditorBottomPanel;
class EditorFileDialog;
class EditorMainScreen;
class EditorRunBar;
class EditorSceneTabs;
class EditorTitleBar;
class ExportTemplateManager;
class MenuBar;
class OrphanResourcesDialog;
class PopupMenu;
class ProgressDialog;
class ProjectUpgradeTool;
class SubViewport;
class UIDViewer;
class VSplitContainer;
class VBoxContainer;

/**
 * Area-based editor shell implementing the EditorShell interface.
 * Uses a drag-and-drop area layout system instead of the traditional
 * dock-based layout. Integrates with EditorCore through set_shell().
 *
 * All functional behavior (menu dispatch, save/open flows, filesystem
 * scan reactions, dialogs, quit/restart) comes from `EditorShell`; this
 * class only provides the visual layout (title bar, menus, dialogs, and
 * the `AreaPane`/`AreaSplit` grid).
 */
class AreaEditorNode : public EditorShell {
	GDCLASS(AreaEditorNode, EditorShell);

	static AreaEditorNode *singleton;

	// ── Editor component members ──────────────────────────────────────

	VBoxContainer *main_vbox = nullptr;
	Control *layout_root = nullptr;
	MenuBar *main_menu_bar = nullptr;
	EditorMainScreen *editor_main_screen = nullptr;
	EditorBottomPanel *bottom_panel = nullptr;
	EditorTitleBar *title_bar = nullptr;
	EditorSceneTabs *scene_tabs = nullptr;
	EditorRunBar *project_run_bar = nullptr;
	VSplitContainer *top_split = nullptr;
	SubViewport *scene_root = nullptr;
	AreaSplit *root_split = nullptr;

	// ── Menus ─────────────────────────────────────────────────────────

	PopupMenu *file_menu = nullptr;
	PopupMenu *project_menu = nullptr;
	PopupMenu *debug_menu = nullptr;
	PopupMenu *settings_menu = nullptr;
	PopupMenu *help_menu = nullptr;
	PopupMenu *export_as_menu = nullptr;
	PopupMenu *tool_menu = nullptr;
	PopupMenu *recent_scenes = nullptr;

	// ── Dialogs ───────────────────────────────────────────────────────

	ConfirmationDialog *confirmation = nullptr;
	Button *confirmation_button = nullptr;
	ConfirmationDialog *save_confirmation = nullptr;
	ConfirmationDialog *open_imported = nullptr;
	Button *new_inherited_button = nullptr;
	ConfirmationDialog *pick_main_scene = nullptr;
	Button *select_current_scene_button = nullptr;

	EditorFileDialog *file = nullptr;
	EditorFileDialog *file_pack_zip = nullptr;
	EditorFileDialog *file_export_lib = nullptr;

	EditorAbout *about = nullptr;
	ExportTemplateManager *export_template_manager = nullptr;
	OrphanResourcesDialog *orphan_resources = nullptr;
	EditorBuildProfileManager *build_profile_manager = nullptr;
	UIDViewer *uid_viewer = nullptr;
	ProjectUpgradeTool *project_upgrade_tool = nullptr;

	DependencyErrorDialog *dependency_error = nullptr;
	DependencyEditor *dependency_fixer = nullptr;
	ProgressDialog *progress_dialog = nullptr;

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

	bool distraction_free = false;

	AreaSplit *pending_offset_split = nullptr;
	int pending_offset_px = 0;

	/// Initial offset for a layout split, applied once the split has a
	/// non-zero size. `from_end` offsets are measured from the far end
	/// of the split (i.e. they reserve `offset_px` for the second child).
	struct InitialSplitOffset {
		AreaSplit *split = nullptr;
		int offset_px = 0;
		bool from_end = false;
	};
	List<InitialSplitOffset> splits_needing_offset;

	/// Test-harness counter used to title/color panes created by "+ Pane".
	int pane_count = 0;

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _apply_theme_styles(const Ref<Theme> &p_theme) override;

private:
	/// Local handler for the "Export As..." submenu.
	void _export_as_menu_option(int p_idx);

	/// Local handler for the "Version Control" submenu.
	void _version_control_menu_option(int p_idx);

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

	Control *get_gui_base() const override;
	SubViewport *get_scene_root() const override;
	EditorMainScreen *get_editor_main_screen() const override;
	EditorBottomPanel *get_bottom_panel() const override;
	EditorTitleBar *get_title_bar() const override;
	EditorSceneTabs *get_scene_tabs() const override;
	EditorRunBar *get_project_run_bar() const override;
	VSplitContainer *get_top_split() const override;

	// ── EditorShell: Dialog / menu accessors ──────────────────────────

	EditorFileDialog *get_file_dialog() override;
	Button *get_confirmation_button() override;
	ConfirmationDialog *get_pick_main_scene_dialog() override;
	Button *get_select_current_scene_button() override;
	EditorAbout *get_about_dialog() override;
	ExportTemplateManager *get_export_template_manager() override;
	EditorFileDialog *get_pack_zip_file_dialog() override;
	EditorFileDialog *get_export_lib_file_dialog() override;
	OrphanResourcesDialog *get_orphan_resources_dialog() override;
	EditorBuildProfileManager *get_build_profile_manager() override;
	ProjectUpgradeTool *get_project_upgrade_tool() override;
	UIDViewer *get_uid_viewer() override;
	PopupMenu *get_recent_scenes_menu() override;
	PopupMenu *get_tool_menu() override;
	PopupMenu *get_export_as_menu() override;

	// ── EditorShell: State accessors used by core ─────────────────────

	DependencyErrorDialog *get_dependency_error_dialog() override;
	DependencyEditor *get_dependency_fixer() override;

	// ── EditorShell: State accessors used by scene manager ────────────

	PopupMenu *get_file_menu() override;
	ConfirmationDialog *get_save_confirmation_dialog() override;
	ConfirmationDialog *get_confirmation_dialog() override;
	ConfirmationDialog *get_open_imported_dialog() override;
	Button *get_new_inherited_button() override;

	// ── EditorShell: Distraction-free mode ────────────────────────────

	void set_distraction_free_mode(bool p_enter) override;
	void update_distraction_free_mode() override;
	bool is_distraction_free_mode_enabled() const override;

	// ── EditorShell: Run / debug ──────────────────────────────────────

	int execute_and_show_output(const String &p_title, const String &p_path, const List<String> &p_arguments, bool p_close_on_ok, bool p_close_on_errors, String *r_output) override;
	bool validate_custom_directory() override;

	// ── EditorShell: Scene-session UI hooks ───────────────────────────

	void _save_scene_with_preview(String p_file, int p_idx = -1) override;

	AreaEditorNode();
	~AreaEditorNode();
};

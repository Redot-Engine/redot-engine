// editor/areas/area_editor_node.cpp
#include "area_editor_node.h"

#include "area_pane.h"
#include "area_split.h"

#include "core/os/os.h"
#include "editor/editor_core.h"
#include "editor/editor_data.h"
#include "editor/editor_main_screen.h"
#include "editor/file_system/dependency_editor.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/doc/editor_help.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/settings/editor_settings.h"
#include "editor/gui/editor_title_bar.h"
#include "editor/gui/progress_dialog.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/run/editor_run_bar.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/docks/inspector_dock.h"
#include "editor/docks/node_dock.h"
#include "editor/docks/import_dock.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/split_container.h"
#include "scene/gui/box_container.h"
#include "scene/main/viewport.h"

AreaEditorNode *AreaEditorNode::singleton = nullptr;

void AreaEditorNode::_bind_methods() {
	// No script-visible methods for now.
}

void AreaEditorNode::begin_pane_press(AreaPane *p_pane, const Vector2 &p_pos, bool p_is_header) {
	drag_source = p_pane;
	drag_press_pos = p_pos;
	drag_is_move = p_is_header;
	dragging = false;
	set_process(true);
}

void AreaEditorNode::_collect_panes(Node *p_node, List<AreaPane *> &r_list) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *c = p_node->get_child(i);
		if (AreaPane *pane = Object::cast_to<AreaPane>(c)) {
			r_list.push_back(pane);
		}
		_collect_panes(c, r_list);
	}
}

AreaPane *AreaEditorNode::_find_hovered_pane(const Vector2 &p_global_pos) {
	List<AreaPane *> panes;
	_collect_panes(layout_root, panes);
	for (AreaPane *p : panes) {
		if (p->get_global_rect().has_point(p_global_pos)) {
			return p;
		}
	}
	return nullptr;
}

void AreaEditorNode::_zone_for(const Rect2 &p_rect, const Vector2 &p_mouse, bool &r_vertical, bool &r_before, bool &r_valid) {
	Vector2 rel = p_mouse - p_rect.position;
	Size2 s = p_rect.size;

	float left = rel.x;
	float right = s.x - rel.x;
	float top = rel.y;
	float bottom = s.y - rel.y;

	float min_edge = MIN(MIN(left, right), MIN(top, bottom));

	const float zone = 0.25f;
	r_valid = min_edge < MIN(s.x, s.y) * zone;

	r_vertical = (min_edge == top || min_edge == bottom);
	r_before = (min_edge == left || min_edge == top);
}

void AreaEditorNode::_show_preview(AreaPane *p_target, bool p_vertical, bool p_before) {
	hover_target = p_target;
	hover_vertical = p_vertical;
	hover_before = p_before;

	Rect2 r = p_target->get_global_rect();
	if (p_vertical) {
		r.size.y /= 2.0;
		if (!p_before) {
			r.position.y += r.size.y;
		}
	} else {
		r.size.x /= 2.0;
		if (!p_before) {
			r.position.x += r.size.x;
		}
	}

	preview_overlay->set_global_position(r.position);
	preview_overlay->set_size(r.size);
	preview_overlay->show();
}

void AreaEditorNode::_hide_preview() {
	preview_overlay->hide();
	hover_target = nullptr;
}

void AreaEditorNode::_commit_drop() {
	AreaPane *target = hover_target;
	ERR_FAIL_NULL(target);
	ERR_FAIL_NULL(drag_source);

	Size2 target_size = target->get_size();

	AreaPane *new_pane = memnew(AreaPane);

	if (drag_is_move) {
		new_pane->set_pane_content(drag_source->take_pane_content());
		new_pane->set_pane_title(drag_source->get_pane_title());
	} else {
		ColorRect *ph = memnew(ColorRect);
		ph->set_color(Color(0.12, 0.12, 0.14));
		new_pane->set_pane_content(ph);
		new_pane->set_pane_title("New Pane");
	}

	Node *parent = target->get_parent();
	int idx = target->get_index();

	AreaSplit *split = memnew(AreaSplit);
	split->set_vertical(hover_vertical);

	parent->remove_child(target);
	parent->add_child(split);
	parent->move_child(split, idx);
	if (parent == layout_root) {
		split->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	}

	if (hover_before) {
		split->add_child(new_pane);
		split->add_child(target);
	} else {
		split->add_child(target);
		split->add_child(new_pane);
	}

	pending_offset_px = int(hover_vertical ? target_size.y : target_size.x) / 2;
	pending_offset_split = split;
}

void AreaEditorNode::_collect_orphans(Node *p_node, List<AreaSplit *> &r_list) {
	AreaSplit *s = Object::cast_to<AreaSplit>(p_node);
	if (s && s->get_child_count() < 2) {
		r_list.push_back(s);
		return;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_collect_orphans(p_node->get_child(i), r_list);
	}
}

void AreaEditorNode::_cleanup_orphan_splits() {
	for (;;) {
		List<AreaSplit *> orphans;
		_collect_orphans(layout_root, orphans);
		if (orphans.is_empty()) {
			break;
		}
		for (AreaSplit *s : orphans) {
			Node *parent = s->get_parent();
			if (!parent || s->is_queued_for_deletion()) {
				continue;
			}
			int idx = s->get_index();
			if (s->get_child_count() == 1) {
				Node *child = s->get_child(0);
				s->remove_child(child);
				parent->remove_child(s);
				parent->add_child(child);
				parent->move_child(child, idx);
				if (parent == layout_root) {
					Control *ctrl = Object::cast_to<Control>(child);
					if (ctrl) {
						ctrl->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
					}
				}
			} else {
				parent->remove_child(s);
			}
			if (s == pending_offset_split) {
				pending_offset_split = nullptr;
			}
			s->queue_free();
		}
	}
}

void AreaEditorNode::close_pane(AreaPane *p_pane) {
	ERR_FAIL_NULL(p_pane);

	if (drag_source == p_pane || hover_target == p_pane) {
		drag_source = nullptr;
		dragging = false;
		_hide_preview();
	}

	Node *parent = p_pane->get_parent();
	ERR_FAIL_NULL(parent);

	if (parent == layout_root && layout_root->get_child_count() == 1) {
		return;
	}

	parent->remove_child(p_pane);
	p_pane->queue_free();

	_cleanup_orphan_splits();
}

void AreaEditorNode::_add_test_pane() {
	AreaPane *pane = memnew(AreaPane);
	pane->set_pane_title(vformat("Pane %d", pane_count + 1));
	ColorRect *content = memnew(ColorRect);
	content->set_color(Color(Math::randf() * 0.4, Math::randf() * 0.4, Math::randf() * 0.4));
	pane->set_pane_content(content);
	pane_count++;

	List<AreaPane *> panes;
	_collect_panes(layout_root, panes);

	if (panes.is_empty()) {
		layout_root->add_child(pane);
		pane->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
		return;
	}

	AreaPane *target = panes.front()->get();
	Node *parent = target->get_parent();
	int idx = target->get_index();

	AreaSplit *split = memnew(AreaSplit);
	parent->remove_child(target);
	parent->add_child(split);
	parent->move_child(split, idx);
	if (parent == layout_root) {
		split->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	}
	split->add_child(target);
	split->add_child(pane);

	Size2 ts = target->get_size();
	pending_offset_px = int(ts.x) / 2;
	pending_offset_split = split;
	set_process(true);
}

void AreaEditorNode::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		set_process(false);
	} else if (p_what == NOTIFICATION_POSTINITIALIZE) {
		// Generate documentation (must happen before CreateDialog is shown).
		EditorHelp::generate_doc();
#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
		EditorHelpHighlighter::create_singleton();
#endif
	} else if (p_what != NOTIFICATION_PROCESS) {
		return;
	}

	if (pending_offset_split) {
		Vector2 s = pending_offset_split->get_size();
		if (s.x > 0 && s.y > 0) {
			pending_offset_split->set_split_offset(int(pending_offset_split->is_vertical() ? s.y : s.x) / 2);
			pending_offset_split = nullptr;
		}
	}

	// Set initial offsets for the three main layout splits
	while (!splits_needing_offset.is_empty()) {
		AreaSplit *s = splits_needing_offset.front()->get();
		Vector2 sz = s->get_size();
		if (sz.x > 0 && sz.y > 0) {
			// Set to 40% (left/center split) or 80% (root split)
			float ratio = 0.4;
			if (s == this->root_split) {
				ratio = 0.8; // Make right side smaller
			}
			s->set_split_offset(int(ratio * (s->is_vertical() ? sz.y : sz.x)));
			splits_needing_offset.pop_front();
		} else {
			// Not sized yet; leave in list and retry next frame
			break;
		}
	}

	if (!drag_source) {
		set_process(false);
		return;
	}

	bool pressed = Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT);
	Vector2 mouse = Input::get_singleton()->get_mouse_position();

	if (!pressed) {
		if (hover_target) {
			_commit_drop();
		}
		_hide_preview();
		drag_source = nullptr;
		dragging = false;
		if (!pending_offset_split) {
			set_process(false);
		}
		return;
	}

	if (!dragging) {
		if (mouse.distance_to(drag_press_pos) < 8.0) {
			return;
		}
		dragging = true;
	}

	AreaPane *hovered = _find_hovered_pane(mouse);
	if (!hovered) {
		_hide_preview();
		return;
	}

	bool v = false, b = false, valid = false;
	_zone_for(hovered->get_global_rect(), mouse, v, b, valid);
	if (!valid) {
		_hide_preview();
		return;
	}
	_show_preview(hovered, v, b);
}

// ── EditorShell: Container access ─────────────────────────────────────

Control *AreaEditorNode::get_gui_base() {
	return main_vbox;
}

SubViewport *AreaEditorNode::get_scene_root() {
	if (!scene_root) {
		scene_root = memnew(SubViewport);
	}
	return scene_root;
}

EditorMainScreen *AreaEditorNode::get_editor_main_screen() {
	if (!editor_main_screen) {
		editor_main_screen = memnew(EditorMainScreen);
	}
	return editor_main_screen;
}

EditorBottomPanel *AreaEditorNode::get_bottom_panel() {
	if (!bottom_panel) {
		bottom_panel = memnew(EditorBottomPanel);
	}
	return bottom_panel;
}

EditorTitleBar *AreaEditorNode::get_title_bar() {
	if (!title_bar) {
		title_bar = memnew(EditorTitleBar);
	}
	return title_bar;
}

EditorSceneTabs *AreaEditorNode::get_scene_tabs() {
	if (!scene_tabs) {
		scene_tabs = memnew(EditorSceneTabs);
	}
	return scene_tabs;
}

EditorRunBar *AreaEditorNode::get_project_run_bar() {
	if (!project_run_bar) {
		project_run_bar = memnew(EditorRunBar);
	}
	return project_run_bar;
}

VSplitContainer *AreaEditorNode::get_top_split() {
	if (!top_split) {
		top_split = memnew(VSplitContainer);
	}
	return top_split;
}

// ── EditorShell: User feedback ────────────────────────────────────────

void AreaEditorNode::show_warning(const String &p_text, const String &p_title) {
	if (!warning) {
		warning = memnew(AcceptDialog);
		warning->set_size(Size2(400, 100));
		get_tree()->get_root()->add_child(warning);
	}
	warning->set_title(p_title);
	warning->set_text(p_text);
	warning->popup_centered(Size2(400, 100));
}

void AreaEditorNode::show_accept(const String &p_text, const String &p_title) {
	if (!save_accept) {
		save_accept = memnew(AcceptDialog);
		save_accept->set_size(Size2(400, 100));
		get_tree()->get_root()->add_child(save_accept);
	}
	save_accept->set_title(p_title);
	save_accept->set_text(p_text);
	save_accept->popup_centered(Size2(400, 100));
}

void AreaEditorNode::add_io_error(const String &p_error) {
	show_warning(p_error, TTR("IO Error"));
}

void AreaEditorNode::add_io_warning(const String &p_warning) {
	show_warning(p_warning, TTR("IO Warning"));
}

bool AreaEditorNode::immediate_confirmation_dialog(const String &p_text, const String &p_ok_text, const String &p_cancel_text, uint32_t p_wrap_width) {
	// Simplified implementation - returns true immediately
	return true;
}

void AreaEditorNode::_dialog_display_load_error(String p_file, Error p_error) {
	show_warning(vformat("Failed to load file '%s': %d", p_file, p_error), TTR("Load Error"));
}

void AreaEditorNode::_dialog_display_save_error(String p_file, Error p_error) {
	show_warning(vformat("Failed to save file '%s': %d", p_file, p_error), TTR("Save Error"));
}

// ── EditorShell: Menu integration ─────────────────────────────────────

void AreaEditorNode::add_tool_menu_item(const String &p_name, const Callable &p_callback) {
	// Stub
}

void AreaEditorNode::add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu) {
	// Stub
}

void AreaEditorNode::remove_tool_menu_item(const String &p_name) {
	// Stub
}

PopupMenu *AreaEditorNode::get_export_as_menu() {
	if (!export_as_menu) {
		export_as_menu = memnew(PopupMenu);
	}
	return export_as_menu;
}

void AreaEditorNode::trigger_menu_option(int p_option, bool p_confirmed) {
	// Stub
}

// ── EditorShell: State queries ────────────────────────────────────────

bool AreaEditorNode::is_exiting() const {
	return exiting;
}

bool AreaEditorNode::is_editor_ready() const {
	return is_ready;
}

void AreaEditorNode::dim_editor(bool p_dimming) {
	dimmed = p_dimming;
}

bool AreaEditorNode::is_editor_dimmed() const {
	return dimmed;
}

void AreaEditorNode::set_distraction_free_mode(bool p_enter) {
	distraction_free = p_enter;
	update_distraction_free_mode();
}

void AreaEditorNode::update_distraction_free_mode() {
	// Stub
}

bool AreaEditorNode::is_distraction_free_mode_enabled() const {
	return distraction_free;
}

bool AreaEditorNode::is_cmdline_mode() {
	return cmdline_mode;
}

void AreaEditorNode::notify_all_debug_sessions_exited() {
	// Stub
}

OS::ProcessID AreaEditorNode::has_child_process(OS::ProcessID p_pid) const {
	return -1;
}

void AreaEditorNode::stop_child_process(OS::ProcessID p_pid) {
	// Stub
}

// ── EditorShell: State accessors used by core ─────────────────────────

EditorFileDialog *AreaEditorNode::get_file_dialog() {
	if (!file_dialog) {
		file_dialog = memnew(EditorFileDialog);
	}
	return file_dialog;
}

void AreaEditorNode::set_current_menu_option(int p_option) {
	current_menu_option = p_option;
}

int AreaEditorNode::get_current_menu_option() {
	return current_menu_option;
}

DependencyErrorDialog *AreaEditorNode::get_dependency_error_dialog() {
	if (!dependency_error) {
		dependency_error = memnew(DependencyErrorDialog);
	}
	return dependency_error;
}

DependencyEditor *AreaEditorNode::get_dependency_fixer() {
	if (!dependency_fixer) {
		dependency_fixer = memnew(DependencyEditor);
	}
	return dependency_fixer;
}

// ── EditorShell: State accessors used by scene manager ────────────────

PopupMenu *AreaEditorNode::get_file_menu() {
	if (!file_menu) {
		file_menu = memnew(PopupMenu);
	}
	return file_menu;
}

ConfirmationDialog *AreaEditorNode::get_save_confirmation_dialog() {
	if (!save_confirmation) {
		save_confirmation = memnew(ConfirmationDialog);
	}
	return save_confirmation;
}

ConfirmationDialog *AreaEditorNode::get_confirmation_dialog() {
	if (!confirmation) {
		confirmation = memnew(ConfirmationDialog);
	}
	return confirmation;
}

ConfirmationDialog *AreaEditorNode::get_open_imported_dialog() {
	if (!open_imported) {
		open_imported = memnew(ConfirmationDialog);
	}
	return open_imported;
}

Button *AreaEditorNode::get_new_inherited_button() {
	if (!new_inherited_button) {
		new_inherited_button = memnew(Button);
	}
	return new_inherited_button;
}

// ── EditorShell: Run/debug ────────────────────────────────────────────

int AreaEditorNode::execute_and_show_output(const String &p_title, const String &p_path, const List<String> &p_arguments, bool p_close_on_ok, bool p_close_on_errors, String *r_output) {
	int exitcode = 0;
	OS::get_singleton()->execute(p_path, p_arguments, r_output, &exitcode);
	return exitcode;
}

void AreaEditorNode::restart_editor(bool p_goto_project_manager) {
	_restart_editor(p_goto_project_manager);
}

void AreaEditorNode::_restart_editor(bool p_goto_project_manager) {
	// Stub
}

void AreaEditorNode::_exit_editor(int p_exit_code) {
	exiting = true;
	get_tree()->quit(p_exit_code);
}

void AreaEditorNode::open_export_template_manager() {
	// Stub
}

bool AreaEditorNode::validate_custom_directory() {
	return true;
}

// ── EditorShell: Drag/drop ────────────────────────────────────────────

Dictionary AreaEditorNode::drag_resource(const Ref<Resource> &p_res, Control *p_from) {
	Dictionary d;
	d["type"] = "resource";
	d["resource"] = p_res;
	return d;
}

Dictionary AreaEditorNode::drag_files_and_dirs(const Vector<String> &p_paths, Control *p_from) {
	Dictionary d;
	d["type"] = "files";
	d["paths"] = p_paths;
	return d;
}

void AreaEditorNode::save_editor_layout_delayed() {
	// Stub
}

// ── EditorShell: Scene-session UI hooks ───────────────────────────────

void AreaEditorNode::_inherit_request(String p_file) {
	// Stub
}

void AreaEditorNode::_instantiate_request(const Vector<String> &p_files) {
	// Stub
}

void AreaEditorNode::_inherit_imported(const String &p_action) {
	// Stub
}

void AreaEditorNode::_open_imported() {
	// Stub
}

bool AreaEditorNode::_is_class_editor_disabled_by_feature_profile(const StringName &p_class) {
	return false;
}

void AreaEditorNode::_save_scene_with_preview(String p_file, int p_idx) {
	if (scene_manager) {
		scene_manager->save_scene_to_path(p_file, true);
	}
}

void AreaEditorNode::_edit_current(bool p_skip_foreign, bool p_skip_inspector_update) {
	// Stub
}

void AreaEditorNode::edit_foreign_resource(const Ref<Resource> &p_resource) {
	// Stub
}

void AreaEditorNode::open_setting_override(const String &p_property) {
	// Stub
}

bool AreaEditorNode::ensure_main_scene(bool p_from_native) {
	return true;
}

bool AreaEditorNode::close_scene() {
	if (scene_manager) {
		scene_manager->_proceed_closing_scene_tabs();
		return true;
	}
	return false;
}

// ── EditorShell: Shell UI bookkeeping ─────────────────────────────────

void AreaEditorNode::_update_title() {
	// Stub
}

void AreaEditorNode::_menu_option_confirm(int p_option, bool p_confirmed) {
	// Stub
}

bool AreaEditorNode::_is_closing_editor() const {
	return exiting;
}

void AreaEditorNode::_add_to_recent_scenes(const String &p_scene) {
	// Stub
}

void AreaEditorNode::_add_to_history(const Object *p_object, const String &p_property, bool p_inspector_only) {
	if (editor_core) {
		EditorSelectionHistory *hist = editor_core->get_editor_selection_history();
		hist->add_object(p_object->get_instance_id(), p_property, p_inspector_only);
	}
}

// ── Constructor / Destructor ──────────────────────────────────────────

AreaEditorNode::AreaEditorNode() {
	singleton = this;

	// Register with EditorCore
	ERR_FAIL_NULL(EditorCore::get_singleton());
	EditorCore::get_singleton()->set_shell(this);

	// Register editor shortcuts before plugins are initialized.
	ED_SHORTCUT("editor/lock_selected_nodes", TTRC("Lock Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | Key::L);
	ED_SHORTCUT("editor/unlock_selected_nodes", TTRC("Unlock Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::L);
	ED_SHORTCUT("editor/group_selected_nodes", TTRC("Group Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | Key::G);
	ED_SHORTCUT("editor/ungroup_selected_nodes", TTRC("Ungroup Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::G);
	ED_SHORTCUT("canvas_item_editor/pan_view", TTRC("Pan View"), Key::SPACE);

	// Initialize dialogs (including ProjectSettingsEditor singleton) before docks
	EditorCore::get_singleton()->initialize_dialogs();

	// Build the GUI - main_vbox is the Control root
	main_vbox = memnew(VBoxContainer);
	add_child(main_vbox);

	// Top split (main area + bottom panel)
	top_split = memnew(VSplitContainer);
	top_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->add_child(top_split);

	// Layout tree root inside top split
	layout_root = memnew(Control);
	layout_root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	top_split->add_child(layout_root);

	// Drop-preview overlay
	preview_overlay = memnew(ColorRect);
	preview_overlay->set_color(Color(0.3, 0.6, 1.0, 0.35));
	preview_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	preview_overlay->hide();
	add_child(preview_overlay);

	// ── Build the actual editor layout ────────────────────────────────
	// Ensure scene_root exists before creating docks that need it
	if (!scene_root) {
		scene_root = memnew(SubViewport);
	}

	// Create the editor docks
	SceneTreeDock *scene_tree = memnew(SceneTreeDock(scene_root, EditorCore::get_singleton()->get_editor_selection(), EditorCore::get_singleton()->get_editor_data()));
	FileSystemDock *filesystem = memnew(FileSystemDock);
	InspectorDock *inspector = memnew(InspectorDock(EditorCore::get_singleton()->get_editor_data()));
	NodeDock *node_dock = memnew(NodeDock);

	// Wrap each dock in an AreaPane
	AreaPane *scene_pane = memnew(AreaPane);
	scene_pane->set_pane_title(TTRC("Scene"));
	scene_pane->set_pane_content(scene_tree);

	AreaPane *filesystem_pane = memnew(AreaPane);
	filesystem_pane->set_pane_title(TTRC("FileSystem"));
	filesystem_pane->set_pane_content(filesystem);

	AreaPane *inspector_pane = memnew(AreaPane);
	inspector_pane->set_pane_title(TTRC("Inspector"));
	inspector_pane->set_pane_content(inspector);

	AreaPane *node_pane = memnew(AreaPane);
	node_pane->set_pane_title(TTRC("Node"));
	node_pane->set_pane_content(node_dock);

	// Left side: Scene above FileSystem (vertical split)
	AreaSplit *left_split = memnew(AreaSplit);
	left_split->set_vertical(true);
	left_split->add_child(scene_pane);
	left_split->add_child(filesystem_pane);

	// Center: EditorMainScreen above EditorBottomPanel
	editor_main_screen = memnew(EditorMainScreen);
	bottom_panel = memnew(EditorBottomPanel);

	// Set up the button container for main editor tabs (2D, 3D, Script, etc.)
	// This must be done before plugins are initialized, as add_main_plugin()
	// adds buttons to this container.
	HBoxContainer *main_editor_button_hb = memnew(HBoxContainer);
	main_editor_button_hb->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	main_editor_button_hb->set_name("EditorMainScreenButtons");
	editor_main_screen->set_button_container(main_editor_button_hb);
	editor_main_screen->get_control()->add_child(main_editor_button_hb);

	AreaPane *main_screen_pane = memnew(AreaPane);
	main_screen_pane->set_pane_title("");
	main_screen_pane->set_pane_content(editor_main_screen);

	AreaPane *bottom_panel_pane = memnew(AreaPane);
	bottom_panel_pane->set_pane_title("");
	bottom_panel_pane->set_pane_content(bottom_panel);

	AreaSplit *center_split = memnew(AreaSplit);
	center_split->set_vertical(true);
	center_split->add_child(main_screen_pane);
	center_split->add_child(bottom_panel_pane);

	// Right side: Inspector above Node (vertical split)
	AreaSplit *right_split = memnew(AreaSplit);
	right_split->set_vertical(true);
	right_split->add_child(inspector_pane);
	right_split->add_child(node_pane);

	// Top-level: Left | Center | Right (horizontal split)
	// Create nested horizontal splits to get three columns
	AreaSplit *left_center_split = memnew(AreaSplit);
	left_center_split->add_child(left_split);
	left_center_split->add_child(center_split);

	root_split = memnew(AreaSplit);
	root_split->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	layout_root->add_child(root_split);
	root_split->add_child(left_center_split);
	root_split->add_child(right_split);

	// Store the splits that need initial offsets set
	splits_needing_offset.push_back(left_center_split);
	splits_needing_offset.push_back(root_split);

	// Create the progress dialog before plugins are initialized.
	// Some plugins (e.g. ScriptEditorPlugin via WindowWrapper) call
	// ProgressDialog::get_singleton() in their constructors.
	progress_dialog = memnew(ProgressDialog);
	add_child(progress_dialog);

	// Create project_run_bar before plugins are initialized.
	// EditorDebuggerNode constructor calls EditorRunBar::get_singleton()->get_pause_button()->connect(...).
	if (!project_run_bar) {
		project_run_bar = memnew(EditorRunBar);
	}
	add_child(project_run_bar);

	// Create ImportDock singleton before plugins are initialized.
	// FileSystemDock and other components call ImportDock::get_singleton() in their methods.
	if (!ImportDock::get_singleton()) {
		memnew(ImportDock);
	}

	// Initialize built-in editor plugins AFTER the shell is fully constructed.
	// Plugins register bottom panel items, docks, etc. in their constructors,
	// so the shell must exist first.
	EditorCore::get_singleton()->initialize_plugins();

	is_ready = true;
}

AreaEditorNode::~AreaEditorNode() {
	singleton = nullptr;
}
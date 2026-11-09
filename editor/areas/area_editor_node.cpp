// editor/areas/area_editor_node.cpp
#include "area_editor_node.h"

#include "area_pane.h"
#include "area_split.h"

#include "core/config/project_settings.h"
#include "core/extension/gdextension.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"

#include "editor/doc/editor_help.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/docks/import_dock.h"
#include "editor/docks/inspector_dock.h"
#include "editor/docks/node_dock.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_core.h"
#include "editor/editor_data.h"
#include "editor/editor_log.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_scene_manager.h"
#include "editor/editor_string_names.h"
#include "editor/export/export_template_manager.h"
#include "editor/export/project_export.h"
#include "editor/file_system/dependency_editor.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/file_system/uid_viewer.h"
#include "editor/gui/editor_about.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_quick_open_dialog.h"
#include "editor/gui/editor_title_bar.h"
#include "editor/gui/progress_dialog.h"
#include "editor/import/3d/scene_import_settings.h"
#include "editor/import/audio_stream_import_settings.h"
#include "editor/import/dynamic_font_import_settings.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/project_upgrade/project_upgrade_tool.h"
#include "editor/run/editor_run_bar.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/shader/editor_native_shader_source_visualizer.h"
#include "editor/settings/editor_build_profile.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_feature_profile.h"
#include "editor/settings/editor_settings.h"
#include "editor/settings/editor_settings_dialog.h"
#include "editor/settings/project_settings_editor.h"
#include "editor/themes/editor_scale.h"
#include "editor/version_control/version_control_editor_plugin.h"

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/menu_bar.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/split_container.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/mesh_library.h"

#include "servers/display/display_server.h"
#include "servers/rendering/rendering_server.h"

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
	EditorShell::_notification(p_what);

	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			// Generate documentation (must happen before CreateDialog is shown).
			EditorHelp::generate_doc();
#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
			EditorHelpHighlighter::create_singleton();
#endif
		} break;

		case NOTIFICATION_ENTER_TREE: {
			set_process(true);
			// Theme has already been generated by EditorCore::initialize, so skip creation.
			_update_theme(true);
			_begin_first_scan();
		} break;

		case NOTIFICATION_READY: {
			RenderingServer::get_singleton()->viewport_set_disable_2d(get_scene_root()->get_viewport_rid(), true);
			if (DisplayServer::get_singleton()->window_can_draw()) {
				ProjectSettings::get_singleton()->save();
			}
			EditorFeatureProfileManager::get_singleton()->notify_changed();
		} break;

		case NOTIFICATION_PROCESS: {
			if (pending_offset_split) {
				Vector2 s = pending_offset_split->get_size();
				if (s.x > 0 && s.y > 0) {
					pending_offset_split->set_split_offset(int(pending_offset_split->is_vertical() ? s.y : s.x) / 2);
					pending_offset_split = nullptr;
				}
			}

			// Apply initial offsets to the main layout splits once they are sized.
			while (!splits_needing_offset.is_empty()) {
				const InitialSplitOffset &so = splits_needing_offset.front()->get();
				AreaSplit *s = so.split;
				Vector2 sz = s->get_size();
				if (sz.x > 0 && sz.y > 0) {
					int dim = int(s->is_vertical() ? sz.y : sz.x);
					s->set_split_offset(so.from_end ? dim - so.offset_px : so.offset_px);
					splits_needing_offset.pop_front();
				} else {
					// Not sized yet; leave in list and retry next frame.
					break;
				}
			}

			if (!drag_source && !pending_offset_split && splits_needing_offset.is_empty()) {
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
				if (!pending_offset_split && splits_needing_offset.is_empty()) {
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
		} break;

		default: {
		} break;
	}
}

void AreaEditorNode::_apply_theme_styles(const Ref<Theme> &p_theme) {
	main_vbox->add_theme_style_override(SceneStringName(panel), p_theme->get_stylebox(SNAME("Background"), EditorStringName(EditorStyles)));
	main_vbox->add_theme_constant_override("separation", p_theme->get_constant(SNAME("top_bar_separation"), EditorStringName(Editor)));
	editor_main_screen->add_theme_style_override(SceneStringName(panel), p_theme->get_stylebox(SNAME("Content"), EditorStringName(EditorStyles)));
	bottom_panel->add_theme_style_override(SceneStringName(panel), p_theme->get_stylebox(SNAME("BottomPanel"), EditorStringName(EditorStyles)));
}

void AreaEditorNode::_export_as_menu_option(int p_idx) {
	if (p_idx == 0) { // MeshLibrary
		current_menu_option = FILE_EXPORT_MESH_LIBRARY;

		if (!get_editor_data().get_edited_scene_root()) {
			show_accept(TTR("This operation can't be done without a scene."), TTR("OK"));
			return;
		}

		List<String> extensions;
		Ref<MeshLibrary> ml(memnew(MeshLibrary));
		ResourceSaver::get_recognized_extensions(ml, &extensions);
		file_export_lib->clear_filters();
		for (const String &E : extensions) {
			file_export_lib->add_filter("*." + E);
		}

		file_export_lib->set_title(TTR("Export Mesh Library"));
		file_export_lib->popup_file_dialog();
	} else { // Custom menu options added by plugins.
		if (export_as_menu->get_item_submenu(p_idx).is_empty()) { // If not a submenu
			Callable callback = export_as_menu->get_item_metadata(p_idx);
			Callable::CallError ce;
			Variant result;
			callback.callp(nullptr, 0, result, ce);

			if (ce.error != Callable::CallError::CALL_OK) {
				String err = Variant::get_callable_error_text(callback, nullptr, 0, ce);
				ERR_PRINT("Error calling function from export_as menu: " + err);
			}
		}
	}
}

void AreaEditorNode::_version_control_menu_option(int p_idx) {
	switch (VersionControlEditorPlugin::get_singleton()->get_version_control_actions_panel()->get_item_id(p_idx)) {
		case VCS_METADATA: {
			VersionControlEditorPlugin::get_singleton()->popup_vcs_metadata_dialog();
		} break;
		case VCS_SETTINGS: {
			VersionControlEditorPlugin::get_singleton()->popup_vcs_set_up_dialog(get_gui_base());
		} break;
	}
}

// ── EditorShell: Container access ─────────────────────────────────────

Control *AreaEditorNode::get_gui_base() const {
	return main_vbox;
}

SubViewport *AreaEditorNode::get_scene_root() const {
	return scene_root;
}

EditorMainScreen *AreaEditorNode::get_editor_main_screen() const {
	return editor_main_screen;
}

EditorBottomPanel *AreaEditorNode::get_bottom_panel() const {
	return bottom_panel;
}

EditorTitleBar *AreaEditorNode::get_title_bar() const {
	return title_bar;
}

EditorSceneTabs *AreaEditorNode::get_scene_tabs() const {
	return scene_tabs;
}

EditorRunBar *AreaEditorNode::get_project_run_bar() const {
	return project_run_bar;
}

VSplitContainer *AreaEditorNode::get_top_split() const {
	return top_split;
}

// ── EditorShell: Dialog / menu accessors ──────────────────────────────

EditorFileDialog *AreaEditorNode::get_file_dialog() {
	return file;
}

Button *AreaEditorNode::get_confirmation_button() {
	return confirmation_button;
}

ConfirmationDialog *AreaEditorNode::get_pick_main_scene_dialog() {
	return pick_main_scene;
}

Button *AreaEditorNode::get_select_current_scene_button() {
	return select_current_scene_button;
}

EditorAbout *AreaEditorNode::get_about_dialog() {
	return about;
}

ExportTemplateManager *AreaEditorNode::get_export_template_manager() {
	return export_template_manager;
}

EditorFileDialog *AreaEditorNode::get_pack_zip_file_dialog() {
	return file_pack_zip;
}

EditorFileDialog *AreaEditorNode::get_export_lib_file_dialog() {
	return file_export_lib;
}

OrphanResourcesDialog *AreaEditorNode::get_orphan_resources_dialog() {
	return orphan_resources;
}

EditorBuildProfileManager *AreaEditorNode::get_build_profile_manager() {
	return build_profile_manager;
}

ProjectUpgradeTool *AreaEditorNode::get_project_upgrade_tool() {
	return project_upgrade_tool;
}

UIDViewer *AreaEditorNode::get_uid_viewer() {
	return uid_viewer;
}

PopupMenu *AreaEditorNode::get_recent_scenes_menu() {
	return recent_scenes;
}

PopupMenu *AreaEditorNode::get_tool_menu() {
	return tool_menu;
}

PopupMenu *AreaEditorNode::get_export_as_menu() {
	return export_as_menu;
}

// ── EditorShell: State accessors used by core ─────────────────────────

DependencyErrorDialog *AreaEditorNode::get_dependency_error_dialog() {
	return dependency_error;
}

DependencyEditor *AreaEditorNode::get_dependency_fixer() {
	return dependency_fixer;
}

// ── EditorShell: State accessors used by scene manager ────────────────

PopupMenu *AreaEditorNode::get_file_menu() {
	return file_menu;
}

ConfirmationDialog *AreaEditorNode::get_save_confirmation_dialog() {
	return save_confirmation;
}

ConfirmationDialog *AreaEditorNode::get_confirmation_dialog() {
	return confirmation;
}

ConfirmationDialog *AreaEditorNode::get_open_imported_dialog() {
	return open_imported;
}

Button *AreaEditorNode::get_new_inherited_button() {
	return new_inherited_button;
}

// ── EditorShell: Distraction-free mode ────────────────────────────────

void AreaEditorNode::set_distraction_free_mode(bool p_enter) {
	distraction_free = p_enter;
	update_distraction_free_mode();
}

void AreaEditorNode::update_distraction_free_mode() {
	// The area shell has no chrome to hide in distraction-free mode.
}

bool AreaEditorNode::is_distraction_free_mode_enabled() const {
	return distraction_free;
}

// ── EditorShell: Run / debug ──────────────────────────────────────────

int AreaEditorNode::execute_and_show_output(const String &p_title, const String &p_path, const List<String> &p_arguments, bool p_close_on_ok, bool p_close_on_errors, String *r_output) {
	int exitcode = 0;
	OS::get_singleton()->execute(p_path, p_arguments, r_output, &exitcode);
	return exitcode;
}

bool AreaEditorNode::validate_custom_directory() {
	return true;
}

// ── EditorShell: Scene-session UI hooks ───────────────────────────────

void AreaEditorNode::_save_scene_with_preview(String p_file, int p_idx) {
	EditorSceneManager::get_singleton()->_save_scene(p_file, p_idx);
}

// ── Constructor / Destructor ──────────────────────────────────────────

AreaEditorNode::AreaEditorNode() {
	singleton = this;

	// Register this shell with the core; this also wires the shell's
	// back-pointers to the core, scene manager, and plugin manager.
	ERR_FAIL_NULL(EditorCore::get_singleton());
	EditorCore::get_singleton()->set_shell(this);

	// Detecting headless mode, that means the editor is running in command line.
	if (!DisplayServer::get_singleton()->window_can_draw()) {
		cmdline_mode = true;
	}

	// Register editor shortcuts before plugins are initialized.
	ED_SHORTCUT("editor/lock_selected_nodes", TTRC("Lock Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | Key::L);
	ED_SHORTCUT("editor/unlock_selected_nodes", TTRC("Unlock Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::L);
	ED_SHORTCUT("editor/group_selected_nodes", TTRC("Group Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | Key::G);
	ED_SHORTCUT("editor/ungroup_selected_nodes", TTRC("Ungroup Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::G);
	ED_SHORTCUT("canvas_item_editor/pan_view", TTRC("Pan View"), Key::SPACE);

	// Warm up the project upgrade tool as early as possible.
	project_upgrade_tool = memnew(ProjectUpgradeTool);
	bool run_project_upgrade_tool = EditorSettings::get_singleton()->get_project_metadata(project_upgrade_tool->META_PROJECT_UPGRADE_TOOL, project_upgrade_tool->META_RUN_ON_RESTART, false);
	if (run_project_upgrade_tool) {
		project_upgrade_tool->begin_upgrade();
	}

	// Initialize dialogs (including ProjectSettingsEditor singleton) before docks.
	EditorCore::get_singleton()->initialize_dialogs();

	// Build the GUI - main_vbox is the Control root (the GUI base).
	main_vbox = memnew(VBoxContainer);
	add_child(main_vbox);

	// ── Title bar + main menu bar ─────────────────────────────────────

	title_bar = memnew(EditorTitleBar);
	main_vbox->add_child(title_bar);

	main_menu_bar = memnew(MenuBar);
	main_menu_bar->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	main_menu_bar->set_theme_type_variation("MainMenuBar");
	main_menu_bar->set_switch_on_hover(true);
	title_bar->add_child(main_menu_bar);

	// Scene (file) menu.
	file_menu = memnew(PopupMenu);
	main_menu_bar->add_child(file_menu);
	file_menu->set_name(TTRC("Scene"));

	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/new_scene", TTRC("New Scene"), KeyModifierMask::CMD_OR_CTRL + Key::N), SCENE_NEW_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/new_inherited_scene", TTRC("New Inherited Scene..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::N), SCENE_NEW_INHERITED_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/open_scene", TTRC("Open Scene..."), KeyModifierMask::CMD_OR_CTRL + Key::O), SCENE_OPEN_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/reopen_closed_scene", TTRC("Reopen Closed Scene"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::T), SCENE_OPEN_PREV);

	recent_scenes = memnew(PopupMenu);
	recent_scenes->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	file_menu->add_submenu_node_item(TTRC("Open Recent"), recent_scenes, SCENE_OPEN_RECENT);
	recent_scenes->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_open_recent_scene));

	file_menu->add_separator();
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/save_scene", TTRC("Save Scene"), KeyModifierMask::CMD_OR_CTRL + Key::S), SCENE_SAVE_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/save_scene_as", TTRC("Save Scene As..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::S), SCENE_SAVE_AS_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/save_all_scenes", TTRC("Save All Scenes"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + KeyModifierMask::ALT + Key::S), SCENE_SAVE_ALL_SCENES);

	file_menu->add_separator();
	file_menu->add_shortcut(ED_SHORTCUT_ARRAY_AND_COMMAND("editor/quick_open", TTRC("Quick Open..."), { int32_t(KeyModifierMask::SHIFT + KeyModifierMask::ALT + Key::O), int32_t(KeyModifierMask::CMD_OR_CTRL + Key::P) }), SCENE_QUICK_OPEN);
	ED_SHORTCUT_OVERRIDE_ARRAY("editor/quick_open", "macos", { int32_t(KeyModifierMask::META + KeyModifierMask::CTRL + Key::O), int32_t(KeyModifierMask::CMD_OR_CTRL + Key::P) });
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/quick_open_scene", TTRC("Quick Open Scene..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::O), SCENE_QUICK_OPEN_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/quick_open_script", TTRC("Quick Open Script..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::ALT + Key::O), SCENE_QUICK_OPEN_SCRIPT);

	file_menu->add_separator();
	export_as_menu = memnew(PopupMenu);
	file_menu->add_submenu_node_item(TTRC("Export As..."), export_as_menu);
	export_as_menu->add_shortcut(ED_SHORTCUT("editor/export_as_mesh_library", TTRC("MeshLibrary...")), FILE_EXPORT_MESH_LIBRARY);
	export_as_menu->connect("index_pressed", callable_mp(this, &AreaEditorNode::_export_as_menu_option));

	file_menu->add_separator();
	file_menu->add_shortcut(ED_GET_SHORTCUT("ui_undo"), SCENE_UNDO, false, true);
	file_menu->add_shortcut(ED_GET_SHORTCUT("ui_redo"), SCENE_REDO, false, true);

	file_menu->add_separator();
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/reload_saved_scene", TTRC("Reload Saved Scene")), SCENE_RELOAD_SAVED_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/close_scene", TTRC("Close Scene"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::W), SCENE_CLOSE);
	ED_SHORTCUT_OVERRIDE("editor/close_scene", "macos", KeyModifierMask::CMD_OR_CTRL + Key::W);

	file_menu->add_separator();
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/file_quit", TTRC("Quit"), KeyModifierMask::CMD_OR_CTRL + Key::Q), SCENE_QUIT, true);

	file_menu->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_menu_option));
	file_menu->connect("about_to_popup", callable_mp((EditorShell *)this, &EditorShell::_update_file_menu_opened));

	// Project menu.
	project_menu = memnew(PopupMenu);
	main_menu_bar->add_child(project_menu);
	project_menu->set_name(TTRC("Project"));

	project_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/project_settings", TTRC("Project Settings..."), Key::NONE, TTRC("Project Settings")), PROJECT_OPEN_SETTINGS);
	project_menu->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_menu_option));

	ED_SHORTCUT_AND_COMMAND("editor/find_in_files", TTRC("Find in Files..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::F);
	project_menu->add_shortcut(ED_GET_SHORTCUT("editor/find_in_files"), PROJECT_FIND_IN_FILES);

	project_menu->add_separator();
	project_menu->add_item(TTRC("Version Control"), PROJECT_VERSION_CONTROL);

	PopupMenu *vcs_actions_menu = VersionControlEditorPlugin::get_singleton()->get_version_control_actions_panel();
	vcs_actions_menu->connect("index_pressed", callable_mp(this, &AreaEditorNode::_version_control_menu_option));
	vcs_actions_menu->add_item(TTRC("Create/Override Version Control Metadata..."), VCS_METADATA);
	vcs_actions_menu->add_item(TTRC("Version Control Settings..."), VCS_SETTINGS);
	project_menu->set_item_submenu_node(project_menu->get_item_index(PROJECT_VERSION_CONTROL), vcs_actions_menu);

	project_menu->add_separator();
	project_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/export", TTRC("Export..."), Key::NONE, TTRC("Export")), PROJECT_EXPORT);
	project_menu->add_item(TTRC("Pack Project as ZIP..."), PROJECT_PACK_AS_ZIP);
	project_menu->add_item(TTRC("Open User Data Folder"), PROJECT_OPEN_USER_DATA_FOLDER);

	project_menu->add_separator();

	tool_menu = memnew(PopupMenu);
	tool_menu->connect("index_pressed", callable_mp((EditorShell *)this, &EditorShell::_tool_menu_option));
	project_menu->add_submenu_node_item(TTRC("Tools"), tool_menu);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/orphan_resource_explorer", TTRC("Orphan Resource Explorer...")), TOOLS_ORPHAN_RESOURCES);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/engine_compilation_configuration_editor", TTRC("Engine Compilation Configuration Editor...")), TOOLS_BUILD_PROFILE_MANAGER);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/upgrade_project", TTRC("Upgrade Project Files...")), TOOLS_PROJECT_UPGRADE);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/uid_viewer", TTRC("UID Viewer...")), TOOLS_UID_VIEWER);

	project_menu->add_separator();
	project_menu->add_shortcut(ED_SHORTCUT("editor/reload_current_project", TTRC("Reload Current Project")), PROJECT_RELOAD_CURRENT_PROJECT);
	ED_SHORTCUT_AND_COMMAND("editor/quit_to_project_list", TTRC("Quit to Project List"), KeyModifierMask::CTRL + KeyModifierMask::SHIFT + Key::Q);
	ED_SHORTCUT_OVERRIDE("editor/quit_to_project_list", "macos", KeyModifierMask::META + KeyModifierMask::CTRL + KeyModifierMask::ALT + Key::Q);
	project_menu->add_shortcut(ED_GET_SHORTCUT("editor/quit_to_project_list"), PROJECT_QUIT_TO_PROJECT_MANAGER, true);

	// Debug menu (options are added by DebuggerEditorPlugin).
	debug_menu = memnew(PopupMenu);
	main_menu_bar->add_child(debug_menu);
	debug_menu->set_name(TTRC("Debug"));

	// Editor (settings) menu.
	settings_menu = memnew(PopupMenu);
	main_menu_bar->add_child(settings_menu);
	settings_menu->set_name(TTRC("Editor"));

	ED_SHORTCUT_AND_COMMAND("editor/editor_settings", TTRC("Editor Settings..."));
	ED_SHORTCUT_OVERRIDE("editor/editor_settings", "macos", KeyModifierMask::META + Key::COMMA);
	settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/editor_settings"), EDITOR_OPEN_SETTINGS);
	settings_menu->add_shortcut(ED_SHORTCUT("editor/command_palette", TTRC("Command Palette..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::P), EDITOR_COMMAND_PALETTE);
	settings_menu->add_separator();

	ED_SHORTCUT_AND_COMMAND("editor/take_screenshot", TTRC("Take Screenshot"), KeyModifierMask::CTRL | Key::F12);
	ED_SHORTCUT_OVERRIDE("editor/take_screenshot", "macos", KeyModifierMask::META | Key::F12);
	settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/take_screenshot"), EDITOR_TAKE_SCREENSHOT);
	settings_menu->set_item_tooltip(-1, TTRC("Screenshots are stored in the user data folder (\"user://\")."));

	ED_SHORTCUT_AND_COMMAND("editor/fullscreen_mode", TTRC("Toggle Fullscreen"), KeyModifierMask::SHIFT | Key::F11);
	ED_SHORTCUT_OVERRIDE("editor/fullscreen_mode", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::F);
	settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/fullscreen_mode"), EDITOR_TOGGLE_FULLSCREEN);
	settings_menu->add_separator();

	if (OS::get_singleton()->get_data_path() == OS::get_singleton()->get_config_path()) {
		// Configuration and data folders are located in the same place (Windows/macOS).
		settings_menu->add_item(TTRC("Open Editor Data/Settings Folder"), EDITOR_OPEN_DATA_FOLDER);
	} else {
		// Separate configuration and data folders (Linux).
		settings_menu->add_item(TTRC("Open Editor Data Folder"), EDITOR_OPEN_DATA_FOLDER);
		settings_menu->add_item(TTRC("Open Editor Settings Folder"), EDITOR_OPEN_CONFIG_FOLDER);
	}
	settings_menu->add_separator();

	settings_menu->add_item(TTRC("Manage Editor Features..."), EDITOR_MANAGE_FEATURE_PROFILES);
	settings_menu->add_item(TTRC("Manage Export Templates..."), EDITOR_MANAGE_EXPORT_TEMPLATES);
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	settings_menu->add_item(TTRC("Configure FBX Importer..."), EDITOR_CONFIGURE_FBX_IMPORTER);
#endif

	settings_menu->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_menu_option));

	// Help menu.
	help_menu = memnew(PopupMenu);
	main_menu_bar->add_child(help_menu);
	help_menu->set_name(TTRC("Help"));

	help_menu->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_menu_option));

	ED_SHORTCUT_AND_COMMAND("editor/editor_help", TTRC("Search Help..."), Key::F1);
	ED_SHORTCUT_OVERRIDE("editor/editor_help", "macos", KeyModifierMask::ALT | Key::SPACE);
	help_menu->add_shortcut(ED_GET_SHORTCUT("editor/editor_help"), HELP_SEARCH);
	help_menu->add_separator();
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/online_docs", TTRC("Online Documentation")), HELP_DOCS);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/forum", TTRC("Forum")), HELP_FORUM);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/community", TTRC("Community")), HELP_COMMUNITY);
	help_menu->add_separator();
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/copy_system_info", TTRC("Copy System Info")), HELP_COPY_SYSTEM_INFO);
	help_menu->set_item_tooltip(-1, TTR("Copies the system info as a single-line text into the clipboard."));
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/report_a_bug", TTRC("Report a Bug")), HELP_REPORT_A_BUG);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/suggest_a_feature", TTRC("Suggest a Feature")), HELP_SUGGEST_A_FEATURE);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/send_docs_feedback", TTRC("Send Docs Feedback")), HELP_SEND_DOCS_FEEDBACK);
	help_menu->add_separator();
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/about", TTRC("About Engine...")), HELP_ABOUT);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/support_development", TTRC("Support Engine Development")), HELP_SUPPORT_REDOT_DEVELOPMENT);

	// Center column widgets, created before the title-bar button container
	// because the button container belongs to the main screen.
	editor_main_screen = memnew(EditorMainScreen);
	bottom_panel = memnew(EditorBottomPanel);

	// Spacer to center 2D / 3D / Script buttons.
	HBoxContainer *left_spacer = memnew(HBoxContainer);
	left_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	left_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_bar->add_child(left_spacer);

	// Main editor (2D / 3D / Script, ...) buttons. Plugins add their
	// buttons to this container, so it must exist before initialize_plugins().
	HBoxContainer *main_editor_button_hb = memnew(HBoxContainer);
	main_editor_button_hb->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	main_editor_button_hb->set_name("EditorMainScreenButtons");
	editor_main_screen->set_button_container(main_editor_button_hb);
	title_bar->add_child(main_editor_button_hb);
	title_bar->set_center_control(main_editor_button_hb);

	// Spacer to center 2D / 3D / Script buttons.
	HBoxContainer *right_spacer = memnew(HBoxContainer);
	right_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	right_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_bar->add_child(right_spacer);

	// Run bar (play/stop) lives in the title bar. It must exist before
	// plugins are initialized: the EditorDebuggerNode constructor calls
	// EditorRunBar::get_singleton().
	project_run_bar = memnew(EditorRunBar);
	project_run_bar->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	title_bar->add_child(project_run_bar);
	project_run_bar->connect("play_pressed", callable_mp((EditorShell *)this, &EditorShell::_project_run_started));
	project_run_bar->connect("stop_pressed", callable_mp((EditorShell *)this, &EditorShell::_project_run_stopped));

	// ── Main layout: top split (area grid + bottom panel) ─────────────

	top_split = memnew(VSplitContainer);
	top_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->add_child(top_split);

	// Layout tree root inside the top split.
	layout_root = memnew(Control);
	layout_root->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	top_split->add_child(layout_root);

	// Scene tabs sit above the main screen in the center column.
	scene_tabs = memnew(EditorSceneTabs);
	scene_tabs->connect("tab_changed", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_set_current_scene));
	scene_tabs->connect("tab_closed", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_scene_tab_closed));

	// Viewport that is the root of the scene being edited.
	scene_root = memnew(SubViewport);
	scene_root->set_auto_translate_mode(AUTO_TRANSLATE_MODE_ALWAYS);
	scene_root->set_translation_domain(StringName());
	scene_root->set_embedding_subwindows(true);
	scene_root->set_disable_3d(true);
	scene_root->set_disable_input(true);
	scene_root->set_as_audio_listener_2d(true);

	// Create the editor docks.
	SceneTreeDock *scene_tree = memnew(SceneTreeDock(scene_root, EditorCore::get_singleton()->get_editor_selection(), get_editor_data()));
	FileSystemDock *filesystem = memnew(FileSystemDock);
	InspectorDock *inspector = memnew(InspectorDock(get_editor_data()));
	NodeDock *node_dock = memnew(NodeDock);

	// Wrap each dock in an AreaPane.
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

	// Left side: Scene above FileSystem (vertical split).
	AreaSplit *left_split = memnew(AreaSplit);
	left_split->set_vertical(true);
	left_split->add_child(scene_pane);
	left_split->add_child(filesystem_pane);

	// Center: main screen (with scene tabs above it) over bottom panel.
	VBoxContainer *center_vbox = memnew(VBoxContainer);
	center_vbox->add_theme_constant_override("separation", 0);
	center_vbox->add_child(scene_tabs);
	center_vbox->add_child(editor_main_screen);

	AreaPane *main_screen_pane = memnew(AreaPane);
	main_screen_pane->set_pane_title("");
	main_screen_pane->set_pane_content(center_vbox);

	AreaPane *bottom_panel_pane = memnew(AreaPane);
	bottom_panel_pane->set_pane_title("");
	bottom_panel_pane->set_pane_content(bottom_panel);

	AreaSplit *center_split = memnew(AreaSplit);
	center_split->set_vertical(true);
	center_split->add_child(main_screen_pane);
	center_split->add_child(bottom_panel_pane);

	// Right side: Inspector above Node (vertical split).
	AreaSplit *right_split = memnew(AreaSplit);
	right_split->set_vertical(true);
	right_split->add_child(inspector_pane);
	right_split->add_child(node_pane);

	// Top-level: Left | Center | Right (nested horizontal splits).
	AreaSplit *left_center_split = memnew(AreaSplit);
	left_center_split->add_child(left_split);
	left_center_split->add_child(center_split);

	root_split = memnew(AreaSplit);
	root_split->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	layout_root->add_child(root_split);
	root_split->add_child(left_center_split);
	root_split->add_child(right_split);

	// Initial split offsets (applied in NOTIFICATION_PROCESS once the
	// splits are sized): fixed-width side columns and a fixed-height
	// bottom panel.
	InitialSplitOffset root_offset;
	root_offset.split = root_split;
	root_offset.offset_px = 270 * EDSCALE;
	root_offset.from_end = true;
	splits_needing_offset.push_back(root_offset);

	InitialSplitOffset left_center_offset;
	left_center_offset.split = left_center_split;
	left_center_offset.offset_px = 270 * EDSCALE;
	splits_needing_offset.push_back(left_center_offset);

	InitialSplitOffset center_offset;
	center_offset.split = center_split;
	center_offset.offset_px = 150 * EDSCALE;
	center_offset.from_end = true;
	splits_needing_offset.push_back(center_offset);

	// Drop-preview overlay.
	preview_overlay = memnew(ColorRect);
	preview_overlay->set_color(Color(0.3, 0.6, 1.0, 0.35));
	preview_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
	preview_overlay->hide();
	add_child(preview_overlay);

	// ── Dialogs (parented to the GUI base) ────────────────────────────

	file = memnew(EditorFileDialog);
	main_vbox->add_child(file);
	file->set_current_dir("res://");
	file->set_transient_to_focused(true);
	file->connect("file_selected", callable_mp((EditorShell *)this, &EditorShell::_dialog_action));

	file_pack_zip = memnew(EditorFileDialog);
	file_pack_zip->connect("file_selected", callable_mp((EditorShell *)this, &EditorShell::_dialog_action));
	file_pack_zip->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_pack_zip->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_pack_zip->add_filter("*.zip", "ZIP Archive");
	file_pack_zip->set_title(TTR("Pack Project as ZIP..."));
	main_vbox->add_child(file_pack_zip);

	file_export_lib = memnew(EditorFileDialog);
	file_export_lib->set_title(TTR("Export Library"));
	file_export_lib->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_export_lib->connect("file_selected", callable_mp((EditorShell *)this, &EditorShell::_dialog_action));
	file_export_lib->add_option(TTR("Merge With Existing"), Vector<String>(), true);
	file_export_lib->add_option(TTR("Apply MeshInstance Transforms"), Vector<String>(), false);
	main_vbox->add_child(file_export_lib);

	confirmation = memnew(ConfirmationDialog);
	confirmation_button = confirmation->add_button(TTRC("Don't Save"), DisplayServer::get_singleton()->get_swap_cancel_ok(), "discard");
	main_vbox->add_child(confirmation);
	confirmation->set_min_size(Vector2(450.0 * EDSCALE, 0));
	confirmation->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_menu_confirm_current));
	confirmation->connect("custom_action", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_discard_changes));
	confirmation->connect("canceled", callable_mp((EditorShell *)this, &EditorShell::_cancel_confirmation));

	save_confirmation = memnew(ConfirmationDialog);
	save_confirmation->add_button(TTRC("Don't Save"), DisplayServer::get_singleton()->get_swap_cancel_ok(), "discard");
	main_vbox->add_child(save_confirmation);
	save_confirmation->set_min_size(Vector2(450.0 * EDSCALE, 0));
	save_confirmation->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_menu_confirm_current));
	save_confirmation->connect("custom_action", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_discard_changes));
	save_confirmation->connect("canceled", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_cancel_close_scene_tab));
	save_confirmation->connect("about_to_popup", callable_mp((EditorShell *)this, &EditorShell::_prepare_save_confirmation_popup));

	open_imported = memnew(ConfirmationDialog);
	open_imported->set_ok_button_text(TTR("Open Anyway"));
	new_inherited_button = open_imported->add_button(TTR("New Inherited"), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "inherit");
	open_imported->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_open_imported));
	open_imported->connect("custom_action", callable_mp((EditorShell *)this, &EditorShell::_inherit_imported));
	main_vbox->add_child(open_imported);

	pick_main_scene = memnew(ConfirmationDialog);
	main_vbox->add_child(pick_main_scene);
	pick_main_scene->set_ok_button_text(TTR("Select"));
	pick_main_scene->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_menu_option).bind(SETTINGS_PICK_MAIN_SCENE));
	select_current_scene_button = pick_main_scene->add_button(TTR("Select Current"), true, "select_current");
	pick_main_scene->connect("custom_action", callable_mp((EditorShell *)this, &EditorShell::_pick_main_scene_custom_action));

	about = memnew(EditorAbout);
	main_vbox->add_child(about);

	export_template_manager = memnew(ExportTemplateManager);
	main_vbox->add_child(export_template_manager);

	orphan_resources = memnew(OrphanResourcesDialog);
	main_vbox->add_child(orphan_resources);

	build_profile_manager = memnew(EditorBuildProfileManager);
	main_vbox->add_child(build_profile_manager);

	uid_viewer = memnew(UIDViewer);
	main_vbox->add_child(uid_viewer);

	dependency_error = memnew(DependencyErrorDialog);
	main_vbox->add_child(dependency_error);

	dependency_fixer = memnew(DependencyEditor);
	main_vbox->add_child(dependency_fixer);

	// Import settings dialogs (created lazily by their singletons' users).
	memnew(SceneImportSettingsDialog);
	memnew(AudioStreamImportSettingsDialog);
	memnew(DynamicFontImportSettingsDialog);

	// Core-owned dialogs and windows; the shell parents them into the
	// GUI base (EditorShell::_notification unparents the quick-open ones
	// on predelete).
	EditorCore *core = EditorCore::get_singleton();
	core->get_project_export()->reparent(main_vbox);
	core->get_editor_settings_dialog()->reparent(main_vbox);
	core->get_project_settings_editor()->reparent(main_vbox);
	core->get_command_palette()->set_title(TTR("Command Palette"));
	main_vbox->add_child(core->get_command_palette());
	main_vbox->add_child(core->get_quick_open_dialog());
	main_vbox->add_child(core->get_quick_open_color_palette());
	core->get_native_shader_source_visualizer()->reparent(main_vbox);

	// The log is owned by the core; the shell parents it into the bottom panel.
	EditorLog *output_log = core->get_log();
	Button *output_button = bottom_panel->add_item(TTRC("Output"), output_log, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_output_bottom_panel", TTRC("Toggle Output Bottom Panel"), KeyModifierMask::ALT | Key::O));
	output_log->set_tool_button(output_button);

	// Create the progress dialog before plugins are initialized.
	// Some plugins (e.g. ScriptEditorPlugin via WindowWrapper) call
	// ProgressDialog::get_singleton() in their constructors.
	progress_dialog = memnew(ProgressDialog);
	add_child(progress_dialog);

	// Create ImportDock singleton before plugins are initialized.
	// FileSystemDock and other components call ImportDock::get_singleton() in their methods.
	if (!ImportDock::get_singleton()) {
		memnew(ImportDock);
	}

	// Initialize built-in editor plugins (registers the main-screen plugins,
	// the debugger plugin, version control, and the rest) AFTER the shell is
	// fully constructed. Plugins register bottom panel items in their
	// constructors, so the shell must exist first.
	EditorCore::get_singleton()->initialize_plugins();

	// Register code-registered and GDExtension-provided editor plugins.
	for (int i = 0; i < EditorPlugins::get_plugin_count(); i++) {
		add_editor_plugin(EditorPlugins::create(i));
	}
	// Note: the GDExtension runtime add/remove hooks
	// (GDExtensionEditorPlugins::editor_node_add_plugin / _remove_plugin)
	// can only be set by EditorNode (it is the declared friend in core).
	for (const StringName &extension_class_name : GDExtensionEditorPlugins::get_extension_classes()) {
		add_extension_editor_plugin(extension_class_name);
	}

	FileSystemDock *filesystem_dock = FileSystemDock::get_singleton();
	filesystem_dock->connect("inherit", callable_mp((EditorShell *)this, &EditorShell::_inherit_request));
	filesystem_dock->connect("instantiate", callable_mp((EditorShell *)this, &EditorShell::_instantiate_request));

	// Apply setting presets in case the editor_settings file is missing values.
	EditorSettingsDialog::update_navigation_preset();

	get_editor_data().add_edited_scene(-1);
	get_editor_data().set_edited_scene(0);
	scene_tabs->update_scene_tabs();

	ImportDock::get_singleton()->initialize_import_options();

	_update_recent_scenes();

	EditorFileSystem::get_singleton()->connect("sources_changed", callable_mp((EditorShell *)this, &EditorShell::_sources_changed));
	EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp((EditorShell *)this, &EditorShell::_fs_changed));
	EditorFileSystem::get_singleton()->connect("resources_reload", callable_mp((EditorShell *)this, &EditorShell::_resources_changed));
}

AreaEditorNode::~AreaEditorNode() {
	memdelete(project_upgrade_tool);
	singleton = nullptr;
}

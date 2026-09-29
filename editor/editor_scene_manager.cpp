/**************************************************************************/
/*  editor_scene_manager.cpp                                              */
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

#include "editor_scene_manager.h"

#include "core/config/project_settings.h" // ProjectSettings
#include "core/io/dir_access.h" // DirAccess
#include "core/io/file_access.h" // FileAccess
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_uid.h" // ResourceUID
#include "core/os/time.h"
#include "editor/animation/animation_player_editor_plugin.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/docks/inspector_dock.h"
#include "editor/docks/node_dock.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_core.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_progress.h"
#include "editor/editor_shell.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/dependency_editor.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/file_system/editor_paths.h"
#include "editor/inspector/editor_properties.h"
#include "editor/inspector/multi_node_edit.h"
#include "editor/run/editor_run_bar.h"
#include "editor/scene/editor_scene_tabs.h" // EditorSceneTabs (scene_tabs calls)
#include "editor/script/script_editor_plugin.h"
#include "editor/settings/editor_settings.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/bone_attachment_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_node_state_machine.h"
#include "scene/animation/animation_player.h"
#include "scene/animation/animation_tree.h"
#include "scene/gui/dialogs.h" // ConfirmationDialog, AcceptDialog, FileDialog
#include "scene/property_utils.h"
#include "scene/resources/packed_scene.h"

EditorSceneManager *EditorSceneManager::singleton = nullptr;

// Layout config section shared with the shell's layout save/restore.
static const String EDITOR_NODE_CONFIG_SECTION = "EditorNode";

String EditorSceneManager::_get_unsaved_scene_dialog_text(String p_scene_filename, uint64_t p_started_timestamp) {
	String unsaved_message;

	// Consider editor startup to be a point of saving, so that when you
	// close and reopen the editor, you don't get an excessively long
	// "modified X hours ago".
	const uint64_t last_modified_seconds = Time::get_singleton()->get_unix_time_from_system() - MAX(p_started_timestamp, FileAccess::get_modified_time(p_scene_filename));
	String last_modified_string;
	if (last_modified_seconds < 120) {
		last_modified_string = vformat(TTRN("%d second ago", "%d seconds ago", last_modified_seconds), last_modified_seconds);
	} else if (last_modified_seconds < 7200) {
		last_modified_string = vformat(TTRN("%d minute ago", "%d minutes ago", last_modified_seconds / 60), last_modified_seconds / 60);
	} else {
		last_modified_string = vformat(TTRN("%d hour ago", "%d hours ago", last_modified_seconds / 3600), last_modified_seconds / 3600);
	}
	unsaved_message = vformat(TTR("Scene \"%s\" has unsaved changes.\nLast saved: %s."), p_scene_filename, last_modified_string);

	return unsaved_message;
}

void EditorSceneManager::_reset_animation_mixers(Node *p_node, List<Pair<AnimationMixer *, Ref<AnimatedValuesBackup>>> *r_anim_backups) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		AnimationMixer *mixer = Object::cast_to<AnimationMixer>(p_node->get_child(i));
		if (mixer && mixer->is_active() && mixer->is_reset_on_save_enabled() && mixer->can_apply_reset()) {
			AnimationTree *tree = Object::cast_to<AnimationTree>(p_node->get_child(i));
			if (tree) {
				AnimationPlayer *player = Object::cast_to<AnimationPlayer>(tree->get_node_or_null(tree->get_animation_player()));
				if (player && player->is_active() && player->is_reset_on_save_enabled() && player->can_apply_reset()) {
					continue; // Avoid to process reset/restore many times.
				}
			}
			Ref<AnimatedValuesBackup> backup = mixer->apply_reset();
			if (backup.is_valid()) {
				Pair<AnimationMixer *, Ref<AnimatedValuesBackup>> pair;
				pair.first = mixer;
				pair.second = backup;
				r_anim_backups->push_back(pair);
			}
		}
		_reset_animation_mixers(p_node->get_child(i), r_anim_backups);
	}
}

Node *EditorSceneManager::get_edited_scene() {
	return core->get_editor_data().get_edited_scene_root();
}

int EditorSceneManager::new_scene() {
	int idx = core->get_editor_data().add_edited_scene(-1);
	EditorSceneManager::get_singleton()->_set_current_scene(idx); // Before trying to remove an empty scene, set the current tab index to the newly added tab index.

	// Remove placeholder empty scene.
	if (core->get_editor_data().get_edited_scene_count() > 1) {
		for (int i = 0; i < core->get_editor_data().get_edited_scene_count() - 1; i++) {
			bool unsaved = EditorUndoRedoManager::get_singleton()->is_history_unsaved(core->get_editor_data().get_scene_history_id(i));
			if (!unsaved && core->get_editor_data().get_scene_path(i).is_empty() && core->get_editor_data().get_edited_scene_root(i) == nullptr) {
				core->get_editor_data().remove_scene(i);
				idx--;
			}
		}
	}

	core->get_editor_data().clear_editor_states();
	shell->get_scene_tabs()->update_scene_tabs();
	return idx;
}

Error EditorSceneManager::load_scene(const String &p_scene, bool p_ignore_broken_deps, bool p_set_inherited, bool p_force_open_imported, bool p_silent_change_tab) {
	if (!is_inside_tree()) {
		defer_load_scene = p_scene;
		return OK;
	}

	String lpath = ProjectSettings::get_singleton()->localize_path(ResourceUID::ensure_path(p_scene));
	_update_prev_closed_scenes(lpath, false);

	if (!p_set_inherited) {
		for (int i = 0; i < core->get_editor_data().get_edited_scene_count(); i++) {
			if (core->get_editor_data().get_scene_path(i) == lpath) {
				_set_current_scene(i);
				return OK;
			}
		}

		if (!p_force_open_imported && FileAccess::exists(lpath + ".import")) {
			shell->get_open_imported_dialog()->set_text(vformat(TTR("Scene '%s' was automatically imported, so it can't be modified.\nTo make changes to it, a new inherited scene can be created."), lpath.get_file()));
			shell->get_open_imported_dialog()->popup_centered();
			shell->get_new_inherited_button()->grab_focus();
			open_import_request = lpath;
			return OK;
		}
	}

	if (!lpath.begins_with("res://")) {
		shell->show_accept(TTR("Error loading scene, it must be inside the project path. Use 'Import' to open the scene, then save it inside the project path."), TTR("OK"));
		return ERR_FILE_NOT_FOUND;
	}

	int prev = core->get_editor_data().get_edited_scene();
	int idx = prev;

	if (prev == -1 || core->get_editor_data().get_edited_scene_root() || !core->get_editor_data().get_scene_path(prev).is_empty()) {
		idx = core->get_editor_data().add_edited_scene(-1);

		if (p_silent_change_tab) {
			_set_current_scene_nocheck(idx);
		} else {
			_set_current_scene(idx);
		}
	} else {
		EditorUndoRedoManager::get_singleton()->clear_history(core->get_editor_data().get_current_edited_scene_history_id(), false);
	}

	core->get_dependency_errors().clear();

	Error err;
	Ref<PackedScene> sdata = ResourceLoader::load(lpath, "", ResourceFormatLoader::CACHE_MODE_REPLACE, &err);

	if (!p_ignore_broken_deps && core->get_dependency_errors().has(lpath)) {
		shell->set_current_menu_option(-1);
		Vector<String> errors;
		for (const String &E : core->get_dependency_errors()[lpath]) {
			errors.push_back(E);
		}
		shell->get_dependency_error_dialog()->show(lpath, errors);

		if (prev != -1 && prev != idx) {
			_set_current_scene(prev);
			core->get_editor_data().remove_scene(idx);
		}
		return ERR_FILE_MISSING_DEPENDENCIES;
	}

	if (sdata.is_null()) {
		shell->_dialog_display_load_error(lpath, err);

		if (prev != -1 && prev != idx) {
			_set_current_scene(prev);
			core->get_editor_data().remove_scene(idx);
		}
		return ERR_FILE_NOT_FOUND;
	}

	core->get_dependency_errors().erase(lpath); // At least not self path.

	for (KeyValue<String, HashSet<String>> &E : core->get_dependency_errors()) {
		String txt = vformat(TTR("Scene '%s' has broken dependencies:"), E.key) + "\n";
		for (const String &F : E.value) {
			txt += "\t" + F + "\n";
		}
		shell->add_io_error(txt);
	}

	if (ResourceCache::has(lpath)) {
		// Used from somewhere else? No problem! Update state and replace sdata.
		Ref<PackedScene> ps = ResourceCache::get_ref(lpath);
		if (ps.is_valid()) {
			ps->replace_state(sdata->get_state());
			ps->set_last_modified_time(sdata->get_last_modified_time());
			sdata = ps;
		}

	} else {
		sdata->set_path(lpath, true); // Take over path.
	}

	Node *new_scene = sdata->instantiate(p_set_inherited ? PackedScene::GEN_EDIT_STATE_MAIN_INHERITED : PackedScene::GEN_EDIT_STATE_MAIN);
	if (!new_scene) {
		sdata.unref();
		shell->_dialog_display_load_error(lpath, ERR_FILE_CORRUPT);
		if (prev != -1 && prev != idx) {
			_set_current_scene(prev);
			core->get_editor_data().remove_scene(idx);
		}
		return ERR_FILE_CORRUPT;
	}

	if (p_set_inherited) {
		Ref<SceneState> state = sdata->get_state();
		state->set_path(lpath);
		new_scene->set_scene_inherited_state(state);
		new_scene->set_scene_file_path(String());
	}

	new_scene->set_scene_instance_state(Ref<SceneState>());

	set_edited_scene(new_scene);

	String config_file_path = EditorPaths::get_singleton()->get_project_settings_dir().path_join(lpath.get_file() + "-editstate-" + lpath.md5_text() + ".cfg");
	Ref<ConfigFile> editor_state_cf;
	editor_state_cf.instantiate();
	Error editor_state_cf_err = editor_state_cf->load(config_file_path);
	if (editor_state_cf_err == OK || editor_state_cf->has_section("editor_states")) {
		plugin_manager->_load_editor_plugin_states_from_config(editor_state_cf);
	}

	if (core->get_editor_folding().has_folding_data(lpath)) {
		core->get_editor_folding().load_scene_folding(new_scene, lpath);
	} else if (EDITOR_GET("interface/inspector/auto_unfold_foreign_scenes")) {
		core->get_editor_folding().unfold_scene(new_scene);
		core->get_editor_folding().save_scene_folding(new_scene, lpath);
	}

	EditorDebuggerNode::get_singleton()->update_live_edit_root();

	if (restoring_scenes) {
		// Initialize history for restored scenes.
		ObjectID id = new_scene->get_instance_id();
		if (id != core->get_editor_selection_history()->get_current()) {
			core->get_editor_selection_history()->add_object(id);
		}
	}

	// Load the selected nodes.
	if (editor_state_cf->has_section_key("editor_states", "selected_nodes")) {
		TypedArray<NodePath> selected_node_list = editor_state_cf->get_value("editor_states", "selected_nodes", TypedArray<String>());

		for (int i = 0; i < selected_node_list.size(); i++) {
			Node *selected_node = new_scene->get_node_or_null(selected_node_list[i]);
			if (selected_node) {
				core->get_editor_selection()->add_node(selected_node);
			}
		}
	}

	if (!restoring_scenes) {
		shell->save_editor_layout_delayed();
	}

	if (p_set_inherited) {
		EditorUndoRedoManager::get_singleton()->set_history_as_unsaved(core->get_editor_data().get_current_edited_scene_history_id());
	}

	shell->_update_title();
	shell->get_scene_tabs()->update_scene_tabs();
	if (!restoring_scenes) {
		shell->_add_to_recent_scenes(lpath);
	}

	return OK;
}

void EditorSceneManager::set_edited_scene(Node *p_scene) {
	set_edited_scene_root(p_scene, true);
}

void EditorSceneManager::set_edited_scene_root(Node *p_scene, bool p_auto_add) {
	Node *old_edited_scene_root = core->get_editor_data().get_edited_scene_root();
	ERR_FAIL_COND_MSG(p_scene && p_scene != old_edited_scene_root && p_scene->get_parent(), "Non-null nodes that are set as edited scene should not have a parent node.");

	if (p_auto_add && old_edited_scene_root && old_edited_scene_root->get_parent() == shell->get_scene_root()) {
		shell->get_scene_root()->remove_child(old_edited_scene_root);
	}
	core->get_editor_data().set_edited_scene_root(p_scene);

	if (Object::cast_to<Popup>(p_scene)) {
		Object::cast_to<Popup>(p_scene)->show();
	}
	SceneTreeDock::get_singleton()->set_edited_scene(p_scene);
	if (get_tree()) {
		get_tree()->set_edited_scene_root(p_scene);
	}

	if (p_auto_add && p_scene) {
		shell->get_scene_root()->add_child(p_scene, true);
	}
}

void EditorSceneManager::edit_node(Node *p_node) {
	push_item(p_node);
}

void EditorSceneManager::edit_resource(const Ref<Resource> &p_resource) {
	InspectorDock::get_singleton()->edit_resource(p_resource);
}

void EditorSceneManager::edit_item(Object *p_object, Object *p_editing_owner) {
	ERR_FAIL_NULL(p_editing_owner);

	// Editing for this type of object may be disabled by user's feature profile.
	if (!p_object || shell->_is_class_editor_disabled_by_feature_profile(p_object->get_class())) {
		// Nothing to edit, clean up the owner context and return.
		hide_unused_editors(p_editing_owner);
		return;
	}

	// Get a list of editor plugins that can handle this type of object.
	Vector<EditorPlugin *> available_plugins = core->get_editor_data().get_handling_sub_editors(p_object);
	if (available_plugins.is_empty()) {
		// None, clean up the owner context and return.
		hide_unused_editors(p_editing_owner);
		return;
	}

	ObjectID owner_id = p_editing_owner->get_instance_id();

	// Remove editor plugins no longer used by this editing owner. Keep the ones that can
	// still be reused by the new edited object.

	List<EditorPlugin *> to_remove;
	for (EditorPlugin *plugin : plugin_manager->get_active_plugins()[owner_id]) {
		if (!available_plugins.has(plugin)) {
			to_remove.push_back(plugin);
			if (plugin->can_auto_hide()) {
				plugin_manager->_plugin_over_edit(plugin, nullptr);
			} else {
				// If plugin can't be hidden, make it own itself and become responsible for closing.
				plugin_manager->_plugin_over_self_own(plugin);
			}
		}
	}

	for (EditorPlugin *plugin : to_remove) {
		plugin_manager->get_active_plugins()[owner_id].erase(plugin);
	}

	LocalVector<EditorPlugin *> to_over_edit;

	// Send the edited object to the plugins.
	for (EditorPlugin *plugin : available_plugins) {
		if (plugin_manager->get_active_plugins()[owner_id].has(plugin)) {
			// Plugin was already active, just change the object and ensure it's visible.
			plugin->make_visible(true);
			plugin->edit(p_object);
			continue;
		}

		if (plugin_manager->get_active_plugins().has(plugin->get_instance_id())) {
			// Plugin is already active, but as self-owning, so it needs a separate check.
			plugin->make_visible(true);
			plugin->edit(p_object);
			continue;
		}

		bool need_to_add = true;
		List<EditorPropertyResource *> to_fold;

		// If plugin is already associated with another owner, remove it from there first.
		for (KeyValue<ObjectID, HashSet<EditorPlugin *>> &kv : plugin_manager->get_active_plugins()) {
			if (kv.key == owner_id || !kv.value.has(plugin)) {
				continue;
			}
			EditorPropertyResource *epres = ObjectDB::get_instance<EditorPropertyResource>(kv.key);
			if (epres) {
				// If it's resource property editing the same resource type, fold it later to avoid premature modifications
				// that may result in unsafe iteration of active_plugins.
				to_fold.push_back(epres);
			} else {
				kv.value.erase(plugin);
				need_to_add = false;
			}
		}

		if (!need_to_add && to_fold.is_empty()) {
			plugin->make_visible(true);
			plugin->edit(p_object);
		} else {
			for (EditorPropertyResource *epres : to_fold) {
				epres->fold_resource();
			}

			/// @todo Call the function directly once a proper priority system is implemented.
			to_over_edit.push_back(plugin);
		}

		// Activate previously inactive plugin and edit the object.
		plugin_manager->get_active_plugins()[owner_id].insert(plugin);
	}

	for (EditorPlugin *plugin : to_over_edit) {
		plugin_manager->_plugin_over_edit(plugin, p_object);
	}
}

void EditorSceneManager::push_item(Object *p_object, const String &p_property, bool p_inspector_only) {
	if (!p_object) {
		InspectorDock::get_inspector_singleton()->edit(nullptr);
		NodeDock::get_singleton()->set_node(nullptr);
		SceneTreeDock::get_singleton()->set_selected(nullptr);
		InspectorDock::get_singleton()->update(nullptr);
		hide_unused_editors();
		return;
	}
	shell->_add_to_history(p_object, p_property, p_inspector_only);
	shell->_edit_current();
}

void EditorSceneManager::push_item_no_inspector(Object *p_object) {
	shell->_add_to_history(p_object, "", false);
	shell->_edit_current(false, true);
}

void EditorSceneManager::push_node_item(Node *p_node) {
	if (p_node || !InspectorDock::get_inspector_singleton()->get_edited_object() ||
			Object::cast_to<Node>(InspectorDock::get_inspector_singleton()->get_edited_object()) ||
			Object::cast_to<MultiNodeEdit>(InspectorDock::get_inspector_singleton()->get_edited_object())) {
		// Don't push null if the currently edited object is not a Node.
		push_item(p_node);
	}
}

void EditorSceneManager::edit_previous_item() {
	if (core->get_editor_selection_history()->previous()) {
		shell->_edit_current();
	}
}

void EditorSceneManager::hide_unused_editors(const Object *p_editing_owner) {
	if (p_editing_owner) {
		const ObjectID id = p_editing_owner->get_instance_id();
		for (EditorPlugin *plugin : plugin_manager->get_active_plugins()[id]) {
			if (plugin->can_auto_hide()) {
				plugin_manager->_plugin_over_edit(plugin, nullptr);
			} else {
				plugin_manager->_plugin_over_self_own(plugin);
			}
		}
		plugin_manager->get_active_plugins().erase(id);
	} else {
		// If no editing owner is provided, this method will go over all owners and check if they are valid.
		// This is to sweep properties that were removed from the inspector.
		List<ObjectID> to_remove;
		for (KeyValue<ObjectID, HashSet<EditorPlugin *>> &kv : plugin_manager->get_active_plugins()) {
			Object *context = ObjectDB::get_instance(kv.key);
			if (context) {
				// In case of self-owning plugins, they are disabled here if they can auto hide.
				const EditorPlugin *self_owning = Object::cast_to<EditorPlugin>(context);
				if (self_owning && self_owning->can_auto_hide()) {
					context = nullptr;
				}
			}

			if (!context || context->call(SNAME("_should_stop_editing"))) {
				to_remove.push_back(kv.key);
				for (EditorPlugin *plugin : kv.value) {
					if (plugin->can_auto_hide()) {
						plugin_manager->_plugin_over_edit(plugin, nullptr);
					} else {
						plugin_manager->_plugin_over_self_own(plugin);
					}
				}
			}
		}

		for (const ObjectID &id : to_remove) {
			plugin_manager->get_active_plugins().erase(id);
		}
	}
}

void EditorSceneManager::_update_unsaved_cache() {
	bool is_unsaved = EditorUndoRedoManager::get_singleton()->is_history_unsaved(EditorUndoRedoManager::GLOBAL_HISTORY) ||
			EditorUndoRedoManager::get_singleton()->is_history_unsaved(core->get_editor_data().get_current_edited_scene_history_id());

	if (unsaved_cache != is_unsaved) {
		unsaved_cache = is_unsaved;
		shell->_update_title();
	}
}

void EditorSceneManager::_update_undo_redo_allowed() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	shell->get_file_menu()->set_item_disabled(shell->get_file_menu()->get_item_index(EditorShell::SCENE_UNDO), !undo_redo->has_undo());
	shell->get_file_menu()->set_item_disabled(shell->get_file_menu()->get_item_index(EditorShell::SCENE_REDO), !undo_redo->has_redo());
}

void EditorSceneManager::_set_current_scene(int p_idx) {
	if (p_idx == core->get_editor_data().get_edited_scene()) {
		return; // Pointless.
	}

	_set_current_scene_nocheck(p_idx);
}

void EditorSceneManager::_set_current_scene_nocheck(int p_idx) {
	// Save the folding in case the scene gets reloaded.
	if (core->get_editor_data().get_scene_path(p_idx) != "" && core->get_editor_data().get_edited_scene_root(p_idx)) {
		core->get_editor_folding().save_scene_folding(core->get_editor_data().get_edited_scene_root(p_idx), core->get_editor_data().get_scene_path(p_idx));
	}

	changing_scene = true;
	core->get_editor_data().save_edited_scene_state(core->get_editor_selection(), core->get_editor_selection_history(), _get_main_scene_state());

	Node *old_scene = core->get_editor_data().get_edited_scene_root();

	core->get_editor_selection()->clear();
	SceneTreeDock::get_singleton()->clear_previous_node_selection();
	core->get_editor_data().set_edited_scene(p_idx);

	Node *new_scene = core->get_editor_data().get_edited_scene_root();

	// Remove the scene only if it's a new scene, preventing performance issues when adding and removing scenes.
	if (old_scene && new_scene != old_scene && old_scene->get_parent() == shell->get_scene_root()) {
		shell->get_scene_root()->remove_child(old_scene);
	}

	if (Popup *p = Object::cast_to<Popup>(new_scene)) {
		p->show();
	}

	SceneTreeDock::get_singleton()->set_edited_scene(new_scene);
	if (get_tree()) {
		get_tree()->set_edited_scene_root(new_scene);
	}

	if (new_scene) {
		if (new_scene->get_parent() != shell->get_scene_root()) {
			shell->get_scene_root()->add_child(new_scene, true);
		}
	}

	if (core->get_editor_data().check_and_update_scene(p_idx)) {
		if (!core->get_editor_data().get_scene_path(p_idx).is_empty()) {
			core->get_editor_folding().load_scene_folding(core->get_editor_data().get_edited_scene_root(p_idx), core->get_editor_data().get_scene_path(p_idx));
		}

		EditorUndoRedoManager::get_singleton()->clear_history(core->get_editor_data().get_scene_history_id(p_idx), false);
	}

	Dictionary state = core->get_editor_data().restore_edited_scene_state(core->get_editor_selection(), core->get_editor_selection_history());
	shell->_edit_current(true);

	shell->_update_title();
	callable_mp(shell->get_scene_tabs(), &EditorSceneTabs::update_scene_tabs).call_deferred();

	if (tabs_to_close.is_empty()) {
		callable_mp(this, &EditorSceneManager::_set_main_scene_state); // Do after everything else is done setting up.
	}

	_update_undo_redo_allowed();
	_update_unsaved_cache();
}

void EditorSceneManager::save_scene_to_path(String p_file, bool p_with_preview) {
	if (p_with_preview) {
		shell->_save_scene_with_preview(p_file);
	} else {
		_save_scene(p_file);
	}
}

bool EditorSceneManager::_validate_scene_recursive(const String &p_filename, Node *p_node) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		if (child->get_scene_file_path() == p_filename) {
			return true;
		}

		if (_validate_scene_recursive(p_filename, child)) {
			return true;
		}
	}

	return false;
}

void EditorSceneManager::_save_scene(String p_file, int idx) {
	ERR_FAIL_COND_MSG(!saving_scene.is_empty() && saving_scene == p_file, "Scene saved while already being saved!");

	Node *scene = core->get_editor_data().get_edited_scene_root(idx);

	if (!scene) {
		shell->show_accept(TTR("This operation can't be done without a tree root."), TTR("OK"));
		return;
	}

	if (!scene->get_scene_file_path().is_empty() && _validate_scene_recursive(scene->get_scene_file_path(), scene)) {
		shell->show_accept(TTR("This scene can't be saved because there is a cyclic instance inclusion.\nPlease resolve it and then attempt to save again."), TTR("OK"));
		return;
	}

	scene->propagate_notification(NOTIFICATION_EDITOR_PRE_SAVE);

	core->get_editor_data().apply_changes_in_editors();
	EditorCore::get_singleton()->save_default_environment();
	List<Pair<AnimationMixer *, Ref<AnimatedValuesBackup>>> anim_backups;
	_reset_animation_mixers(scene, &anim_backups);
	_save_editor_states(p_file, idx);

	Ref<PackedScene> sdata;

	if (ResourceCache::has(p_file)) {
		// Something may be referencing this resource and we are good with that.
		// We must update it, but also let the previous scene state go, as
		// old version still work for referencing changes in instantiated or inherited scenes.

		sdata = ResourceCache::get_ref(p_file);
		if (sdata.is_valid()) {
			sdata->recreate_state();
		} else {
			sdata.instantiate();
		}
	} else {
		sdata.instantiate();
	}
	Error err = sdata->pack(scene);

	if (err != OK) {
		shell->show_accept(TTR("Couldn't save scene. Likely dependencies (instances or inheritance) couldn't be satisfied."), TTR("OK"));
		return;
	}

	int flg = 0;
	if (EDITOR_GET("filesystem/on_save/compress_binary_resources")) {
		flg |= ResourceSaver::FLAG_COMPRESS;
	}
	flg |= ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;

	err = ResourceSaver::save(sdata, p_file, flg);

	// This needs to be emitted before saving external resources.
	emit_signal(SNAME("scene_saved"), p_file);
	core->get_editor_data().notify_scene_saved(p_file);

	_save_external_resources();
	saving_scene = p_file; // Some editors may save scenes of built-in resources as external data, so avoid saving this scene again.
	core->get_editor_data().save_editor_external_data();
	saving_scene = "";

	for (Pair<AnimationMixer *, Ref<AnimatedValuesBackup>> &E : anim_backups) {
		E.first->restore(E.second);
	}

	if (err == OK) {
		scene->set_scene_file_path(ProjectSettings::get_singleton()->localize_path(p_file));
		core->get_editor_data().set_scene_as_saved(idx);
		core->get_editor_data().set_scene_modified_time(idx, FileAccess::get_modified_time(p_file));

		core->get_editor_folding().save_scene_folding(scene, p_file);

		shell->_update_title();
		shell->get_scene_tabs()->update_scene_tabs();
	} else {
		shell->_dialog_display_save_error(p_file, err);
	}

	scene->propagate_notification(NOTIFICATION_EDITOR_POST_SAVE);
	_update_unsaved_cache();
}

void EditorSceneManager::_save_all_scenes() {
	scenes_to_save_as.clear(); // In case saving was canceled before.
	for (int i = 0; i < core->get_editor_data().get_edited_scene_count(); i++) {
		if (!_is_scene_unsaved(i)) {
			continue;
		}

		const Node *scene = core->get_editor_data().get_edited_scene_root(i);
		ERR_FAIL_NULL(scene);

		const String &scene_path = scene->get_scene_file_path();
		if (scene_path.is_empty() || !DirAccess::exists(scene_path.get_base_dir())) {
			scenes_to_save_as.push_back(i);
			continue;
		}

		if (i == core->get_editor_data().get_edited_scene()) {
			shell->_save_scene_with_preview(scene_path);
		} else {
			_save_scene(scene_path, i);
		}
	}
	EditorCore::get_singleton()->save_default_environment();

	if (!scenes_to_save_as.is_empty()) {
		_proceed_save_asing_scene_tabs();
	}
}

int EditorSceneManager::_next_unsaved_scene(bool p_valid_filename, int p_start) {
	for (int i = p_start; i < core->get_editor_data().get_edited_scene_count(); i++) {
		if (!core->get_editor_data().get_edited_scene_root(i)) {
			continue;
		}

		String scene_filename = core->get_editor_data().get_edited_scene_root(i)->get_scene_file_path();
		if (p_valid_filename && scene_filename.is_empty()) {
			continue;
		}

		bool unsaved = EditorUndoRedoManager::get_singleton()->is_history_unsaved(core->get_editor_data().get_scene_history_id(i));
		if (unsaved) {
			return i;
		} else {
			for (int j = 0; j < core->get_editor_data().get_editor_plugin_count(); j++) {
				if (!core->get_editor_data().get_editor_plugin(j)->get_unsaved_status(scene_filename).is_empty()) {
					return i;
				}
			}
		}
	}
	return -1;
}

void EditorSceneManager::_discard_changes(const String &p_str) {
	switch (shell->get_current_menu_option()) {
		case EditorShell::SCENE_CLOSE:
		case EditorShell::SCENE_TAB_CLOSE: {
			Node *scene = core->get_editor_data().get_edited_scene_root(tab_closing_idx);
			if (scene != nullptr) {
				_update_prev_closed_scenes(scene->get_scene_file_path(), true);
			}

			// Don't close tabs when exiting the editor (required for "restore_scenes_on_load" setting).
			if (!shell->_is_closing_editor()) {
				_remove_scene(tab_closing_idx);
				shell->get_scene_tabs()->update_scene_tabs();
			}
			_proceed_closing_scene_tabs();
		} break;
		case EditorShell::SCENE_RELOAD_SAVED_SCENE: {
			Node *scene = get_edited_scene();

			String scene_filename = scene->get_scene_file_path();

			int cur_idx = core->get_editor_data().get_edited_scene();

			_remove_edited_scene();

			Error err = load_scene(scene_filename);
			if (err != OK) {
				ERR_PRINT("Failed to load scene");
			}
			core->get_editor_data().move_edited_scene_to_index(cur_idx);
			EditorUndoRedoManager::get_singleton()->clear_history(core->get_editor_data().get_current_edited_scene_history_id(), false);
			shell->get_scene_tabs()->set_current_tab(cur_idx);

			shell->get_confirmation_dialog()->hide();
		} break;
		case EditorShell::SCENE_QUIT: {
			shell->get_project_run_bar()->stop_playing();
			shell->_exit_editor(EXIT_SUCCESS);

		} break;
		case EditorShell::PROJECT_QUIT_TO_PROJECT_MANAGER: {
			shell->_restart_editor(true);
		} break;
		case EditorShell::PROJECT_RELOAD_CURRENT_PROJECT: {
			shell->_restart_editor();
		} break;
	}
}

void EditorSceneManager::_scene_tab_closed(int p_tab) {
	shell->set_current_menu_option(EditorShell::SCENE_TAB_CLOSE);
	tab_closing_idx = p_tab;
	Node *scene = core->get_editor_data().get_edited_scene_root(p_tab);
	if (!scene) {
		_discard_changes();
		return;
	}

	String scene_filename = scene->get_scene_file_path();
	String unsaved_message;

	if (EditorUndoRedoManager::get_singleton()->is_history_unsaved(core->get_editor_data().get_scene_history_id(p_tab))) {
		if (scene_filename.is_empty()) {
			unsaved_message = TTR("This scene was never saved.");
		} else {
			unsaved_message = _get_unsaved_scene_dialog_text(scene_filename, EditorCore::get_singleton()->get_started_timestamp());
		}
	} else {
		// Check if any plugin has unsaved changes in that scene.
		for (int i = 0; i < core->get_editor_data().get_editor_plugin_count(); i++) {
			unsaved_message = core->get_editor_data().get_editor_plugin(i)->get_unsaved_status(scene_filename);
			if (!unsaved_message.is_empty()) {
				break;
			}
		}
	}

	if (!unsaved_message.is_empty()) {
		if (shell->get_scene_tabs()->get_current_tab() != p_tab) {
			_set_current_scene(p_tab);
		}

		shell->get_save_confirmation_dialog()->set_ok_button_text(TTR("Save & Close"));
		shell->get_save_confirmation_dialog()->set_text(unsaved_message + "\n\n" + TTR("Save before closing?"));
		shell->get_save_confirmation_dialog()->reset_size();
		shell->get_save_confirmation_dialog()->popup_centered();
	} else {
		_discard_changes();
	}

	shell->save_editor_layout_delayed();
	shell->get_scene_tabs()->update_scene_tabs();
}

void EditorSceneManager::_cancel_close_scene_tab() {
	if (shell->_is_closing_editor()) {
		tab_closing_menu_option = -1;
	}
	changing_scene = false;
	tabs_to_close.clear();
}

void EditorSceneManager::_remove_edited_scene(bool p_change_tab) {
	hide_unused_editors(SceneTreeDock::get_singleton());
	SceneTreeDock::get_singleton()->clear_previous_node_selection();

	int new_index = core->get_editor_data().get_edited_scene();
	int old_index = new_index;

	if (new_index > 0) {
		new_index = new_index - 1;
	} else if (core->get_editor_data().get_edited_scene_count() > 1) {
		new_index = 1;
	} else {
		core->get_editor_data().add_edited_scene(-1);
		new_index = 1;
	}

	if (p_change_tab) {
		_set_current_scene(new_index);
	}
	core->get_editor_data().remove_scene(old_index);
	shell->_update_title();
	shell->get_scene_tabs()->update_scene_tabs();
}

void EditorSceneManager::_remove_scene(int index, bool p_change_tab) {
	/// Clear icon cache in case some scripts are no longer needed or class icons are outdated.
	/// @todo FIXME: Ideally the cache should never be cleared and only updated on per-script basis, when an icon changes.
	core->get_editor_data().clear_script_icon_cache();
	core->get_class_icon_cache().clear();

	if (core->get_editor_data().get_edited_scene() == index) {
		// Scene to remove is current scene.
		_remove_edited_scene(p_change_tab);
	} else {
		// Scene to remove is not active scene.
		core->get_editor_data().remove_scene(index);
	}
}

void EditorSceneManager::_mark_unsaved_scenes() {
	for (int i = 0; i < core->get_editor_data().get_edited_scene_count(); i++) {
		Node *node = core->get_editor_data().get_edited_scene_root(i);
		if (!node) {
			continue;
		}

		String path = node->get_scene_file_path();
		if (!path.is_empty() && !FileAccess::exists(path)) {
			// Mark scene tab as unsaved if the file is gone.
			EditorUndoRedoManager::get_singleton()->set_history_as_unsaved(core->get_editor_data().get_scene_history_id(i));
		}
	}

	shell->_update_title();
	shell->get_scene_tabs()->update_scene_tabs();
}

bool EditorSceneManager::_is_scene_unsaved(int p_idx) {
	const Node *scene = core->get_editor_data().get_edited_scene_root(p_idx);
	if (!scene) {
		return false;
	}

	if (EditorUndoRedoManager::get_singleton()->is_history_unsaved(core->get_editor_data().get_scene_history_id(p_idx))) {
		return true;
	}

	const String &scene_path = scene->get_scene_file_path();
	if (!scene_path.is_empty()) {
		// Check if scene has unsaved changes in built-in resources.
		for (int j = 0; j < core->get_editor_data().get_editor_plugin_count(); j++) {
			if (!core->get_editor_data().get_editor_plugin(j)->get_unsaved_status(scene_path).is_empty()) {
				return true;
			}
		}
	}
	return false;
}

void EditorSceneManager::_update_prev_closed_scenes(const String &p_scene_path, bool p_add_scene) {
	if (!p_scene_path.is_empty()) {
		if (p_add_scene) {
			prev_closed_scenes.push_back(p_scene_path);
		} else {
			prev_closed_scenes.erase(p_scene_path);
		}
		shell->get_file_menu()->set_item_disabled(shell->get_file_menu()->get_item_index(EditorShell::SCENE_OPEN_PREV), prev_closed_scenes.is_empty());
	}
}

void EditorSceneManager::_proceed_closing_scene_tabs() {
	List<String>::Element *E = tabs_to_close.front();
	if (!E) {
		if (shell->_is_closing_editor()) {
			shell->set_current_menu_option(tab_closing_menu_option);
			shell->_menu_option_confirm(tab_closing_menu_option, true);
		} else {
			shell->set_current_menu_option(-1);
			shell->get_save_confirmation_dialog()->hide();
		}
		return;
	}
	String scene_to_close = E->get();
	tabs_to_close.pop_front();

	int tab_idx = -1;
	for (int i = 0; i < core->get_editor_data().get_edited_scene_count(); i++) {
		if (core->get_editor_data().get_scene_path(i) == scene_to_close) {
			tab_idx = i;
			break;
		}
	}
	ERR_FAIL_COND(tab_idx < 0);

	_scene_tab_closed(tab_idx);
}

void EditorSceneManager::_proceed_save_asing_scene_tabs() {
	if (scenes_to_save_as.is_empty()) {
		return;
	}
	int scene_idx = scenes_to_save_as.front()->get();
	scenes_to_save_as.pop_front();
	_set_current_scene(scene_idx);
	shell->_menu_option_confirm(EditorShell::SCENE_MULTI_SAVE_AS_SCENE, false);
}

Dictionary EditorSceneManager::_get_main_scene_state() {
	Dictionary state;
	state["scene_tree_offset"] = SceneTreeDock::get_singleton()->get_tree_editor()->get_scene_tree()->get_vscroll_bar()->get_value();
	state["property_edit_offset"] = InspectorDock::get_inspector_singleton()->get_scroll_offset();
	state["node_filter"] = SceneTreeDock::get_singleton()->get_filter();
	return state;
}

void EditorSceneManager::_set_main_scene_state(Dictionary p_state, Node *p_for_scene) {
	if (get_edited_scene() != p_for_scene && p_for_scene != nullptr) {
		return; // Not for this scene.
	}

	changing_scene = false;

	if (get_edited_scene()) {
		if (shell->get_editor_main_screen()->can_auto_switch_screens()) {
			// Switch between 2D and 3D if currently in 2D or 3D.
			Node *selected_node = SceneTreeDock::get_singleton()->get_tree_editor()->get_selected();
			if (!selected_node) {
				selected_node = get_edited_scene();
			}
			const int plugin_index = shell->get_editor_main_screen()->get_plugin_index(core->get_editor_data().get_handling_main_editor(selected_node));
			if (plugin_index >= 0) {
				shell->get_editor_main_screen()->select(plugin_index);
			}
		}
	}

	if (p_state.has("scene_tree_offset")) {
		SceneTreeDock::get_singleton()->get_tree_editor()->get_scene_tree()->get_vscroll_bar()->set_value(p_state["scene_tree_offset"]);
	}
	if (p_state.has("property_edit_offset")) {
		InspectorDock::get_inspector_singleton()->set_scroll_offset(p_state["property_edit_offset"]);
	}

	if (p_state.has("node_filter")) {
		SceneTreeDock::get_singleton()->set_filter(p_state["node_filter"]);
	}

	// This should only happen at the very end.

	EditorDebuggerNode::get_singleton()->update_live_edit_root();
	ScriptEditor::get_singleton()->set_scene_root_script(core->get_editor_data().get_scene_root_script(core->get_editor_data().get_edited_scene()));
	core->get_editor_data().notify_edited_scene_changed();
	emit_signal(SNAME("scene_changed"));

	// Reset SDFGI after everything else so that any last-second scene modifications will be processed.
	RenderingServer::get_singleton()->sdfgi_reset();
}

void EditorSceneManager::_save_editor_states(const String &p_file, int p_idx) {
	Node *scene = core->get_editor_data().get_edited_scene_root(p_idx);

	if (!scene) {
		return;
	}

	String path = EditorPaths::get_singleton()->get_project_settings_dir().path_join(p_file.get_file() + "-editstate-" + p_file.md5_text() + ".cfg");

	Ref<ConfigFile> cf;
	cf.instantiate();

	Dictionary md;
	if (p_idx < 0 || core->get_editor_data().get_edited_scene() == p_idx) {
		md = core->get_editor_data().get_editor_plugin_states();
	} else {
		md = core->get_editor_data().get_scene_editor_states(p_idx);
	}

	for (const KeyValue<Variant, Variant> &kv : md) {
		cf->set_value("editor_states", kv.key, kv.value);
	}

	// Save the currently selected nodes.

	List<Node *> selection = core->get_editor_selection()->get_full_selected_node_list();
	TypedArray<NodePath> selection_paths;
	for (Node *selected_node : selection) {
		selection_paths.push_back(selected_node->get_path());
	}
	cf->set_value("editor_states", "selected_nodes", selection_paths);

	Error err = cf->save(path);
	ERR_FAIL_COND_MSG(err != OK, "Cannot save config file to '" + path + "'.");
}

void EditorSceneManager::_bind_methods() {
	ClassDB::bind_method("set_edited_scene", &EditorSceneManager::set_edited_scene);

	ADD_SIGNAL(MethodInfo("scene_saved", PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("scene_changed"));
	ADD_SIGNAL(MethodInfo("scene_closed", PropertyInfo(Variant::STRING, "path")));
}

EditorSceneManager::EditorSceneManager() {
	DEV_ASSERT(!singleton);
	singleton = this;
	core = EditorCore::get_singleton();
}

void EditorSceneManager::_resources_reimporting(const Vector<String> &p_resources) {
	scenes_modification_table.clear();
	scenes_reimported.clear();
	resources_reimported.clear();
	EditorFileSystem *editor_file_system = EditorFileSystem::get_singleton();
	for (const String &res_path : p_resources) {
		// It's faster to use EditorFileSystem::get_file_type than fetching the resource type from disk.
		// This makes a big difference when reimporting many resources.
		String file_type = editor_file_system->get_file_type(res_path);
		if (file_type.is_empty()) {
			file_type = ResourceLoader::get_resource_type(res_path);
		}
		if (file_type == "PackedScene") {
			scenes_reimported.push_back(res_path);
		} else {
			resources_reimported.push_back(res_path);
		}
	}

	if (scenes_reimported.size() > 0) {
		preload_reimporting_with_path_in_edited_scenes(scenes_reimported);
	}
}

void EditorSceneManager::_resources_reimported(const Vector<String> &p_resources) {
	int current_tab = shell->get_scene_tabs()->get_current_tab();

	for (const String &res_path : resources_reimported) {
		if (!ResourceCache::has(res_path)) {
			// Not loaded, no need to reload.
			continue;
		}
		// Reload normally.
		Ref<Resource> resource = ResourceCache::get_ref(res_path);
		if (resource.is_valid()) {
			resource->reload_from_file();
		}
	}

	// Editor may crash when related animation is playing while re-importing GLTF scene, stop it in advance.
	AnimationPlayer *ap = AnimationPlayerEditor::get_singleton()->get_player();
	if (ap && scenes_reimported.size() > 0) {
		ap->stop(true);
	}

	// Only refresh the current scene tab if it's been reimported.
	// Otherwise the scene tab will try to grab focus unnecessarily.
	bool should_refresh_current_scene_tab = false;
	const String current_scene_tab = core->get_editor_data().get_scene_path(current_tab);
	for (const String &E : scenes_reimported) {
		if (!should_refresh_current_scene_tab && E == current_scene_tab) {
			should_refresh_current_scene_tab = true;
		}
		reload_scene(E);
	}

	reload_instances_with_path_in_edited_scenes();

	scenes_modification_table.clear();
	scenes_reimported.clear();
	resources_reimported.clear();

	if (should_refresh_current_scene_tab) {
		_set_current_scene_nocheck(current_tab);
	}
}

HashMap<StringName, Variant> EditorSceneManager::get_modified_properties_for_node(Node *p_node, bool p_node_references_only) {
	HashMap<StringName, Variant> modified_property_map;

	List<PropertyInfo> pinfo;
	p_node->get_property_list(&pinfo);
	for (const PropertyInfo &E : pinfo) {
		if (E.usage & PROPERTY_USAGE_STORAGE) {
			bool node_reference = (E.type == Variant::OBJECT && E.hint == PROPERTY_HINT_NODE_TYPE);
			if (p_node_references_only && !node_reference) {
				continue;
			}
			bool is_valid_revert = false;
			Variant revert_value = EditorPropertyRevert::get_property_revert_value(p_node, E.name, &is_valid_revert);
			Variant current_value = p_node->get(E.name);
			if (is_valid_revert) {
				if (PropertyUtils::is_property_value_different(p_node, current_value, revert_value)) {
					// If this property is a direct node reference, save a NodePath instead to prevent corrupted references.
					if (node_reference) {
						Node *target_node = Object::cast_to<Node>(current_value);
						if (target_node) {
							modified_property_map[E.name] = p_node->get_path_to(target_node);
						}
					} else {
						modified_property_map[E.name] = current_value;
					}
				}
			}
		}
	}

	return modified_property_map;
}

HashMap<StringName, Variant> EditorSceneManager::get_modified_properties_reference_to_nodes(Node *p_node, List<Node *> &p_nodes_referenced_by) {
	HashMap<StringName, Variant> modified_property_map;

	List<PropertyInfo> pinfo;
	p_node->get_property_list(&pinfo);
	for (const PropertyInfo &E : pinfo) {
		if (E.usage & PROPERTY_USAGE_STORAGE) {
			if (E.type != Variant::OBJECT || E.hint != PROPERTY_HINT_NODE_TYPE) {
				continue;
			}
			Variant current_value = p_node->get(E.name);
			Node *target_node = Object::cast_to<Node>(current_value);
			if (target_node && p_nodes_referenced_by.find(target_node)) {
				modified_property_map[E.name] = p_node->get_path_to(target_node);
			}
		}
	}

	return modified_property_map;
}

void EditorSceneManager::update_node_from_node_modification_entry(Node *p_node, ModificationNodeEntry &p_node_modification) {
	if (p_node) {
		// First, attempt to restore the script property since it may affect the get_property_list method.
		Variant *script_property_table_entry = p_node_modification.property_table.getptr(CoreStringName(script));
		if (script_property_table_entry) {
			p_node->set_script(*script_property_table_entry);
		}

		// Get properties for this node.
		List<PropertyInfo> pinfo;
		p_node->get_property_list(&pinfo);

		// Get names of all valid property names.
		HashMap<StringName, bool> property_node_reference_table;
		for (const PropertyInfo &E : pinfo) {
			if (E.usage & PROPERTY_USAGE_STORAGE) {
				if (E.type == Variant::OBJECT && E.hint == PROPERTY_HINT_NODE_TYPE) {
					property_node_reference_table[E.name] = true;
				} else {
					property_node_reference_table[E.name] = false;
				}
			}
		}

		// Restore the modified properties for this node.
		for (const KeyValue<StringName, Variant> &E : p_node_modification.property_table) {
			bool *property_node_reference_table_entry = property_node_reference_table.getptr(E.key);
			if (property_node_reference_table_entry) {
				// If the property is a node reference, attempt to restore from the node path instead.
				bool is_node_reference = *property_node_reference_table_entry;
				if (is_node_reference) {
					if (E.value.get_type() == Variant::NODE_PATH) {
						p_node->set(E.key, p_node->get_node_or_null(E.value));
					}
				} else {
					p_node->set(E.key, E.value);
				}
			}
		}

		// Restore the connections to other nodes.
		for (const ConnectionWithNodePath &E : p_node_modification.connections_to) {
			Connection conn = E.connection;

			// Get the node the callable is targeting.
			Node *target_node = Object::cast_to<Node>(conn.callable.get_object());

			// If the callable object no longer exists or is marked for deletion,
			// attempt to reaccquire the closest match by using the node path
			// we saved earlier.
			if (!target_node || !target_node->is_queued_for_deletion()) {
				target_node = p_node->get_node_or_null(E.node_path);
			}

			if (target_node) {
				// Reconstruct the callable.
				Callable new_callable = Callable(target_node, conn.callable.get_method());

				if (!p_node->is_connected(conn.signal.get_name(), new_callable)) {
					ERR_FAIL_COND(p_node->connect(conn.signal.get_name(), new_callable, conn.flags) != OK);
				}
			}
		}

		// Restore the connections from other nodes.
		for (const Connection &E : p_node_modification.connections_from) {
			Connection conn = E;

			bool valid = p_node->has_method(conn.callable.get_method()) || Ref<Script>(p_node->get_script()).is_null() || Ref<Script>(p_node->get_script())->has_method(conn.callable.get_method());
			ERR_CONTINUE_MSG(!valid, vformat("Attempt to connect signal '%s.%s' to nonexistent method '%s.%s'.", conn.signal.get_object()->get_class(), conn.signal.get_name(), conn.callable.get_object()->get_class(), conn.callable.get_method()));

			// Get the object which the signal is connected from.
			Object *source_object = conn.signal.get_object();

			if (source_object) {
				ERR_FAIL_COND(source_object->connect(conn.signal.get_name(), Callable(p_node, conn.callable.get_method()), conn.flags) != OK);
			}
		}

		// Re-add the groups.
		for (const Node::GroupInfo &E : p_node_modification.groups) {
			p_node->add_to_group(E.name, E.persistent);
		}
	}
}

bool EditorSceneManager::is_additional_node_in_scene(Node *p_edited_scene, Node *p_reimported_root, Node *p_node) {
	if (p_node == p_reimported_root) {
		return false;
	}

	bool node_part_of_subscene = p_node != p_edited_scene &&
			p_edited_scene->get_scene_inherited_state().is_valid() &&
			p_edited_scene->get_scene_inherited_state()->find_node_by_path(p_edited_scene->get_path_to(p_node)) >= 0 &&
			// It's important to process added nodes from the base scene in the inherited scene as
			// additional nodes to ensure they do not disappear on reload.
			// When p_reimported_root == p_edited_scene that means the edited scene
			// is the reimported scene, in that case the node is in the root base scene,
			// so it's not an addition, otherwise, the node would be added twice on reload.
			(p_node->get_owner() != p_edited_scene || p_reimported_root == p_edited_scene);

	if (node_part_of_subscene) {
		return false;
	}

	// Loop through the owners until either we reach the root node or nullptr
	Node *valid_node_owner = p_node->get_owner();
	while (valid_node_owner) {
		if (valid_node_owner == p_reimported_root) {
			break;
		}
		valid_node_owner = valid_node_owner->get_owner();
	}

	// When the owner is the imported scene and the owner is also the edited scene,
	// that means the node was added in the current edited scene.
	// We can be sure here because if the node that the node does not come from
	// the base scene because we checked just over with 'get_scene_inherited_state()->find_node_by_path'.
	if (valid_node_owner == p_reimported_root && p_reimported_root != p_edited_scene) {
		return false;
	}

	return true;
}

void EditorSceneManager::get_scene_editor_data_for_node(Node *p_root, Node *p_node, HashMap<NodePath, SceneEditorDataEntry> &p_table) {
	SceneEditorDataEntry new_entry;
	new_entry.is_display_folded = p_node->is_displayed_folded();

	if (p_root != p_node) {
		new_entry.is_editable = p_root->is_editable_instance(p_node);
	}

	p_table.insert(p_root->get_path_to(p_node), new_entry);

	for (int i = 0; i < p_node->get_child_count(); i++) {
		get_scene_editor_data_for_node(p_root, p_node->get_child(i), p_table);
	}
}

void EditorSceneManager::get_preload_scene_modification_table(
		Node *p_edited_scene,
		Node *p_reimported_root,
		Node *p_node, InstanceModificationsEntry &p_instance_modifications) {
	if (is_additional_node_in_scene(p_edited_scene, p_reimported_root, p_node)) {
		// Only save additional nodes which have an owner since this was causing issues transient ownerless nodes
		// which get recreated upon scene tree entry.
		// For now instead, assume all ownerless nodes are transient and will have to be recreated.
		if (p_node->get_owner()) {
			HashMap<StringName, Variant> modified_properties = get_modified_properties_for_node(p_node, true);
			if (p_node->get_owner() == p_edited_scene) {
				AdditiveNodeEntry new_additive_node_entry;
				new_additive_node_entry.node = p_node;
				new_additive_node_entry.parent = p_reimported_root->get_path_to(p_node->get_parent());
				new_additive_node_entry.owner = p_node->get_owner();
				new_additive_node_entry.index = p_node->get_index();

				Node2D *node_2d = Object::cast_to<Node2D>(p_node);
				if (node_2d) {
					new_additive_node_entry.transform_2d = node_2d->get_transform();
				}
				Node3D *node_3d = Object::cast_to<Node3D>(p_node);
				if (node_3d) {
					new_additive_node_entry.transform_3d = node_3d->get_transform();
				}

				p_instance_modifications.addition_list.push_back(new_additive_node_entry);
			}
			if (!modified_properties.is_empty()) {
				ModificationNodeEntry modification_node_entry;
				modification_node_entry.property_table = modified_properties;

				p_instance_modifications.modifications[p_reimported_root->get_path_to(p_node)] = modification_node_entry;
			}
		}
	} else {
		HashMap<StringName, Variant> modified_properties = get_modified_properties_for_node(p_node, false);

		// Find all valid connections to other nodes.
		List<Connection> connections_to;
		p_node->get_all_signal_connections(&connections_to);

		List<ConnectionWithNodePath> valid_connections_to;
		for (const Connection &c : connections_to) {
			Node *connection_target_node = Object::cast_to<Node>(c.callable.get_object());
			if (connection_target_node) {
				/// @todo Add support for reinstating custom callables
				if (!c.callable.is_custom()) {
					ConnectionWithNodePath connection_to;
					connection_to.connection = c;
					connection_to.node_path = p_node->get_path_to(connection_target_node);
					valid_connections_to.push_back(connection_to);
				}
			}
		}

		// Find all valid connections from other nodes.
		List<Connection> connections_from;
		p_node->get_signals_connected_to_this(&connections_from);

		List<Connection> valid_connections_from;
		for (const Connection &c : connections_from) {
			Node *source_node = Object::cast_to<Node>(c.signal.get_object());

			Node *valid_source_owner = nullptr;
			if (source_node) {
				valid_source_owner = source_node->get_owner();
				while (valid_source_owner) {
					if (valid_source_owner == p_reimported_root) {
						break;
					}
					valid_source_owner = valid_source_owner->get_owner();
				}
			}

			if (!source_node || valid_source_owner == nullptr) {
				/// @todo Add support for reinstating custom callables
				if (!c.callable.is_custom()) {
					valid_connections_from.push_back(c);
				}
			}
		}

		// Find all node groups.
		List<Node::GroupInfo> groups;
		p_node->get_groups(&groups);

		if (!modified_properties.is_empty() || !valid_connections_to.is_empty() || !valid_connections_from.is_empty() || !groups.is_empty()) {
			ModificationNodeEntry modification_node_entry;
			modification_node_entry.property_table = modified_properties;
			modification_node_entry.connections_to = valid_connections_to;
			modification_node_entry.connections_from = valid_connections_from;
			modification_node_entry.groups = groups;

			p_instance_modifications.modifications[p_reimported_root->get_path_to(p_node)] = modification_node_entry;
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		get_preload_scene_modification_table(p_edited_scene, p_reimported_root, p_node->get_child(i), p_instance_modifications);
	}
}

void EditorSceneManager::get_preload_modifications_reference_to_nodes(
		Node *p_root,
		Node *p_node,
		HashSet<Node *> &p_excluded_nodes,
		List<Node *> &p_instance_list_with_children,
		HashMap<NodePath, ModificationNodeEntry> &p_modification_table) {
	if (!p_excluded_nodes.find(p_node)) {
		HashMap<StringName, Variant> modified_properties = get_modified_properties_reference_to_nodes(p_node, p_instance_list_with_children);

		if (!modified_properties.is_empty()) {
			ModificationNodeEntry modification_node_entry;
			modification_node_entry.property_table = modified_properties;

			p_modification_table[p_root->get_path_to(p_node)] = modification_node_entry;
		}

		for (int i = 0; i < p_node->get_child_count(); i++) {
			get_preload_modifications_reference_to_nodes(p_root, p_node->get_child(i), p_excluded_nodes, p_instance_list_with_children, p_modification_table);
		}
	}
}

void EditorSceneManager::get_children_nodes(Node *p_node, List<Node *> &p_nodes) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		p_nodes.push_back(child);
		get_children_nodes(child, p_nodes);
	}
}

void EditorSceneManager::replace_history_reimported_nodes(Node *p_original_root_node, Node *p_new_root_node, Node *p_node) {
	NodePath scene_path_to_node = p_original_root_node->get_path_to(p_node);
	Node *new_node = p_new_root_node->get_node_or_null(scene_path_to_node);
	if (new_node) {
		core->get_editor_selection_history()->replace_object(p_node->get_instance_id(), new_node->get_instance_id());
	} else {
		core->get_editor_selection_history()->replace_object(p_node->get_instance_id(), ObjectID());
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		replace_history_reimported_nodes(p_original_root_node, p_new_root_node, p_node->get_child(i));
	}
}

void EditorSceneManager::find_all_instances_inheriting_path_in_node(Node *p_root, Node *p_node, const String &p_instance_path, HashSet<Node *> &p_instance_list) {
	String scene_file_path = p_node->get_scene_file_path();

	bool valid_instance_found = false;

	// Attempt to find all the instances matching path we're going to reload.
	if (p_node->get_scene_file_path() == p_instance_path) {
		valid_instance_found = true;
	} else {
		Node *current_node = p_node;

		Ref<SceneState> inherited_state = current_node->get_scene_inherited_state();
		while (inherited_state.is_valid()) {
			String inherited_path = inherited_state->get_path();
			if (inherited_path == p_instance_path) {
				valid_instance_found = true;
				break;
			}

			inherited_state = inherited_state->get_base_scene_state();
		}
	}

	// Instead of adding this instance directly, if its not owned by the scene, walk its ancestors
	// and find the first node still owned by the scene. This is what we will reloading instead.
	if (valid_instance_found) {
		Node *current_node = p_node;
		while (true) {
			if (current_node->get_owner() == p_root || current_node->get_owner() == nullptr) {
				p_instance_list.insert(current_node);
				break;
			}
			current_node = current_node->get_parent();
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		find_all_instances_inheriting_path_in_node(p_root, p_node->get_child(i), p_instance_path, p_instance_list);
	}
}

void EditorSceneManager::preload_reimporting_with_path_in_edited_scenes(const List<String> &p_scenes) {
	EditorProgress progress("preload_reimporting_scene", TTR("Preparing scenes for reload"), core->get_editor_data().get_edited_scene_count());

	int original_edited_scene_idx = core->get_editor_data().get_edited_scene();

	// Walk through each opened scene to get a global list of all instances which match
	// the current reimported scenes.
	for (int current_scene_idx = 0; current_scene_idx < core->get_editor_data().get_edited_scene_count(); current_scene_idx++) {
		progress.step(vformat(TTR("Analyzing scene %s"), core->get_editor_data().get_scene_title(current_scene_idx)), current_scene_idx);

		Node *edited_scene_root = core->get_editor_data().get_edited_scene_root(current_scene_idx);

		if (edited_scene_root) {
			SceneModificationsEntry scene_modifications;

			for (const String &instance_path : p_scenes) {
				if (core->get_editor_data().get_scene_path(current_scene_idx) == instance_path) {
					continue;
				}

				HashSet<Node *> instances_to_reimport;
				find_all_instances_inheriting_path_in_node(edited_scene_root, edited_scene_root, instance_path, instances_to_reimport);
				if (instances_to_reimport.size() > 0) {
					core->get_editor_data().set_edited_scene(current_scene_idx);

					List<Node *> instance_list_with_children;
					for (Node *original_node : instances_to_reimport) {
						InstanceModificationsEntry instance_modifications;

						// Fetching all the modified properties of the nodes reimported scene.
						get_preload_scene_modification_table(edited_scene_root, original_node, original_node, instance_modifications);

						instance_modifications.original_node = original_node;
						instance_modifications.instance_path = instance_path;
						scene_modifications.instance_list.push_back(instance_modifications);

						instance_list_with_children.push_back(original_node);
						get_children_nodes(original_node, instance_list_with_children);
					}

					// Search the scene to find nodes that references the nodes will be recreated.
					get_preload_modifications_reference_to_nodes(edited_scene_root, edited_scene_root, instances_to_reimport, instance_list_with_children, scene_modifications.other_instances_modifications);
				}
			}

			if (scene_modifications.instance_list.size() > 0) {
				scenes_modification_table[current_scene_idx] = scene_modifications;
			}
		}
	}

	core->get_editor_data().set_edited_scene(original_edited_scene_idx);

	progress.step(TTR("Preparation done."), core->get_editor_data().get_edited_scene_count());
}

void EditorSceneManager::reload_instances_with_path_in_edited_scenes() {
	if (scenes_modification_table.is_empty()) {
		return;
	}
	EditorProgress progress("reloading_scene", TTR("Scenes reloading"), core->get_editor_data().get_edited_scene_count());
	progress.step(TTR("Reloading..."), 0, true);

	Error err;
	Array replaced_nodes;
	HashMap<String, Ref<PackedScene>> local_scene_cache;

	// Reload the new instances.
	for (KeyValue<int, SceneModificationsEntry> &scene_modifications_elem : scenes_modification_table) {
		for (InstanceModificationsEntry instance_modifications : scene_modifications_elem.value.instance_list) {
			if (!local_scene_cache.has(instance_modifications.instance_path)) {
				Ref<PackedScene> instance_scene_packed_scene = ResourceLoader::load(instance_modifications.instance_path, "", ResourceFormatLoader::CACHE_MODE_REPLACE, &err);

				ERR_FAIL_COND(err != OK);
				ERR_FAIL_COND(instance_scene_packed_scene.is_null());

				local_scene_cache[instance_modifications.instance_path] = instance_scene_packed_scene;
			}
		}
	}

	// Save the current scene state/selection in case of lost.
	Dictionary editor_state = _get_main_scene_state();
	core->get_editor_data().save_edited_scene_state(core->get_editor_selection(), core->get_editor_selection_history(), editor_state);
	core->get_editor_selection()->clear();

	int original_edited_scene_idx = core->get_editor_data().get_edited_scene();

	for (KeyValue<int, SceneModificationsEntry> &scene_modifications_elem : scenes_modification_table) {
		// Set the current scene.
		int current_scene_idx = scene_modifications_elem.key;
		SceneModificationsEntry *scene_modifications = &scene_modifications_elem.value;

		core->get_editor_data().set_edited_scene(current_scene_idx);
		Node *current_edited_scene = core->get_editor_data().get_edited_scene_root(current_scene_idx);

		// Make sure the node is in the tree so that editor_selection can add node smoothly.
		if (original_edited_scene_idx != current_scene_idx) {
			// Prevent scene roots with the same name from being in the tree at the same time.
			Node *original_edited_scene_root = core->get_editor_data().get_edited_scene_root(original_edited_scene_idx);
			if (original_edited_scene_root && original_edited_scene_root->get_name() == current_edited_scene->get_name()) {
				shell->get_scene_root()->remove_child(original_edited_scene_root);
			}
			shell->get_scene_root()->add_child(current_edited_scene);
		}

		// Restore the state so that the selection can be updated.
		editor_state = core->get_editor_data().restore_edited_scene_state(core->get_editor_selection(), core->get_editor_selection_history());

		int current_history_id = core->get_editor_data().get_current_edited_scene_history_id();
		bool is_unsaved = EditorUndoRedoManager::get_singleton()->is_history_unsaved(current_history_id);

		// Clear the history for this affected tab.
		EditorUndoRedoManager::get_singleton()->clear_history(current_history_id, false);

		// Update the version
		core->get_editor_data().is_scene_changed(current_scene_idx);

		for (InstanceModificationsEntry instance_modifications : scene_modifications->instance_list) {
			Node *original_node = instance_modifications.original_node;
			String original_node_file_path = original_node->get_scene_file_path();
			Ref<PackedScene> instance_scene_packed_scene = local_scene_cache[instance_modifications.instance_path];

			// Load a replacement scene for the node.
			Ref<PackedScene> current_packed_scene;
			Ref<PackedScene> base_packed_scene;
			if (original_node_file_path == instance_modifications.instance_path) {
				// If the node file name directly matches the scene we're replacing,
				// just load it since we already cached it.
				current_packed_scene = instance_scene_packed_scene;
			} else {
				// Otherwise, check the inheritance chain, reloading and caching any scenes
				// we require along the way.
				List<String> required_load_paths;

				// Do we need to check if the paths are empty?
				if (!original_node_file_path.is_empty()) {
					required_load_paths.push_front(original_node_file_path);
				}
				Ref<SceneState> inherited_state = original_node->get_scene_inherited_state();
				while (inherited_state.is_valid()) {
					String inherited_path = inherited_state->get_path();
					// Do we need to check if the paths are empty?
					if (!inherited_path.is_empty()) {
						required_load_paths.push_front(inherited_path);
					}
					inherited_state = inherited_state->get_base_scene_state();
				}

				// Ensure the inheritance chain is loaded in the correct order so that cache can
				// be properly updated.
				for (String path : required_load_paths) {
					if (current_packed_scene.is_valid()) {
						base_packed_scene = current_packed_scene;
					}
					if (!local_scene_cache.find(path)) {
						current_packed_scene = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_REPLACE, &err);
						local_scene_cache[path] = current_packed_scene;
					} else {
						current_packed_scene = local_scene_cache[path];
					}
				}
			}

			ERR_FAIL_COND(current_packed_scene.is_null());

			// Instantiate early so that caches cleared on load in SceneState can be rebuilt early.
			Node *instantiated_node = nullptr;

			// If we are in a inherited scene, it's easier to create a new base scene and
			// grab the node from there.
			// When scene_path_to_node is '.' and we have scene_inherited_state, it's because
			// it's a multi-level inheritance scene. We should use
			NodePath scene_path_to_node = current_edited_scene->get_path_to(original_node);
			Ref<SceneState> scene_state = current_edited_scene->get_scene_inherited_state();
			if (String(scene_path_to_node) != "." && scene_state.is_valid() && scene_state->get_path() != instance_modifications.instance_path && scene_state->find_node_by_path(scene_path_to_node) >= 0) {
				Node *root_node = scene_state->instantiate(SceneState::GenEditState::GEN_EDIT_STATE_INSTANCE);
				instantiated_node = root_node->get_node(scene_path_to_node);

				if (instantiated_node) {
					if (instantiated_node->get_parent()) {
						// Remove from the root so we can delete it from memory.
						instantiated_node->get_parent()->remove_child(instantiated_node);
						// No need of the additional children that could have been added to the node
						// in the base scene. That will be managed by the 'addition_list' later.
						_remove_all_not_owned_children(instantiated_node, instantiated_node);
						memdelete(root_node);
					}
				} else {
					// Should not happen because we checked with find_node_by_path before, just in case.
					memdelete(root_node);
				}
			}

			if (!instantiated_node) {
				// If no base scene was found to create the node, we will use the reimported packed scene directly.
				// But, when the current edited scene is the reimported scene, it's because it's an inherited scene
				// derived from the reimported scene. In that case, we will not instantiate current_packed_scene, because
				// we would reinstantiate ourself. Using the base scene is better.
				if (current_edited_scene == original_node) {
					if (base_packed_scene.is_valid()) {
						instantiated_node = base_packed_scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
					} else {
						instantiated_node = instance_scene_packed_scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
					}
				} else {
					instantiated_node = current_packed_scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
				}
			}
			ERR_FAIL_NULL(instantiated_node);

			// Disconnect all relevant connections, all connections from and persistent connections to.
			for (const KeyValue<NodePath, ModificationNodeEntry> &modification_table_entry : instance_modifications.modifications) {
				for (Connection conn : modification_table_entry.value.connections_from) {
					conn.signal.get_object()->disconnect(conn.signal.get_name(), conn.callable);
				}
				for (ConnectionWithNodePath cwnp : modification_table_entry.value.connections_to) {
					Connection conn = cwnp.connection;
					if (conn.flags & CONNECT_PERSIST) {
						conn.signal.get_object()->disconnect(conn.signal.get_name(), conn.callable);
					}
				}
			}

			// Store all the paths for any selected nodes which are ancestors of the node we're replacing.
			List<NodePath> selected_node_paths;
			for (Node *selected_node : core->get_editor_selection()->get_top_selected_node_list()) {
				if (selected_node == original_node || original_node->is_ancestor_of(selected_node)) {
					selected_node_paths.push_back(original_node->get_path_to(selected_node));
					core->get_editor_selection()->remove_node(selected_node);
				}
			}

			// Remove all nodes which were added as additional elements (they will be restored later).
			for (AdditiveNodeEntry additive_node_entry : instance_modifications.addition_list) {
				Node *addition_node = additive_node_entry.node;
				addition_node->get_parent()->remove_child(addition_node);
			}

			/// @todo Clear ownership of the nodes (kind of hack to workaround an issue with
			/// replace_by when called on nodes in other tabs).
			List<Node *> nodes_owned_by_original_node;
			original_node->get_owned_by(original_node, &nodes_owned_by_original_node);
			for (Node *owned_node : nodes_owned_by_original_node) {
				owned_node->set_owner(nullptr);
			}

			// Replace the old nodes in the history with the new ones.
			// Otherwise, the history will contain old nodes, and some could still be
			// instantiated if used elsewhere, causing the "current edited item" to be
			// linked to a node that will be destroyed later. This caused the editor to
			// crash when reimporting scenes with animations when "Editable children" was enabled.
			replace_history_reimported_nodes(original_node, instantiated_node, original_node);

			// Reset the editable instance state.
			HashMap<NodePath, SceneEditorDataEntry> scene_editor_data_table;
			Node *owner = original_node->get_owner();
			if (!owner) {
				owner = original_node;
			}

			get_scene_editor_data_for_node(owner, original_node, scene_editor_data_table);

			// The current node being reloaded may also be an additional node for another node
			// that is in the process of being reloaded.
			// Replacing the additional node with the new one prevents a crash where nodes
			// in 'addition_list' are removed from the scene tree and queued for deletion.
			for (InstanceModificationsEntry &im : scene_modifications->instance_list) {
				for (AdditiveNodeEntry &additive_node_entry : im.addition_list) {
					if (additive_node_entry.node == original_node) {
						additive_node_entry.node = instantiated_node;
					}
				}
			}

			bool original_node_scene_instance_load_placeholder = original_node->get_scene_instance_load_placeholder();

			// Delete all the remaining node children.
			while (original_node->get_child_count()) {
				Node *child = original_node->get_child(0);

				original_node->remove_child(child);
				child->queue_free();
			}

			// Update the name to match
			instantiated_node->set_name(original_node->get_name());

			// Is this replacing the edited root node?

			if (current_edited_scene == original_node) {
				// Set the instance as un inherited scene of itself.
				instantiated_node->set_scene_inherited_state(instantiated_node->get_scene_instance_state());
				instantiated_node->set_scene_instance_state(nullptr);
				instantiated_node->set_scene_file_path(original_node_file_path);
				current_edited_scene = instantiated_node;
				core->get_editor_data().set_edited_scene_root(current_edited_scene);

				if (original_edited_scene_idx == current_scene_idx) {
					// How that the editor executes a redraw while destroying or progressing the EditorProgress,
					// it crashes when the root scene has been replaced because the edited scene
					// was freed and no longer in the scene tree.
					SceneTreeDock::get_singleton()->set_edited_scene(current_edited_scene);
					if (get_tree()) {
						get_tree()->set_edited_scene_root(current_edited_scene);
					}
				}
			}

			// Replace the original node with the instantiated version.
			original_node->replace_by(instantiated_node, false);

			// Mark the old node for deletion.
			original_node->queue_free();

			// Restore the placeholder state from the original node.
			instantiated_node->set_scene_instance_load_placeholder(original_node_scene_instance_load_placeholder);

			// Attempt to re-add all the additional nodes.
			for (AdditiveNodeEntry additive_node_entry : instance_modifications.addition_list) {
				Node *parent_node = instantiated_node->get_node_or_null(additive_node_entry.parent);

				if (!parent_node) {
					parent_node = current_edited_scene;
				}

				parent_node->add_child(additive_node_entry.node);
				parent_node->move_child(additive_node_entry.node, additive_node_entry.index);
				// If the additive node's owner was the node which got replaced, update it.
				if (additive_node_entry.owner == original_node) {
					additive_node_entry.owner = instantiated_node;
				}

				additive_node_entry.node->set_owner(additive_node_entry.owner);

				// If the parent node was lost, attempt to restore the original global transform.
				{
					Node2D *node_2d = Object::cast_to<Node2D>(additive_node_entry.node);
					if (node_2d) {
						node_2d->set_transform(additive_node_entry.transform_2d);
					}

					Node3D *node_3d = Object::cast_to<Node3D>(additive_node_entry.node);
					if (node_3d) {
						node_3d->set_transform(additive_node_entry.transform_3d);
					}
				}
			}

			// Restore the scene's editable instance and folded states.
			for (HashMap<NodePath, SceneEditorDataEntry>::Iterator I = scene_editor_data_table.begin(); I; ++I) {
				Node *node = owner->get_node_or_null(I->key);
				if (node) {
					if (owner != node) {
						owner->set_editable_instance(node, I->value.is_editable);
					}
					node->set_display_folded(I->value.is_display_folded);
				}
			}

			// Restore the selection.
			if (selected_node_paths.size()) {
				for (NodePath selected_node_path : selected_node_paths) {
					Node *selected_node = instantiated_node->get_node_or_null(selected_node_path);
					if (selected_node) {
						core->get_editor_selection()->add_node(selected_node);
					}
				}
				core->get_editor_selection()->update();
			}

			// Attempt to restore the modified properties and signals for the instantitated node and all its owned children.
			for (KeyValue<NodePath, ModificationNodeEntry> &E : instance_modifications.modifications) {
				NodePath new_current_path = E.key;
				Node *modifiable_node = instantiated_node->get_node_or_null(new_current_path);

				update_node_from_node_modification_entry(modifiable_node, E.value);
			}
			// Add the newly instantiated node to the edited scene's replaced node list.
			replaced_nodes.push_back(instantiated_node);
		}

		// Attempt to restore the modified properties and signals for the instantitated node and all its owned children.
		for (KeyValue<NodePath, ModificationNodeEntry> &E : scene_modifications->other_instances_modifications) {
			NodePath new_current_path = E.key;
			Node *modifiable_node = current_edited_scene->get_node_or_null(new_current_path);

			if (modifiable_node) {
				update_node_from_node_modification_entry(modifiable_node, E.value);
			}
		}

		if (is_unsaved) {
			EditorUndoRedoManager::get_singleton()->set_history_as_unsaved(current_history_id);
		}

		// Save the current handled scene state.
		core->get_editor_data().save_edited_scene_state(core->get_editor_selection(), core->get_editor_selection_history(), editor_state);
		core->get_editor_selection()->clear();

		// Cleanup the history of the changes.
		core->get_editor_selection_history()->cleanup_history();

		if (original_edited_scene_idx != current_scene_idx) {
			shell->get_scene_root()->remove_child(current_edited_scene);

			// Ensure the current edited scene is re-added if removed earlier because it has the same name
			// as the reimported scene. The editor could crash when reloading SceneTreeDock if the current
			// edited scene is not in the scene tree.
			Node *original_edited_scene_root = core->get_editor_data().get_edited_scene_root(original_edited_scene_idx);
			if (original_edited_scene_root && !original_edited_scene_root->get_parent()) {
				shell->get_scene_root()->add_child(original_edited_scene_root);
			}
		}
	}

	// For the whole editor, call the _notify_nodes_scene_reimported with a list of replaced nodes.
	// To inform anything that depends on them that they should update as appropriate.
	_notify_nodes_scene_reimported(shell->get_scene_root(), replaced_nodes);

	core->get_editor_data().set_edited_scene(original_edited_scene_idx);

	core->get_editor_data().restore_edited_scene_state(core->get_editor_selection(), core->get_editor_selection_history());

	progress.step(TTR("Reloading done."), core->get_editor_data().get_edited_scene_count());
}

void EditorSceneManager::_remove_all_not_owned_children(Node *p_node, Node *p_owner) {
	Vector<Node *> nodes_to_remove;
	if (p_node != p_owner && p_node->get_owner() != p_owner) {
		nodes_to_remove.push_back(p_node);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child_node = p_node->get_child(i);
		_remove_all_not_owned_children(child_node, p_owner);
	}

	for (Node *node : nodes_to_remove) {
		node->get_parent()->remove_child(node);
		node->queue_free();
	}
}

void EditorSceneManager::_notify_nodes_scene_reimported(Node *p_node, Array p_reimported_nodes) {
	Skeleton3D *skel_3d = Object::cast_to<Skeleton3D>(p_node);
	if (skel_3d) {
		skel_3d->reset_bone_poses();
	} else {
		BoneAttachment3D *attachment = Object::cast_to<BoneAttachment3D>(p_node);
		if (attachment) {
			attachment->notify_rebind_required();
		}
	}

	if (p_node->has_method("_nodes_scene_reimported")) {
		p_node->call("_nodes_scene_reimported", p_reimported_nodes);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_notify_nodes_scene_reimported(p_node->get_child(i), p_reimported_nodes);
	}
}

void EditorSceneManager::reload_scene(const String &p_path) {
	int scene_idx = -1;

	String lpath = ProjectSettings::get_singleton()->localize_path(p_path);

	for (int i = 0; i < core->get_editor_data().get_edited_scene_count(); i++) {
		if (core->get_editor_data().get_scene_path(i) == lpath) {
			scene_idx = i;
			break;
		}
	}

	int current_tab = core->get_editor_data().get_edited_scene();

	if (scene_idx == -1) {
		if (get_edited_scene()) {
			int current_history_id = core->get_editor_data().get_current_edited_scene_history_id();
			bool is_unsaved = EditorUndoRedoManager::get_singleton()->is_history_unsaved(current_history_id);

			// Scene is not open, so at it might be instantiated. We'll refresh the whole scene later.
			EditorUndoRedoManager::get_singleton()->clear_history(current_history_id, false);
			if (is_unsaved) {
				EditorUndoRedoManager::get_singleton()->set_history_as_unsaved(current_history_id);
			}
		}
		return;
	}

	if (current_tab == scene_idx) {
		core->get_editor_data().apply_changes_in_editors();
		_save_editor_states(p_path);
	}

	// Reload scene.
	_remove_scene(scene_idx, false);
	load_scene(p_path, true, false, true);

	// Adjust index so tab is back a the previous position.
	core->get_editor_data().move_edited_scene_to_index(scene_idx);
	EditorUndoRedoManager::get_singleton()->clear_history(core->get_editor_data().get_scene_history_id(scene_idx), false);

	// Recover the tab.
	shell->get_scene_tabs()->set_current_tab(current_tab);
}

bool EditorSceneManager::is_scene_open(const String &p_path) {
	for (int i = 0; i < core->get_editor_data().get_edited_scene_count(); i++) {
		if (core->get_editor_data().get_scene_path(i) == p_path) {
			return true;
		}
	}

	return false;
}

bool EditorSceneManager::is_scene_in_use(const String &p_path) {
	Node *es = get_edited_scene();
	if (es) {
		return _find_scene_in_use(es, p_path);
	}
	return false;
}

bool EditorSceneManager::_find_scene_in_use(Node *p_node, const String &p_path) const {
	if (p_node->get_scene_file_path() == p_path) {
		return true;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		if (_find_scene_in_use(p_node->get_child(i), p_path)) {
			return true;
		}
	}

	return false;
}

bool EditorSceneManager::has_scenes_in_session() {
	if (!bool(EDITOR_GET("interface/scene_tabs/restore_scenes_on_load"))) {
		return false;
	}
	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));
	if (err != OK) {
		return false;
	}
	if (!config->has_section(EDITOR_NODE_CONFIG_SECTION) || !config->has_section_key(EDITOR_NODE_CONFIG_SECTION, "open_scenes")) {
		return false;
	}
	Array scenes = config->get_value(EDITOR_NODE_CONFIG_SECTION, "open_scenes");
	return !scenes.is_empty();
}

void EditorSceneManager::request_instantiate_scene(const String &p_path) {
	SceneTreeDock::get_singleton()->instantiate(p_path);
}

void EditorSceneManager::request_instantiate_scenes(const Vector<String> &p_files) {
	SceneTreeDock::get_singleton()->instantiate_scenes(p_files);
}

void EditorSceneManager::save_all_open_scenes() {
	shell->get_project_run_bar()->stop_playing();
	_save_all_scenes();
}

void EditorSceneManager::save_scene_if_open(const String &p_scene_path) {
	int idx = core->get_editor_data().get_edited_scene_from_path(p_scene_path);
	if (idx >= 0) {
		_save_scene(p_scene_path, idx);
	}
}

void EditorSceneManager::save_scene_list(const HashSet<String> &p_scene_paths) {
	for (int i = 0; i < core->get_editor_data().get_edited_scene_count(); i++) {
		Node *scene = core->get_editor_data().get_edited_scene_root(i);

		if (scene && p_scene_paths.has(scene->get_scene_file_path())) {
			_save_scene(scene->get_scene_file_path(), i);
		}
	}
}

void EditorSceneManager::save_before_run() {
	shell->set_current_menu_option(EditorShell::SAVE_AND_RUN);
	shell->trigger_menu_option(EditorShell::SCENE_SAVE_AS_SCENE, true);
	shell->get_file_dialog()->set_title(TTR("Save scene before running..."));
}

void EditorSceneManager::try_autosave() {
	if (!bool(EDITOR_GET("run/auto_save/save_before_running"))) {
		return;
	}

	if (unsaved_cache) {
		Node *scene = core->get_editor_data().get_edited_scene_root();

		if (scene && !scene->get_scene_file_path().is_empty()) { // Only autosave if there is a scene and if it has a path.
			shell->_save_scene_with_preview(scene->get_scene_file_path());
		}
	}
	shell->trigger_menu_option(EditorShell::SCENE_SAVE_ALL_SCENES, false);
	core->get_editor_data().save_editor_external_data();
}

void EditorSceneManager::_save_scene_silently() {
	Node *scene = core->get_editor_data().get_edited_scene_root();
	if (scene && !scene->get_scene_file_path().is_empty() && DirAccess::exists(scene->get_scene_file_path().get_base_dir())) {
		_save_scene(scene->get_scene_file_path());
		shell->save_editor_layout_delayed();
	}
}

int EditorSceneManager::_save_external_resources(bool p_also_save_external_data) {
	int flg = 0;
	if (EDITOR_GET("filesystem/on_save/compress_binary_resources")) {
		flg |= ResourceSaver::FLAG_COMPRESS;
	}
	flg |= ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;

	HashSet<String> edited_resources;
	int saved = 0;
	List<Ref<Resource>> cached;
	ResourceCache::get_cached_resources(&cached);

	for (Ref<Resource> res : cached) {
		if (!res->is_edited()) {
			continue;
		}

		String path = res->get_path();
		if (path.begins_with("res://")) {
			int subres_pos = path.find("::");
			if (subres_pos == -1) {
				// Actual resource.
				edited_resources.insert(path);
			} else {
				edited_resources.insert(path.substr(0, subres_pos));
			}
		}

		res->set_edited(false);
	}

	bool script_was_saved = false;
	for (const String &E : edited_resources) {
		Ref<Resource> res = ResourceCache::get_ref(E);
		if (res.is_null()) {
			continue; // Maybe it was erased in a thread, who knows.
		}
		Ref<PackedScene> ps = res;
		if (ps.is_valid()) {
			continue; // Do not save PackedScenes, this will mess up the editor.
		}
		if (!script_was_saved) {
			Ref<Script> scr = res;
			script_was_saved = scr.is_valid();
		}
		ResourceSaver::save(res, res->get_path(), flg);
		saved++;
	}

	if (script_was_saved) {
		ScriptEditor::get_singleton()->update_script_times();
	}

	if (p_also_save_external_data) {
		for (int i = 0; i < core->get_editor_data().get_editor_plugin_count(); i++) {
			EditorPlugin *plugin = core->get_editor_data().get_editor_plugin(i);
			if (!plugin->get_unsaved_status().is_empty()) {
				plugin->save_external_data();
				saved++;
			}
		}
	}

	EditorUndoRedoManager::get_singleton()->set_history_as_saved(EditorUndoRedoManager::GLOBAL_HISTORY);
	_update_unsaved_cache();

	return saved;
}

void EditorSceneManager::undo() {
	shell->trigger_menu_option(EditorShell::SCENE_UNDO, true);
}

void EditorSceneManager::redo() {
	shell->trigger_menu_option(EditorShell::SCENE_REDO, true);
}

void EditorSceneManager::mark_scene_as_unsaved() {
	ERR_FAIL_NULL(core);
	EditorUndoRedoManager::get_singleton()->set_history_as_unsaved(core->get_editor_data().get_current_edited_scene_history_id());
	if (shell) {
		shell->get_scene_tabs()->update_scene_tabs();
	}
}

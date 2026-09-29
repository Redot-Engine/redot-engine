/**************************************************************************/
/*  editor_scene_manager.h                                                */
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

#include "core/object/ref_counted.h"
#include "core/templates/list.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/animation/animation_mixer.h"
#include "scene/main/node.h"

class EditorCore;
class EditorPluginManager;
class EditorShell;
class Resource;

/**
 * Owns the editor's open-scene session state: which scenes are loaded, which
 * is current, which are unsaved, and which object is being edited.
 *
 * Functional services are reached through `core` (`EditorCore`), the plugin
 * registry through `plugin_manager`, and shell UI (scene tabs, dialogs,
 * menus, title) through `shell` (`EditorShell`).
 */
class EditorSceneManager : public Node {
	GDCLASS(EditorSceneManager, Node);

	friend class EditorCore;

	/// @note Any class that is intended to build an editor UI must be friended here!
	friend class EditorNode;

	static EditorSceneManager *singleton;

	/// The core that owns this scene manager. Set in the constructor.
	EditorCore *core = nullptr;
	/// The active editor shell. Set by `EditorCore::set_shell()`; `nullptr` until then.
	EditorShell *shell = nullptr;
	/// The plugin registry. Set by `EditorCore::_initialize_services()`; `nullptr` until then.
	EditorPluginManager *plugin_manager = nullptr;

	struct AdditiveNodeEntry {
		Node *node = nullptr;
		NodePath parent;
		Node *owner = nullptr;
		int index = 0;
		/// Used if the original parent node is lost
		Transform2D transform_2d;
		Transform3D transform_3d;
	};

	struct ConnectionWithNodePath {
		Connection connection;
		NodePath node_path;
	};

	struct ModificationNodeEntry {
		HashMap<StringName, Variant> property_table;
		List<ConnectionWithNodePath> connections_to;
		List<Connection> connections_from;
		List<Node::GroupInfo> groups;
	};

	struct InstanceModificationsEntry {
		Node *original_node;
		String instance_path;
		List<Node *> instance_list;
		HashMap<NodePath, ModificationNodeEntry> modifications;
		List<AdditiveNodeEntry> addition_list;
	};

	struct SceneModificationsEntry {
		List<InstanceModificationsEntry> instance_list;
		HashMap<NodePath, ModificationNodeEntry> other_instances_modifications;
	};

	struct SceneEditorDataEntry {
		bool is_editable = false;
		bool is_display_folded = false;
	};

	/// Per-open-scene modifications captured before a reimport replaces instanced scenes.
	HashMap<int, SceneModificationsEntry> scenes_modification_table;
	/// Scene files being reimported (current reimport batch).
	List<String> scenes_reimported;
	/// Non-scene resources being reimported (current reimport batch).
	List<String> resources_reimported;
	/// Scene path pending an open-imported / new-inherited dialog confirmation.
	String open_import_request;

	/// Index of the tab currently being closed, while a close confirmation is pending.
	int tab_closing_idx = 0;
	/// Scene paths queued for closing once pending confirmations resolve.
	List<String> tabs_to_close;
	/// Tab indices queued for "save as" before closing.
	List<int> scenes_to_save_as;
	/// Menu option that was in progress when a tab-close confirmation interrupted it.
	int tab_closing_menu_option = -1;

	/// Paths of recently closed scenes, used by "Reopen Closed Scene".
	List<String> prev_closed_scenes;
	/// Scene path deferred until the editor finishes its first filesystem scan.
	String defer_load_scene;
	/// Scene node most recently instantiated from the filesystem dock.
	Node *_last_instantiated_scene = nullptr;

	/// True while a scene load/close is mutating the edited scene tree.
	bool changing_scene = false;
	/// True while restoring the previous session's open scenes at startup.
	bool restoring_scenes = false;
	/// Cached "any unsaved changes" flag; UI reads it via has_unsaved_changes().
	bool unsaved_cache = false;
	/// Path of the scene currently being saved, valid during _save_scene().
	String saving_scene;
	/// File pending an import-then-reload cycle.
	String import_reload_fn;

	/// The object currently being edited/inspected, regardless of scene.
	Object *current = nullptr;

	/// Sets the active editor shell. Called by `EditorCore::set_shell()`.
	void set_shell(EditorShell *p_shell) { shell = p_shell; }
	/// Sets the plugin registry. Called by `EditorCore::_initialize_services()`.
	void set_plugin_manager(EditorPluginManager *p_plugin_manager) { plugin_manager = p_plugin_manager; }

	/// Backs up and resets AnimationMixer state on the scene being saved so
	/// editor-time playback isn't serialized into the PackedScene.
	static void _reset_animation_mixers(Node *p_node, List<Pair<AnimationMixer *, Ref<AnimatedValuesBackup>>> *r_backups);
	/// Builds the "unsaved changes" body text listing the currently open,
	/// unsaved scenes for the close/discard confirmation dialog.
	static String _get_unsaved_scene_dialog_text(String p_scene_filename, uint64_t p_started_timestamp);

protected:
	static void _bind_methods();

public:
	static EditorSceneManager *get_singleton() { return singleton; }

	bool is_changing_scene() const { return changing_scene; } ///< @returns `true` while the editor is in the middle of switching scenes.
	bool has_unsaved_changes() const { return unsaved_cache; }
	bool has_previous_closed_scenes() const { return !prev_closed_scenes.is_empty(); } ///< @returns `true` if there are recently closed scenes that can be reopened.
	Object *get_current_object() const { return current; }

	int new_scene(); ///< Creates a new empty scene tab; @return New scene's index.
	/// Loads and opens a scene file in the editor, with options for ignoring broken dependencies or opening imported files.
	Error load_scene(const String &p_scene, bool p_ignore_broken_deps = false, bool p_set_inherited = false, bool p_force_open_imported = false, bool p_silent_change_tab = false);
	void set_edited_scene(Node *p_scene); ///< Sets the scene currently being edited.
	void set_edited_scene_root(Node *p_scene, bool p_auto_add); ///< Sets the root node of the edited scene, optionally adding it to the tree automatically.
	Node *get_edited_scene(); ///< @return The root node of the scene currently being edited.

	void edit_node(Node *p_node); ///< Makes the given scene node the currently inspected/edited object.
	void edit_resource(const Ref<Resource> &p_resource); ///< Makes the given resource the currently inspected/edited object.
	void edit_item(Object *p_object, Object *p_editing_owner); ///< Edits an object, recording the node that owns it for context.
	void push_item(Object *p_object, const String &p_property = "", bool p_inspector_only = false); ///< Pushes an object onto the inspector so the user can edit it (optionally focusing a specific property).
	void push_item_no_inspector(Object *p_object); ///< Edits an object without showing it in the inspector.
	void push_node_item(Node *p_node); ///< Convenience wrapper to edit a scene tree node.
	void edit_previous_item(); ///< Returns to the previously edited item in the inspector history.
	void hide_unused_editors(const Object *p_editing_owner = nullptr); ///< Hides editor plugins/screens that don't apply to the object being edited.

	void _update_unsaved_cache();
	void _update_undo_redo_allowed(); ///< Recomputes whether Undo/Redo menu items should be enabled.
	void _set_current_scene(int p_idx); ///< Switches the active scene tab, performing validity checks first.
	void _set_current_scene_nocheck(int p_idx);
	void _save_scene(String p_file, int p_idx = -1);
	void _save_all_scenes();
	/// Finds the next open scene with unsaved changes at or after index p_start.
	/// A scene counts as unsaved if its undo history is dirty or a plugin reports unsaved changes in it.
	/// @param p_valid_filename If true, skip scenes that have never been saved
	///        (no file path) — used when the caller needs a path to show or
	///        save to, e.g. listing unsaved scenes in the quit confirmation.
	/// @param p_start Scene index to begin searching from.
	/// @return Index of the next unsaved scene, or -1 if none remain.
	int _next_unsaved_scene(bool p_valid_filename, int p_start = 0);
	/// Completion step for actions that asked "save or discard?"
	/// Runs after the user clicks "Don't Save" in the confirmation dialogs
	/// (or immediately when nothing was unsaved), and finishes whatever
	/// operation was pending — closing a scene tab, reloading the scene
	/// from disk, quitting, or restarting — letting unsaved state be freed.
	/// @param p_str The dialog's custom action id (unused; required by the custom_action signal signature).
	void _discard_changes(const String &p_str = String());
	void _scene_tab_closed(int p_tab); ///< Begins the close-scene-tab flow for a given tab index.
	void _cancel_close_scene_tab(); ///< Cancels an in-progress scene-tab close (e.g. user pressed Cancel).
	void _remove_edited_scene(bool p_change_tab = true);
	void _remove_scene(int p_index, bool p_change_tab = true);
	void _mark_unsaved_scenes(); ///< Marks all scenes with unsaved modifications in their tab display.
	bool _is_scene_unsaved(int p_idx); ///< @return `true` if the scene at the given tab index has unsaved changes.
	void _update_prev_closed_scenes(const String &p_scene_path, bool p_add_scene); ///< Maintains the stack of recently closed scenes for "reopen" support.
	void _proceed_closing_scene_tabs(); ///< Continues closing queued scene tabs after confirmations resolve.
	void _proceed_save_asing_scene_tabs(); ///< Continues the "Save As" flow across multiple scene tabs.
	Dictionary _get_main_scene_state(); ///< Captures main-screen editor state (scroll, cursor, etc.) into a Dictionary.
	void _set_main_scene_state(Dictionary p_state, Node *p_for_scene); ///< Restores main-screen editor state captured by _get_main_scene_state.
	void _save_editor_states(const String &p_file, int p_idx = -1); ///< Saves per-scene editor state (selection, folding, etc.) alongside the scene.

	// ── Scene reimport / save operations ──────────────────────────────────

	/// @returns the scene path pending an open-imported / new-inherited dialog confirmation.
	String get_open_import_request() const { return open_import_request; }

	/// Handles the start of a resource reimport: records which open scenes instance the reimported files.
	void _resources_reimporting(const Vector<String> &p_resources);
	/// Handles the end of a resource reimport: reloads affected scenes and re-applies recorded modifications.
	void _resources_reimported(const Vector<String> &p_resources);

	/// Collects the properties on a node that differ from its instanced scene (i.e. local modifications).
	HashMap<StringName, Variant> get_modified_properties_for_node(Node *p_node, bool p_node_references_only);
	/// Collects modified properties that reference other nodes, and fills the list of nodes those properties point to.
	HashMap<StringName, Variant> get_modified_properties_reference_to_nodes(Node *p_node, List<Node *> &p_nodes_referenced_by);

	/// Applies a recorded set of property/connection/group modifications onto a node (used when scenes are reimported).
	void update_node_from_node_modification_entry(Node *p_node, ModificationNodeEntry &p_node_modification);
	/// Gathers per-node editor data (editable/folded state) for a scene subtree.
	void get_scene_editor_data_for_node(Node *p_root, Node *p_node, HashMap<NodePath, SceneEditorDataEntry> &p_table);
	/// Captures modifications made to scene instances before a reimport replaces them, so they can be re-applied afterwards.
	void get_preload_scene_modification_table(
			Node *p_edited_scene,
			Node *p_reimported_root,
			Node *p_node, InstanceModificationsEntry &p_instance_modifications);
	/// Collects modifications that reference nodes, ahead of a scene reimport.
	void get_preload_modifications_reference_to_nodes(
			Node *p_root,
			Node *p_node,
			HashSet<Node *> &p_excluded_nodes,
			List<Node *> &p_instance_list_with_children,
			HashMap<NodePath, ModificationNodeEntry> &p_modification_table);
	/// Fills a list with all descendant nodes of the given node.
	void get_children_nodes(Node *p_node, List<Node *> &p_nodes);
	/// @return `true` if a node was added on top of an instanced scene rather than coming from the original scene file.
	bool is_additional_node_in_scene(Node *p_edited_scene, Node *p_reimported_root, Node *p_node);
	/// Rewires selection/edit history entries so they point at nodes in the newly reimported scene instead of the old, freed nodes.
	void replace_history_reimported_nodes(Node *p_original_root_node, Node *p_new_root_node, Node *p_node);
	/// Recursively finds nodes that instance the given scene path, collecting them into the instance list.
	void find_all_instances_inheriting_path_in_node(Node *p_root, Node *p_node, const String &p_instance_path, HashSet<Node *> &p_instance_list);
	/// Removes children of p_node that aren't owned by p_owner (e.g. before reinstancing a scene).
	void _remove_all_not_owned_children(Node *p_node, Node *p_owner);
	/// Notifies nodes (and scripts) in the tree that their scenes were reimported.
	void _notify_nodes_scene_reimported(Node *p_node, Array p_reimported_nodes);

	/// Reloads an open scene from disk.
	void reload_scene(const String &p_path);
	/// Records modifications on open scenes that instance any of the given paths, before a reimport replaces them.
	void preload_reimporting_with_path_in_edited_scenes(const List<String> &p_scenes);
	/// Re-instantiates instances of reimported scenes in the open scenes, re-applying recorded modifications.
	void reload_instances_with_path_in_edited_scenes();

	/// @returns `true` if the given scene path is currently open in a tab.
	bool is_scene_open(const String &p_path);
	/// @returns `true` if the scene file is currently open in an editor tab.
	bool is_scene_in_use(const String &p_path);
	/// @return `true` if any node in the subtree instances the given scene path.
	bool _find_scene_in_use(Node *p_node, const String &p_path) const;
	/// @return `true` if any scenes are open in this editor session.
	bool has_scenes_in_session();

	/// Requests instancing a scene file into the current scene.
	void request_instantiate_scene(const String &p_path);
	/// Requests instancing multiple scene files into the current scene.
	void request_instantiate_scenes(const Vector<String> &p_files);

	/// Saves the current scene to the given path, optionally generating a preview thumbnail.
	void save_scene_to_path(String p_file, bool p_with_preview = true);
	/// Recursively validates that a scene and its nodes can be saved to disk
	/// (e.g. no cyclic instance inclusion of the scene itself).
	bool _validate_scene_recursive(const String &p_filename, Node *p_node);

	/// Saves every open scene.
	void save_all_open_scenes();
	/// Saves a scene if it is open.
	void save_scene_if_open(const String &p_scene_path);
	/// Saves every scene whose path is in the given set.
	void save_scene_list(const HashSet<String> &p_scene_paths);
	/// Saves everything that needs saving before running the project.
	void save_before_run();
	/// Triggers the autosave routine if configured.
	void try_autosave();
	/// Save scene without displaying progress dialog. Used to work around
	/// errors about parent node being busy setting up children
	/// when Save on Focus Loss kicks in.
	void _save_scene_silently();
	/// Saves external resources and modified subresources.
	/// @return How many were saved (or error count semantics used by callers).
	int _save_external_resources(bool p_also_save_external_data = false);

	/// Undoes the last edit.
	void undo();
	/// Redoes the last undo.
	void redo();
	/// Marks the current scene's undo history as unsaved and refreshes the scene tabs.
	void mark_scene_as_unsaved();

	EditorSceneManager();
};

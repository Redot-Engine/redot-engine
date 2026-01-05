/**************************************************************************/
/*  uid_viewer_dock.cpp                                                   */
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

//
// Created by Andrew Martin on 1/2/26.
//

#include "editor/docks/uid_viewer_dock.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_uid.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/main/window.h" // For MouseButton enum in Godot 4.3+
#include "servers/display_server.h"

UIDViewerDock::UIDViewerDock() {
	set_name("UID Viewer");

	set_mouse_filter(Control::MOUSE_FILTER_STOP);
	set_focus_mode(Control::FOCUS_ALL);

	// Top bar
	HBoxContainer *top_bar = memnew(HBoxContainer);
	add_child(top_bar);

	search_edit = memnew(LineEdit);
	search_edit->set_placeholder("Search for UID or path...");
	search_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	search_edit->connect("text_changed", callable_mp(this, &UIDViewerDock::_on_search_text_changed));
	top_bar->add_child(search_edit);

	refresh_button = memnew(Button);
	refresh_button->set_text("Refresh");
	refresh_button->connect("pressed", callable_mp(this, &UIDViewerDock::_on_refresh_pressed));
	top_bar->add_child(refresh_button);

	// Tree
	uid_tree = memnew(Tree);
	uid_tree->set_hide_root(true);
	uid_tree->set_columns(2);
	uid_tree->set_column_title(0, "UID");
	uid_tree->set_column_title(1, "Resource Path");
	uid_tree->set_column_custom_minimum_width(0, 280);
	uid_tree->set_column_expand(1, true);
	uid_tree->set_v_size_flags(SIZE_EXPAND_FILL);
	uid_tree->set_h_size_flags(SIZE_EXPAND_FILL);
	uid_tree->set_allow_rmb_select(true);
	uid_tree->connect("item_activated", callable_mp(this, &UIDViewerDock::_on_item_activated));
	add_child(uid_tree);

	// Context menu (owned by dock)
	context_menu = memnew(PopupMenu);
	context_menu->add_item("Copy UID");
	context_menu->add_item("Copy Path");
	context_menu->connect("id_pressed", callable_mp(this, &UIDViewerDock::_on_context_menu_id_pressed));
	add_child(context_menu);

	_refresh_uid_list();

	uid_tree->connect("item_mouse_selected", callable_mp(this, &UIDViewerDock::_on_tree_rmb_selected));

	// Auto-refresh on filesystem changes
	EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &UIDViewerDock::_refresh_uid_list));
}

void UIDViewerDock::_refresh_uid_list() {
	uid_tree->clear();
	TreeItem *root = uid_tree->create_item();

	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	if (!efs || !efs->get_filesystem()) {
		return;
	}

	Vector<String> queue;
	queue.push_back("res://");

	while (!queue.is_empty()) {
		String dir_path = queue[queue.size() - 1];
		queue.resize(queue.size() - 1);

		EditorFileSystemDirectory *dir = efs->get_filesystem_path(dir_path);
		if (!dir) {
			continue;
		}

		for (int i = 0; i < dir->get_file_count(); i++) {
			String file_path = dir_path + dir->get_file(i);

			int64_t uid = ResourceLoader::get_resource_uid(file_path);
			if (uid != -1) {
				String uid_text = ResourceUID::get_singleton()->id_to_text(uid);
				TreeItem *item = uid_tree->create_item(root);
				item->set_text(0, uid_text);
				item->set_text(1, file_path);
			}
		}

		for (int i = 0; i < dir->get_subdir_count(); i++) {
			EditorFileSystemDirectory *subdir = dir->get_subdir(i);
			queue.push_back(dir_path + subdir->get_name() + "/");
		}
	}

	// Re-apply current search
	_on_search_text_changed(search_edit->get_text());
}

void UIDViewerDock::_on_search_text_changed(const String &text) {
	String search_lower = text.to_lower().strip_edges();

	TreeItem *root = uid_tree->get_root();
	if (!root) {
		return;
	}

	if (search_lower.is_empty()) {
		_show_all_items(root);
		return;
	}

	root->set_visible(true);
	_filter_tree_recursive(root, search_lower);
}

void UIDViewerDock::_on_refresh_pressed() {
	EditorFileSystem::get_singleton()->scan();
	_refresh_uid_list();
}

void UIDViewerDock::_on_item_activated() {
	TreeItem *selected = uid_tree->get_selected();
	if (!selected) {
		return;
	}

	String file_path = selected->get_text(1);

	FileSystemDock *fs_dock = FileSystemDock::get_singleton();
	if (fs_dock) {
		fs_dock->navigate_to_path(file_path);
		fs_dock->select_file(file_path);
	}
}

void UIDViewerDock::_on_context_menu_id_pressed(int id) {
	if (!last_selected_item) {
		return;
	}

	String text_to_copy;
	if (id == 0) { // Copy UID
		text_to_copy = last_selected_item->get_text(0);
	} else if (id == 1) { // Copy Path
		text_to_copy = last_selected_item->get_text(1);
	}

	if (!text_to_copy.is_empty()) {
		DisplayServer::get_singleton()->clipboard_set(text_to_copy);
	}
}

void UIDViewerDock::_show_all_items(TreeItem *item) {
	if (!item) {
		item = uid_tree->get_root();
		if (!item) {
			return;
		}
	}

	item->set_visible(true);

	TreeItem *child = item->get_first_child();
	while (child) {
		_show_all_items(child);
		child = child->get_next();
	}
}

bool UIDViewerDock::_filter_tree_recursive(TreeItem *item, const String &search_lower) {
	if (!item) {
		return false;
	}

	bool has_visible_child = false;

	TreeItem *child = item->get_first_child();
	while (child) {
		bool child_visible = _filter_tree_recursive(child, search_lower);
		if (child_visible) {
			has_visible_child = true;
		}
		child = child->get_next();
	}

	String uid_text = item->get_text(0).to_lower();
	String path_text = item->get_text(1).to_lower();

	bool matches = uid_text.contains(search_lower) || path_text.contains(search_lower);
	bool visible = matches || has_visible_child;

	item->set_visible(visible);

	return visible;
}

void UIDViewerDock::_on_tree_rmb_selected(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}

	TreeItem *item = uid_tree->get_item_at_position(p_pos);
	if (item) {
		uid_tree->set_selected(item, 0);
		last_selected_item = item;

		if (context_menu) {
			context_menu->set_position(uid_tree->get_screen_position() + p_pos);
			context_menu->popup();
		}
	}
}

/**************************************************************************/
/*  uid_viewer_dock.h                                                     */
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

//
// Created by Andrew Martin on 1/2/26.
//

#ifndef UID_VIEWER_DOCK_H
#define UID_VIEWER_DOCK_H

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/tree.h"

class UIDViewerDock : public VBoxContainer {
private:
	Tree *uid_tree = nullptr;
	LineEdit *search_edit = nullptr;
	Button *refresh_button = nullptr;
	PopupMenu *context_menu = nullptr;
	TreeItem *last_selected_item = nullptr;

	void _refresh_uid_list();
	void _on_search_text_changed(const String &text);
	void _on_refresh_pressed();
	void _on_item_activated();
	void _on_context_menu_id_pressed(int id);
	void _gui_input(const Ref<InputEvent> &event);
	void _on_tree_rmb_selected(const Vector2 &p_pos, MouseButton p_button);

	void _show_all_items(TreeItem *item);
	bool _filter_tree_recursive(TreeItem *item, const String &search_lower);

public:
	UIDViewerDock();
};

#endif // UID_VIEWER_DOCK_H

/**************************************************************************/
/*  editor_plugin_list.h                                                  */
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
 * @file editor_plugin_list.h
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "core/object/object.h"
#include "editor/plugins/editor_plugin.h"

class Control;
class Camera3D;

/// Holds a set of plugins interested in a particular editor event channel
/// (viewport overlay drawing, input, etc.) and re-broadcasts each event to
/// all of them, so calling code invokes one method instead of tracking and
/// iterating plugins itself.
class EditorPluginList : public Object {
private:
	Vector<EditorPlugin *> plugins_list;

public:
	/// Replaces the list of plugins this wrapper dispatches to.
	void set_plugins_list(Vector<EditorPlugin *> p_plugins_list) {
		plugins_list = p_plugins_list;
	}
	/// @returns The list of plugins this wrapper dispatches to.
	Vector<EditorPlugin *> &get_plugins_list() {
		return plugins_list;
	}

	void make_visible(bool p_visible); ///< Shows or hides all plugins' UI contributions.
	void edit(Object *p_object); ///< Tells all plugins that the given object is being edited.
	bool forward_gui_input(const Ref<InputEvent> &p_event); ///< Calls the GUI input handler on each plugin in the list; @return `true` if a plugin consumed the event.
	void forward_canvas_draw_over_viewport(Control *p_overlay); ///< Calls the 2D draw-over callback on each plugin in the list, passing the viewport overlay to draw on.
	void forward_canvas_force_draw_over_viewport(Control *p_overlay); ///< Calls the 2D force-draw-over callback on each plugin in the list, passing the viewport overlay to draw on.
	/// Calls the 3D input handler on each plugin in the list and reports how the event was handled.
	EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event, bool serve_when_force_input_enabled);
	void forward_3d_draw_over_viewport(Control *p_overlay); ///< Lets plugins draw over the 3D viewport.
	void forward_3d_force_draw_over_viewport(Control *p_overlay); ///< Lets plugins force-draw over the 3D viewport.
	void add_plugin(EditorPlugin *p_plugin); ///< Adds a plugin to this dispatch list.
	void remove_plugin(EditorPlugin *p_plugin); ///< Removes a plugin from this dispatch list.
	void clear(); ///< Removes all plugins from this dispatch list.
	bool is_empty(); ///< @returns `true` if the dispatch list is empty.
};

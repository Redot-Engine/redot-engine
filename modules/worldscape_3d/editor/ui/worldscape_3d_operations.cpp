/**************************************************************************/
/*  worldscape_3d_operations.cpp                                          */
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

// Terrain3D Godot plugin: Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "worldscape_3d_operations.h"
#include "multi_picker.h"
#include "worldscape_3d_tools.h"

#include "modules/worldscape_3d/editor/worldscape_3d_editor.h"

#include <cassert>

MultiPicker *WorldScape3DGradientOperationBuilder::get_point_picker() const {
	return Object::cast_to<MultiPicker>(_tool_settings->convert_setting("gradient_points"));
}

real_t WorldScape3DGradientOperationBuilder::get_brush_size() const {
	return _tool_settings->convert_setting("size");
}

bool WorldScape3DGradientOperationBuilder::is_drawable() const {
	return _tool_settings->convert_setting("drawable");
}

bool WorldScape3DGradientOperationBuilder::is_picking() const {
	auto picker = get_point_picker();
	return picker && !picker->all_points_selected();
}

void WorldScape3DGradientOperationBuilder::pick(Vector3 position, WorldScape3D *) {
	if (is_picking()) {
		get_point_picker()->add_point(position);
	}
}

bool WorldScape3DGradientOperationBuilder::is_ready() const {
	auto picker = get_point_picker();
	return picker && picker->all_points_selected() && !is_drawable();
}

void WorldScape3DGradientOperationBuilder::apply_operation(WorldScape3DEditor *editor, Vector3 position, real_t camera_direction) {
	auto picker = get_point_picker();
	if (!picker) {
		return;
	}

	auto points = picker->get_points();
	if (points.size() != 2 || is_drawable()) {
		return;
	}

	real_t brush_size = get_brush_size();
	if (brush_size <= 0.f) {
		return;
	}

	auto start = points[0];
	auto end = points[1];

	editor->start_operation(start);
	auto dir = (end - start).normalized();
	auto pos = start;
	while (dir.dot(end - pos) > 0.f) {
		editor->operate(pos, camera_direction);
		pos += dir * 0.2f * brush_size;
	}
	editor->stop_operation();

	picker->clear();
}

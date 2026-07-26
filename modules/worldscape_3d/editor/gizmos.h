/**************************************************************************/
/*  gizmos.h                                                              */
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

#pragma once

#include "../constants.h"
#include "core/math/vector2.h"
#include "editor/scene/3d/node_3d_editor_gizmos.h"
#include "scene/resources/material.h"

class WorldScape3DEditor;

class WorldScape3DRegionGizmo final : public EditorNode3DGizmo {
	GDCLASS(WorldScape3DRegionGizmo, EditorNode3DGizmo);
	CLASS_NAME();

	Ref<StandardMaterial3D> _material;
	Ref<StandardMaterial3D> _selection_material;
	Vector2 _region_position;
	real_t _region_size = 1024.f;
	TypedArray<Vector2i> _grid;
	bool _use_secondary_color = false;
	bool _show_rect = true;

	static constexpr auto _main_color = Colors::GreenYellow;
	static constexpr auto _secondary_color = Colors::Red;
	static constexpr auto _grid_color = Colors::White;
	static constexpr auto _border_color = Colors::Blue;

public:
	WorldScape3DRegionGizmo();

	void update(Vector2 position = Vector2{}, const WorldScape3DEditor *editor = nullptr);

	void redraw() override;
};

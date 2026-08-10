/**************************************************************************/
/*  gizmos.cpp                                                            */
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

#include "gizmos.h"

#include "../worldscape_3d.h"
#include "../worldscape_3d_data.h"
#include "worldscape_3d_editor.h"

#include "scene/resources/material.h"

WorldScape3DRegionGizmo::WorldScape3DRegionGizmo() :
		_material{ memnew(StandardMaterial3D) } {
	using enum BaseMaterial3D::Flags;
	_material->set_flag(FLAG_DISABLE_DEPTH_TEST, true);
	_material->set_flag(FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	_material->set_shading_mode(BaseMaterial3D::ShadingMode::SHADING_MODE_UNSHADED);
	_material->set_albedo(Colors::White);

	_selection_material = _material->duplicate();
	_selection_material->set_render_priority(0);
}

void WorldScape3DRegionGizmo::update(const Vector2 position, const WorldScape3DEditor *editor) {
	if (position == Vector2{} && !editor) {
		_show_rect = false;
		_region_size = 1024.f;
		_grid = TypedArray<Vector2i>{ {} };
		return;
	}

	_show_rect = editor->get_tool() == WorldScape3DEditor::Tool::REGION;
	_use_secondary_color = editor->get_operation() == WorldScape3DEditor::Operation::SUBTRACT;
	_region_position = position;
	auto terrain = editor->get_terrain();
	_region_size = static_cast<real_t>(terrain->get_region_size()) * terrain->get_vertex_spacing();
	_grid = terrain->get_data()->get_region_locations();
	terrain->update_gizmos();
}

void WorldScape3DRegionGizmo::redraw() {
	auto draw_rect = [this](const Vector2 pos, const real_t size, const Ref<StandardMaterial3D> &material, Color modulate) {
		Vector<Vector3> lines = {
			{ -1, 0, -1 },
			{ -1, 0, 1 },
			{ 1, 0, 1 },
			{ 1, 0, -1 },
			{ -1, 0, 1 },
			{ 1, 0, 1 },
			{ 1, 0, -1 },
			{ -1, 0, -1 }
		};
		for (auto &line : lines) {
			line = (line / 2.f) * size + Vector3{ pos.x, 0, pos.y };
		}
		add_lines(lines, material, false, modulate);
	};

	clear();
	const auto rect_position = Vector2i{ _region_position } * _region_size;
	if (_show_rect) {
		const auto center = Vector2{ _region_size, _region_size } * .5f;

		auto modulate = _use_secondary_color ? _secondary_color : _main_color;
		if (std::abs(_region_position.x) > .5 * WorldScape3DData::REGION_MAP_SIZE || std::abs(_region_position.y) > .5 * WorldScape3DData::REGION_MAP_SIZE) {
			modulate = Colors::Gray;
		}
		draw_rect(center + rect_position, _region_size, _selection_material, modulate);
		for (auto &pos : _grid) {
			auto grid_tile_pos = Vector2i{ pos } * _region_size;
			if (_show_rect && grid_tile_pos == rect_position) {
				// Skip this one, otherwise focused region borders are not always visible due to draw order
				continue;
			}
			draw_rect(center + grid_tile_pos, _region_size, _material, _grid_color);
		}
		draw_rect(Vector2{}, _region_size * WorldScape3DData::REGION_MAP_SIZE, _material, _border_color);
	}
}

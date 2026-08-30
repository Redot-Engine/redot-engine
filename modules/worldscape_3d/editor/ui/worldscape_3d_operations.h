/**************************************************************************/
/*  worldscape_3d_operations.h                                            */
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

// Terrain3D Godot plugin: Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.
#include "core/math/math_defs.h"

struct Vector3;
class WorldScape3D;
class WorldScape3DEditor;
class WorldScape3DToolSettings;
class MultiPicker;

class WorldScape3DOperationBuilder {
public:
	virtual ~WorldScape3DOperationBuilder() = default;

	virtual bool is_picking() const = 0;
	virtual void pick(Vector3 position, WorldScape3D *terrain) = 0;
	virtual bool is_ready() const = 0;
	virtual void apply_operation(WorldScape3DEditor *editor, Vector3 position, real_t camera_direction) = 0;
};

class WorldScape3DGradientOperationBuilder final : public WorldScape3DOperationBuilder {
	WorldScape3DToolSettings *_tool_settings;

public:
	explicit WorldScape3DGradientOperationBuilder(WorldScape3DToolSettings *tool_settings) :
			_tool_settings(tool_settings) {}

	~WorldScape3DGradientOperationBuilder() override = default;

	MultiPicker *get_point_picker() const;
	real_t get_brush_size() const;
	bool is_drawable() const;

	bool is_picking() const override;
	void pick(Vector3 position, WorldScape3D *terrain) override;
	bool is_ready() const override;
	void apply_operation(WorldScape3DEditor *editor, Vector3 position, real_t camera_direction) override;
};

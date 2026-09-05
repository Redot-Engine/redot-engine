/**************************************************************************/
/*  double_slider.h                                                       */
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

#include "scene/gui/control.h"

class DoubleSlider final : public Control {
	GDCLASS(DoubleSlider, Control);

	Label *_label = nullptr;
	String _suffix;
	int _grabbed_handle = 0; // -1 left, 0 none, 1 right
	real_t _min_value = 0.f;
	real_t _max_value = 100.f;
	real_t _step = 1.f;
	Vector2 _range{ 0.f, 100.f };
	real_t _display_scale = 1.f;
	real_t _position_x = 0.f;
	real_t _minimum_x = 60.f;

public:
	DoubleSlider();
	~DoubleSlider() override = default;

	real_t get_min() const { return _min_value; }
	void set_min(real_t value);
	real_t get_max() const { return _max_value; }
	void set_max(real_t value);
	real_t get_step() const { return _step; }
	void set_step(real_t value) { _step = value; }

	void set_label(Label *value);
	void set_suffix(const String &value) { _suffix = value; }

	void set_value(Vector2 range);
	Vector2 get_value() const { return _range; }
	void update_label();
	void set_slider(real_t xpos, bool relative = false);
	void gui_input(const Ref<InputEvent> &event) override;

	void _notification(int what);
	static void _bind_methods();
};

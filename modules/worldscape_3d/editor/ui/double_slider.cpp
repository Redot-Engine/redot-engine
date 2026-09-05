/**************************************************************************/
/*  double_slider.cpp                                                     */
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

#include "double_slider.h"

#include "editor/editor_interface.h"
#include "scene/gui/label.h"

#include <cassert>
#include <cmath>

DoubleSlider::DoubleSlider() {
	// Setup Display Scale
	// 0 auto, 1 75%, 2 100%, 3 125%, 4 150%, 5 175%, 6 200%, 7 custom
	auto editor_settings = EditorInterface::get_singleton();
	if (int ds = editor_settings->get("interface/editor/display_scale"); ds == 0) {
		ds = 2;
	} else if (ds == 7) {
		_display_scale = editor_settings->get("interface/editor/custom_display_scale");
	} else {
		_display_scale = .25f * static_cast<real_t>(ds + 2);
	}
	update_label();
}

void DoubleSlider::set_min(real_t value) {
	_min_value = value;
	if (_range.x <= _min_value) {
		_range.x = _min_value;
		set_value(_range);
	}
	update_label();
}

void DoubleSlider::set_max(real_t value) {
	_max_value = value;
	if (_range.y == 0 || _range.y >= _max_value) {
		_range.y = _max_value;
		set_value(_range);
	}
	update_label();
}

void DoubleSlider::set_label(Label *value) {
	assert(!_label);
	_label = value;
}

void DoubleSlider::set_value(Vector2 range) {
	_range.x = Math::clamp(range.x, _min_value, _max_value);
	_range.y = Math::clamp(range.y, _min_value, _max_value);
	if (_range.y < _range.x) {
		std::swap(_range.x, _range.y);
	}
	update_label();
	emit_signal("value_changed", _range);
	queue_redraw();
}

void DoubleSlider::update_label() {
	if (_label) {
		_label->set_text(String::num(_range.x, 1) + "/" + String::num(_range.y, 1) + " " + _suffix);
		if (_position_x == 0) {
			_position_x = _label->get_position().x;
		} else {
			auto pos = _label->get_position();
			_label->set_position(Point2{ _position_x + 5.f * _display_scale, pos.y });
		}
		auto cms = _label->get_custom_minimum_size();
		_label->set_custom_minimum_size(Size2{ _minimum_x + 5.f * _display_scale, cms.y });
	}
}

void DoubleSlider::set_slider(real_t xpos, bool relative) {
	if (_grabbed_handle == 0) {
		return;
	}
	real_t xpos_step = Math::clamp(static_cast<real_t>(Math::snapped((xpos / get_size().x) * _max_value, _step)), _min_value, _max_value);
	if (_grabbed_handle < 0) {
		_range.x = relative ? _range.x + xpos : xpos_step;
	} else {
		_range.y = relative ? _range.y + xpos : xpos_step;
	}
	set_value(_range);
}

void DoubleSlider::gui_input(const Ref<InputEvent> &event) {
	Ref<InputEventMouseButton> mbev = event;
	if (mbev.is_valid()) {
		auto idx = mbev->get_button_index();
		if (idx == MouseButton::LEFT || idx == MouseButton::WHEEL_UP || idx == MouseButton::WHEEL_DOWN) {
			if (mbev->is_pressed()) {
				real_t mid_point = 0.5f * (_range.x + _range.y);
				real_t xpos = mbev->get_position().x * 2.f;
				if (xpos >= mid_point) {
					_grabbed_handle = 1;
				} else {
					_grabbed_handle = -1;
				}
				switch (idx) {
					case MouseButton::LEFT:
						set_slider(mbev->get_position().x);
						break;
					case MouseButton::WHEEL_DOWN:
						set_slider(-1.f, true);
						break;
					case MouseButton::WHEEL_UP:
						set_slider(1.f, true);
						break;
					default:
						break;
				}
			} else {
				_grabbed_handle = 0;
			}
		}
	}
	Ref<InputEventMouseMotion> ev = event;
	if (ev.is_valid() && _grabbed_handle) {
		set_slider(ev->get_position().x);
	}
}

void DoubleSlider::_bind_methods() {
	ADD_SIGNAL(MethodInfo("value_changed", PropertyInfo(Variant::VECTOR2, "value")));
}

void DoubleSlider::_notification(int what) {
	if (what == NOTIFICATION_DRAW) {
		// Draw background bar
		auto bg = get_theme_stylebox("slider", "HSlider");
		auto bg_height = bg->get_minimum_size().y;
		auto size = get_size();
		auto mid_y = 0.5f * (size.y - bg_height);
		draw_style_box(bg, Rect2{ Vector2{ 0, mid_y }, Vector2{ size.x, bg_height } });

		// Draw foreground bar
		auto handle = get_theme_icon("grabber", "HSlider");
		auto area = get_theme_stylebox("grabber_area", "HSlider");
		auto startx = (_range.x / _max_value) * size.x;
		auto endx = (_range.y / _max_value) * size.x;
		draw_style_box(area, Rect2{ Vector2{ startx, mid_y }, Vector2{ endx - startx, bg_height } });

		// Draw handles
		Vector2 handle_pos;
		handle_pos.x = std::clamp(startx - .05f * handle->get_size().x, -10.f, size.x);
		handle_pos.y = std::clamp(endx - .05f * handle->get_size().x, 0.f, size.x - 10.f);
		draw_texture(handle, Vector2{ handle_pos.x, -mid_y - 10.f * (_display_scale - 1.f) });
		draw_texture(handle, Vector2{ handle_pos.y, -mid_y - 10.f * (_display_scale - 1.f) });

		update_label();
	}
}

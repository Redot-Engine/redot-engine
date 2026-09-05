/**************************************************************************/
/*  multi_picker.cpp                                                      */
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

#include "multi_picker.h"
#include "scene/gui/button.h"

namespace {
constexpr int MAX_POINTS = 2;
}

class MultiPickerButton final : public Button {
	GDCLASS(MultiPickerButton, Button);

	int _point_index;

public:
	static void _bind_methods() {
		ADD_SIGNAL(MethodInfo("picked", PropertyInfo(Variant::INT, "index")));
	}

	explicit MultiPickerButton(const int p_index, Ref<Texture2D> p_icon) :
			_point_index{ p_index } {
		Button::set_meta("icon", p_icon);
		Button::set_meta("point_index", p_index);
	}

	void _notification(int what) {
		BaseButton::_notification(what);
		Button::_notification(what);
		if (what == NOTIFICATION_POSTINITIALIZE) {
			set_button_icon(Button::get_meta("icon"));
			set_tooltip_text("Pick point on the Terrain");
		}
	}

	int get_index() const {
		return _point_index;
	}

	void pressed() override {
		emit_signal("picked", _point_index);
	}
};

void MultiPicker::_bind_methods() {
	ADD_SIGNAL(MethodInfo("pressed"));
	ADD_SIGNAL(MethodInfo("value_changed",
			PropertyInfo(Variant::ARRAY, "setting", PROPERTY_HINT_ARRAY_TYPE)));
}

void MultiPicker::on_button_pressed(int index) {
	_points.set(index, Vector3{});
	_picking_index = index;
	update_buttons();
	emit_signal("pressed");
}

void MultiPicker::clear() {
	_points.fill(Vector3{});
	update_buttons();
	emit_signal("value_changed", _points);
}

bool MultiPicker::all_points_selected() const {
	return _points.count(Vector3{}) == 0;
}

void MultiPicker::add_point(Vector3 p) {
	if (_points.has(p)) {
		return;
	}

	// If manually selecting a point individually
	if (_picking_index != -1) {
		_points.set(_picking_index, p);
		_picking_index = -1;
	} else {
		// Else picking a sequence of points (non-drawable)
		for (int i = 0; i < MAX_POINTS; ++i) {
			if (_points[i] == Vector3{}) {
				_points.set(i, p);
				break;
			}
		}
	}
	update_buttons();
	emit_signal("value_changed", _points);
}

PackedVector3Array MultiPicker::get_points() const {
	return _points;
}

void MultiPicker::set_points(const PackedVector3Array &points) {
	_points = points;
}

void MultiPicker::_notification(int what) {
	Container::_notification(what);
	if (what == NOTIFICATION_POST_ENTER_TREE) {
		init();
	}
}

void MultiPicker::init() {
	_icon_picker = get_theme_icon("ColorPick", "EditorIcons");
	_icon_picker_checked = get_editor_theme_icon(SNAME("picker_checked"));
	_points.resize(MAX_POINTS);

	for (int i = 0; i < MAX_POINTS; ++i) {
		auto button = memnew(MultiPickerButton(i, _icon_picker));
		button->connect("pressed", callable_mp(this, &MultiPicker::on_button_pressed).bind(i));
		add_child(button);
	}
}

void MultiPicker::update_buttons() {
	for (auto nodes = get_children(); auto &obj : nodes) {
		auto *button = cast_to<MultiPickerButton>(obj);
		if (button) {
			const int index = button->get_index();
			if (_points[index] != Vector3{}) {
				button->set_button_icon(_icon_picker_checked);
			} else {
				button->set_button_icon(_icon_picker);
			}
		}
	}
}

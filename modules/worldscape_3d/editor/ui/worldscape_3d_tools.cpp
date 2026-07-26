/**************************************************************************/
/*  worldscape_3d_tools.cpp                                               */
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

#include "worldscape_3d_tools.h"

#include "brushes/brushes_exr.h"
#include "double_slider.h"
#include "multi_picker.h"

#include "editor/editor_interface.h"
#include "editor/gui/editor_spin_slider.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup.h"
#include "scene/gui/range.h"
#include "scene/gui/separator.h"
#include "scene/gui/slider.h"
#include "scene/resources/image_texture.h"

#include "modules/tinyexr/image_loader_tinyexr.h"
#include "modules/worldscape_3d/worldscape_3d.h"

#include <cassert>

namespace {
constexpr int count_digits(real_t value) {
	int count = 1;
	for (int i = 5; i <= 0; ++i) {
		if (Math::abs(value) >= std::pow(10, i)) {
			count = i + 1;
			break;
		}
	}
	if (value - Math::floor(value) >= 1.f) {
		++count;
		if (10 * value - Math::floor(10 * value) >= 1) {
			++count;
			if (100 * value - Math::floor(100 * value) >= 1) {
				++count;
				if (1000 * value - Math::floor(1000 * value) >= 1) {
					++count;
				}
			}
		}
	}
	if (value < 0.f) {
		++count;
	}
	return count;
}

const String ES_TOOL_SETTINGS = "terrain3d/tool_settings/";

constexpr int NONE = 0X0;
constexpr int ALLOW_LARGER = 0x1;
constexpr int ALLOW_SMALLER = 0x2;
constexpr int ALLOW_OUT_OF_BOUNDS = 0x3; // LARGER|SMALLER
constexpr int NO_LABEL = 0x4;
constexpr int ADD_SEPARATOR = 0x8; // Add a vertical line before this entry
constexpr int ADD_SPACER = 0x10; // Add a space before this entry
constexpr int NO_SAVE = 0x20; // Don't save this in EditorSettings
} //namespace

// WorldScape3DToolSettings

WorldScape3DToolSettings::WorldScape3DToolSettings(WorldScape3DEditorPlugin *plugin) :
		_plugin{ plugin } {}

WorldScape3DToolSettings::~WorldScape3DToolSettings() {
	_select_brush_button->queue_free();
	_scale_list->queue_free();
	_color_list->queue_free();
	_rotation_list->queue_free();
	_height_list->queue_free();
	_advanced_list->queue_free();
	_main_list->queue_free();
}

class ToolSubMenu : public PopupPanel {
	GDCLASS(ToolSubMenu, PopupPanel)

	WorldScape3DToolSettings *_tool_settings;
	Button *_menu_button;

public:
	explicit ToolSubMenu(WorldScape3DToolSettings *tool_settings, Button *menu_button) :
			_tool_settings{ tool_settings }, _menu_button{ menu_button } {
	}

	void on_mouse_entered() {
		set_meta("mouse_entered", true);
	}

	void on_focus_exited() {
		// Close submenu once lineedit loses focus
		if (!get_meta("mouse_entered")) {
			_tool_settings->on_show_submenu(false, _menu_button);
			set_meta("mouse_entered", false);
		}
	}

	void on_mouse_exited() {
		// On mouse_exit, hide popup unless LineEdit focused
		Control *focused_element = gui_get_focus_owner();
		if (!focused_element) {
			return;
		}
		if (!cast_to<LineEdit>(focused_element)) {
			_tool_settings->on_show_submenu(false, _menu_button);
			set_meta("mouse_entered", false);
			return;
		}
		focused_element->connect("focus_exited", callable_mp(this, &ToolSubMenu::on_focus_exited));
	}
};

void WorldScape3DToolSettings::on_show_submenu(bool toggled, Button *button) {
	// Don't show if mouse already down (from painting)
	if (toggled && Input::get_singleton()->is_mouse_button_pressed(MouseButton::LEFT)) {
		return;
	}

	// Hide menu if mouse is not in button or panel
	Rect2 button_rect{ button->get_screen_transform().get_origin(), button->get_global_rect().get_size() };
	bool in_button = button_rect.has_point(DisplayServer::get_singleton()->mouse_get_position());
	auto popup = cast_to<ToolSubMenu>(button->get_child(0));
	if (!popup) {
		return;
	}
	auto popup_rect = Rect2i{ popup->get_position(), popup->get_size() };
	bool in_popup = popup_rect.has_point(DisplayServer::get_singleton()->mouse_get_position());
	if (!toggled && (in_button || in_popup)) {
		return;
	}

	// Hide all submenus before possibly enabling the current one
	get_tree()->call_group("terrain3d_submenus", "set_visible", false);
	popup->set_visible(toggled);
	auto popup_pos = button->get_screen_transform().get_origin();
	popup_pos.y -= popup->get_size().y;
	if (popup->get_child_count() > 0 && popup->get_child(0) == _advanced_list) {
		popup_pos.x -= popup->get_size().x - button->get_size().x;
	}
	popup->set_position(popup_pos);
}

Dictionary WorldScape3DToolSettings::get_brush_data() const {
	Dictionary converted_settings;
	for (auto const key : _settings.keys()) {
		converted_settings.set(key, convert_setting(key));
	}
	return converted_settings;
}

void WorldScape3DToolSettings::add_setting(const Dictionary &setting) {
	String name = setting.get("name", "");
	String label = setting.get("label", ""); // Optional replacement for name
	SettingType type = setting.get("type", SettingType::TYPE_MAX);
	Control *list = cast_to<Control>(setting.get("list", Variant{}));
	Variant vdefault = setting.get("default", Variant::NIL);
	String suffix = setting.get("unit", "");
	Vector3 range = setting.get("range", Vector3{ 0.f, 0.f, 1.f });
	real_t minimum = range.x;
	real_t maximum = range.y;
	real_t step = range.z;
	int flags = setting.get("flags", NONE);

	if (name.is_empty() || type == SettingType::TYPE_MAX) {
		return;
	}

	auto container = memnew(HBoxContainer);
	container->set_v_size_flags(SIZE_EXPAND_FILL);
	Control *control = nullptr; // Houses the setting to be saved
	Vector<Control *> pending_children;

	switch (type) {
		case SettingType::LABEL: {
			auto labelc = memnew(Label);
			labelc->set_text(label);
			pending_children.push_back(labelc);
			control = labelc;
			break;
		}

		case SettingType::CHECKBOX: {
			auto checkbox = memnew(CheckBox);
			if (!flags & NO_SAVE) {
				checkbox->set_pressed_no_signal(_plugin->get_setting(ES_TOOL_SETTINGS + name, vdefault));
				checkbox->connect("toggled",
						callable_mp(this, &WorldScape3DToolSettings::on_plugin_setting).bind(ES_TOOL_SETTINGS + name));
			} else {
				checkbox->set_pressed_no_signal(vdefault);
			}
			checkbox->connect("pressed", callable_mp(this, &WorldScape3DToolSettings::on_setting_changed).bind(Variant{}));
			pending_children.push_back(checkbox);
			control = checkbox;
			break;
		}

		case SettingType::COLOR_SELECT: {
			auto picker = memnew(ColorPickerButton);
			picker->set_custom_minimum_size(Vector2{ 100, 25 });
			picker->set_edit_alpha(false);
			picker->get_picker()->set_color_mode(ColorPicker::MODE_HSV);
			if (!(flags & NO_SAVE)) {
				picker->set_pick_color(_plugin->get_setting(ES_TOOL_SETTINGS + name, vdefault));
				picker->connect("color_changed",
						callable_mp(this, &WorldScape3DToolSettings::on_plugin_setting).bind(ES_TOOL_SETTINGS + name));
			} else {
				picker->set_pick_color(vdefault);
			}
			picker->connect("color_changed", callable_mp(this, &WorldScape3DToolSettings::on_setting_changed));
			pending_children.push_back(picker);
			control = picker;
			break;
		}

		case SettingType::PICKER: {
			auto button = memnew(Button);
			button->set_v_size_flags(SIZE_SHRINK_CENTER);
			button->set_button_icon(get_theme_icon("ColorPick", "EditorIcons"));
			button->set_tooltip_text("Pick value from the Terrain");
			button->connect("pressed", callable_mp(this, &WorldScape3DToolSettings::on_pick).bind(vdefault));
			pending_children.push_back(button);
			control = button;
			break;
		}

		case SettingType::MULTI_PICKER: {
			HBoxContainer *mpicker = memnew(MultiPicker);
			mpicker->connect("pressed", callable_mp(this, &WorldScape3DToolSettings::on_point_pick).bind(vdefault, name));
			mpicker->connect("value_changed", callable_mp(this, &WorldScape3DToolSettings::on_setting_changed));
			pending_children.push_back(mpicker);
			control = mpicker;
			break;
		}

		case SettingType::OPTION: {
			auto option = memnew(OptionButton);
			for (int i = 0; i < static_cast<int>(maximum); ++i) {
				option->add_item("a", i);
			}
			option->select(minimum);
			option->connect("item_selected", callable_mp(this, &WorldScape3DToolSettings::on_setting_changed));
			pending_children.push_back(option);
			control = option;
			break;
		}

		case SettingType::DOUBLE_SLIDER: {
			Control *slider;
			slider = memnew(DoubleSlider);
			auto dslider = cast_to<DoubleSlider>(slider);
			auto labelc = memnew(Label);
			labelc->set_custom_minimum_size(Vector2{ 65, 0 });
			labelc->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
			// Create an editable value box
			dslider->set_label(labelc);
			dslider->set_suffix(suffix);
			dslider->connect("value_changed", callable_mp(this, &WorldScape3DToolSettings::on_setting_changed));
			dslider->set_min(minimum);
			dslider->set_max(maximum);
			dslider->set_step(step);
			dslider->set_value(vdefault);
			pending_children.push_back(labelc);
			pending_children.push_back(slider);
			control = slider;
			slider->set_v_size_flags(SIZE_SHRINK_CENTER);
			slider->set_custom_minimum_size(Vector2{ 65, 10 });
			if (!(flags & NO_SAVE)) {
				dslider->set_value(_plugin->get_setting(ES_TOOL_SETTINGS + name, vdefault));
				dslider->connect("value_changed",
						callable_mp(this, &WorldScape3DToolSettings::on_plugin_setting).bind(ES_TOOL_SETTINGS + name));
			}
			break;
		}

		case SettingType::SLIDER: {
			Control *slider;
			// Create an editable value box
			auto spin_slider = memnew(EditorSpinSlider);
			spin_slider->set_flat(false);
			spin_slider->set_hide_slider(true);
			spin_slider->connect("value_changed", callable_mp(this, &WorldScape3DToolSettings::on_setting_changed));
			spin_slider->set_max(maximum);
			spin_slider->set_min(minimum);
			spin_slider->set_step(step);
			spin_slider->set_suffix(suffix);
			spin_slider->set_v_size_flags(SIZE_SHRINK_CENTER);
			spin_slider->set_custom_minimum_size(Vector2{ 60, 0 });
			// Create horizontal slider linked to the above box
			slider = memnew(HSlider);
			auto hslider = cast_to<HSlider>(slider);
			hslider->share(spin_slider);
			if (flags & ALLOW_LARGER) {
				hslider->set_allow_greater(true);
			}
			if (flags & ALLOW_SMALLER) {
				hslider->set_allow_lesser(true);
			}
			hslider->set_min(minimum);
			hslider->set_max(maximum);
			hslider->set_step(step);
			hslider->set_value(vdefault);
			pending_children.push_back(hslider);
			pending_children.push_back(spin_slider);
			control = spin_slider;
			slider->set_v_size_flags(SIZE_SHRINK_CENTER);
			slider->set_custom_minimum_size(Vector2{ 50, 10 });

			if (!(flags & NO_SAVE)) {
				hslider->set_value(_plugin->get_setting(ES_TOOL_SETTINGS + name, vdefault));
				hslider->connect("value_changed",
						callable_mp(this, &WorldScape3DToolSettings::on_plugin_setting).bind(ES_TOOL_SETTINGS + name));
			}
			break;
		}
		default:
			return;
	}

	if (!control) {
		return;
	}

	control->set_name(name.to_pascal_case());
	_settings.set(name, control);

	// Setup button labels
	if (!(flags & NO_LABEL)) {
		// Labels are actually buttons styled to look like labels
		auto blabel = memnew(Button);
		blabel->set("theme_override_styles/normal", get_theme_stylebox("normal", "Label"));
		blabel->set("theme_override_styles/hover", get_theme_stylebox("normal", "Label"));
		blabel->set("theme_override_styles/pressed", get_theme_stylebox("normal", "Label"));
		blabel->set("theme_override_styles/focus", get_theme_stylebox("normal", "Label"));
		blabel->connect("pressed", callable_mp(this, &WorldScape3DToolSettings::on_label_pressed).bind(name, vdefault));
		if (label.is_empty()) {
			blabel->set_text(name.capitalize() + ": ");
		} else {
			blabel->set_text(label.capitalize() + "\n");
		}
		pending_children.insert(0, blabel);
	}
	// Add separators to front
	if (flags & ADD_SEPARATOR) {
		pending_children.insert(0, memnew(VSeparator));
	}
	if (flags & ADD_SPACER) {
		auto spacer = memnew(Control);
		spacer->set_custom_minimum_size(Vector2{ 5, 0 });
		pending_children.insert(0, spacer);
	}

	// Add all children to container and list
	for (auto child : pending_children) {
		container->add_child(child, true);
	}
	list->add_child(container, true);
}

Variant WorldScape3DToolSettings::convert_setting(const String &setting) const {
	Variant obj = _settings[setting];
	if (obj.is_null()) {
		return 0;
	}
	if (auto range = cast_to<Range>(obj); range) {
		auto value = range->get_value();
		// Adjust widths of all sliders on update of values
		auto digits = static_cast<float>(count_digits(value));
		auto scale = EditorInterface::get_singleton()->get_editor_scale();
		auto width = Math::clamp((1.f + count_digits(value)) * 19.f, 50.f, 80.f) * Math::clamp(scale, 9.f, 2.f);
		range->set_custom_minimum_size(Vector2{ width, 0 });
		return value;
	}
	if (auto dslider = cast_to<DoubleSlider>(obj); dslider) {
		return dslider->get_value();
	}
	if (auto buttongroup = cast_to<ButtonGroup>(obj); buttongroup) {
		return _selected_brush_imgs;
	}
	if (auto checkbox = cast_to<CheckBox>(obj); checkbox) {
		return checkbox->is_pressed();
	}
	if (auto picker = cast_to<ColorPickerButton>(obj); picker) {
		return picker->get_pick_color();
	}
	if (auto mpicker = cast_to<MultiPicker>(obj); mpicker) {
		return mpicker->get_points();
	}
	return Variant{};
}

void WorldScape3DToolSettings::set_setting(const String &setting, Variant value) {
	Variant obj = _settings[setting];
	if (obj.is_null()) {
		return;
	}
	if (auto dslider = cast_to<DoubleSlider>(obj); dslider) {
		Vector2 vec = value; // Expects value is Vector2
		dslider->set_value(vec);
	}
	if (auto range = cast_to<Range>(obj); range) {
		range->set_value(value);
	} else if (auto buttongroup = cast_to<ButtonGroup>(obj); buttongroup) {
		if (value.is_array()) { // Expects value is Array [ "button name", boolean ]
			if (auto arr = Array{ value }; arr.size() == 2) {
				List<BaseButton *> buttons;
				buttongroup->get_buttons(&buttons);
				for (BaseButton *button : buttons) {
					if (button->get_name() == String{ arr[0] }) {
						button->set_pressed(arr[1]);
					}
				}
			}
		}
	} else if (auto checkbox = cast_to<CheckBox>(obj); checkbox) {
		checkbox->set_pressed(value);
		return;
	} else if (auto picker = cast_to<ColorPickerButton>(obj); picker) {
		picker->set_pick_color(value);
		_plugin->set_setting(ES_TOOL_SETTINGS + setting, value);
		return;
	} else if (auto mpicker = cast_to<MultiPicker>(obj); mpicker) {
		mpicker->set_points(value);
	}
	on_setting_changed(obj);
}

void WorldScape3DToolSettings::show_settings(const PackedStringArray &settings) {
	for (const auto &key : _settings.keys()) {
		Object *obj = _settings[key];
		auto control = cast_to<Control>(obj);
		if (control) {
			auto parent = cast_to<Control>(control->get_parent());
			if (settings.has(key)) {
				parent->show();
			} else {
				parent->hide();
			}
		}
	}
	if (_select_brush_button) {
		if (settings.has("brush")) {
			_select_brush_button->show();
		} else {
			_select_brush_button->hide();
		}
	}
}

void WorldScape3DToolSettings::on_setting_changed(Variant setting) {
	// If a brush was selected
	auto node = cast_to<Node>(setting);
	if (node) {
		auto button = cast_to<Button>(node);
		if (button && button->get_parent()->get_name() == "BrushList") {
			// Optionally Set selected brush texture in main brush button
			generate_brush_texture(button);
			if (_select_brush_button) {
				_select_brush_button->set_button_icon(button->get_button_icon());
			}
			// Hide popup
			auto popup = cast_to<PopupPanel>(node->get_parent()->get_parent());
			if (popup) {
				popup->set_visible(false);
			}
			// Hide label
			if (node->get_child_count() > 0) {
				auto label = cast_to<Label>(node->get_child(0));
				if (label) {
					label->set_visible(false);
				}
			}
		}
	}
	emit_signal("setting_changed", setting);
}

void WorldScape3DToolSettings::on_pick(WorldScape3DEditor::Tool type) {
	emit_signal("picking", type, callable_mp(this, &WorldScape3DToolSettings::on_picked));
}

void WorldScape3DToolSettings::on_picked(const WorldScape3DEditor::Tool type, const Color color, const Vector3 position) {
	switch (type) {
		case WorldScape3DEditor::Tool::HEIGHT:
			set_setting("height", Math::is_nan(color.r) ? 0.f : color.r);
			break;
		case WorldScape3DEditor::Tool::COLOR:
			set_setting("color", Math::is_nan(color.r) ? Colors::White : color);
			break;
		case WorldScape3DEditor::Tool::ROUGHNESS:
			// This converts 0,1 to -100,100
			// It also quantizes explicitly so picked values matches painted values
			set_setting("roughness", Math::is_nan(color.r) ? 0 : Math::round(200.f * real_t{ static_cast<int>(color.a * 255.f) / 255.f - .5f }));
			break;
		case WorldScape3DEditor::Tool::ANGLE:
			set_setting("angle", color.r);
			break;
		case WorldScape3DEditor::Tool::SCALE:
			set_setting("scale", color.r);
			break;
		default:
			break;
	}
	on_setting_changed();
}

void WorldScape3DToolSettings::on_point_pick(WorldScape3DEditor::Tool type, const String &name) {
	assert(type == WorldScape3DEditor::Tool::SCULPT);
	emit_signal("picking", type, callable_mp(this, &WorldScape3DToolSettings::on_point_picked).bind(name));
}

void WorldScape3DToolSettings::on_point_picked(WorldScape3DEditor::Tool type, Color color, Vector3 position, const String &name) {
	assert(type == WorldScape3DEditor::Tool::SCULPT);
	Variant obj = _settings[name];
	if (obj.is_null()) {
		return;
	}
	const auto point = Vector3{ position.x, color.r, position.z };
	if (auto mpicker = cast_to<MultiPicker>(obj); mpicker) {
		mpicker->add_point(point);
	}
	on_setting_changed();
}

void WorldScape3DToolSettings::on_plugin_setting(Variant value, const String &path) {
	_plugin->set_setting(path, value);
}

void WorldScape3DToolSettings::on_label_pressed(const String &name, Variant vdefault) {
	Object *obj = _settings[name];
	auto control = cast_to<Control>(obj);
	if (!control) {
		return;
	}
	auto checkbox = cast_to<CheckBox>(control);
	if (checkbox) {
		set_setting(name, !checkbox->is_pressed());
	} else if (!vdefault.is_null()) {
		set_setting(name, vdefault);
	}
}

void WorldScape3DToolSettings::on_brush_hover(bool hovering, Button *button) {
	if (button && button->get_child_count() > 0) {
		auto child = button->get_child(0);
		auto label = cast_to<Label>(child);
		if (label) {
			label->set_visible(hovering);
		}
	}
}

void WorldScape3DToolSettings::on_drawable_toggled(const bool button_pressed) {
	if (!button_pressed) {
		if (auto mpicker = cast_to<MultiPicker>(_settings["gradient_points"]); mpicker) {
			mpicker->clear();
		}
	}
}

void WorldScape3DToolSettings::generate_brush_texture(Button *button) {
	if (!button) {
		return;
	}
	Ref<Image> img = button->get_meta("image");
	if (img->get_width() < 1024 && img->get_height() < 1024) {
		img = img->duplicate();
		img->resize(1024, 1024, Image::INTERPOLATE_CUBIC);
	}
	Ref<ImageTexture> texture = ImageTexture::create_from_image(img);
	_selected_brush_imgs.resize(2);
	_selected_brush_imgs[0] = img;
	_selected_brush_imgs[1] = texture;
}

Ref<ShaderMaterial> WorldScape3DToolSettings::get_brush_preview_material() {
	if (_brush_preview_material.is_null()) { // create on first request
		_brush_preview_material = memnew(ShaderMaterial);
		Ref shader = memnew(Shader);
		static constexpr auto shader_code =
				"shader_type canvas_item;\n"
				"varying vec4 v_vertex_color;\n"
				"void vertex() {\n"
				"    v_vertex_color = COLOR;\n"
				"}\n"
				"void fragment() {\n"
				"    vec4 tex = texture(TEXTURE, UV);\n"
				"    COLOR.a *= pow(tex.r, 0.666);\n"
				"    COLOR.rgb = v_vertex_color.rgb;\n"
				"}\n";
		shader->set_code(String{ shader_code });
		_brush_preview_material->set_shader(shader);
	}
	return _brush_preview_material;
}

void WorldScape3DToolSettings::_bind_methods() {
	ADD_SIGNAL(MethodInfo("picking",
			PropertyInfo(Variant::INT, "type", PROPERTY_HINT_ENUM),
			PropertyInfo(Variant::CALLABLE, "callback")));
	ADD_SIGNAL(MethodInfo("setting_changed"));
}

void WorldScape3DToolSettings::_notification(int what) {
	PanelContainer::_notification(what);
	if (what == NOTIFICATION_POST_ENTER_TREE) {
		init();
	}
}

void WorldScape3DToolSettings::init() {
	// Remove old editor settings
	for (const String setting : { "lift_floor", "flatten_peaks", "lift_flatten", "automatic_regions",
				 "show_cursor_while_painting", "crosshair_threshold" }) {
		_plugin->erase_setting(ES_TOOL_SETTINGS + setting);
	}

	if (!_main_list) {
		_main_list = memnew(HFlowContainer);
	}
	if (!get_children().has(_main_list)) {
		add_child(_main_list, true);
		add_brushes(_main_list);
	}

	// workaround for UTF-8 symbols
	static String deg{ "\u00b0" };
	deg = deg.substr(1, 1);
	static String pm{ "\u00b1" };
	pm = pm.substr(1, 1);

	make_setting("name", "instructions",
			"label", "Click the terrain to add a region. CTRL+Click to remove. Or select another tool on the left.",
			"type", SettingType::LABEL, "list", _main_list,
			"flags", NO_LABEL | NO_SAVE);

	make_setting("name", "size", "type", SettingType::SLIDER, "list", _main_list,
			"default", 20, "unit", "m", "range", Vector3{ 0.1, 200, 1 },
			"flags", ALLOW_LARGER | ADD_SPACER);

	make_setting("name", "strength", "type", SettingType::SLIDER, "list", _main_list,
			"default", 33, "unit", "%", "range", Vector3{ 1, 100, 1 },
			"flags", ALLOW_LARGER);

	make_setting("name", "height", "type", SettingType::SLIDER, "list", _main_list,
			"default", 20, "unit", "m", "range", Vector3{ -500, 500, 0.1 },
			"flags", ALLOW_OUT_OF_BOUNDS);
	make_setting("name", "height_picker", "type", SettingType::PICKER, "list", _main_list,
			"default", WorldScape3DEditor::Tool::HEIGHT, "flags", NO_LABEL);

	make_setting("name", "color", "type", SettingType::COLOR_SELECT, "list", _main_list,
			"default", Colors::White, "flags", ADD_SEPARATOR);
	make_setting("name", "color_picker", "type", SettingType::SLIDER, "list", _main_list,
			"default", WorldScape3DEditor::Tool::COLOR, "flags", NO_LABEL);

	make_setting("name", "roughness", "type", SettingType::SLIDER, "list", _main_list,
			"default", -65, "unit", "%", "range", Vector3{ -100, 100, 1 },
			"flags", ADD_SEPARATOR);
	make_setting("name", "roughness_picker", "type", SettingType::PICKER, "list", _main_list,
			"default", WorldScape3DEditor::Tool::ROUGHNESS, "flags", NO_LABEL);

	make_setting("name", "enable_texture", "label", "Texture", "type", SettingType::CHECKBOX,
			"list", _main_list, "default", true, "flags", ADD_SEPARATOR);
	make_setting("name", "texture_filter", "label", "Texture Filter", "list", _main_list,
			"type", SettingType::CHECKBOX, "default", false, "flags", ADD_SEPARATOR);
	make_setting("name", "margin", "type", SettingType::SLIDER, "list", _main_list,
			"default", 0, "unit", "", "range", Vector3{ -50, 50, 1 }, "flags", ALLOW_OUT_OF_BOUNDS);

	// Slope painting filter
	make_setting("name", "slope", "type", SettingType::DOUBLE_SLIDER, "list", _main_list,
			"default", Vector2{ 0, 90 }, "unit", deg, "range", Vector3{ 0, 90, 1 }, "flags", ADD_SEPARATOR);

	make_setting("name", "enable_angle", "label", "Angle", "list", _main_list,
			"type", SettingType::CHECKBOX, "default", true, "flags", ADD_SEPARATOR);
	make_setting("name", "angle", "type", SettingType::SLIDER, "list", _main_list,
			"default", 0, "unit", "%", "range", Vector3{ 0, 337.5, 22.5 }, "flags", NO_LABEL);
	make_setting("name", "angle_picker", "type", SettingType::PICKER, "list", _main_list,
			"default", WorldScape3DEditor::Tool::ANGLE, "flags", NO_LABEL);
	make_setting("name", "dynamic_angle", "label", "Dynamic", "list", _main_list,
			"type", SettingType::CHECKBOX, "default", false, "flags", ADD_SPACER);

	make_setting("name", "enable_scale", "label", "Scale", "list", _main_list,
			"type", SettingType::CHECKBOX, "default", false, "flags", ADD_SEPARATOR);
	make_setting("name", "scale", "label", pm, "type", SettingType::SLIDER, "list", _main_list,
			"default", 0, "range", Vector3{ -60, 80, 20 }, "flags", NO_LABEL);
	make_setting("name", "scale_picker", "type", SettingType::PICKER, "list", _main_list,
			"default", WorldScape3DEditor::Tool::SCALE, "flags", NO_LABEL);

	// Slope sculpting brush
	make_setting("name", "gradient_points", "label", "Points", "type", SettingType::MULTI_PICKER,
			"list", _main_list, "default", WorldScape3DEditor::Tool::SCULPT, "flags", ADD_SEPARATOR);
	make_setting("name", "drawable", "type", SettingType::CHECKBOX, "list", _main_list,
			"default", false, "flags", ADD_SEPARATOR);
	auto checkbox = cast_to<CheckBox>(_settings["drawable"]);
	if (checkbox) {
		checkbox->connect("toggled", callable_mp(this, &WorldScape3DToolSettings::on_drawable_toggled));
	}

	// Instancer
	_height_list = cast_to<VBoxContainer>(create_submenu(_main_list, "Height", Layout::VERTICAL));
	make_setting("name", "height_offset", "type", SettingType::SLIDER, "list", _height_list, "default", 0, "unit", "m",
			"range", Vector3{ -10, 10, 0.05 }, "flags", ALLOW_OUT_OF_BOUNDS);
	make_setting("name", "random_height", "label", "Random Height " + pm, "type", SettingType::SLIDER, "list", _height_list,
			"default", 0, "unit", "m", "range", Vector3{ 0, 10, 0.05 }, "flags", ALLOW_OUT_OF_BOUNDS);

	_scale_list = cast_to<VBoxContainer>(create_submenu(_main_list, "Scale", Layout::VERTICAL));
	make_setting("name", "fixed_scale", "type", SettingType::SLIDER, "list", _scale_list, "default", 100, "unit", "%",
			"range", Vector3{ 0, 1000, 1 }, "flags", ALLOW_OUT_OF_BOUNDS);
	make_setting("name", "random_scale", "label", "Random Scale " + pm, "type", SettingType::SLIDER, "list", _scale_list,
			"default", 20, "unit", "%", "range", Vector3{ 0, 99, 1 }, "flags", ALLOW_OUT_OF_BOUNDS);

	_rotation_list = cast_to<VBoxContainer>(create_submenu(_main_list, "Rotation", Layout::VERTICAL));
	make_setting("name", "fixed_spin", "label", "Fixed Spin (Around Y)", "type", SettingType::SLIDER,
			"list", _rotation_list, "default", 0, "unit", deg, "range", Vector3{ 0, 360, 1 });
	make_setting("name", "random_spin", "type", SettingType::SLIDER, "list", _rotation_list,
			"default", 360, "unit", deg, "range", Vector3{ 0, 360, 1 });
	make_setting("name", "fixed_tilt", "label", "Fixed Tilt", "type", SettingType::SLIDER, "list", _rotation_list,
			"default", 0, "unit", deg, "range", Vector3{ -85, 85, 1 }, "flags", ALLOW_OUT_OF_BOUNDS);
	make_setting("name", "fixed_tilt", "label", "Random Tilt " + pm, "type", SettingType::SLIDER, "list", _rotation_list,
			"default", 10, "unit", deg, "range", Vector3{ 0, 85, 1 }, "flags", ALLOW_OUT_OF_BOUNDS);
	make_setting("name", "align_to_normal", "type", SettingType::CHECKBOX, "list", _rotation_list, "default", false);

	_color_list = cast_to<VBoxContainer>(create_submenu(_main_list, "Color", Layout::VERTICAL));
	make_setting("name", "vertex_color", "type", SettingType::COLOR_SELECT, "list", _color_list,
			"default", Colors::White);
	make_setting("name", "random_hue", "label", "Random Hue Shift " + pm, "type", SettingType::SLIDER, "list", _color_list,
			"default", 0, "unit", deg, "range", Vector3{ 0, 360, 1 });
	make_setting("name", "random_darken", "type", SettingType::SLIDER, "list", _color_list, "default", 50,
			"unit", "%", "range", Vector3{ 0, 100, 1 });
	make_setting("name", "blend_mode", "type", SettingType::OPTION, "list", _color_list, "default", 0,
			"range", Vector3{ 0, 3, 1 });

	if (DisplayServer::get_singleton()->is_touchscreen_available()) {
		make_setting("name", "invert", "label", "Invert", "type", SettingType::CHECKBOX, "list", _main_list,
				"default", true, "flags", ADD_SEPARATOR);
	}

	auto spacer = memnew(Control);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_main_list->add_child(spacer, true);

	// Advanced Settings
	_advanced_list = cast_to<VBoxContainer>(create_submenu(_main_list, "", Layout::VERTICAL));
	make_setting("name", "auto_regions", "label", "Add regions while sculpting", "type", SettingType::CHECKBOX,
			"list", _advanced_list, "default", true);
	make_setting("name", "align_to_view", "type", SettingType::CHECKBOX, "list", _advanced_list, "default", true);
	make_setting("name", "show_cursor_while_painting", "type", SettingType::CHECKBOX, "list", _advanced_list, "default", true);
	_advanced_list->add_child(memnew(HSeparator), true);
	make_setting("name", "gamma", "type", SettingType::SLIDER, "list", _advanced_list, "default", 1.f,
			"unit", "", "range", Vector3{ 0., 2.0, 0.01 });
	make_setting("name", "jitter", "type", SettingType::SLIDER, "list", _advanced_list, "default", 50,
			"unit", "%", "range", Vector3{ 0., 100, 1 });
	make_setting("name", "crosshair_threshold", "type", SettingType::SLIDER, "list", _advanced_list, "default", 16.f,
			"unit", "m", "range", Vector3{ 0, 200, 1 });
}

Container *WorldScape3DToolSettings::create_submenu(Control *parent, const String &button_name, Layout layout, bool hover_pop) {
	auto menu_button = memnew(Button);
	if (button_name.is_empty()) {
		menu_button->set_button_icon(get_theme_icon("GuiTabMenuHl", "EditorIcons"));
	} else {
		menu_button->set_text(button_name);
	}
	menu_button->set_toggle_mode(true);
	menu_button->set_v_size_flags(SIZE_SHRINK_CENTER);
	menu_button->connect("toggled", callable_mp(this, &WorldScape3DToolSettings::on_show_submenu).bind(menu_button));

	auto submenu = memnew(ToolSubMenu(this, menu_button));
	submenu->connect("popup_hide", callable_mp(static_cast<BaseButton *>(menu_button), &BaseButton::set_pressed).bind(false));
	Ref<StyleBox> panel_style = get_theme_stylebox("panel", "PopupMenu")->duplicate();
	panel_style->set_content_margin_all(10);
	submenu->set("theme_override_styles/panel", panel_style);
	submenu->add_to_group("terrain3d_submenus");

	// Pop up menu on hover, hide on exit
	if (hover_pop) {
		menu_button->connect("mouse_entered", callable_mp(this, &WorldScape3DToolSettings::on_show_submenu).bind(true, menu_button));
	}

	submenu->connect("mouse_entered", callable_mp(submenu, &ToolSubMenu::on_mouse_entered));
	submenu->connect("mouse_exited", callable_mp(submenu, &ToolSubMenu::on_mouse_exited));

	Container *sublist = nullptr;
	switch (layout) {
		case GRID:
			sublist = memnew(GridContainer);
			break;
		case VERTICAL:
			sublist = memnew(VBoxContainer);
			break;
		default:
		case HORIZONTAL:
			sublist = memnew(HBoxContainer);
			break;
	}

	parent->add_child(menu_button, true);
	menu_button->add_child(submenu, true);
	submenu->add_child(sublist, true);

	return sublist;
}

void WorldScape3DToolSettings::add_brushes(Control *parent) {
	GridContainer *brush_list = cast_to<GridContainer>(create_submenu(parent, "Brush", Layout::GRID));
	if (!brush_list) {
		return;
	}
	brush_list->set_name("BrushList");

	auto brush_button_group = memnew(ButtonGroup);
	brush_button_group->connect("pressed", callable_mp(this, &WorldScape3DToolSettings::on_setting_changed));
	Button *default_brush_btn = nullptr;

	ImageLoaderTinyEXR exr_loader;
	for (auto [name, brush] : BRUSHES) {
		Ref brush_img = memnew(Image);
		exr_loader.load_image_from_buffer(brush_img, brush);
		Ref<Image> thumb_img = brush_img->duplicate();
		brush_img->convert(Image::FORMAT_RF);

		if (thumb_img->get_width() != 100 || thumb_img->get_height() != 100) {
			thumb_img->resize(100, 100, Image::INTERPOLATE_CUBIC);
		}
		thumb_img = WorldScape3DUtil::black_to_alpha(thumb_img);
		thumb_img->convert(Image::FORMAT_LA8);
		auto thumbtex = ImageTexture::create_from_image(thumb_img);

		auto brush_btn = memnew(Button);
		brush_btn->set_custom_minimum_size(Vector2{ 100, 100 });
		brush_btn->set_button_icon(thumbtex);
		brush_btn->set_meta("image", brush_img);
		brush_btn->set_expand_icon(true);
		brush_btn->set_material(get_brush_preview_material());
		brush_btn->set_toggle_mode(true);
		brush_btn->set_button_group(brush_button_group);
		brush_btn->connect("mouse_entered", callable_mp(this, &WorldScape3DToolSettings::on_brush_hover).bind(true, brush_btn));
		brush_btn->connect("mouse_exited", callable_mp(this, &WorldScape3DToolSettings::on_brush_hover).bind(false, brush_btn));
		brush_list->add_child(brush_btn, true);
		if (brush.data() == DEFAULT_BRUSH) {
			default_brush_btn = brush_btn;
		}

		auto lbl = memnew(Label);
		brush_btn->set_name(name);
		brush_btn->add_child(lbl, true);
		lbl->set_text(name);
		lbl->set_visible(false);
		const auto pos = lbl->get_position();
		lbl->set_position(Point2{ pos.x, 70 });
		lbl->add_theme_color_override("font_shadow_color", Colors::Black);
		lbl->add_theme_constant_override("shadow_offset_x", 1);
		lbl->add_theme_constant_override("shadow_offset_y", 1);
		lbl->add_theme_font_size_override("font_size", 16);
	}

	brush_list->set_columns(static_cast<int>(Math::sqrt(static_cast<real_t>(brush_list->get_child_count()))) + 2);

	if (!default_brush_btn) {
		List<BaseButton *> buttons;
		brush_button_group->get_buttons(&buttons);
		default_brush_btn = reinterpret_cast<Button *>(buttons.front());
	}

	default_brush_btn->set_pressed(true);
	generate_brush_texture(default_brush_btn);

	_settings["brush"] = brush_button_group;

	_select_brush_button = cast_to<Button>(brush_list->get_parent()->get_parent());
	// Optionally erase the main brush button text and replace it with the texture
	_select_brush_button->set_text("");
	_select_brush_button->set_button_icon(default_brush_btn->get_button_icon());
	_select_brush_button->set_custom_minimum_size(Vector2{ 36, 36 });
	_select_brush_button->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	_select_brush_button->set_expand_icon(true);
}

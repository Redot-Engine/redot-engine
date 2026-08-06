/**************************************************************************/
/*  worldscape_3d_ui.cpp                                                  */
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

#include "worldscape_3d_ui.h"

#include "modules/worldscape_3d/editor/menu.h"
#include "modules/worldscape_3d/editor/worldscape_3d_editor.h"
#include "modules/worldscape_3d/worldscape_3d.h"
#include "worldscape_3d_asset_dock.h"
#include "worldscape_3d_operations.h"

#include "brushes/ring1.h"

#include "core/os/time.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "editor/translations/editor_translation.h"
#include "modules/tinyexr/image_loader_tinyexr.h"
#include "scene/animation/tween.h"
#include "scene/gui/check_box.h"
#include "scene/gui/separator.h"
#include "scene/resources/image_texture.h"

class WorldScape3DToolbarButton final : public Button {
	WorldScape3DEditor *_editor;
	WorldScape3DEditor::Tool _tool;
	WorldScape3DEditor::Operation _operation;

	static constexpr Color toggled_tint{ .55f, .55f, .55f, 1.f };

public:
	WorldScape3DToolbarButton(
			WorldScape3DEditor *parent,
			const WorldScape3DEditor::Tool tool,
			const WorldScape3DEditor::Operation operation) :
			_editor{ parent }, _tool{ tool }, _operation{ operation } {}

	void toggled(const bool on) override {
		if (on) {
			_editor->set_tool(_tool);
			_editor->set_operation(_operation);
			set_modulate(toggled_tint);
		} else {
			set_modulate(Colors::White);
		}
		queue_redraw();
	}
};

WorldScape3DToolbar::WorldScape3DToolbar(WorldScape3DEditor *editor) :
		_terrain(editor) {
	set_custom_minimum_size({ 20, 0 });

	_add_tools = memnew(ButtonGroup);
	_sub_tools = memnew(ButtonGroup);

	_add_tools->connect("pressed", callable_mp(this, &WorldScape3DToolbar::on_tool_selected));
	_sub_tools->connect("pressed", callable_mp(this, &WorldScape3DToolbar::on_tool_selected));

	int add_id = 0;
	int sub_id = 0;

	auto add_tool_btn = [this, &add_id, &sub_id](const WorldScape3DEditor::Tool tool,
								const StringName &add_icon, WorldScape3DEditor::Operation add_op, const String &add_text,
								const StringName &sub_icon = "", WorldScape3DEditor::Operation sub_op = WorldScape3DEditor::OP_MAX, const String &sub_text = "") {
		// additive button
		WorldScape3DToolbarButton *add_btn = memnew(WorldScape3DToolbarButton(_terrain, tool, add_op));
		add_btn->set_meta("Tool", tool);
		add_btn->set_meta("Operation", add_op);
		add_btn->set_meta("ID", ++add_id);
		add_btn->set_meta("icon", add_icon);
		add_btn->set_tooltip_text(add_text);
		add_btn->set_flat(true);
		add_btn->set_toggle_mode(true);
		add_btn->set_h_size_flags(SIZE_SHRINK_END);
		add_btn->set_button_group(_add_tools);
		add_child(add_btn, true);

		// subtractive button
		if (!sub_icon.is_empty() && !sub_text.is_empty()) {
			WorldScape3DToolbarButton *sub_btn = memnew(WorldScape3DToolbarButton(_terrain, tool, sub_op));
			sub_btn->set_meta("Tool", tool);
			sub_btn->set_meta("Operation", sub_op);
			sub_btn->set_meta("ID", ++sub_id);
			sub_btn->set_meta("icon", sub_icon);
			sub_btn->set_tooltip_text(sub_text);
			sub_btn->set_flat(true);
			sub_btn->set_toggle_mode(true);
			sub_btn->set_h_size_flags(SIZE_SHRINK_END);
			sub_btn->set_button_group(_sub_tools);
			add_child(sub_btn, true);
		}
	};

	add_tool_btn(WorldScape3DEditor::Tool::REGION,
			SNAME("TerrainRegionAdd"), WorldScape3DEditor::Operation::ADD, "Add Region (E)",
			SNAME("TerrainRegionRemove"), WorldScape3DEditor::Operation::SUBTRACT, "Remove Region");

	add_child(memnew(HSeparator));

	add_tool_btn(WorldScape3DEditor::Tool::SCULPT,
			SNAME("TerrainHeightAdd"), WorldScape3DEditor::Operation::ADD, "Raise (R)",
			SNAME("TerrainHeightSub"), WorldScape3DEditor::Operation::SUBTRACT, "Lower (R)");

	add_tool_btn(WorldScape3DEditor::Tool::SCULPT,
			SNAME("TerrainHeightSmooth"), WorldScape3DEditor::Operation::AVERAGE, "Smooth (Shift)",
			SNAME("TerrainHeightSmooth"), WorldScape3DEditor::Operation::AVERAGE, "Smooth (Shift)");

	add_tool_btn(WorldScape3DEditor::Tool::HEIGHT,
			SNAME("TerrainHeightFlat"), WorldScape3DEditor::Operation::ADD, "Height (H)",
			SNAME("TerrainHeightFlat"), WorldScape3DEditor::Operation::SUBTRACT, "Height (H)");

	add_tool_btn(WorldScape3DEditor::Tool::SCULPT,
			SNAME("TerrainHeightSlope"), WorldScape3DEditor::Operation::GRADIENT, "Slope (S)",
			SNAME("TerrainHeightSlope"), WorldScape3DEditor::Operation::GRADIENT, "Slope (S)");

	add_child(memnew(HSeparator));

	add_tool_btn(WorldScape3DEditor::Tool::TEXTURE,
			SNAME("TerrainPaintTexture"), WorldScape3DEditor::Operation::REPLACE, "Paint Texture (B)",
			SNAME("TerrainPaintTexture"), WorldScape3DEditor::Operation::REPLACE, "Paint Texture (B)");

	add_tool_btn(WorldScape3DEditor::Tool::TEXTURE,
			SNAME("TerrainSprayTexture"), WorldScape3DEditor::Operation::REPLACE, "Spray Texture (V)",
			SNAME("TerrainSprayTexture"), WorldScape3DEditor::Operation::REPLACE, "Spray Texture (V)");

	add_tool_btn(WorldScape3DEditor::Tool::AUTOSHADER,
			SNAME("TerrainAutoshader"), WorldScape3DEditor::Operation::ADD, "Paint Autoshader (A)",
			SNAME("TerrainAutoshader"), WorldScape3DEditor::Operation::SUBTRACT, "Disable Autoshader (A)");

	add_child(memnew(HSeparator));

	add_tool_btn(WorldScape3DEditor::Tool::COLOR,
			SNAME("TerrainColor"), WorldScape3DEditor::Operation::ADD, "Paint Color (C)",
			SNAME("TerrainColor"), WorldScape3DEditor::Operation::SUBTRACT, "Remove Color (C)");

	add_tool_btn(WorldScape3DEditor::Tool::ROUGHNESS,
			SNAME("TerrainWetness"), WorldScape3DEditor::Operation::ADD, "Paint Wetness (W)",
			SNAME("TerrainWetness"), WorldScape3DEditor::Operation::SUBTRACT, "Remove Wetness (W)");

	add_child(memnew(HSeparator));

	add_tool_btn(WorldScape3DEditor::Tool::HOLES,
			SNAME("TerrainHoles"), WorldScape3DEditor::Operation::ADD, "Add Holes (H)",
			SNAME("TerrainHoles"), WorldScape3DEditor::Operation::SUBTRACT, "Remove Holes (H)");

	add_tool_btn(WorldScape3DEditor::Tool::NAVIGATION,
			SNAME("TerrainNavigation"), WorldScape3DEditor::Operation::ADD, "Paint Navigable Area (N)",
			SNAME("TerrainNavigation"), WorldScape3DEditor::Operation::SUBTRACT, "Remove Navigable Area (N)");

	add_tool_btn(WorldScape3DEditor::Tool::INSTANCER,
			SNAME("TerrainInstancer"), WorldScape3DEditor::Operation::ADD, "Instance Meshes (I)",
			SNAME("TerrainInstancer"), WorldScape3DEditor::Operation::SUBTRACT, "Remove Meshes (I)");
}

WorldScape3DToolbar::~WorldScape3DToolbar() {
	_add_tools->disconnect("pressed", callable_mp(this, &WorldScape3DToolbar::on_tool_selected));
	_sub_tools->disconnect("pressed", callable_mp(this, &WorldScape3DToolbar::on_tool_selected));
	List<BaseButton *> buttons;
	_add_tools->get_buttons(&buttons);
	_sub_tools->get_buttons(&buttons);
	for (auto *button : buttons) {
		button->set_button_group(nullptr);
		button->queue_free();
	}
}

Button *WorldScape3DToolbar::find_button(const StringName &icon) const {
	List<BaseButton *> buttons;
	_add_tools->get_buttons(&buttons);
	_sub_tools->get_buttons(&buttons);
	for (auto *button : buttons) {
		if (StringName{ button->get_meta("icon") } == icon && button->is_visible()) {
			return reinterpret_cast<Button *>(button);
		}
	}
	return nullptr;
}

void WorldScape3DToolbar::on_tool_selected(BaseButton *const button) {
	// Select same tool on negative bar
	const auto group = button->get_button_group();
	const auto change_group = (group == _sub_tools) ? _add_tools : _sub_tools;
	const int id = button->get_meta("ID", -2);
	List<BaseButton *> buttons;
	change_group->get_buttons(&buttons);
	for (const auto b : buttons) {
		const auto btn = cast_to<Button>(b);
		const int btn_id = btn->get_meta("ID", -1);
		btn->set_pressed_no_signal(btn_id == id);
	}
	emit_signal("tool_changed",
			static_cast<WorldScape3DEditor::Tool>(button->get_meta("Tool", WorldScape3DEditor::TOOL_MAX)),
			static_cast<WorldScape3DEditor::Operation>(button->get_meta("Operation", WorldScape3DEditor::OP_MAX)));
}

void WorldScape3DToolbar::show_buttons(const bool show_add, const bool show_sub) {
	List<BaseButton *> buttons;
	_add_tools->get_buttons(&buttons);
	for (auto *btn : buttons) {
		auto add_btn = static_cast<Button *>(btn);
		if (add_btn->get_button_icon().is_null()) {
			auto icon = get_editor_theme_icon(add_btn->get_meta("icon"));
			if (!icon.is_null()) {
				add_btn->set_button_icon(icon);
			}
		}
		add_btn->set_visible(show_add);
	}
	buttons.clear();
	_sub_tools->get_buttons(&buttons);
	for (auto *btn : buttons) {
		auto sub_btn = static_cast<Button *>(btn);
		if (sub_btn->get_button_icon().is_null()) {
			auto icon = get_editor_theme_icon(sub_btn->get_meta("icon"));
			if (!icon.is_null()) {
				sub_btn->set_button_icon(icon);
			}
		}
		sub_btn->set_visible(show_sub);
	}
}

void WorldScape3DToolbar::_notification(int what) {
	if (what == NOTIFICATION_POST_ENTER_TREE) {
		emit_signal("tool_changed", WorldScape3DEditor::Tool::REGION, WorldScape3DEditor::Operation::ADD);
	}
}

void WorldScape3DToolbar::_bind_methods() {
	ADD_SIGNAL(MethodInfo("tool_changed",
			PropertyInfo(Variant::INT, "tool", PROPERTY_HINT_ENUM),
			PropertyInfo(Variant::INT, "operation", PROPERTY_HINT_ENUM)));
}

WorldScape3DUI::WorldScape3DUI(WorldScape3DEditorPlugin *plugin) :
		_plugin{ plugin } {
	_region_texture = memnew(ImageTexture);

	_toolbar = memnew(WorldScape3DToolbar(_plugin->get_editor()));
	_toolbar->connect("tool_changed", callable_mp(this, &WorldScape3DUI::on_tool_changed));
	_toolbar->hide();

	_tool_settings = memnew(WorldScape3DToolSettings(_plugin));
	_tool_settings->connect("setting_changed", callable_mp(this, &WorldScape3DUI::on_setting_changed));
	_tool_settings->connect("picking", callable_mp(this, &WorldScape3DUI::on_picking));
	_tool_settings->hide();

	_menu = memnew(WorldScape3DMenu(_plugin));
	_menu->set_shortcut_context(this);
	_menu->hide();

	using enum EditorPlugin::CustomControlContainer;
	_plugin->add_control_to_container(CONTAINER_SPATIAL_EDITOR_SIDE_LEFT, _toolbar);
	_plugin->add_control_to_container(CONTAINER_SPATIAL_EDITOR_BOTTOM, _tool_settings);
	_plugin->add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, _menu);

	on_tool_changed(WorldScape3DEditor::Tool::REGION, WorldScape3DEditor::Operation::ADD);

	_editor_decal_timer = memnew(Timer);
	_editor_decal_timer->set_wait_time(0.5);
	_editor_decal_timer->set_one_shot(true);
	_editor_decal_timer->connect("timeout", callable_mp(this, &WorldScape3DUI::WorldScape3DUI::on_decal_timer));
	add_child(_editor_decal_timer);

	ImageLoaderTinyEXR exr_loader;
	Ref<Image> img = memnew(Image);
	exr_loader.load_image_from_buffer(img, std::span{ ring1, ring1_len });
	img->convert(Image::FORMAT_R8);
	_ring_texture = ImageTexture::create_from_image(img);
	editor_ring_texture_rid = _ring_texture->get_rid();
}

WorldScape3DUI::~WorldScape3DUI() {
	if (_plugin) {
		using enum EditorPlugin::CustomControlContainer;
		_plugin->remove_control_from_container(CONTAINER_SPATIAL_EDITOR_MENU, _menu);
		_plugin->remove_control_from_container(CONTAINER_SPATIAL_EDITOR_BOTTOM, _tool_settings);
		_plugin->remove_control_from_container(CONTAINER_SPATIAL_EDITOR_SIDE_LEFT, _toolbar);
	}
	if (_editor_decal_timer) {
		_editor_decal_timer->queue_free();
	}
	if (_toolbar) {
		_toolbar->queue_free();
	}
	if (_menu) {
		_menu->queue_free();
	}
	if (_tool_settings) {
		if (_tool_settings->is_connected("setting_changed", callable_mp(this, &WorldScape3DUI::on_setting_changed))) {
			_tool_settings->disconnect("setting_changed", callable_mp(this, &WorldScape3DUI::on_setting_changed));
		}
		if (_tool_settings->is_connected("picking", callable_mp(_tool_settings, &WorldScape3DToolSettings::on_pick))) {
			_tool_settings->disconnect("picking", callable_mp(_tool_settings, &WorldScape3DToolSettings::on_pick));
		}
		_tool_settings->queue_free();
	}
}

void WorldScape3DUI::on_visible_update() {
	if (auto editor = _plugin->get_editor(); editor) {
		editor->set_tool(_selected_tool);
		editor->set_operation(_selected_operation);
	}
}

void WorldScape3DUI::update_decal() {
	auto terrain = _plugin->get_terrain();
	if (!terrain || _brush_data.size() <= 3) {
		return;
	}

	_mat_rid = terrain->get_material()->get_material_rid();
	_editor_decal_timer->start();

	auto mouse_position = _plugin->get_mouse_position();

	// If not a state that should show the decal, hide everything and return
	if (!_visible || _plugin->get_mouse_mode() == WorldScape3DEditorPlugin::CameraMove || mouse_position == Vector3{} // After moving camera, wait for mouse cursor to update before revealing
																													  // See https://github.com/godotengine/godot/issues/70098
			|| Time::get_singleton()->get_ticks_msec() - _plugin->get_rmb_release_time() <= 100 || (_plugin->get_mouse_mode() == WorldScape3DEditorPlugin::Operating && !bool{ _brush_data["show_cursor_while_painting"] })) {
		hide_decal();
		return;
	}

	reset_decals();
	editor_decal_position.set(0, Vector2{ mouse_position.x, mouse_position.z });
	// Show cursor by default
	editor_decal_visible.set(0, true);

	Color color = editor_decal_color.get(0);

	// Set region size, and modify region map for none background mode.
	auto r_map = terrain->get_data()->get_region_map();
	if (_plugin->get_editor()->get_tool() == WorldScape3DEditor::Tool::REGION) {
		auto r_size = static_cast<real_t>(terrain->get_region_size()) * terrain->get_vertex_spacing();
		int map_size = WorldScape3DData::REGION_MAP_SIZE;
		auto half_r_size = .5f * r_size;
		auto pos = (Vector2{ mouse_position.x, mouse_position.z } + Vector2{ half_r_size, half_r_size }).snappedf(r_size) - Vector2{ half_r_size, half_r_size };
		editor_brush_texture_rid = _region_texture->get_rid();
		editor_decal_position.set(0, pos);
		editor_decal_size.set(0, r_size);
		editor_decal_rotation.set(0, 0.f);

		auto loc = terrain->get_data()->get_region_location(mouse_position);
		loc += Vector2i{ map_size / 2, map_size / 2 };
		if (!(loc.x < 0 || loc.x > map_size - 1 || loc.y < 0 || loc.y > map_size - 1)) {
			const auto index = Math::clamp(loc.y * map_size + loc.x, 0, map_size * map_size - 1);
			if (terrain->get_material()->get_world_background() == WorldScape3DMaterial::WorldBackground::NONE) {
				if (r_map.get(index) == 0 && _active_operation == WorldScape3DEditor::Operation::ADD) {
					r_map.set(index, -index - 1);
				} else {
					r_map.set(index, r_map.get(index)); // ?
				}
			}

			switch (_active_operation) {
				case WorldScape3DEditor::Operation::ADD: {
					if (r_map[index] <= 0) {
						color = Colors::White;
						color.a = 0.25f;
					} else {
						hide_decal();
					}
					break;
				}
				case WorldScape3DEditor::Operation::SUBTRACT: {
					if (r_map.get(index) > 0) {
						color = Colors::White * 0.15f;
						color.a = 0.75f;
					} else {
						hide_decal();
					}
					break;
				}
				default:
					break;
			}
		} else {
			hide_decal();
		}
		// Set texture and color
	} else if (_picking != WorldScape3DEditor::Tool::TOOL_MAX) { // Picking
		//editor_decal_part[0] = false; // Hide brush
		editor_brush_texture_rid = editor_ring_texture_rid;
		editor_decal_size[0] = 10.f * terrain->get_vertex_spacing();
		switch (_picking) {
			case WorldScape3DEditor::Tool::HEIGHT:
				color = ColorPickHeight;
				break;
			case WorldScape3DEditor::Tool::COLOR:
				color = ColorPickColor;
				break;
			case WorldScape3DEditor::Tool::ROUGHNESS:
				color = ColorPickRough;
				break;
			default:
				break;
		}
		color.a = 1.f;
	} else { // Brushing operations
		Array selected_imgs = _brush_data["brush"];
		if (selected_imgs.size() == 2) {
			Ref<ImageTexture> texture = selected_imgs.get(1);
			editor_brush_texture_rid = texture->get_rid();
		}
		real_t decal_size = _brush_data["size"];
		editor_decal_size.set(0, std::max(decal_size, .5f));
		if (_brush_data.has("align_to_view") && bool{ _brush_data["align_to_view"] }) {
			auto cam = terrain->get_camera();
			editor_decal_rotation.set(0, cam ? cam->get_rotation().y : 0.f);
		}
		for (int i = 0; i < 3; ++i) {
			editor_decal_visible.set(i, true);
		}

		const auto strength = static_cast<real_t>(_brush_data["strength"]);

		switch (_plugin->get_editor()->get_tool()) {
			case WorldScape3DEditor::Tool::SCULPT: {
				switch (_active_operation) {
					case WorldScape3DEditor::Operation::ADD:
						color = _plugin->is_alt_modifier_on() ? ColorLift : ColorRaise;
						color.a = Math::clamp(strength, _plugin->is_alt_modifier_on() ? .2f : .25f, .5f);
						break;
					case WorldScape3DEditor::Operation::SUBTRACT:
						color = _plugin->is_alt_modifier_on() ? ColorFlatten : ColorLower;
						color.a = Math::clamp(strength, _plugin->is_alt_modifier_on() ? .25f : .2f, .5f) + .1f;
						break;
					case WorldScape3DEditor::Operation::AVERAGE:
						color = ColorSmooth;
						color.a = Math::clamp(strength, .2f, .5f) + .25f;
						break;
					case WorldScape3DEditor::Operation::GRADIENT:
						color = ColorSlope;
						color.a = Math::clamp(strength, .2f, .4f);
						break;
					default:
						break;
				}
				break;
			}
			case WorldScape3DEditor::Tool::HEIGHT: {
				color = ColorHeight;
				color.a = Math::clamp(strength, .2f, .5f) + .25f;
				break;
			}
			case WorldScape3DEditor::Tool::TEXTURE: {
				switch (_active_operation) {
					case WorldScape3DEditor::Operation::REPLACE:
						color = ColorPaint;
						color.a = .6f;
						break;
					case WorldScape3DEditor::Operation::SUBTRACT:
						color = ColorPaint;
						color.a = Math::clamp(strength, .2f, .5f) + .1f;
						break;
					case WorldScape3DEditor::Operation::ADD:
						color = ColorSpray;
						color.a = Math::clamp(strength, .15f, .4f);
						break;
					default:
						break;
				}
				break;
			}
			case WorldScape3DEditor::Tool::COLOR: {
				color = _brush_data["color"];
				color = color.srgb_to_linear();
				color.a = Math::clamp(strength, .2f, .5f);
				break;
			}
			case WorldScape3DEditor::Tool::ROUGHNESS: {
				color = ColorRoughness;
				color.a = Math::clamp(strength, .2f, .5f) + .1f;
				break;
			}
			case WorldScape3DEditor::Tool::AUTOSHADER:
				color = ColorAutoshader;
				color.a = Math::clamp(strength, .2f, .5f) + .1f;
				break;
			case WorldScape3DEditor::Tool::HOLES:
				color = ColorHoles;
				color.a = .75f;
				break;
			case WorldScape3DEditor::Tool::NAVIGATION:
				color = ColorNavigation;
				color.a = .8f;
				break;
			case WorldScape3DEditor::Tool::INSTANCER:
				editor_brush_texture_rid = _ring_texture->get_rid();
				color = ColorInstancer;
				color.a = .75f;
				break;
			default:
				break;
		}
	}

	if (_active_operation == WorldScape3DEditor::Operation::GRADIENT && _brush_data.has("gradient_points")) {
		if (auto points = static_cast<PackedVector3Array>(_brush_data["gradient_points"]); points.size() == 2) {
			auto point1 = points[0];
			if (point1 != Vector3{}) {
				editor_decal_color.set(1, ColorSlope);
				editor_decal_size.set(1, 10.f * _plugin->get_terrain()->get_vertex_spacing());
				editor_decal_visible.set(1, true);
				editor_decal_position.set(1, Vector2{ point1.x, point1.z });
			}
			auto point2 = points[1];
			if (point2 != Vector3{}) {
				editor_decal_color.set(2, ColorSlope);
				editor_decal_size.set(2, 10.f * _plugin->get_terrain()->get_vertex_spacing());
				editor_decal_visible.set(2, true);
				editor_decal_position.set(2, Vector2{ point2.x, point2.z });
			}
		}
	} else {
		editor_decal_visible.set(1, false);
		editor_decal_visible.set(2, false);
	}

	editor_decal_color.set(0, color);

	auto server = RenderingServer::get_singleton();
	if (server->get_current_rendering_method().contains("gl_compatibility")) {
		for (int i = 0; i < 3; ++i) {
			Color c = editor_decal_color.get(i);
			c.a = std::max(.1f, c.a - .25f);
			editor_decal_color.set(i, c);
		}
	}

	set_editor_decal_fade(color.a);
	// Update Shader params
	if (is_shader_valid()) {
		server->material_set_param(_mat_rid, "_editor_brush_texture", editor_brush_texture_rid);
		server->material_set_param(_mat_rid, "_editor_ring_texture", editor_ring_texture_rid);
		server->material_set_param(_mat_rid, "_editor_decal_position", editor_decal_position);
		server->material_set_param(_mat_rid, "_editor_decal_rotation", editor_decal_rotation);
		server->material_set_param(_mat_rid, "_editor_decal_size", editor_decal_size);
		server->material_set_param(_mat_rid, "_editor_decal_color", editor_decal_color);
		server->material_set_param(_mat_rid, "_editor_decal_visible", editor_decal_visible);
		server->material_set_param(_mat_rid, "_editor_crosshair_threshold", real_t{ _brush_data["crosshair_threshold"] } + 0.1f);
		server->material_set_param(_mat_rid, "_region_map", r_map);
	}
}

void WorldScape3DUI::set_region_texture(Ref<ImageTexture> texture) {
	auto image = Image::create_empty(1, 1, false, Image::FORMAT_R8);
	image->fill(Colors::White);
	texture->create_from_image(image);
	std::swap(_region_texture, texture);
}

bool WorldScape3DUI::is_shader_valid() const {
	// As long as the compiled shader contains at least 1 uniform, we can use it to check
	// if the shader compilation has failed, as this will then return an empty dictionary.
	auto terrain = _plugin->get_terrain();
	if (!terrain) {
		return false;
	}
	List<PropertyInfo> params;
	RenderingServer::get_singleton()->get_shader_parameter_list(terrain->get_material()->get_shader_rid(), &params);
	return !params.is_empty();
}

void WorldScape3DUI::hide_decal() {
	for (int i = 0; i < 3; ++i) {
		editor_decal_visible.set(i, false);
	}
	if (is_shader_valid()) {
		const auto r_map = _plugin->get_terrain()->get_data()->get_region_map();
		auto server = RenderingServer::get_singleton();
		server->material_set_param(_mat_rid, "_editor_decal_visible", editor_decal_visible);
		server->material_set_param(_mat_rid, "_region_map", r_map);
	}
}

void WorldScape3DUI::reset_decals() {
	if (editor_decal_color.size() < 3) {
		for (int i = 0; i < 3; ++i) {
			editor_decal_position.set(i, Vector2{});
			editor_decal_rotation.set(i, 0.f);
			editor_decal_size.set(i, 0.f);
			editor_decal_color.set(i, Color{});
			editor_decal_visible.set(i, false);
		}
		editor_brush_texture_rid = RID{};
	}
}

void WorldScape3DUI::set_editor_decal_fade(const real_t value) {
	editor_decal_fade = value;
	Color color = editor_decal_color.get(0);
	color.a = value;
	editor_decal_color.set(0, color);
	if (is_shader_valid()) {
		auto server = RenderingServer::get_singleton();
		server->material_set_param(_mat_rid, "_editor_decal_color", editor_decal_color);
		if (value < 0.001f) {
			const auto r_map = _plugin->get_terrain()->get_data()->get_region_map();
			server->material_set_param(_mat_rid, "_region_map", r_map);
		}
	}
}

void WorldScape3DUI::set_decal_rotation(real_t angle) {
	editor_decal_rotation.set(0, angle);
	if (is_shader_valid()) {
		RenderingServer::get_singleton()->material_set_param(_mat_rid, "_editor_decal_rotation", editor_decal_rotation);
	}
}

void WorldScape3DUI::on_picking(const WorldScape3DEditor::Tool tool, const Callable &callback) {
	_picking = tool;
	_pick_callback = callback;
	update_decal();
}

void WorldScape3DUI::clear_picking() {
	_picking = WorldScape3DEditor::Tool::TOOL_MAX;
}

bool WorldScape3DUI::is_picking() const {
	if (_picking != WorldScape3DEditor::Tool::TOOL_MAX) {
		return true;
	}

	if (_operation_builder && _operation_builder->is_picking()) {
		return true;
	}

	return false;
}

void WorldScape3DUI::pick(Vector3 global_pos) {
	auto terrain = _plugin->get_terrain();
	if (_picking != WorldScape3DEditor::Tool::TOOL_MAX) {
		Color color;
		switch (_picking) {
			case WorldScape3DEditor::Tool::HEIGHT:
			case WorldScape3DEditor::Tool::SCULPT:
				color = Color{ terrain->get_data()->get_height(global_pos), 0.f, 0.f, 1.f };
				break;
			case WorldScape3DEditor::Tool::ROUGHNESS:
				color = terrain->get_data()->get_pixel(WorldScape3DRegion::TYPE_COLOR, global_pos);
				break;
			case WorldScape3DEditor::Tool::COLOR:
				color = terrain->get_data()->get_color(global_pos);
				break;
			case WorldScape3DEditor::Tool::ANGLE:
				color = Color{ terrain->get_data()->get_control_angle(global_pos), 0.f, 0.f, 1.f };
				break;
			case WorldScape3DEditor::Tool::SCALE:
				color = Color{ terrain->get_data()->get_control_scale(global_pos), 0.f, 0.f, 1.f };
				break;
			default:
				print_error("Unsupported picking type");
				return;
		}
		if (_pick_callback.is_valid()) {
			[[maybe_unused]] auto _ = _pick_callback.call(_picking, color, global_pos);
			_pick_callback = Callable{};
		}
		_picking = WorldScape3DEditor::Tool::TOOL_MAX;
	} else if (_operation_builder && _operation_builder->is_picking()) {
		_operation_builder->pick(global_pos, terrain);
	}
}

void WorldScape3DUI::set_active_operation() {
	bool mod_ctrl = _plugin->is_modifier_on();
	bool inverted = mod_ctrl || _inverted_input;

	// toggle toolbar buttons
	_toolbar->show_buttons(!inverted, inverted);

	// if shift, smoothness
	if (_plugin->is_shift_on() && !inverted) {
		_active_tool = WorldScape3DEditor::Tool::SCULPT;
		_active_operation = WorldScape3DEditor::Operation::AVERAGE;
		// else if ctrl/invert checked, opposite
	} else if (_selected_operation == WorldScape3DEditor::Operation::ADD && inverted) {
		_active_tool = _selected_tool;
		_active_operation = WorldScape3DEditor::Operation::SUBTRACT;
	} else if (_selected_operation == WorldScape3DEditor::Operation::SUBTRACT && !inverted) {
		_active_tool = _selected_tool;
		_active_operation = WorldScape3DEditor::Operation::ADD;
		// else was default and set
	} else {
		_active_tool = _selected_tool;
		_active_operation = _selected_operation;
	}

	if (auto editor = _plugin->get_editor(); editor) {
		editor->set_tool(_active_tool);
		editor->set_operation(_active_operation);
	}
}

void WorldScape3DUI::set_visible(bool visible, bool menu_only) {
	if (_visible == visible) {
		return;
	}
	_visible = visible;
	_menu->set_visible(_visible);
	_toolbar->set_visible(menu_only ? false : _visible);
	_tool_settings->set_visible(menu_only ? false : _visible);
	if (!menu_only) {
		bool mod_ctrl = _plugin->is_modifier_on();
		_toolbar->show_buttons(!mod_ctrl, mod_ctrl);
	}

	if (auto editor = _plugin->get_editor(); editor) {
		if (_visible) {
			_update_timer = get_tree()->create_timer(.01);
			_update_timer->connect("timeout", callable_mp(this, &WorldScape3DUI::on_visible_update)); // won't work otherwise
		} else {
			editor->set_tool(WorldScape3DEditor::Tool::TOOL_MAX);
			editor->set_operation(WorldScape3DEditor::Operation::OP_MAX);
		}
	}
}

void WorldScape3DUI::set_menu_visibility(Control *list, const bool visible) {
	if (list) {
		if (auto parent = list->get_parent(); parent) {
			parent = parent->get_parent();
			if (auto control = cast_to<Control>(parent); control) {
				control->set_visible(visible);
			}
		}
	}
}

void WorldScape3DUI::on_tool_changed(const WorldScape3DEditor::Tool tool, const WorldScape3DEditor::Operation operation) {
	_selected_tool = tool;
	_selected_operation = operation;
	clear_picking();
	set_menu_visibility(_tool_settings->advanced_list(), true);
	set_menu_visibility(_tool_settings->scale_list(), false);
	set_menu_visibility(_tool_settings->rotation_list(), false);
	set_menu_visibility(_tool_settings->height_list(), false);
	set_menu_visibility(_tool_settings->color_list(), false);

	// Select which settings to show. Options in WorldScape3DToolSettings
	PackedStringArray to_show;

	switch (_selected_tool) {
		case WorldScape3DEditor::Tool::REGION:
			to_show.push_back("instructions");
			to_show.push_back("invert");
			set_menu_visibility(_tool_settings->advanced_list(), false);
			break;

		case WorldScape3DEditor::Tool::SCULPT: {
			to_show.push_back("brush");
			to_show.push_back("size");
			to_show.push_back("strength");
			if (_selected_operation == WorldScape3DEditor::Operation::ADD || _selected_operation == WorldScape3DEditor::Operation::SUBTRACT) {
				to_show.push_back("invert");
			} else if (_selected_operation == WorldScape3DEditor::Operation::GRADIENT) {
				to_show.push_back("gradient_points");
				to_show.push_back("drawable");
			}
			break;
		}

		case WorldScape3DEditor::Tool::HEIGHT:
			to_show.push_back("brush");
			to_show.push_back("size");
			to_show.push_back("strength");
			to_show.push_back("height");
			to_show.push_back("height_picker");
			to_show.push_back("invert");
			break;

		case WorldScape3DEditor::Tool::TEXTURE: {
			to_show.push_back("brush");
			to_show.push_back("size");
			to_show.push_back("enable_texture");
			if (_selected_operation == WorldScape3DEditor::Operation::ADD) {
				to_show.push_back("strength");
				to_show.push_back("invert");
			}
			to_show.push_back("slope");
			to_show.push_back("enable_angle");
			to_show.push_back("angle");
			to_show.push_back("angle_picker");
			to_show.push_back("dynamic_angle");
			to_show.push_back("enable_scale");
			to_show.push_back("scale");
			to_show.push_back("scale_picker");
			break;
		}

		case WorldScape3DEditor::Tool::COLOR:
			to_show.push_back("brush");
			to_show.push_back("size");
			to_show.push_back("strength");
			to_show.push_back("color");
			to_show.push_back("color_picker");
			to_show.push_back("slope");
			to_show.push_back("texture_filter");
			to_show.push_back("margin");
			to_show.push_back("invert");
			break;

		case WorldScape3DEditor::Tool::ROUGHNESS:
			to_show.push_back("brush");
			to_show.push_back("size");
			to_show.push_back("strength");
			to_show.push_back("roughness");
			to_show.push_back("roughness_picker");
			to_show.push_back("slope");
			to_show.push_back("texture_filter");
			to_show.push_back("margin");
			to_show.push_back("invert");
			break;

		case WorldScape3DEditor::Tool::AUTOSHADER:
		case WorldScape3DEditor::Tool::HOLES:
		case WorldScape3DEditor::Tool::NAVIGATION:
			to_show.push_back("brush");
			to_show.push_back("size");
			to_show.push_back("invert");
			break;

		case WorldScape3DEditor::Tool::INSTANCER:
			to_show.push_back("size");
			to_show.push_back("strength");
			to_show.push_back("slope");
			set_menu_visibility(_tool_settings->height_list(), true);
			to_show.push_back("height_offset");
			to_show.push_back("random_height");
			set_menu_visibility(_tool_settings->scale_list(), true);
			to_show.push_back("fixed_scale");
			to_show.push_back("random_scale");
			set_menu_visibility(_tool_settings->rotation_list(), true);
			to_show.push_back("fixed_spin");
			to_show.push_back("random_spin");
			to_show.push_back("fixed_tilt");
			to_show.push_back("random_tilt");
			to_show.push_back("align_to_normal");
			set_menu_visibility(_tool_settings->color_list(), true);
			to_show.push_back("vertex_color");
			to_show.push_back("random_darken");
			to_show.push_back("random_hue");
			//to_show.push_back("on_collision");
			to_show.push_back("invert");
			break;
		default:
			return;
	}

	// Advanced menu settings
	to_show.push_back("auto_regions");
	to_show.push_back("align_to_view");
	to_show.push_back("show_cursor_while_painting");
	to_show.push_back("gamma");
	to_show.push_back("jitter");
	to_show.push_back("crosshair_threshold");

	_tool_settings->show_settings(to_show);

	_operation_builder.reset();
	if (_selected_operation == WorldScape3DEditor::Operation::GRADIENT) {
		_operation_builder = std::make_unique<WorldScape3DGradientOperationBuilder>(_tool_settings);
	}

	on_setting_changed();
	_plugin->update_region_grid();
}

void WorldScape3DUI::on_setting_changed(const Variant &setting) {
	auto asset_dock = _plugin->get_asset_dock();
	if (!asset_dock) {
		return; // Skip function if not ready
	}

	_brush_data = _tool_settings->get_brush_data();
	_brush_data.set("asset_id", asset_dock->get_current_list()->get_selected_id());
	if (auto editor = _plugin->get_editor(); editor) {
		editor->set_brush_data(_brush_data);
	}
	_inverted_input = _brush_data.get("invert", false);
	if (auto checkbox = cast_to<CheckBox>(setting); checkbox && checkbox->get_name() == SNAME("Invert")) {
		_plugin->read_input({}); // Revalidate keyboard input for modifier_ctrl
	}

	set_active_operation();
	update_decal();
}

void WorldScape3DUI::on_decal_timer() {
	NodePath const path{ "editor_decal_fade" };
	auto tree = get_tree();
	if (tree) {
		auto tween = tree->create_tween();
		tween->tween_property(this, path, .0, 0.15);
	}
}

void WorldScape3DUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_editor_decal_fade", "value"), &WorldScape3DUI::set_editor_decal_fade);
	ClassDB::bind_method(D_METHOD("get_editor_decal_fade"), &WorldScape3DUI::get_editor_decal_fade);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "editor_decal_fade"), "set_editor_decal_fade", "get_editor_decal_fade");
}

void WorldScape3DUI::_notification(int what) {
	Node::_notification(what);
	switch (what) {
		case NOTIFICATION_ENTER_TREE: {
			on_tool_changed(WorldScape3DEditor::Tool::REGION, WorldScape3DEditor::Operation::ADD);
			break;
		}
		default:
			break;
	}
}

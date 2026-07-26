/**************************************************************************/
/*  worldscape_3d_material.h                                              */
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

#include "scene/resources/shader.h"

#include "constants.h"
#include "generated_texture.h"

class WorldScape3D;

class WorldScape3DMaterial : public Resource {
	GDCLASS(WorldScape3DMaterial, Resource);
	CLASS_NAME();
	friend class WorldScape3D;

public: // Constants
	enum WorldBackground {
		NONE,
		FLAT,
		NOISE,
	};

	enum TextureFiltering {
		LINEAR,
		NEAREST,
	};

private:
	WorldScape3D *_terrain = nullptr;

	RID _material;
	Ref<Shader> _shader; // Active shader
	Dictionary _shader_code; // All loaded shader and INSERT code
	bool _shader_override_enabled = false;
	Ref<Shader> _shader_override; // User's shader we copy code from
	mutable TypedArray<StringName> _active_params; // All shader params in the current shader
	mutable Dictionary _shader_params; // Public shader params saved to disk

	// Material Features
	WorldBackground _world_background = FLAT;
	TextureFiltering _texture_filtering = LINEAR;
	bool _dual_scaling = false;
	bool _auto_shader = false;

	// Overlays
	bool _show_region_grid = false;
	bool _show_instancer_grid = false;
	bool _show_vertex_grid = false;
	bool _show_contours = false;
	bool _show_navigation = false;

	// Debug Views
	bool _debug_view_checkered = false;
	bool _debug_view_grey = false;
	bool _debug_view_heightmap = false;
	bool _debug_view_colormap = false;
	bool _debug_view_roughmap = false;
	bool _debug_view_control_texture = false;
	bool _debug_view_control_angle = false;
	bool _debug_view_control_scale = false;
	bool _debug_view_control_blend = false;
	bool _debug_view_autoshader = false;
	bool _debug_view_holes = false;
	bool _debug_view_tex_height = false;
	bool _debug_view_tex_normal = false;
	bool _debug_view_tex_rough = false;
	bool _debug_view_jaggedness = false;

	// Functions
	void _preload_shaders();
	void _parse_shader(const String &p_shader, const String &p_name);
	String _apply_inserts(const String &p_shader, const Array &p_excludes = Array()) const;
	String _generate_shader_code() const;
	String _strip_comments(const String &p_shader) const;
	String _inject_editor_code(const String &p_shader) const;
	void _update_shader();
	void _update_maps();
	void _update_texture_arrays();
	void _set_shader_parameters(const Dictionary &p_dict);
	Dictionary _get_shader_parameters() const { return _shader_params; }

public:
	WorldScape3DMaterial() {}
	~WorldScape3DMaterial() override { destroy(); }
	void initialize(WorldScape3D *p_terrain);
	bool is_initialized() { return _terrain != nullptr; }
	void uninitialize();
	void destroy();

	void update();
	RID get_material_rid() const { return _material; }
	RID get_shader_rid() const { return _shader.is_valid() ? _shader->get_rid() : RID(); }

	// Material settings
	void set_world_background(const WorldBackground p_background);
	WorldBackground get_world_background() const { return _world_background; }
	void set_texture_filtering(const TextureFiltering p_filtering);
	TextureFiltering get_texture_filtering() const { return _texture_filtering; }
	void set_auto_shader(const bool p_enabled);
	bool get_auto_shader() const { return _auto_shader; }
	void set_dual_scaling(const bool p_enabled);
	bool get_dual_scaling() const { return _dual_scaling; }

	void enable_shader_override(const bool p_enabled);
	bool is_shader_override_enabled() const { return _shader_override_enabled; }
	void set_shader_override(const Ref<Shader> &p_shader);
	Ref<Shader> get_shader_override() const { return _shader_override; }

	void set_shader_param(const StringName &p_name, const Variant &p_value);
	Variant get_shader_param(const StringName &p_name) const;

	// Overlays
	void set_show_region_grid(const bool p_enabled);
	bool get_show_region_grid() const { return _show_region_grid; }
	void set_show_instancer_grid(const bool p_enabled);
	bool get_show_instancer_grid() const { return _show_instancer_grid; }
	void set_show_vertex_grid(const bool p_enabled);
	bool get_show_vertex_grid() const { return _show_vertex_grid; }
	void set_show_contours(const bool p_enabled);
	bool get_show_contours() const { return _show_contours; }
	void set_show_navigation(const bool p_enabled);
	bool get_show_navigation() const { return _show_navigation; }

	// Debug views
	void set_show_checkered(const bool p_enabled);
	bool get_show_checkered() const { return _debug_view_checkered; }
	void set_show_grey(const bool p_enabled);
	bool get_show_grey() const { return _debug_view_grey; }
	void set_show_heightmap(const bool p_enabled);
	bool get_show_heightmap() const { return _debug_view_heightmap; }
	void set_show_jaggedness(const bool p_enabled);
	bool get_show_jaggedness() const { return _debug_view_jaggedness; }
	void set_show_colormap(const bool p_enabled);
	bool get_show_colormap() const { return _debug_view_colormap; }
	void set_show_roughmap(const bool p_enabled);
	bool get_show_roughmap() const { return _debug_view_roughmap; }
	void set_show_control_texture(const bool p_enabled);
	bool get_show_control_texture() const { return _debug_view_control_texture; }
	void set_show_control_angle(const bool p_enabled);
	bool get_show_control_angle() const { return _debug_view_control_angle; }
	void set_show_control_scale(const bool p_enabled);
	bool get_show_control_scale() const { return _debug_view_control_scale; }
	void set_show_control_blend(const bool p_enabled);
	bool get_show_control_blend() const { return _debug_view_control_blend; }
	void set_show_autoshader(const bool p_enabled);
	bool get_show_autoshader() const { return _debug_view_autoshader; }
	void set_show_texture_height(const bool p_enabled);
	bool get_show_texture_height() const { return _debug_view_tex_height; }
	void set_show_texture_normal(const bool p_enabled);
	bool get_show_texture_normal() const { return _debug_view_tex_normal; }
	void set_show_texture_rough(const bool p_enabled);
	bool get_show_texture_rough() const { return _debug_view_tex_rough; }

	Error save(const String &p_path = "");

protected:
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;
	bool _set(const StringName &p_name, const Variant &p_property);
	bool _get(const StringName &p_name, Variant &r_property) const;

	static void _bind_methods();
};

VARIANT_ENUM_CAST(WorldScape3DMaterial::WorldBackground);
VARIANT_ENUM_CAST(WorldScape3DMaterial::TextureFiltering);

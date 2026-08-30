// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

// These special inserts are injected into the shader code at the end of fragment().

R"(
//INSERT: EDITOR_NAVIGATION
	// Show navigation
	{
		if(bool(floatBitsToUint(texelFetch(_control_maps, get_index_coord(floor(uv + 0.5), SKIP_PASS), 0)).r >>1u & 0x1u)) {
			ALBEDO *= vec3(.5, .0, .85);
		}
	}

//INSERT: EDITOR_DECAL_SETUP
uniform highp sampler2D _editor_brush_texture : source_color, filter_linear, repeat_disable;
uniform highp sampler2D _editor_ring_texture : source_color, filter_linear, repeat_disable;
uniform vec2 _editor_decal_position[3];
uniform float _editor_decal_size[3];
uniform float _editor_decal_rotation[3];
uniform vec4 _editor_decal_color[3] : source_color;
uniform bool _editor_decal_visible[3];
uniform float _editor_crosshair_threshold = 16.0;

// expects uv (Texture/world space 0 to +/- inf 1m units).
vec3 get_decal(vec3 albedo, vec2 uv) {
	for (int i = 0; i < 3; ++i) {
		if (!_editor_decal_visible[i]) {
			continue;
		}
		float size = 1.0 / _editor_decal_size[i];
		float cosa = cos(_editor_decal_rotation[i]);
		float sina = sin(_editor_decal_rotation[i]);
		vec2 decal_uv = (vec2(cosa * uv.x - sina * uv.y, sina * uv.x + cosa * uv.y) -
			vec2(cosa * _editor_decal_position[i].x - sina * _editor_decal_position[i].y,
			sina * _editor_decal_position[i].x + cosa * _editor_decal_position[i].y) * _vertex_density) * size * _vertex_spacing;
		if (abs(decal_uv.x) > 0.499 || abs(decal_uv.y) > 0.499) {
			continue;
		}
		float decal = 0.0;
		switch (i) {
			case 0 :
				decal = texture(_editor_brush_texture, decal_uv + 0.5).r;
				break;
			case 1:
			case 2:
				decal = texture(_editor_ring_texture, decal_uv + 0.5).r;
				break;
		}
		// Blend in decal; square for better visual blend
		albedo =  mix(albedo, _editor_decal_color[i].rgb, decal * decal * _editor_decal_color[i].a);
	}
	// Crosshair
	if (_editor_decal_visible[0] && _editor_decal_size[0] <= _editor_crosshair_threshold) {
		vec2 cross_uv = ((uv - _editor_decal_position[0] * _vertex_density) * _vertex_spacing) * 16.0;
		cross_uv /= sqrt(length(_camera_pos - v_vertex));
		float line_thickness = 0.5;
		float line_start = _editor_decal_size[0] * 0.5 + 8.;
		float line_end = _editor_decal_size[0] * 0.5 + 16.;
		bool h = abs(cross_uv.y) < line_thickness && abs(cross_uv.x) < line_end && abs(cross_uv.x) > line_start;
		bool v = abs(cross_uv.x) < line_thickness && abs(cross_uv.y) < line_end && abs(cross_uv.y) > line_start;
		albedo = (h || v) ? mix(albedo, _editor_decal_color[0].rgb, _editor_decal_color[0].a) : albedo;
	}
	return albedo;
}

//INSERT: EDITOR_DECAL_RENDER
	// Render decal
	{
		ALBEDO = get_decal(ALBEDO, uv);
	}
)"

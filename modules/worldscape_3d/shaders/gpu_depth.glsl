// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

// This shader reads the screen and returns absolute depth encoded in Albedo.rg
// It is not used as an INSERT

R"(
shader_type spatial;
render_mode unshaded;

uniform highp sampler2D depth_texture : source_color, hint_depth_texture, filter_nearest, repeat_disable;

void fragment() {
	float depth = textureLod(depth_texture, SCREEN_UV, 0.).x;
	#if CURRENT_RENDERER == RENDERER_COMPATIBILITY
	depth = depth * 2.0 - 1.0;
	#endif
	vec3 ndc = vec3(SCREEN_UV * 2.0 - 1.0, depth);
	vec4 view = INV_PROJECTION_MATRIX * vec4(ndc, 1.0);
	view.xyz /= view.w;
	float depth_linear = -view.z;

	// Normalize depth to the range 0 - 1
	highp float scaledDepth = clamp(depth_linear / 100000.0, 0.0, 1.0);

	// Encode using 127 steps, which map to the 128 - 255 range.
	// Avoids precision loss for compatibility and mobile renderer
	// 21bit depth value
	highp float r = (floor(scaledDepth * 127.0) + 128.0) / 255.0;
	highp float g = (floor(fract(scaledDepth * 127.0) * 127.0) + 128.0) / 255.0;
	highp float b = (floor(fract(scaledDepth * 127.0 * 127.0) * 127.0) + 128.0) / 255.0;

	ALBEDO = vec3(r, g, b); // Return encoded value, required for Mobile & Compatibility renderers

}

)"

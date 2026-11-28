/* clang-format off */
#[vertex]
#version 450

// Light-space caustic map -- the real-time-rendering analog of a shadow map.
// Renders the water grid from an ORTHOGRAPHIC projection along the sun
// direction (not a fixed floor_y plane), producing a 2D texture indexed by
// light-space XY that's independent of what's actually receiving the light.
// Any receiving surface (a flat floor, a wall, an uneven terrain, a
// submerged prop) samples this SAME texture by projecting its own world
// position into the same light-space basis -- exactly how a shadow map
// lookup works, and the reason this generalizes where the old flat-floor-
// intersection technique couldn't (a wall has almost no XZ variation, which
// is all the old technique looked at; light-space position captures a
// wall's real variation instead).
//
// A genuine RenderingDevice graphics (draw) pipeline, not a SubViewport --
// this module already does 100% of its other GPU work via direct RD compute
// dispatch (see water_ripple.glsl et al.), and a real, confirmed bug this
// session found is that adding a second live SubViewport (its own World3D +
// Camera3D rendering every frame) corrupts the MAIN viewport's transparent-
// object rendering in this engine fork -- disabling the SubViewport was the
// one change that fixed the "floor washes to white" bug; nothing at the
// shader or Environment level did. This pass sidesteps that bug class
// entirely by never creating a second Viewport at all.

layout(set = 0, binding = 0, std140) uniform Params {
	vec4 sun_dir_ior;       // xyz sun_direction (unit, pointing FROM sun TOWARD scene), w ior
	vec4 light_right_ext;   // xyz light-space "right" basis vector (unit), w half_extent (world units)
	vec4 light_up_refdepth; // xyz light-space "up" basis vector (unit), w reference_depth (world units along the refracted ray)
	vec4 ripple_domain;     // xy ripple layer's domain size, z 1 if per-cell depth is set, w shallow_fade_depth
	vec4 ocean_domain;      // xy ocean layer's domain size, zw unused
	vec4 viewport_wh;       // xy render target size in pixels, zw unused
	vec4 light_origin;      // xyz center of the projected map, w unused
};

layout(set = 0, binding = 1) uniform sampler2D ripple_height_tex;
layout(set = 0, binding = 2) uniform sampler2D ocean_height_tex;

// Still-water depth over the ripple domain (see water_inc.glsl): light only
// passes through water, and the ocean chop fades out over the shallows the
// same way the rendered surface does.
layout(set = 0, binding = 3) uniform sampler2D cell_depth_tex;

float depth_at(vec2 xz) {
	vec2 uv = xz / ripple_domain.xy + 0.5;
	// ripple_domain.z: +1 per-cell depth is set, +2 the water carries on past
	// the grid (rendered out there: render_extent).
	bool has_depth = mod(ripple_domain.z, 2.0) > 0.5;
	bool outside = any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)));
	if (outside && ripple_domain.z < 1.5) {
		return 0.0; // the water ends at the grid (a pool's walls, a lake's banks)
	}
	if (outside && !has_depth) {
		return 1e6; // open water
	}
	// Per-cell depth carries on past the grid edge (the map spans a little
	// more than one ocean tile, see _rt_build_caustics_grid), the same way
	// the surface material treats it.
	return texture(cell_depth_tex, clamp(uv, vec2(0.0), vec2(1.0))).r;
}

// The rendered chop fades out over shallow_fade_depth so it can't cut
// through the sand, but the waves still arrive there: the light keeps
// focusing into caustics right up to the shore, fading only over the last
// 30 cm.
float ocean_fade(vec2 xz) {
	return smoothstep(0.0, min(max(ripple_domain.w, 1e-3), 0.3), depth_at(xz));
}

layout(location = 0) in vec2 in_xz;

layout(location = 0) out vec2 v_flat_xz;
// 0 = refracted caustics, 1 = the flat water mask (see main()).
layout(location = 1) flat out int v_mode;

float sample_h(vec2 world_xz) {
	// Ocean wraps (it's periodic -- that's what lets receivers tile the
	// map); ripples mirror past their grid, seam-free.
	vec2 uv_r = world_xz / ripple_domain.xy + 0.5;
	uv_r = 1.0 - abs(mod(uv_r, 2.0) - 1.0);
	vec2 uv_o = world_xz / ocean_domain.xy + 0.5; // repeat sampler
	return texture(ripple_height_tex, uv_r).r + texture(ocean_height_tex, uv_o).r * ocean_fade(world_xz);
}

void main() {
	vec2 xz = in_xz;
	float h = sample_h(xz);

	vec2 height_texel_size = ocean_domain.xy / max(ocean_domain.zw - vec2(1.0), vec2(1.0));
	float e = min(height_texel_size.x, height_texel_size.y) * 0.5;
	float hx1 = sample_h(xz + vec2(e, 0.0));
	float hx0 = sample_h(xz - vec2(e, 0.0));
	float hz1 = sample_h(xz + vec2(0.0, e));
	float hz0 = sample_h(xz - vec2(0.0, e));
	vec3 n = normalize(vec3(hx0 - hx1, 2.0 * e, hz0 - hz1));

	vec3 origin = light_origin.xyz;
	vec3 world_pos = origin + vec3(xz.x, h, xz.y);
	vec3 lin = normalize(sun_dir_ior.xyz);
	vec3 r = refract(lin, n, 1.0 / max(sun_dir_ior.w, 1.001));
	// Photon position after traveling a fixed reference distance along its
	// refracted path -- the standard practical caustic-map approximation
	// (one reference depth for the whole map, not a true per-receiver
	// intersection). Real caustic sharpness does vary with receiver depth in
	// reality too; this is the same tradeoff every real-time caustic-map
	// implementation makes.
	vec3 ref_point = world_pos + r * light_up_refdepth.w;

	vec3 light_right = light_right_ext.xyz;
	vec3 light_up = light_up_refdepth.xyz;
	float half_extent = max(light_right_ext.w, 1e-3);
	float lu = dot(ref_point - origin, light_right) / half_extent;
	float lv = dot(ref_point - origin, light_up) / half_extent;

	v_flat_xz = xz;
	v_mode = gl_InstanceIndex;
	if (gl_InstanceIndex == 1) {
		// Second instance: the flat water surface projected straight along
		// the sun, unrefracted, writing the water mask into G. Every point on
		// one light-space line shares where that sun ray crosses the water's
		// height, so a receiver reads G at its own UV to know whether its
		// sunlight came through water -- the outside of a pool's wall lines up
		// with lit photons inside the pool, but its light never touched water.
		vec3 flat_pos = origin + vec3(xz.x, 0.0, xz.y);
		lu = dot(flat_pos - origin, light_right) / half_extent;
		lv = dot(flat_pos - origin, light_up) / half_extent;
	}

	gl_Position = vec4(lu, lv, 0.0, 1.0);
}

#[fragment]
#version 450

layout(set = 0, binding = 0, std140) uniform Params {
	vec4 sun_dir_ior;
	vec4 light_right_ext;
	vec4 light_up_refdepth;
	vec4 ripple_domain;
	vec4 ocean_domain;
	vec4 viewport_wh;
	vec4 light_origin;
};

layout(set = 0, binding = 1) uniform sampler2D ripple_height_tex;
layout(set = 0, binding = 2) uniform sampler2D ocean_height_tex;
// Still-water depth over the ripple domain (see water_inc.glsl): light only
// passes through water, and the ocean chop fades out over the shallows the
// same way the rendered surface does.
layout(set = 0, binding = 3) uniform sampler2D cell_depth_tex;

float depth_at(vec2 xz) {
	vec2 uv = xz / ripple_domain.xy + 0.5;
	// ripple_domain.z: +1 per-cell depth is set, +2 the water carries on past
	// the grid (rendered out there: render_extent).
	bool has_depth = mod(ripple_domain.z, 2.0) > 0.5;
	bool outside = any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)));
	if (outside && ripple_domain.z < 1.5) {
		return 0.0; // the water ends at the grid (a pool's walls, a lake's banks)
	}
	if (outside && !has_depth) {
		return 1e6; // open water
	}
	// Per-cell depth carries on past the grid edge (the map spans a little
	// more than one ocean tile, see _rt_build_caustics_grid), the same way
	// the surface material treats it.
	return texture(cell_depth_tex, clamp(uv, vec2(0.0), vec2(1.0))).r;
}

// The rendered chop fades out over shallow_fade_depth so it can't cut
// through the sand, but the waves still arrive there: the light keeps
// focusing into caustics right up to the shore, fading only over the last
// 30 cm.
float ocean_fade(vec2 xz) {
	return smoothstep(0.0, min(max(ripple_domain.w, 1e-3), 0.3), depth_at(xz));
}

layout(location = 0) in vec2 v_flat_xz;
layout(location = 1) flat in int v_mode;

layout(location = 0) out vec4 out_color;

float sample_h(vec2 world_xz) {
	vec2 uv_r = world_xz / ripple_domain.xy + 0.5;
	uv_r = 1.0 - abs(mod(uv_r, 2.0) - 1.0);
	vec2 uv_o = world_xz / ocean_domain.xy + 0.5; // repeat sampler
	return texture(ripple_height_tex, uv_r).r + texture(ocean_height_tex, uv_o).r * ocean_fade(world_xz);
}

vec2 project_refracted_point(vec2 xz, out float incidence) {
	vec2 texel_size = ocean_domain.xy / max(ocean_domain.zw - vec2(1.0), vec2(1.0));
	float e = min(texel_size.x, texel_size.y) * 0.5;
	float h = sample_h(xz);
	float hx1 = sample_h(xz + vec2(e, 0.0));
	float hx0 = sample_h(xz - vec2(e, 0.0));
	float hz1 = sample_h(xz + vec2(0.0, e));
	float hz0 = sample_h(xz - vec2(0.0, e));
	vec3 n = normalize(vec3(hx0 - hx1, 2.0 * e, hz0 - hz1));
	vec3 origin = light_origin.xyz;
	vec3 world_pos = origin + vec3(xz.x, h, xz.y);
	vec3 lin = normalize(sun_dir_ior.xyz);
	vec3 refracted = refract(lin, n, 1.0 / max(sun_dir_ior.w, 1.001));
	incidence = max(dot(-lin, n), 0.0);
	vec3 ref_point = world_pos + refracted * light_up_refdepth.w;
	return vec2(dot(ref_point - origin, light_right_ext.xyz),
			dot(ref_point - origin, light_up_refdepth.xyz));
}

float wet_at(vec2 xz) {
	return clamp(depth_at(xz) * 20.0, 0.0, 1.0);
}

void main() {
	if (v_mode == 1) {
		out_color = vec4(0.0, wet_at(v_flat_xz), 0.0, 0.0);
		return;
	}
	vec2 source_dx = dFdx(v_flat_xz);
	vec2 source_dy = dFdy(v_flat_xz);
	float incidence;
	vec2 refracted = project_refracted_point(v_flat_xz, incidence);
	vec2 refracted_dx = dFdx(refracted);
	vec2 refracted_dy = dFdy(refracted);

	float source_area = abs(source_dx.x * source_dy.y - source_dx.y * source_dy.x);
	float refracted_area = abs(refracted_dx.x * refracted_dy.y - refracted_dx.y * refracted_dy.x);
	float flat_projection_area = abs(light_right_ext.x * light_up_refdepth.z -
			light_right_ext.z * light_up_refdepth.x);
	float concentration = flat_projection_area * source_area / max(refracted_area, 1e-9);
	float focused_light = max(concentration - 1.0, 0.0);
	float tone = focused_light / (focused_light + 8.0);
	out_color = vec4(incidence * tone * wet_at(v_flat_xz), 0.0, 0.0, 0.0);
}

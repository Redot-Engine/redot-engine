/**************************************************************************/
/*  physx_water_surface_3d.cpp                                            */
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

#include "physx_water_surface_3d.h"

#include "physx_water_wake_3d.h"

#include "scene/3d/camera_3d.h"
#include "scene/main/viewport.h"

#include "core/config/engine.h"
#include "core/math/face3.h"
#include "core/object/class_db.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/shader.h"
#include "scene/resources/texture_rd.h"
#include "servers/physics_3d/physics_server_3d.h"

namespace {
// A plain spatial Shader (Godot's own shading language, NOT the RD compute
// shaders the rest of this module uses). Displaces the mesh's own VERTEX.y
// by two independently-sampled height fields (ripple + FFT ocean, summed)
// sampled directly from the solver's shared-RenderingDevice textures via
// Texture2DRD; the separate CPU readback cache is only for sample_height().
//
// Real transparency via Godot's own GPU alpha blending (ALPHA, blend_mix),
// not manual screen-space refraction. An earlier version manually sampled
// hint_screen_texture and composited by hand -- confirmed via direct testing
// that this custom engine build's screen-texture capture reads severely
// overbright (even attenuated to 15% strength it still blew the surface to
// solid white; the engine has no REFRACTION built-in either, confirmed
// directly against servers/rendering/shader_types.cpp, so that wasn't an
// option). Real GPU alpha blending sidesteps the broken capture path
// entirely -- the renderer composites against whatever's already correctly
// drawn underneath, no re-sampling needed. Loses the distortion detail a
// real refraction offset would add; not worth it while the capture itself
// is broken.
//
// Real per-vertex normals from a central-difference of the SAME combined
// height field the vertex is displaced by -- the surface used to move but
// never actually shade like it moved, which was a real, separate
// contributor to an earlier "looks flat" complaint. Feeds a small, bounded,
// self-contained shading term in fragment() (unshaded, not Godot's real
// PBR NORMAL) -- see fragment()'s own comment for why real PBR lighting on
// this material caused a genuine overexposure bug.
const char *WATER_SHADER_SRC = R"(
shader_type spatial;
render_mode blend_mix, cull_disabled, unshaded;

uniform sampler2D ripple_height_tex : hint_default_black, filter_linear, repeat_disable;
uniform sampler2D ocean_height_tex : hint_default_black, filter_linear, repeat_enable;
// Ocean chop multiplier over the ripple domain: 1 in deep water, fading to 0
// at the waterline (set by PhysXWaterSurface3D when it has a seabed).
uniform sampler2D ocean_fade_tex : hint_default_white, filter_linear, repeat_disable;
// Choppy horizontal displacement (x, z) of the ocean layer; scaled by
// choppiness (both set by PhysXWaterSurface3D).
uniform sampler2D ocean_disp_tex : hint_default_black, filter_linear, repeat_enable;
uniform float choppiness = 1.0;
// Ocean slopes and displacement derivatives (dh/dx, dh/dz, dDx/dx, dDz/dz),
// straight from the FFT, for exact per-pixel normals and foam.
uniform sampler2D ocean_deriv_tex : hint_default_black, filter_linear, repeat_enable;
// true: normals and foam per pixel from ocean_deriv_tex (smooth crests at any
// mesh density). false: per vertex, from finite differences of the displaced
// mesh (set by PhysXWaterSurface3D::normal_mode).
uniform bool per_pixel_normals = true;
uniform vec2 ripple_domain_size = vec2(20.0, 20.0);
uniform vec2 ocean_domain_size = vec2(40.0, 40.0);
// World XZ both height fields are centered on (set by PhysXWaterSurface3D).
uniform vec2 grid_center = vec2(0.0);
// World xz the whole open-water mesh is moved by, so its dense core stays
// under the camera; set by the node every frame.
uniform vec2 mesh_offset = vec2(0.0);
// Moving wake grids (PhysXWaterWake3D), up to four: height textures and
// their rects (xy world xz of the corner, z side length -- 0 = unused, w how
// much the wake fades out over the shore ripple grid, 0..1).
uniform sampler2D wake_height_0 : hint_default_black, filter_linear, repeat_disable;
uniform sampler2D wake_height_1 : hint_default_black, filter_linear, repeat_disable;
uniform sampler2D wake_height_2 : hint_default_black, filter_linear, repeat_disable;
uniform sampler2D wake_height_3 : hint_default_black, filter_linear, repeat_disable;
uniform vec4 wake_rect_0 = vec4(0.0);
uniform vec4 wake_rect_1 = vec4(0.0);
uniform vec4 wake_rect_2 = vec4(0.0);
uniform vec4 wake_rect_3 = vec4(0.0);
// Mean surface height (node-relative), what the ripple layer settles back to
// past the edge of its domain when the surface is rendered further out.
uniform float water_level = 0.0;
// Animation clock for the shore noise, set by PhysXWaterSurface3D while the
// game runs -- not the shader's built-in clock: a material that reads it makes
// the editor redraw its 3D view every frame, and with a big scene open that
// ate half of Play Scene's frame rate. In the editor this stays 0 (the water
// is a static preview there anyway).
uniform float water_time = 0.0;
uniform vec4 water_color : source_color = vec4(0.09, 0.32, 0.42, 0.65);
// Persistent whitecap foam (0..1) on the ocean grid, injected where the
// waves fold and fading over the node's foam_persistence (set by
// PhysXWaterSurface3D).
uniform sampler2D ocean_foam_tex : hint_default_black, filter_linear, repeat_enable;
uniform vec4 foam_color : source_color = vec4(0.92, 0.95, 0.97, 0.95);
// How much fading foam breaks up into lace instead of thinning evenly
// (0 = solid sheet until it's gone).
uniform float foam_breakup : hint_range(0.0, 1.0) = 0.6;
// Size of that lace: noise cells per meter, fixed to the water.
uniform float foam_detail_scale = 1.5;
// Shore foam (0..1) on the ripple grid: the foamy sheet where waves wash in
// over the shallows (set by PhysXWaterSurface3D; needs a seabed).
uniform sampler2D shore_foam_tex : hint_default_black, filter_linear, repeat_disable;
// Shore foam is a thin sheet, not a whitecap: fainter, finer and lacier.
uniform float shore_foam_strength : hint_range(0.0, 1.0) = 0.65;
uniform float shore_foam_breakup : hint_range(0.0, 1.0) = 0.85;
// Churned, milky water over the shallows: tints toward shallow_color as the
// ocean fade drops off near the shore (shallow_tint 0 = off). Its alpha also
// replaces water_color's there.
uniform vec4 shallow_color : source_color = vec4(0.42, 0.66, 0.64, 0.55);
uniform float shallow_tint : hint_range(0.0, 1.0) = 0.6;
// Still-water depth in meters over the ripple domain (dry shore negative,
// pool walls deep), set by PhysXWaterSurface3D: the waterline is measured in
// real depth, independent of shallow_fade_depth.
uniform sampler2D shore_depth_tex : hint_default_white, filter_linear, repeat_disable;
// Soft contact with the shore: the surface fades out over this much water
// depth (m) at the waterline instead of meeting the sand at a hard line.
uniform float shore_edge_softness : hint_range(0.0, 0.5, 0.005) = 0.08;
// Moving waterline: how much water depth (m) the visible edge can pull back
// over. Arriving crests push it toward the sand, troughs pull it back, and
// slow ground-fixed noise keeps it irregular -- so the edge scallops with the
// waves instead of following a depth contour, but only ever goes clear where
// the water really is that thin.
uniform float swash_reach : hint_range(0.0, 1.0, 0.01) = 0.25;
// Swash and wet sand from PhysXWaterSurface3D (swash_run_up, ...): r = how
// high (m) above still water the last waves ran up, g = sand wetness.
uniform sampler2D swash_tex : hint_default_black, filter_linear, repeat_disable;
// Thickness (m) of the swash film over the sand -- just enough to stay above
// the ground mesh.
uniform float swash_film_thickness : hint_range(0.0, 0.2, 0.005) = 0.03;
// Fine ragged fingering of the swash edge: how much water depth (m) drifting
// small-scale noise adds or takes away there (on a 1:14 beach, 0.035 m is
// about half a meter of ragged edge).
uniform float swash_edge_breakup : hint_range(0.0, 0.2, 0.005) = 0.035;
// Size of the shore's small-scale detail -- the lace on the waterline rim,
// the fingering of the swash edge and the ragged wet-sand line, which
// foam_detail_scale doesn't reach. 1 = default; higher = finer.
uniform float shore_detail_scale : hint_range(0.1, 5.0, 0.05) = 1.0;
// Wet sand: rgb blended over the ground where the water has been, alpha the
// strength -- darkens it, fading as it dries.
uniform vec4 wet_sand_color : source_color = vec4(0.16, 0.12, 0.07, 0.45);
// Wave height (m) that pushes the waterline all the way in.
uniform float swash_wave_height = 0.3;
// Thin bright foam rim just behind the moving edge.
uniform float shore_rim_strength : hint_range(0.0, 1.0) = 0.8;

varying vec3 v_world_normal;
varying float v_foam;
varying vec2 v_rest_xz;

// One wake grid's height (x) and foam (y) at world_xz, faded out toward its edges.
vec2 wake_one(sampler2D tex, vec4 rect, vec2 world_xz) {
	if (rect.z <= 0.0) {
		return vec2(0.0);
	}
	vec2 uv = (world_xz - rect.xy) / rect.z;
	float edge = min(min(uv.x, uv.y), min(1.0 - uv.x, 1.0 - uv.y));
	if (edge <= 0.0) {
		return vec2(0.0);
	}
	// Optionally fade out over the shore ripple grid (where a floater's own
	// ripples can take over).
	vec2 from_center = abs((world_xz - grid_center) / ripple_domain_size);
	float outside = smoothstep(0.46, 0.5, max(from_center.x, from_center.y));
	vec4 t = texture(tex, uv); // height, hull foam, propeller foam
	return vec2(t.r, t.g + t.b) * smoothstep(0.0, 0.1, edge) * mix(1.0, outside, rect.w);
}

// All wakes at world_xz, everywhere (inside the shore ripple grid too).
vec2 wake_sample(vec2 world_xz) {
	vec2 w = wake_one(wake_height_0, wake_rect_0, world_xz) + wake_one(wake_height_1, wake_rect_1, world_xz)
			+ wake_one(wake_height_2, wake_rect_2, world_xz) + wake_one(wake_height_3, wake_rect_3, world_xz);
	// Not over dry sand: the wake dies out over the last 30 cm of depth.
	vec2 cuv_r = clamp((world_xz - grid_center) / ripple_domain_size + 0.5, vec2(0.0), vec2(1.0));
	return w * smoothstep(0.0, 0.3, texture(shore_depth_tex, cuv_r).r);
}

float wake_height(vec2 world_xz) {
	return wake_sample(world_xz).x;
}

// Foam the wakes churned in (bow wave, propeller wash, breaking wake waves).
float wake_foam(vec2 world_xz) {
	return min(wake_sample(world_xz).y, 1.0);
}

// Offset of the surface point that rests at world_xz: (dx, height, dz).
vec3 surface_offset(vec2 world_xz) {
	vec2 rel = world_xz - grid_center;
	vec2 cuv_r = clamp(rel / ripple_domain_size + 0.5, vec2(0.0), vec2(1.0));
	vec2 cuv_o = rel / ocean_domain_size + 0.5; // wraps: the FFT ocean is periodic
	float fade = texture(ocean_fade_tex, cuv_r).r;
	// Negative: the displacement points away from crests in this FFT's sign
	// convention, so moving against it gathers points into the crests.
	vec2 d = -choppiness * fade * texture(ocean_disp_tex, cuv_o).rg;
	// The ripple layer isn't periodic: past its domain, ease back to still water.
	vec2 from_center = abs(rel / ripple_domain_size);
	float inside = 1.0 - smoothstep(0.46, 0.5, max(from_center.x, from_center.y));
	float ripple = mix(water_level, texture(ripple_height_tex, cuv_r).r, inside);
	float h = ripple + texture(ocean_height_tex, cuv_o).r * fade;
	// The wake grid is laid out where the water is now, not where it rests
	// (its sources sit under the hull, and sample_height() reads it there):
	// read it at this point's displaced position.
	h += wake_height(world_xz + d);
	// Never below the ground: over dry sand (and in the deepest troughs over
	// shallows) the surface lies on the sand as a thin film, which is where
	// the swash shows.
	float ground = water_level - texture(shore_depth_tex, cuv_r).r;
	h = max(h, ground + swash_film_thickness);
	return vec3(d.x, h, d.y);
}

// Exact normal of the displaced surface at the point resting at world_xz:
// tangents of P(x0) = x0 + s D(x0) + h(x0) from the FFT's own derivatives. The ripple layer has no FFT, so its slope
// is a small central difference.
vec3 surface_normal_pixel(vec2 world_xz) {
	vec2 rel = world_xz - grid_center;
	vec2 cuv_r = clamp(rel / ripple_domain_size + 0.5, vec2(0.0), vec2(1.0));
	vec2 cuv_o = rel / ocean_domain_size + 0.5; // wraps: the FFT ocean is periodic
	float fade = texture(ocean_fade_tex, cuv_r).r;
	float s = -choppiness * fade;
	vec4 der = texture(ocean_deriv_tex, cuv_o);
	float dxz = texture(ocean_disp_tex, cuv_o).b;
	const float e = 0.2;
	vec2 du = vec2(e) / ripple_domain_size;
	float rx = (texture(ripple_height_tex, cuv_r + vec2(du.x, 0.0)).r - texture(ripple_height_tex, cuv_r - vec2(du.x, 0.0)).r) / (2.0 * e);
	float rz = (texture(ripple_height_tex, cuv_r + vec2(0.0, du.y)).r - texture(ripple_height_tex, cuv_r - vec2(0.0, du.y)).r) / (2.0 * e);
	// The ripple layer steps at the waterline (dry cells report their wet
	// neighbours' mean), which the sun picked out as bright streaks along the
	// shore; over the sand the swash film just follows the ground.
	float wet_w = smoothstep(0.0, 0.15, texture(shore_depth_tex, cuv_r).r);
	rx *= wet_w;
	rz *= wet_w;
	// Wake slopes (zero over the shore grid), at the displaced position as
	// surface_offset() reads the wake.
	const float ew = 0.12;
	vec2 wake_xz = world_xz + s * texture(ocean_disp_tex, cuv_o).rg;
	rx += (wake_height(wake_xz + vec2(ew, 0.0)) - wake_height(wake_xz - vec2(ew, 0.0))) / (2.0 * ew);
	rz += (wake_height(wake_xz + vec2(0.0, ew)) - wake_height(wake_xz - vec2(0.0, ew))) / (2.0 * ew);
	vec3 tx = vec3(1.0 + s * der.z, der.x * fade + rx, s * dxz);
	vec3 tz = vec3(s * dxz, der.y * fade + rz, 1.0 + s * der.w);
	return normalize(cross(tz, tx));
}

float foam_hash(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float foam_value_noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(foam_hash(i), foam_hash(i + vec2(1.0, 0.0)), u.x),
			mix(foam_hash(i + vec2(0.0, 1.0)), foam_hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

// Visible foam for the water resting at world_xz: the persistent foam amount,
// eroded by fixed-to-the-water noise so it breaks into lace as it fades.
float foam_at(vec2 world_xz) {
	vec2 rel = world_xz - grid_center;
	vec2 cuv_r = clamp(rel / ripple_domain_size + 0.5, vec2(0.0), vec2(1.0));
	vec2 cuv_o = rel / ocean_domain_size + 0.5; // wraps: the FFT ocean is periodic
	float fade = texture(ocean_fade_tex, cuv_r).r;
	float amount = texture(ocean_foam_tex, cuv_o).r * fade;
	// Wake foam at the displaced position, as surface_offset() reads the wake.
	amount = max(amount, wake_foam(world_xz - choppiness * fade * texture(ocean_disp_tex, cuv_o).rg));
	vec2 p = world_xz * foam_detail_scale;
	float n = 0.5 * foam_value_noise(p) + 0.3 * foam_value_noise(p * 2.7 + 17.0) + 0.2 * foam_value_noise(p * 7.3 - 5.0);
	// Gentle ramp: fresh foam is solid, thinning foam turns translucent and
	// lacy as the noise eats into it, rather than cutting off at an edge.
	float f = clamp(amount * (1.0 + foam_breakup) - n * foam_breakup, 0.0, 1.0);
	return f * f * (3.0 - 2.0 * f);
}

// Visible shore foam at world_xz: finer, heavier lace at partial strength.
)"
							   R"(
// Ripple-grid UV for the shore's moving patterns (swash, wet sand, shore
// foam), mirror-repeated past the simulated square: they carry on down the
// beach without a seam instead of stretching the edge row into straight
// lines. (The still-water depth itself stays clamped, so land and sea are
// never mirrored into each other.)
vec2 shore_pattern_uv(vec2 world_xz) {
	vec2 u = (world_xz - grid_center) / ripple_domain_size + 0.5;
	return 1.0 - abs(mod(u, 2.0) - 1.0);
}

float shore_foam_at(vec2 world_xz) {
	vec2 cuv_r = clamp((world_xz - grid_center) / ripple_domain_size + 0.5, vec2(0.0), vec2(1.0));
	vec2 q = world_xz * foam_detail_scale * 2.2 + 31.0;
	float ns = 0.5 * foam_value_noise(q) + 0.5 * foam_value_noise(q * 2.3 - 11.0);
	float shore = texture(shore_foam_tex, shore_pattern_uv(world_xz)).r;
	float g = clamp(shore * (1.0 + shore_foam_breakup) - ns * shore_foam_breakup, 0.0, 1.0);
	return g * g * (3.0 - 2.0 * g) * shore_foam_strength;
}

// Water depth (m) above the moving waterline: the still-water depth minus an
// offset in [0, swash_reach] -- small under an arriving crest (the edge
// reaches in), large under a through, jittered by slow noise. <= 0 is past
// the visible edge.
float shore_signal(vec2 world_xz, float time) {
	vec2 rel = world_xz - grid_center;
	vec2 cuv_r = clamp(rel / ripple_domain_size + 0.5, vec2(0.0), vec2(1.0));
	vec2 cuv_o = rel / ocean_domain_size + 0.5; // wraps: the FFT ocean is periodic
	float depth = texture(shore_depth_tex, cuv_r).r;
	float eta = texture(ocean_height_tex, cuv_o).r; // unfaded: the waves arriving here
	// Lobes a few meters across (like real swash), drifting, with the waves
	// pushing the whole edge in and out over time.
	vec2 p = world_xz * 0.45;
	float n = 0.65 * foam_value_noise(p + vec2(time * 0.12, time * 0.05)) + 0.35 * foam_value_noise(p * 2.4 - vec2(time * 0.07, 0.0));
	float wave = clamp(eta / max(swash_wave_height, 1e-3), -1.0, 1.0);
	float offset = swash_reach * clamp(0.5 + (n - 0.5) * 1.6 - 0.35 * wave, 0.0, 1.0);
	vec2 q = world_xz * shore_detail_scale;
	float eb = 0.5 * foam_value_noise(q * 2.3 + vec2(time * 0.25, 0.0)) + 0.3 * foam_value_noise(q * 6.1 + vec2(3.0, time * 0.4)) + 0.2 * foam_value_noise(q * 15.0 - 7.0);
	float run_up = texture(swash_tex, shore_pattern_uv(world_xz)).r;
	float jitter = (eb - 0.5) * 2.0 * swash_edge_breakup;
	// On dry sand the jitter may only pull the edge back, never add water
	// past what the swash covers -- or it left a permanent fringe of rim
	// foam just above the still-water line.
	jitter = depth > 0.0 ? jitter : min(jitter, run_up);
	// The pull-back gives way where a swash is running up, so over the sand
	// the film reaches exactly as far as the run-up (and the wet sand); with
	// no swash it applies right up to the still-water line, or a sliver of
	// water was left showing along it.
	return depth - offset * (1.0 - smoothstep(0.0, 0.08, run_up)) + run_up + jitter;
}

void vertex() {
	VERTEX += (inverse(MODEL_MATRIX) * vec4(mesh_offset.x, 0.0, mesh_offset.y, 0.0)).xyz;
	vec3 world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_rest_xz = world_pos.xz;
	VERTEX += surface_offset(world_pos.xz);
	v_world_normal = vec3(0.0, 1.0, 0.0);
	v_foam = 0.0;
	// Per pixel, fragment() computes the normal and foam instead.
	if (!per_pixel_normals) {
		// Per-vertex: normal from central differences of the displaced
		// surface -- e is in world meters, small against the grids' cell
		// size.
		const float e = 0.2;
		vec3 px1 = vec3(e, 0.0, 0.0) + surface_offset(world_pos.xz + vec2(e, 0.0));
		vec3 px0 = vec3(-e, 0.0, 0.0) + surface_offset(world_pos.xz - vec2(e, 0.0));
		vec3 pz1 = vec3(0.0, 0.0, e) + surface_offset(world_pos.xz + vec2(0.0, e));
		vec3 pz0 = vec3(0.0, 0.0, -e) + surface_offset(world_pos.xz - vec2(0.0, e));
		vec3 tx = px1 - px0;
		vec3 tz = pz1 - pz0;
		v_world_normal = normalize(cross(tz, tx));
		v_foam = foam_at(world_pos.xz);
	}
}

void fragment() {
	// Draw only the side of the surface facing the camera's side of the
	// water: the top from above, the underside from below. The surface is
	// transparent and double-sided, and its triangles blend in mesh order,
	// not by distance -- so where it overlaps itself (the back of a crest
	// behind its front, far waves seen through near ones) the far side
	// painted over the near one as jagged translucent teeth, and the
	// pattern changed wherever the mesh order did, drawing a seam.
	bool camera_above = CAMERA_POSITION_WORLD.y > MODEL_MATRIX[3].y + water_level;
	if (FRONT_FACING != camera_above) {
		discard;
	}
	vec3 world_normal = v_world_normal;
	float foam = v_foam;
	if (per_pixel_normals) {
		world_normal = surface_normal_pixel(v_rest_xz);
		foam = foam_at(v_rest_xz);
	}
	// unshaded + a small self-contained, bounded normal-based shading term --
	// NOT Godot's real diffuse/specular PBR response. Real root cause of the
	// "whitewash": this material used render_mode diffuse_burley,
	// specular_schlick_ggx, so it got its own full sun+ambient PBR light
	// response computed as if fully opaque, and THAT result was then alpha-
	// blended on top of the floor's OWN independently-lit PBR response.
	// Alpha blending two independently, fully-lit PBR surfaces in linear HDR
	// sums MORE light than either surface alone before the single tonemap
	// pass sees it -- exceeding any reasonable reference white, regardless
	// of how dark either material's own albedo is on its own (confirmed:
	// the bare floor alone renders correctly; only the blended combination
	// blows out). Going unshaded removes the water from that PBR light
	// budget entirely -- only the floor is now really lit by the scene, and
	// the water contributes a fixed, self-bounded tint on top of it.
	vec3 n = normalize((VIEW_MATRIX * vec4(world_normal, 0.0)).xyz);
	// Fixed, hardcoded pseudo-light direction -- not a real scene light, just
	// a bounded shaping term so wave normals still visibly modulate the
	// water's own tint (the whole reason per-vertex normals were added this
	// session in the first place), capped to a small multiplier so it can
	// never itself become a source of overexposure.
	float ndotl = clamp(dot(n, vec3(0.3, 0.85, 0.4)), 0.0, 1.0);
	float shore = shore_signal(v_rest_xz, water_time);
	// Shore foam belongs to the moving edge: full at it, thinning over the
	// next ~0.6 m of depth, so the white sheet scallops with the edge instead
	// of filling the band. (Whitecaps are left alone.)
	foam = max(foam, shore_foam_at(v_rest_xz) * (1.0 - smoothstep(0.02, 0.6, shore)));
	// Bright rim right behind the moving edge, broken into lace.
	// Over dry sand the swash film is only as thick as the run-up's excess
	// over the sand -- a few cm -- so it fades in over a fifth of the
	// softness there, or the uprush would barely show.
	vec2 cuv_here = clamp((v_rest_xz - grid_center) / ripple_domain_size + 0.5, vec2(0.0), vec2(1.0));
	float soft = max(shore_edge_softness, 1e-3) * mix(0.2, 1.0, smoothstep(-0.05, 0.1, texture(shore_depth_tex, cuv_here).r));
	float rim = smoothstep(0.0, soft * 0.5, shore) * (1.0 - smoothstep(soft * 0.6, soft * 2.2, shore));
	rim *= shore_rim_strength * (0.55 + 0.45 * foam_value_noise(v_rest_xz * 6.0 * shore_detail_scale));
	foam = max(foam, rim);
	vec2 cuv_fade = clamp((v_rest_xz - grid_center) / ripple_domain_size + 0.5, vec2(0.0), vec2(1.0));
	float shallow = (1.0 - texture(ocean_fade_tex, cuv_fade).r) * shallow_tint;
	vec3 base_rgb = mix(water_color.rgb, shallow_color.rgb, shallow) * mix(0.75, 1.15, ndotl);
	float base_a = mix(water_color.a, shallow_color.a, shallow);
	// Thin water is clear: the swash film and the last few cm at the edge let
	// the sand show through, turning opaque by ~25 cm of depth.
	base_a *= mix(0.4, 1.0, smoothstep(0.0, 0.25, shore));
	ALBEDO = mix(base_rgb, foam_color.rgb, foam);
	float water_a = mix(base_a, foam_color.a, foam) * smoothstep(0.0, soft, shore);
	// Wet sand under and behind the swash: blended in where the water is
	// thin or gone, so it shows through the film and lingers after it.
	// Noise eats into the wet edge, so it dries back ragged rather than
	// along a clean line.
	vec2 wp = v_rest_xz * shore_detail_scale;
	float wn = 0.6 * foam_value_noise(wp * 1.7) + 0.4 * foam_value_noise(wp * 5.3 + 7.0);
	float wet = clamp((texture(swash_tex, shore_pattern_uv(v_rest_xz)).g - 0.35 * wn) / 0.65, 0.0, 1.0);
	// Only where sand is near the surface: under deeper water the "wet" flag
	// is meaningless, and blending it there tinted and roughened all deep
	// water -- which showed as a seam past the simulated square, where the
	// mirrored patterns bring dry beach (not wet) out over the open sea.
	wet *= 1.0 - smoothstep(0.05, 0.35, texture(shore_depth_tex, cuv_fade).r);
	float wet_a = wet * wet * (3.0 - 2.0 * wet) * wet_sand_color.a * (1.0 - water_a);
	ALPHA = water_a + wet_a;
	ALBEDO = (ALBEDO * water_a + wet_sand_color.rgb * wet_a) / max(ALPHA, 1e-4);
}
)";

// Sign of the X/Z cross product of an up-facing PlaneMesh triangle -- measured
// from PlaneMesh itself rather than assumed from a winding convention.
real_t plane_mesh_xz_winding() {
	Ref<PlaneMesh> ref_plane;
	ref_plane.instantiate();
	const Vector<Face3> faces = ref_plane->get_faces();
	if (faces.is_empty()) {
		return 1.0;
	}
	const Face3 &f = faces[0];
	const Vector2 tri_a(f.vertex[0].x, f.vertex[0].z), tri_b(f.vertex[1].x, f.vertex[1].z), tri_c(f.vertex[2].x, f.vertex[2].z);
	return (tri_b - tri_a).cross(tri_c - tri_a) < 0 ? -1.0 : 1.0;
}

// Inside or on the edge of triangle abc, either winding.
bool point_in_triangle(const Vector2 &p_p, const Vector2 &p_a, const Vector2 &p_b, const Vector2 &p_c) {
	const real_t d1 = (p_b - p_a).cross(p_p - p_a);
	const real_t d2 = (p_c - p_b).cross(p_p - p_b);
	const real_t d3 = (p_a - p_c).cross(p_p - p_c);
	const bool has_neg = d1 < 0 || d2 < 0 || d3 < 0;
	const bool has_pos = d1 > 0 || d2 > 0 || d3 > 0;
	return !(has_neg && has_pos);
}

// Liang-Barsky: does segment p0-p1 touch the box [p_min, p_max]?
bool segment_touches_box(const Vector2 &p_p0, const Vector2 &p_p1, const Vector2 &p_min, const Vector2 &p_max) {
	real_t t0 = 0.0, t1 = 1.0;
	const Vector2 segment_delta = p_p1 - p_p0;
	for (int axis = 0; axis < 2; axis++) {
		const real_t q0 = p_min[axis] - p_p0[axis];
		const real_t q1 = p_max[axis] - p_p0[axis];
		if (Math::abs(segment_delta[axis]) < (real_t)1e-12) {
			if (q0 > 0 || q1 < 0) {
				return false;
			}
			continue;
		}
		real_t ta = q0 / segment_delta[axis];
		real_t tb = q1 / segment_delta[axis];
		if (ta > tb) {
			SWAP(ta, tb);
		}
		t0 = MAX(t0, ta);
		t1 = MIN(t1, tb);
		if (t0 > t1) {
			return false;
		}
	}
	return true;
}

// Sutherland-Hodgman: clip a convex polygon to a counter-clockwise triangle.
void clip_to_triangle(LocalVector<Vector2> &r_poly, const Vector2 p_tri[3]) {
	LocalVector<Vector2> out;
	for (int e = 0; e < 3 && r_poly.size() >= 3; e++) {
		const Vector2 clip_vertex = p_tri[e];
		const Vector2 edge = p_tri[(e + 1) % 3] - clip_vertex;
		out.clear();
		for (uint32_t i = 0; i < r_poly.size(); i++) {
			const Vector2 cur = r_poly[i];
			const Vector2 prev = r_poly[(i + r_poly.size() - 1) % r_poly.size()];
			const real_t side_cur = edge.cross(cur - clip_vertex);
			const real_t side_prev = edge.cross(prev - clip_vertex);
			if (side_cur >= 0) {
				if (side_prev < 0) {
					out.push_back(prev + (cur - prev) * (side_prev / (side_prev - side_cur)));
				}
				out.push_back(cur);
			} else if (side_prev >= 0) {
				out.push_back(prev + (cur - prev) * (side_prev / (side_prev - side_cur)));
			}
		}
		r_poly = out;
	}
}

} // namespace

void PhysXWaterSurface3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_domain_size", "size"), &PhysXWaterSurface3D::set_domain_size);
	ClassDB::bind_method(D_METHOD("get_domain_size"), &PhysXWaterSurface3D::get_domain_size);
	ClassDB::bind_method(D_METHOD("set_grid_resolution", "n"), &PhysXWaterSurface3D::set_grid_resolution);
	ClassDB::bind_method(D_METHOD("get_grid_resolution"), &PhysXWaterSurface3D::get_grid_resolution);
	ClassDB::bind_method(D_METHOD("set_depth", "depth"), &PhysXWaterSurface3D::set_depth);
	ClassDB::bind_method(D_METHOD("get_depth"), &PhysXWaterSurface3D::get_depth);
	ClassDB::bind_method(D_METHOD("set_water_level", "level"), &PhysXWaterSurface3D::set_water_level);
	ClassDB::bind_method(D_METHOD("get_water_level"), &PhysXWaterSurface3D::get_water_level);
	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &PhysXWaterSurface3D::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &PhysXWaterSurface3D::get_damping);
	ClassDB::bind_method(D_METHOD("set_ripple_amplitude", "amplitude"), &PhysXWaterSurface3D::set_ripple_amplitude);
	ClassDB::bind_method(D_METHOD("get_ripple_amplitude"), &PhysXWaterSurface3D::get_ripple_amplitude);

	ClassDB::bind_method(D_METHOD("set_ocean_grid_resolution", "n"), &PhysXWaterSurface3D::set_ocean_grid_resolution);
	ClassDB::bind_method(D_METHOD("get_ocean_grid_resolution"), &PhysXWaterSurface3D::get_ocean_grid_resolution);
	ClassDB::bind_method(D_METHOD("set_ocean_domain_size", "size"), &PhysXWaterSurface3D::set_ocean_domain_size);
	ClassDB::bind_method(D_METHOD("get_ocean_domain_size"), &PhysXWaterSurface3D::get_ocean_domain_size);
	ClassDB::bind_method(D_METHOD("set_wind_speed", "speed"), &PhysXWaterSurface3D::set_wind_speed);
	ClassDB::bind_method(D_METHOD("get_wind_speed"), &PhysXWaterSurface3D::get_wind_speed);
	ClassDB::bind_method(D_METHOD("set_wind_direction", "dir"), &PhysXWaterSurface3D::set_wind_direction);
	ClassDB::bind_method(D_METHOD("get_wind_direction"), &PhysXWaterSurface3D::get_wind_direction);
	ClassDB::bind_method(D_METHOD("set_wave_amplitude", "amplitude"), &PhysXWaterSurface3D::set_wave_amplitude);
	ClassDB::bind_method(D_METHOD("get_wave_amplitude"), &PhysXWaterSurface3D::get_wave_amplitude);
	ClassDB::bind_method(D_METHOD("set_fetch", "fetch"), &PhysXWaterSurface3D::set_fetch);
	ClassDB::bind_method(D_METHOD("get_fetch"), &PhysXWaterSurface3D::get_fetch);
	ClassDB::bind_method(D_METHOD("get_effective_fetch"), &PhysXWaterSurface3D::get_effective_fetch);
	ClassDB::bind_method(D_METHOD("recenter", "world_position"), &PhysXWaterSurface3D::recenter);
	ClassDB::bind_method(D_METHOD("set_sea_state", "wind_speed", "wind_direction", "wave_amplitude", "fetch"), &PhysXWaterSurface3D::set_sea_state);
	ClassDB::bind_method(D_METHOD("is_solver_available"), &PhysXWaterSurface3D::is_solver_available);
	ClassDB::bind_method(D_METHOD("get_grid_center"), &PhysXWaterSurface3D::get_grid_center);
	ClassDB::bind_method(D_METHOD("set_choppiness", "choppiness"), &PhysXWaterSurface3D::set_choppiness);
	ClassDB::bind_method(D_METHOD("get_choppiness"), &PhysXWaterSurface3D::get_choppiness);
	ClassDB::bind_method(D_METHOD("set_normal_mode", "mode"), &PhysXWaterSurface3D::set_normal_mode);
	ClassDB::bind_method(D_METHOD("get_normal_mode"), &PhysXWaterSurface3D::get_normal_mode);
	ClassDB::bind_method(D_METHOD("set_foam_enabled", "enabled"), &PhysXWaterSurface3D::set_foam_enabled);
	ClassDB::bind_method(D_METHOD("get_foam_enabled"), &PhysXWaterSurface3D::get_foam_enabled);
	ClassDB::bind_method(D_METHOD("set_foam_threshold", "threshold"), &PhysXWaterSurface3D::set_foam_threshold);
	ClassDB::bind_method(D_METHOD("get_foam_threshold"), &PhysXWaterSurface3D::get_foam_threshold);
	ClassDB::bind_method(D_METHOD("set_foam_persistence", "seconds"), &PhysXWaterSurface3D::set_foam_persistence);
	ClassDB::bind_method(D_METHOD("get_foam_persistence"), &PhysXWaterSurface3D::get_foam_persistence);
	ClassDB::bind_method(D_METHOD("set_shore_foam_band", "depth"), &PhysXWaterSurface3D::set_shore_foam_band);
	ClassDB::bind_method(D_METHOD("get_shore_foam_band"), &PhysXWaterSurface3D::get_shore_foam_band);
	ClassDB::bind_method(D_METHOD("set_shore_undertow", "speed"), &PhysXWaterSurface3D::set_shore_undertow);
	ClassDB::bind_method(D_METHOD("get_shore_undertow"), &PhysXWaterSurface3D::get_shore_undertow);
	ClassDB::bind_method(D_METHOD("set_swash_run_up", "height"), &PhysXWaterSurface3D::set_swash_run_up);
	ClassDB::bind_method(D_METHOD("get_swash_run_up"), &PhysXWaterSurface3D::get_swash_run_up);
	ClassDB::bind_method(D_METHOD("set_swash_drain_speed", "speed"), &PhysXWaterSurface3D::set_swash_drain_speed);
	ClassDB::bind_method(D_METHOD("get_swash_drain_speed"), &PhysXWaterSurface3D::get_swash_drain_speed);
	ClassDB::bind_method(D_METHOD("set_wet_sand_dry_time", "seconds"), &PhysXWaterSurface3D::set_wet_sand_dry_time);
	ClassDB::bind_method(D_METHOD("get_wet_sand_dry_time"), &PhysXWaterSurface3D::get_wet_sand_dry_time);

	ClassDB::bind_method(D_METHOD("set_surface_mesh", "mesh"), &PhysXWaterSurface3D::set_surface_mesh);
	ClassDB::bind_method(D_METHOD("get_surface_mesh"), &PhysXWaterSurface3D::get_surface_mesh);
	ClassDB::bind_method(D_METHOD("set_seabed_from_floor", "enabled"), &PhysXWaterSurface3D::set_seabed_from_floor);
	ClassDB::bind_method(D_METHOD("get_seabed_from_floor"), &PhysXWaterSurface3D::get_seabed_from_floor);
	ClassDB::bind_method(D_METHOD("set_seabed_collision_mask", "mask"), &PhysXWaterSurface3D::set_seabed_collision_mask);
	ClassDB::bind_method(D_METHOD("get_seabed_collision_mask"), &PhysXWaterSurface3D::get_seabed_collision_mask);
	ClassDB::bind_method(D_METHOD("set_shallow_fade_depth", "depth"), &PhysXWaterSurface3D::set_shallow_fade_depth);
	ClassDB::bind_method(D_METHOD("get_shallow_fade_depth"), &PhysXWaterSurface3D::get_shallow_fade_depth);
	ClassDB::bind_method(D_METHOD("set_render_extent", "extent"), &PhysXWaterSurface3D::set_render_extent);
	ClassDB::bind_method(D_METHOD("get_render_extent"), &PhysXWaterSurface3D::get_render_extent);
	ClassDB::bind_method(D_METHOD("set_water_material", "material"), &PhysXWaterSurface3D::set_water_material);
	ClassDB::bind_method(D_METHOD("get_water_material"), &PhysXWaterSurface3D::get_water_material);

	ClassDB::bind_method(D_METHOD("set_caustics_enabled", "enabled"), &PhysXWaterSurface3D::set_caustics_enabled);
	ClassDB::bind_method(D_METHOD("get_caustics_enabled"), &PhysXWaterSurface3D::get_caustics_enabled);
	ClassDB::bind_method(D_METHOD("set_caustics_sun_direction", "direction"), &PhysXWaterSurface3D::set_caustics_sun_direction);
	ClassDB::bind_method(D_METHOD("get_caustics_sun_direction"), &PhysXWaterSurface3D::get_caustics_sun_direction);
	ClassDB::bind_method(D_METHOD("set_caustics_reference_depth", "depth"), &PhysXWaterSurface3D::set_caustics_reference_depth);
	ClassDB::bind_method(D_METHOD("get_caustics_reference_depth"), &PhysXWaterSurface3D::get_caustics_reference_depth);
	ClassDB::bind_method(D_METHOD("get_caustics_texture"), &PhysXWaterSurface3D::get_caustics_texture);
	ClassDB::bind_method(D_METHOD("get_caustics_light_right"), &PhysXWaterSurface3D::get_caustics_light_right);
	ClassDB::bind_method(D_METHOD("get_caustics_light_up"), &PhysXWaterSurface3D::get_caustics_light_up);
	ClassDB::bind_method(D_METHOD("get_caustics_origin"), &PhysXWaterSurface3D::get_caustics_origin);
	ClassDB::bind_method(D_METHOD("get_caustics_half_extent"), &PhysXWaterSurface3D::get_caustics_half_extent);
	ClassDB::bind_method(D_METHOD("get_caustics_tile_size"), &PhysXWaterSurface3D::get_caustics_tile_size);

	ClassDB::bind_method(D_METHOD("sample_height", "world_pos"), &PhysXWaterSurface3D::sample_height);
	ClassDB::bind_method(D_METHOD("is_wet", "world_pos"), &PhysXWaterSurface3D::is_wet);
	ClassDB::bind_method(D_METHOD("submit_sphere", "owner", "world_pos", "radius", "strength"), &PhysXWaterSurface3D::submit_sphere, DEFVAL(1.0f));
	ClassDB::bind_method(D_METHOD("clear_sphere", "owner"), &PhysXWaterSurface3D::clear_sphere);
	ClassDB::bind_method(D_METHOD("submit_impulse", "world_pos", "radius", "strength"), &PhysXWaterSurface3D::submit_impulse);

	BIND_ENUM_CONSTANT(NORMAL_MODE_PER_PIXEL);
	BIND_ENUM_CONSTANT(NORMAL_MODE_PER_VERTEX);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "surface_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_surface_mesh", "get_surface_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "domain_size"), "set_domain_size", "get_domain_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "grid_resolution"), "set_grid_resolution", "get_grid_resolution");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "depth"), "set_depth", "get_depth");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "water_level"), "set_water_level", "get_water_level");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping"), "set_damping", "get_damping");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ripple_amplitude", PROPERTY_HINT_RANGE, "0,2,0.01,or_greater"), "set_ripple_amplitude", "get_ripple_amplitude");

	ADD_GROUP("Seabed", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "seabed_from_floor"), "set_seabed_from_floor", "get_seabed_from_floor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "seabed_collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_seabed_collision_mask", "get_seabed_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shallow_fade_depth", PROPERTY_HINT_RANGE, "0.01,10,0.01,or_greater,suffix:m"), "set_shallow_fade_depth", "get_shallow_fade_depth");

	ADD_GROUP("Ocean", "ocean_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ocean_grid_resolution"), "set_ocean_grid_resolution", "get_ocean_grid_resolution");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "ocean_domain_size"), "set_ocean_domain_size", "get_ocean_domain_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wind_speed"), "set_wind_speed", "get_wind_speed");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "wind_direction"), "set_wind_direction", "get_wind_direction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wave_amplitude"), "set_wave_amplitude", "get_wave_amplitude");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fetch", PROPERTY_HINT_RANGE, "0,100000,1,or_greater,suffix:m"), "set_fetch", "get_fetch");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "choppiness", PROPERTY_HINT_RANGE, "0,3,0.01,or_greater"), "set_choppiness", "get_choppiness");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "foam_enabled"), "set_foam_enabled", "get_foam_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "foam_threshold", PROPERTY_HINT_RANGE, "0,1.5,0.01"), "set_foam_threshold", "get_foam_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shore_foam_band", PROPERTY_HINT_RANGE, "0,5,0.05,or_greater,suffix:m"), "set_shore_foam_band", "get_shore_foam_band");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shore_undertow", PROPERTY_HINT_RANGE, "0,1.5,0.01,or_greater,suffix:m/s"), "set_shore_undertow", "get_shore_undertow");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "foam_persistence", PROPERTY_HINT_RANGE, "0.05,20,0.05,or_greater,suffix:s"), "set_foam_persistence", "get_foam_persistence");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "swash_run_up", PROPERTY_HINT_RANGE, "0,1,0.01,or_greater,suffix:m"), "set_swash_run_up", "get_swash_run_up");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "swash_drain_speed", PROPERTY_HINT_RANGE, "0.01,1,0.01,or_greater,suffix:m/s"), "set_swash_drain_speed", "get_swash_drain_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wet_sand_dry_time", PROPERTY_HINT_RANGE, "0.1,60,0.1,or_greater,suffix:s"), "set_wet_sand_dry_time", "get_wet_sand_dry_time");

	ADD_GROUP("Rendering", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "render_extent", PROPERTY_HINT_RANGE, "0,2000,1,or_greater,suffix:m"), "set_render_extent", "get_render_extent");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "normal_mode", PROPERTY_HINT_ENUM, "Per Pixel,Per Vertex"), "set_normal_mode", "get_normal_mode");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "water_material", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial"), "set_water_material", "get_water_material");

	ADD_GROUP("Caustics", "caustic");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "caustics_enabled"), "set_caustics_enabled", "get_caustics_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "caustics_sun_direction"), "set_caustics_sun_direction", "get_caustics_sun_direction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "caustics_reference_depth", PROPERTY_HINT_RANGE, "0.1,50,0.1,or_greater"), "set_caustics_reference_depth", "get_caustics_reference_depth");
}

void PhysXWaterSurface3D::set_domain_size(Vector2 p_size) {
	domain_size = p_size;
	_rebuild();
}

void PhysXWaterSurface3D::set_grid_resolution(int p_n) {
	grid_resolution = MAX(p_n, 4);
	_rebuild();
}

void PhysXWaterSurface3D::set_depth(float p_depth) {
	depth = MAX(p_depth, 0.1f);
	// WATER-3: depth/damping/water_level/ripple_amplitude are rt_step
	// ARGUMENTS (uploaded every tick) -- a solver-front settings write makes
	// them live; _rebuild() here tore down and recreated the entire GPU
	// solver (13 textures + pipelines) per setter call.
	solver.set_step_constants(depth, damping, water_level, ripple_amplitude);
}

void PhysXWaterSurface3D::set_water_level(float p_level) {
	water_level = p_level;
	solver.set_step_constants(depth, damping, water_level, ripple_amplitude);
}

void PhysXWaterSurface3D::set_damping(float p_damping) {
	damping = p_damping;
	solver.set_step_constants(depth, damping, water_level, ripple_amplitude);
}

void PhysXWaterSurface3D::set_ripple_amplitude(float p_amplitude) {
	ripple_amplitude = MAX(p_amplitude, 0.0f);
	solver.set_step_constants(depth, damping, water_level, ripple_amplitude);
}

void PhysXWaterSurface3D::set_ocean_grid_resolution(int p_n) {
	// Must stay a power of two -- the Stockham FFT passes require it (see
	// water_solver.h). Round up rather than silently misbehave.
	int clamped_resolution = MAX(p_n, 4);
	int pow2 = 4;
	while (pow2 < clamped_resolution) {
		pow2 <<= 1;
	}
	ocean_grid_resolution = pow2;
	_rebuild();
}

void PhysXWaterSurface3D::set_ocean_domain_size(Vector2 p_size) {
	ocean_domain_size = p_size;
	_rebuild();
}

void PhysXWaterSurface3D::set_wind_speed(float p_speed) {
	wind_speed = MAX(p_speed, 0.01f);
	_reinit_spectrum();
}

void PhysXWaterSurface3D::set_wind_direction(Vector2 p_dir) {
	wind_direction = p_dir;
	_reinit_spectrum();
}

void PhysXWaterSurface3D::set_wave_amplitude(float p_amp) {
	wave_amplitude = MAX(p_amp, 0.0f);
	_reinit_spectrum();
}

void PhysXWaterSurface3D::set_choppiness(float p_choppiness) {
	choppiness = MAX(p_choppiness, 0.0f);
	if (water_material.is_valid()) {
		water_material->set_shader_parameter("choppiness", choppiness);
	}
	solver.set_foam_settings(foam_enabled, choppiness, foam_threshold, foam_persistence, shore_foam_band, shore_undertow);
}

void PhysXWaterSurface3D::set_foam_enabled(bool p_enabled) {
	foam_enabled = p_enabled;
	solver.set_foam_settings(foam_enabled, choppiness, foam_threshold, foam_persistence, shore_foam_band, shore_undertow);
}

void PhysXWaterSurface3D::set_shore_undertow(float p_speed) {
	shore_undertow = MAX(p_speed, 0.0f);
	solver.set_foam_settings(foam_enabled, choppiness, foam_threshold, foam_persistence, shore_foam_band, shore_undertow);
}

void PhysXWaterSurface3D::set_swash_run_up(float p_height) {
	swash_run_up = MAX(p_height, 0.0f);
	solver.set_swash_settings(swash_run_up, swash_drain_speed, wet_sand_dry_time);
}

void PhysXWaterSurface3D::set_swash_drain_speed(float p_speed) {
	swash_drain_speed = MAX(p_speed, 0.01f);
	solver.set_swash_settings(swash_run_up, swash_drain_speed, wet_sand_dry_time);
}

void PhysXWaterSurface3D::set_wet_sand_dry_time(float p_seconds) {
	wet_sand_dry_time = MAX(p_seconds, 0.1f);
	solver.set_swash_settings(swash_run_up, swash_drain_speed, wet_sand_dry_time);
}

void PhysXWaterSurface3D::set_shore_foam_band(float p_depth) {
	shore_foam_band = MAX(p_depth, 0.0f);
	solver.set_foam_settings(foam_enabled, choppiness, foam_threshold, foam_persistence, shore_foam_band, shore_undertow);
}

void PhysXWaterSurface3D::set_foam_threshold(float p_threshold) {
	foam_threshold = p_threshold;
	solver.set_foam_settings(foam_enabled, choppiness, foam_threshold, foam_persistence, shore_foam_band, shore_undertow);
}

void PhysXWaterSurface3D::set_foam_persistence(float p_seconds) {
	foam_persistence = MAX(p_seconds, 0.05f);
	solver.set_foam_settings(foam_enabled, choppiness, foam_threshold, foam_persistence, shore_foam_band, shore_undertow);
}

void PhysXWaterSurface3D::set_normal_mode(NormalMode p_mode) {
	normal_mode = p_mode;
	if (water_material.is_valid()) {
		water_material->set_shader_parameter("per_pixel_normals", normal_mode == NORMAL_MODE_PER_PIXEL);
	}
}

void PhysXWaterSurface3D::set_fetch(float p_fetch) {
	fetch = MAX(p_fetch, 0.0f);
	_reinit_spectrum();
}

void PhysXWaterSurface3D::recenter(Vector3 p_world_position) {
	Vector3 p = get_global_position();
	p.x = p_world_position.x;
	p.z = p_world_position.z;
	set_global_position(p);
	// A footprint / per-cell seabed is baked in node-local space, so it dies
	// with the old grid: rebuild. Same when there is no solver to move.
	if (!solver.is_available() || surface_mesh.is_valid() || !cell_depth.is_empty()) {
		_rebuild();
		return;
	}
	grid_center = Vector2(p.x, p.z);
	if (water_material.is_valid()) {
		water_material->set_shader_parameter("grid_center", grid_center);
	}
	solver.recenter(grid_center);
	// The old square's heights are not this one's.
	cached_ripple_height.clear();
}

void PhysXWaterSurface3D::set_sea_state(float p_wind_speed, Vector2 p_wind_direction, float p_wave_amplitude, float p_fetch) {
	wind_speed = MAX(p_wind_speed, 0.01f);
	wind_direction = p_wind_direction;
	wave_amplitude = MAX(p_wave_amplitude, 0.0f);
	fetch = MAX(p_fetch, 0.0f);
	_reinit_spectrum();
}

// The sea state changed: a built solver regenerates its spectrum IN PLACE
// (no rebuild -- the waves keep their phases); before a build the values
// are simply picked up by the next one.
void PhysXWaterSurface3D::_reinit_spectrum() {
	if (solver.is_available()) {
		solver.reinit_spectrum(wind_speed, wind_direction, wave_amplitude, get_effective_fetch(), depth);
	}
}

float PhysXWaterSurface3D::get_effective_fetch() const {
	if (fetch > 0.0f) {
		return fetch;
	}
	// Enclosed water (a surface_mesh outline) can't have more fetch than its
	// own size; an open square stands for a patch of open sea.
	return footprint_extent > 0.0 ? (float)footprint_extent : 100000.0f;
}

void PhysXWaterSurface3D::set_seabed_from_floor(bool p_enabled) {
	seabed_from_floor = p_enabled;
	_rebuild();
}

void PhysXWaterSurface3D::set_seabed_collision_mask(uint32_t p_mask) {
	seabed_collision_mask = p_mask;
	_rebuild();
}

void PhysXWaterSurface3D::set_shallow_fade_depth(float p_depth) {
	shallow_fade_depth = MAX(p_depth, 0.01f);
	_rebuild();
}

void PhysXWaterSurface3D::set_render_extent(float p_extent) {
	render_extent = MAX(p_extent, 0.0f);
	_rebuild();
}

void PhysXWaterSurface3D::set_surface_mesh(const Ref<Mesh> &p_mesh) {
	surface_mesh = p_mesh;
	_rebuild();
}

void PhysXWaterSurface3D::set_water_material(const Ref<ShaderMaterial> &p_material) {
	water_material = p_material;
	if (mesh_instance != nullptr && water_material.is_valid()) {
		mesh_instance->set_material_override(water_material);
	}
}

void PhysXWaterSurface3D::set_caustics_enabled(bool p_enabled) {
	caustics_enabled = p_enabled;
	_rebuild();
}

void PhysXWaterSurface3D::set_caustics_sun_direction(Vector3 p_direction) {
	caustics_sun_direction = p_direction;
}

void PhysXWaterSurface3D::set_caustics_reference_depth(float p_depth) {
	caustics_reference_depth = MAX(p_depth, 0.1f);
}

Ref<Texture2D> PhysXWaterSurface3D::get_caustics_texture() const {
	if (!caustics_enabled || !solver.is_available() || caustics_texture.is_null()) {
		return Ref<Texture2D>();
	}
	return caustics_texture;
}

void PhysXWaterSurface3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			_rebuild();
			set_physics_process_internal(true);
			set_process_internal(true);
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			set_physics_process_internal(false);
			set_process_internal(false);
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			_update(get_physics_process_delta_time());
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				_update_mesh_offset();
			}
		} break;
	}
}

void PhysXWaterSurface3D::_rebuild() {
	if (!is_inside_world()) {
		return;
	}
	if (mesh_instance == nullptr) {
		mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		add_child(mesh_instance, false, INTERNAL_MODE_BACK);
	}
	if (water_material.is_null()) {
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code(WATER_SHADER_SRC);
		Ref<ShaderMaterial> m;
		m.instantiate();
		m->set_shader(shader);
		water_material = m;
	}
	mesh_instance->set_material_override(water_material);

	// Shape: a surface_mesh footprint fits the ripple grid to its bounds and
	// masks everything outside it dry; without one, the plain square domain.
	Vector2 local_center;
	active_domain_size = domain_size;
	footprint_mask.clear();
	cell_depth.clear();
	footprint_extent = 0.0;
	water_mesh.unref();
	if (surface_mesh.is_valid()) {
		const Vector<Vector2> tris = _collect_footprint();
		if (tris.is_empty()) {
			WARN_PRINT("PhysXWaterSurface3D: surface_mesh has no triangles with any X/Z extent; using the square domain instead.");
		} else {
			Rect2 bounds(tris[0], Vector2());
			for (const Vector2 &v : tris) {
				bounds.expand_to(v);
			}
			local_center = bounds.get_center();
			// One dry cell of margin on each side, so the shoreline never sits
			// on the grid edge.
			const real_t side = MAX(bounds.size.x, bounds.size.y);
			footprint_extent = side;
			const real_t padded = side * (real_t)grid_resolution / (real_t)MAX(grid_resolution - 2, 1);
			active_domain_size = Vector2(padded, padded);
			footprint_mask = _rasterize_wet_mask(tris, local_center, active_domain_size, grid_resolution);
			water_mesh = _build_footprint_mesh(tris, bounds, MAX(grid_resolution, ocean_grid_resolution));
		}
	}
	if (water_mesh.is_null() && render_extent > MAX(ocean_domain_size.x, ocean_domain_size.y)) {
		water_mesh = _build_extended_plane(ocean_domain_size, ocean_grid_resolution, render_extent);
	}
	if (water_mesh.is_null()) {
		Ref<PlaneMesh> plane;
		plane.instantiate();
		plane->set_size(ocean_domain_size);
		// Subdivisions, not vertex count -- PlaneMesh's own convention.
		plane->set_subdivide_width(ocean_grid_resolution);
		plane->set_subdivide_depth(ocean_grid_resolution);
		water_mesh = plane;
	}
	mesh_instance->set_mesh(water_mesh);
	mesh_offset = Vector2(Math::INF, Math::INF); // re-applied for the new mesh on the next update

	const Vector3 origin = get_global_position();
	grid_center = Vector2(origin.x, origin.z) + local_center;

	water_material->set_shader_parameter("ripple_domain_size", active_domain_size);
	water_material->set_shader_parameter("ocean_domain_size", ocean_domain_size);
	water_material->set_shader_parameter("grid_center", grid_center);
	water_material->set_shader_parameter("water_level", water_level);
	water_material->set_shader_parameter("choppiness", choppiness);
	water_material->set_shader_parameter("per_pixel_normals", normal_mode == NORMAL_MODE_PER_PIXEL);

	// NOTIFICATION_INTERNAL_PHYSICS_PROCESS fires in the editor too, not just
	// Play -- same gotcha PhysXVehicle3D/PhysXDestructible3D already guard
	// against (see PhysXVehicle3D::_build()'s own comment on it). Without
	// this, just having the scene open dispatches real GPU compute work
	// every physics tick, which is exactly what a user reported seeing (the
	// water visibly animating while only hovering nodes in the editor,
	// followed by an editor crash likely from the render-thread dispatch
	// getting into an inconsistent state under the editor's non-realtime
	// update loop). No solver setup at all in the editor -- the node shows
	// a static flat mesh there instead, matching this class's own doc
	// comment ("only runs in an actual running game").
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (seabed_from_floor) {
		// Rays need the floor already in the physics space -- defer to the
		// first physics tick (see _update()).
		seabed_pending = true;
		return;
	}
	if (!footprint_mask.is_empty()) {
		const int cells = grid_resolution * grid_resolution;
		cell_depth.resize(cells);
		float *dw = cell_depth.ptrw();
		for (int i = 0; i < cells; i++) {
			dw[i] = footprint_mask[i] ? depth : WaterSolver::WALL_DEPTH;
		}
	}
	_configure_solver();
}

void PhysXWaterSurface3D::_sample_seabed() {
	// One ray per ripple cell center, straight down from well above the water
	// onto static bodies only: anything else it hits (a floater, a player) is
	// excluded and the ray cast again. The seabed can rise above the water --
	// that's dry beach, depth <= 0.
	constexpr real_t PROBE_ABOVE = 100.0;
	constexpr real_t PROBE_BELOW = 1000.0;
	constexpr int MAX_SKIPS = 8;
	const int grid_size = grid_resolution;
	cell_depth.resize(grid_size * grid_size);
	float *dw = cell_depth.ptrw();
	const real_t surface_y = get_global_position().y + water_level;
	const Vector2 cell = active_domain_size / (real_t)grid_size;
	const Vector2 grid_min = grid_center - active_domain_size * 0.5f;
	PhysicsDirectSpaceState3D *space = get_world_3d()->get_direct_space_state();
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	int hits = 0;
	for (int z = 0; z < grid_size; z++) {
		for (int x = 0; x < grid_size; x++) {
			const int i = z * grid_size + x;
			if (!footprint_mask.is_empty() && !footprint_mask[i]) {
				dw[i] = WaterSolver::WALL_DEPTH;
				continue;
			}
			const Vector2 sample_center = grid_min + Vector2(x + 0.5f, z + 0.5f) * cell;
			PhysicsDirectSpaceState3D::RayParameters ray;
			ray.from = Vector3(sample_center.x, surface_y + PROBE_ABOVE, sample_center.y);
			ray.to = Vector3(sample_center.x, surface_y - PROBE_BELOW, sample_center.y);
			ray.collision_mask = seabed_collision_mask;
			ray.collide_with_areas = false;
			float seabed_depth = depth; // nothing below: open water of the constant depth
			for (int skip = 0; skip < MAX_SKIPS; skip++) {
				PhysicsDirectSpaceState3D::RayResult result;
				if (space == nullptr || !space->intersect_ray(ray, result)) {
					break;
				}
				if (ps->body_get_mode(result.rid) == PhysicsServer3D::BODY_MODE_STATIC) {
					seabed_depth = surface_y - result.position.y;
					hits++;
					break;
				}
				ray.exclude.insert(result.rid);
			}
			dw[i] = seabed_depth;
		}
	}
	if (hits == 0) {
		WARN_PRINT("PhysXWaterSurface3D: seabed_from_floor found no static body under the water (check seabed_collision_mask); using the constant depth.");
	}
}

void PhysXWaterSurface3D::_update_cell_fade() {
	cell_fade.resize(cell_depth.size());
	float *fw = cell_fade.ptrw();
	for (int i = 0; i < cell_depth.size(); i++) {
		fw[i] = WaterSolver::shallow_fade(cell_depth[i], shallow_fade_depth);
	}
}

void PhysXWaterSurface3D::_configure_solver() {
	_update_cell_fade();
	WaterSolver::Settings solver_settings;
	solver_settings.grid_resolution = grid_resolution;
	solver_settings.domain_size = active_domain_size;
	solver_settings.grid_center = grid_center;
	solver_settings.cell_depth = cell_depth;
	solver_settings.shallow_fade_depth = shallow_fade_depth;
	solver_settings.depth = depth;
	solver_settings.damping = damping;
	solver_settings.ripple_amplitude = ripple_amplitude;
	solver_settings.gravity = 9.81f;
	solver_settings.water_level = water_level;
	solver_settings.ocean_grid_resolution = ocean_grid_resolution;
	solver_settings.ocean_domain_size = ocean_domain_size;
	solver_settings.wind_speed = wind_speed;
	solver_settings.wind_direction = wind_direction;
	solver_settings.wave_amplitude = wave_amplitude;
	solver_settings.fetch = get_effective_fetch();
	solver_settings.caustics_enabled = caustics_enabled;
	solver.configure(solver_settings);
	solver.set_foam_settings(foam_enabled, choppiness, foam_threshold, foam_persistence, shore_foam_band, shore_undertow);
	solver.set_swash_settings(swash_run_up, swash_drain_speed, wet_sand_dry_time);
	if (!solver.has_device() && !warned_no_device) {
		warned_no_device = true;
		WARN_PRINT("PhysXWaterSurface3D: the water compute solver could not start (no RenderingDevice / compute support).");
	}

	textures_bound = false; // solver was just rebuilt -- its old texture RIDs (if any) are gone, rebind once available
	// WATER-1: unbind EVERY wrapper, not just caustics -- _rt_build_buffers
	// frees and recreates all 13 solver textures, and the other 9 wrappers
	// kept sampling the freed RD image handles for 1-2 frames (validation
	// errors / garbage frames on every runtime property change).
	_unbind_all_textures();
	frames_since_refresh = REFRESH_EVERY_FRAMES; // force an immediate CPU cache refresh on the next _update()
}

void PhysXWaterSurface3D::_update_mesh_offset() {
	// Open water past the simulated square: the mesh's vertices spread out
	// with distance from its dense core, and far out they're meters apart --
	// too coarse for the short waves, so what's drawn there missed what
	// sample_height() (and so a boat) sees by tenths of a meter. The core
	// follows the camera instead, smoothly, every frame: near the camera the
	// dense vertices sample the waves finely wherever they sit, and the
	// coarse surface past it reshapes gradually (~1 cm per 5 cm moved, 20-35
	// m out) instead of jumping. (Snapping popped: whole 40 m ocean tiles
	// re-meshed everything past the core at once -- 15 cm jumps -- and
	// core-spacing steps made a constant 6 cm shimmer.)
	if (mesh_instance == nullptr || water_material.is_null() || surface_mesh.is_valid() || render_extent <= MAX(ocean_domain_size.x, ocean_domain_size.y)) {
		return;
	}
	const Viewport *vp = get_viewport();
	const Camera3D *cam = vp != nullptr ? vp->get_camera_3d() : nullptr;
	if (cam == nullptr) {
		return;
	}
	const Vector3 cp = cam->get_global_position();
	const Vector2 rel = Vector2(cp.x, cp.z) - grid_center;
	const Vector2 offset = rel;
	if (offset == mesh_offset) {
		return;
	}
	mesh_offset = offset;
	water_material->set_shader_parameter("mesh_offset", mesh_offset);
	if (water_mesh.is_valid()) {
		AABB box = water_mesh->get_aabb();
		box.position += mesh_instance->get_global_transform().basis.xform_inv(Vector3(offset.x, 0, offset.y));
		mesh_instance->set_custom_aabb(box.grow(MAX(wave_amplitude * 4.0f, 2.0f)));
	}
}

void PhysXWaterSurface3D::_update(double p_delta) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	water_time += p_delta;
	if (water_material.is_valid()) {
		// Wrapped so float precision holds up in long sessions.
		water_material->set_shader_parameter("water_time", (float)Math::fmod(water_time, 3600.0));
	}
	if (seabed_pending) {
		seabed_pending = false;
		_sample_seabed();
		_configure_solver();
		return;
	}
	if (!solver.has_device()) {
		return;
	}
	solver.step(p_delta); // no-ops internally until solver.is_available() (see water_solver.cpp)
	if (!textures_bound && solver.is_available()) {
		_bind_textures();
	}
	if (solver.is_available() && caustics_enabled) {
		solver.render_caustics(Vector3(grid_center.x, get_global_position().y, grid_center.y), caustics_sun_direction, caustics_reference_depth, 1.333f, _open_beyond());
	}
	frames_since_refresh++;
	if (frames_since_refresh >= REFRESH_EVERY_FRAMES) {
		frames_since_refresh = 0;
		_refresh_cpu_cache();
	}
}

void PhysXWaterSurface3D::_unbind_all_textures() {
	// Clear the RD RID on every wrapper so nothing samples a freed texture
	// between a solver rebuild and the next _bind_textures().
	if (ripple_height_tex.is_valid()) {
		ripple_height_tex->set_texture_rd_rid(RID());
	}
	if (ocean_height_tex.is_valid()) {
		ocean_height_tex->set_texture_rd_rid(RID());
	}
	if (ocean_fade_tex.is_valid()) {
		ocean_fade_tex->set_texture_rd_rid(RID());
	}
	if (ocean_disp_tex.is_valid()) {
		ocean_disp_tex->set_texture_rd_rid(RID());
	}
	if (ocean_deriv_tex.is_valid()) {
		ocean_deriv_tex->set_texture_rd_rid(RID());
	}
	if (ocean_foam_tex.is_valid()) {
		ocean_foam_tex->set_texture_rd_rid(RID());
	}
	if (shore_foam_tex.is_valid()) {
		shore_foam_tex->set_texture_rd_rid(RID());
	}
	if (swash_tex.is_valid()) {
		swash_tex->set_texture_rd_rid(RID());
	}
	if (shore_depth_tex.is_valid()) {
		shore_depth_tex->set_texture_rd_rid(RID());
	}
	if (caustics_texture.is_valid()) {
		caustics_texture->set_texture_rd_rid(RID());
	}
}

void PhysXWaterSurface3D::_bind_textures() {
	// Zero-copy: wrap the solver's own RD texture RIDs directly. Set once --
	// Texture2DRD holds the RID by reference, so whatever the blit shaders
	// write each step (see water_solver.cpp's rt_step()) is what the
	// material samples automatically; no per-frame reassignment needed.
	if (ripple_height_tex.is_null()) {
		ripple_height_tex.instantiate();
	}
	if (ocean_height_tex.is_null()) {
		ocean_height_tex.instantiate();
	}
	if (ocean_fade_tex.is_null()) {
		ocean_fade_tex.instantiate();
	}
	if (ocean_disp_tex.is_null()) {
		ocean_disp_tex.instantiate();
	}
	ocean_disp_tex->set_texture_rd_rid(solver.get_ocean_displacement_texture_rd_rid());
	if (ocean_deriv_tex.is_null()) {
		ocean_deriv_tex.instantiate();
	}
	ocean_deriv_tex->set_texture_rd_rid(solver.get_ocean_derivative_texture_rd_rid());
	if (ocean_foam_tex.is_null()) {
		ocean_foam_tex.instantiate();
	}
	ocean_foam_tex->set_texture_rd_rid(solver.get_ocean_foam_texture_rd_rid());
	if (shore_foam_tex.is_null()) {
		shore_foam_tex.instantiate();
	}
	shore_foam_tex->set_texture_rd_rid(solver.get_shore_foam_texture_rd_rid());
	if (swash_tex.is_null()) {
		swash_tex.instantiate();
	}
	swash_tex->set_texture_rd_rid(solver.get_swash_texture_rd_rid());
	ripple_height_tex->set_texture_rd_rid(solver.get_ripple_height_texture_rd_rid());
	ocean_height_tex->set_texture_rd_rid(solver.get_ocean_height_texture_rd_rid());
	ocean_fade_tex->set_texture_rd_rid(solver.get_ocean_fade_texture_rd_rid());
	if (shore_depth_tex.is_null()) {
		shore_depth_tex.instantiate();
	}
	shore_depth_tex->set_texture_rd_rid(solver.get_shore_depth_texture_rd_rid());
	if (caustics_enabled) {
		if (caustics_texture.is_null()) {
			caustics_texture.instantiate();
		}
		caustics_texture->set_texture_rd_rid(solver.get_caustics_texture_rd_rid());
	}
	if (water_material.is_valid()) {
		water_material->set_shader_parameter("ripple_height_tex", ripple_height_tex);
		water_material->set_shader_parameter("ocean_height_tex", ocean_height_tex);
		water_material->set_shader_parameter("ocean_fade_tex", ocean_fade_tex);
		water_material->set_shader_parameter("shore_depth_tex", shore_depth_tex);
		water_material->set_shader_parameter("ocean_disp_tex", ocean_disp_tex);
		water_material->set_shader_parameter("ocean_deriv_tex", ocean_deriv_tex);
		water_material->set_shader_parameter("ocean_foam_tex", ocean_foam_tex);
		water_material->set_shader_parameter("shore_foam_tex", shore_foam_tex);
		water_material->set_shader_parameter("swash_tex", swash_tex);
	}
	textures_bound = true;
}

void PhysXWaterSurface3D::_refresh_cpu_cache() {
	// CPU-side cache for sample_height() only -- rendering doesn't touch
	// this at all (see _bind_textures()). Non-blocking: just whatever the
	// solver's last async readback landed, a few frames stale -- fine for
	// buoyancy, see water_solver.h's own note on why this stays a separate
	// path from the zero-copy texture above.
	solver.get_height_grid(cached_ripple_height, cached_ripple_n, cached_ripple_domain);
	solver.get_ocean_height_grid(cached_ocean_height, cached_ocean_dx, cached_ocean_n, cached_ocean_domain);
	solver.get_ocean_dz_grid(cached_ocean_dz);
}

float PhysXWaterSurface3D::_bilinear_sample(const Vector<float> &p_grid, int p_n, Vector2 p_domain, float p_world_x, float p_world_z, bool p_repeat) const {
	if (p_grid.is_empty() || p_n <= 0) {
		return 0.0f;
	}
	const float gx = (p_world_x / (p_domain.x * 0.5f) + 1.0f) * 0.5f * p_n - 0.5f;
	const float gz = (p_world_z / (p_domain.y * 0.5f) + 1.0f) * 0.5f * p_n - 0.5f;
	int x0, z0, x1, z1;
	float fx, fz;
	if (p_repeat) {
		const float flx = floorf(gx);
		const float flz = floorf(gz);
		auto wrap = [p_n](int p_i) { return ((p_i % p_n) + p_n) % p_n; };
		x0 = wrap((int)flx);
		z0 = wrap((int)flz);
		x1 = wrap((int)flx + 1);
		z1 = wrap((int)flz + 1);
		fx = gx - flx;
		fz = gz - flz;
	} else {
		x0 = CLAMP((int)floorf(gx), 0, p_n - 1);
		z0 = CLAMP((int)floorf(gz), 0, p_n - 1);
		x1 = CLAMP(x0 + 1, 0, p_n - 1);
		z1 = CLAMP(z0 + 1, 0, p_n - 1);
		fx = CLAMP(gx - x0, 0.0f, 1.0f);
		fz = CLAMP(gz - z0, 0.0f, 1.0f);
	}
	const float h00 = p_grid[z0 * p_n + x0];
	const float h10 = p_grid[z0 * p_n + x1];
	const float h01 = p_grid[z1 * p_n + x0];
	const float h11 = p_grid[z1 * p_n + x1];
	return Math::lerp(Math::lerp(h00, h10, fx), Math::lerp(h01, h11, fx), fz);
}

float PhysXWaterSurface3D::sample_height(Vector3 p_world_pos) const {
	return sample_height_excluding_wake(p_world_pos, ObjectID());
}

int PhysXWaterSurface3D::register_wake(ObjectID p_wake) {
	for (int i = 0; i < MAX_WAKES; i++) {
		if (wakes[i] == p_wake || ObjectDB::get_instance(wakes[i]) == nullptr) {
			wakes[i] = p_wake;
			return i;
		}
	}
	return -1;
}

void PhysXWaterSurface3D::unregister_wake(int p_slot) {
	ERR_FAIL_INDEX(p_slot, MAX_WAKES);
	wakes[p_slot] = ObjectID();
	if (water_material.is_valid()) {
		water_material->set_shader_parameter("wake_height_" + itos(p_slot), Variant());
		water_material->set_shader_parameter("wake_rect_" + itos(p_slot), Vector4());
	}
}

void PhysXWaterSurface3D::set_wake_slot(int p_slot, const Ref<Texture2D> &p_height, const Vector4 &p_rect) {
	ERR_FAIL_INDEX(p_slot, MAX_WAKES);
	if (water_material.is_null()) {
		return;
	}
	const String i = itos(p_slot);
	if (water_material->get_shader_parameter("wake_height_" + i) != Variant(p_height)) {
		water_material->set_shader_parameter("wake_height_" + i, p_height);
	}
	water_material->set_shader_parameter("wake_rect_" + i, p_rect);
}

float PhysXWaterSurface3D::sample_height_excluding_wake(Vector3 p_world_pos, ObjectID p_wake) const {
	if (!is_wet(p_world_pos)) {
		return -Math::INF;
	}
	const float grid_local_x = p_world_pos.x - grid_center.x;
	const float grid_local_z = p_world_pos.z - grid_center.y;
	// The ripple readback already includes water_level (it'displacement_scale the composed
	// height the renderer draws), so only fall back to it before the first
	// readback lands. Both are relative to the node, like the rendered mesh.
	// Past the simulated square the ripples ease back to still water, as the
	// shader draws them.
	float ripple = cached_ripple_height.is_empty() ? water_level : _bilinear_sample(cached_ripple_height, cached_ripple_n, cached_ripple_domain, grid_local_x, grid_local_z);
	if (!cached_ripple_height.is_empty()) {
		const float from_center = MAX(Math::abs(grid_local_x / cached_ripple_domain.x), Math::abs(grid_local_z / cached_ripple_domain.y));
		const float inside = 1.0f - Math::smoothstep(0.46f, 0.5f, from_center);
		ripple = Math::lerp(water_level, ripple, inside);
	}
	const float fade = cell_fade.is_empty() ? 1.0f : _bilinear_sample(cell_fade, grid_resolution, active_domain_size, grid_local_x, grid_local_z);
	// With choppy displacement the surface point above (grid_local_x, grid_local_z) rested
	// somewhere else: find p0 with p0 + displacement_scale * D(p0) = (grid_local_x, grid_local_z) by fixed-point
	// iteration (displacement_scale matches the shader'displacement_scale -choppiness * fade), then read the
	// ocean height there.
	Vector2 p0(grid_local_x, grid_local_z);
	const float displacement_scale = -choppiness * fade;
	if (displacement_scale != 0.0f && cached_ocean_dx.size() == cached_ocean_height.size() && cached_ocean_dz.size() == cached_ocean_height.size()) {
		for (int i = 0; i < 4; i++) {
			const Vector2 displacement(_bilinear_sample(cached_ocean_dx, cached_ocean_n, cached_ocean_domain, p0.x, p0.y, true),
					_bilinear_sample(cached_ocean_dz, cached_ocean_n, cached_ocean_domain, p0.x, p0.y, true));
			p0 = Vector2(grid_local_x, grid_local_z) - displacement * displacement_scale;
		}
	}
	const float ocean = _bilinear_sample(cached_ocean_height, cached_ocean_n, cached_ocean_domain, p0.x, p0.y, true) * fade;
	// Wakes, everywhere (as the shader draws them).
	float wake = 0.0f;
	for (int i = 0; i < MAX_WAKES; i++) {
		if (wakes[i] == p_wake) {
			continue;
		}
		const PhysXWaterWake3D *w = ObjectDB::get_instance<PhysXWaterWake3D>(wakes[i]);
		if (w != nullptr) {
			const float out = Math::smoothstep(0.46f, 0.5f, MAX(Math::abs(grid_local_x / active_domain_size.x), Math::abs(grid_local_z / active_domain_size.y)));
			wake += w->sample_height(p_world_pos) * Math::lerp(1.0f, out, w->get_shore_grid_fade());
		}
	}
	// Not over dry sand (as the shader): fades out over the last 30 cm of depth.
	if (wake != 0.0f && !cell_depth.is_empty()) {
		wake *= Math::smoothstep(0.0f, 0.3f, _bilinear_sample(cell_depth, grid_resolution, active_domain_size, grid_local_x, grid_local_z));
	}
	return get_global_position().y + ripple + ocean + wake;
}

bool PhysXWaterSurface3D::_open_beyond() const {
	return surface_mesh.is_null() && render_extent > MAX(domain_size.x, domain_size.y);
}

bool PhysXWaterSurface3D::_inside_render_extent(float p_rel_x, float p_rel_z) const {
	return Math::abs(p_rel_x) <= render_extent * 0.5f && Math::abs(p_rel_z) <= render_extent * 0.5f;
}

bool PhysXWaterSurface3D::is_wet(Vector3 p_world_pos) const {
	const float rel_x = p_world_pos.x - grid_center.x;
	const float rel_z = p_world_pos.z - grid_center.y;
	if (_open_beyond() && !_inside_render_extent(rel_x, rel_z)) {
		return false; // past the drawn surface
	}
	if (cell_depth.is_empty()) {
		return true;
	}
	const int grid_size = grid_resolution;
	if (cell_depth.size() != grid_size * grid_size) {
		return false;
	}
	int gx = (int)Math::floor((rel_x / active_domain_size.x + 0.5f) * grid_size);
	int gz = (int)Math::floor((rel_z / active_domain_size.y + 0.5f) * grid_size);
	if (gx < 0 || gz < 0 || gx >= grid_size || gz >= grid_size) {
		if (!_open_beyond()) {
			return false;
		}
		// Open water past the simulated square carries the edge's seabed on
		// outward, as the shader draws it: the sea side stays sea, the beach
		// side stays sand.
		gx = CLAMP(gx, 0, grid_size - 1);
		gz = CLAMP(gz, 0, grid_size - 1);
	}
	return cell_depth[gz * grid_size + gx] > 0.0f;
}

Vector<Vector2> PhysXWaterSurface3D::_collect_footprint() const {
	Vector<Vector2> tris;
	if (surface_mesh.is_null()) {
		return tris;
	}
	// Only up-facing triangles: a closed mesh (a baked CSG disc has a top and
	// a bottom) would otherwise cover its footprint twice. A mesh with no
	// up-facing triangles at all (flipped winding) falls back to every face.
	const real_t up = plane_mesh_xz_winding();
	const Vector<Face3> faces = surface_mesh->get_faces();
	Vector<Vector2> any_facing;
	for (const Face3 &f : faces) {
		const Vector2 a(f.vertex[0].x, f.vertex[0].z);
		const Vector2 tri_b(f.vertex[1].x, f.vertex[1].z);
		const Vector2 tri_c(f.vertex[2].x, f.vertex[2].z);
		const real_t area = (tri_b - a).cross(tri_c - a);
		if (Math::abs(area) < (real_t)1e-8) {
			continue; // no X/Z area (degenerate, or a vertical face)
		}
		Vector<Vector2> &dst = (area * up > 0) ? tris : any_facing;
		dst.push_back(a);
		dst.push_back(tri_b);
		dst.push_back(tri_c);
	}
	if (tris.is_empty()) {
		return any_facing;
	}
	return tris;
}

PackedByteArray PhysXWaterSurface3D::_rasterize_wet_mask(const Vector<Vector2> &p_tris, Vector2 p_center, Vector2 p_domain, int p_n) {
	// A cell is wet when its center lies inside any footprint triangle -- the
	// same cell-centre convention as water_ripple.glsl's xz.
	PackedByteArray mask;
	mask.resize(p_n * p_n);
	memset(mask.ptrw(), 0, mask.size());
	uint8_t *mw = mask.ptrw();
	const Vector2 cell = p_domain / (real_t)p_n;
	const Vector2 grid_min = p_center - p_domain * 0.5f;
	for (int t = 0; t + 2 < p_tris.size(); t += 3) {
		const Vector2 tri_a = p_tris[t], tri_b = p_tris[t + 1], tri_c = p_tris[t + 2];
		const Vector2 lo = tri_a.min(tri_b).min(tri_c);
		const Vector2 hi = tri_a.max(tri_b).max(tri_c);
		const int x0 = CLAMP((int)Math::floor((lo.x - grid_min.x) / cell.x), 0, p_n - 1);
		const int x1 = CLAMP((int)Math::floor((hi.x - grid_min.x) / cell.x), 0, p_n - 1);
		const int z0 = CLAMP((int)Math::floor((lo.y - grid_min.y) / cell.y), 0, p_n - 1);
		const int z1 = CLAMP((int)Math::floor((hi.y - grid_min.y) / cell.y), 0, p_n - 1);
		for (int z = z0; z <= z1; z++) {
			for (int x = x0; x <= x1; x++) {
				const Vector2 sample_center = grid_min + Vector2(x + 0.5f, z + 0.5f) * cell;
				if (point_in_triangle(sample_center, tri_a, tri_b, tri_c)) {
					mw[z * p_n + x] = 1;
				}
			}
		}
	}
	return mask;
}

Ref<Mesh> PhysXWaterSurface3D::_build_extended_plane(Vector2 p_inner, int p_inner_cells, real_t p_extent) const {
	// A regular grid of (2K+1)^2 vertices whose square rings are spread out
	// from the center: rings up to K0 are the simulated span at uniform
	// spacing, and each ring past it is 15% further out than the step before
	// -- detail where the waves are simulated, idx_top_left few hundred rings out to the
	// horizon. Spacing grows with the ring (distance from the center), the
	// same along X and Z. (A per-axis spacing left the simulated span's fine
	// columns/rows running out to the horizon as two strips of fine mesh
	// along the axes, and the same waves looked different either side of
	// each strip's edge: idx_top_left seam straight out to sea.)
	const int k0 = MAX(p_inner_cells / 2, 1);
	const Vector2 half_inner = p_inner * 0.5f;
	const real_t max_half = MAX(half_inner.x, half_inner.y);
	// radius[idx_bottom_left]: ring idx_bottom_left's half-size, in units of the inner step (ring idx_bottom_left <= k0: idx_bottom_left).
	LocalVector<real_t> radius;
	for (int idx_bottom_left = 0; idx_bottom_left <= k0; idx_bottom_left++) {
		radius.push_back((real_t)idx_bottom_left);
	}
	{
		const real_t limit = (p_extent * 0.5f) / max_half * (real_t)k0;
		real_t ring_radius = (real_t)k0;
		real_t step = 1.0f;
		while (ring_radius < limit) {
			step *= 1.15f;
			ring_radius = MIN(ring_radius + step, limit);
			radius.push_back(ring_radius);
		}
	}
	const int kmax = (int)radius.size() - 1;
	const int nx = 2 * kmax + 1;
	const int nz = nx;
	auto position = [&](int p_i, int p_j) {
		const int ai = p_i < 0 ? -p_i : p_i;
		const int aj = p_j < 0 ? -p_j : p_j;
		const int ring = ai > aj ? ai : aj;
		const real_t scale = ring == 0 ? (real_t)1.0 : radius[ring] / (real_t)ring;
		return Vector2((real_t)p_i * scale * half_inner.x / (real_t)k0, (real_t)p_j * scale * half_inner.y / (real_t)k0);
	};

	PackedVector3Array verts;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	verts.resize(nx * nz);
	normals.resize(nx * nz);
	uvs.resize(nx * nz);
	for (int j = 0; j < nz; j++) {
		for (int i = 0; i < nx; i++) {
			const int vertex_index = j * nx + i;
			const Vector2 xz = position(i - kmax, j - kmax);
			verts.set(vertex_index, Vector3(xz.x, 0.0f, xz.y));
			normals.set(vertex_index, Vector3(0, 1, 0));
			uvs.set(vertex_index, xz / p_extent + Vector2(0.5f, 0.5f));
		}
	}
	const bool flip = plane_mesh_xz_winding() < 0;
	PackedInt32Array indices;
	indices.resize((nx - 1) * (nz - 1) * 6);
	int write_index = 0;
	for (int j = 0; j < nz - 1; j++) {
		for (int i = 0; i < nx - 1; i++) {
			const int idx_top_left = j * nx + i;
			const int idx_top_right = idx_top_left + 1;
			const int idx_bottom_left = idx_top_left + nx;
			const int idx_bottom_right = idx_bottom_left + 1;
			// (idx_top_left, idx_top_right, idx_bottom_right) has idx_top_left positive X/Z cross product; flip to PlaneMesh's.
			const int tri[6] = { idx_top_left, flip ? idx_bottom_right : idx_top_right, flip ? idx_top_right : idx_bottom_right, idx_top_left, flip ? idx_bottom_left : idx_bottom_right, flip ? idx_bottom_right : idx_bottom_left };
			for (int t = 0; t < 6; t++) {
				indices.set(write_index++, tri[t]);
			}
		}
	}
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

Ref<Mesh> PhysXWaterSurface3D::_build_footprint_mesh(const Vector<Vector2> &p_tris, const Rect2 &p_bounds, int p_cells) const {
	// Resample the footprint into an even grid so the height fields have
	// vertices to displace everywhere (a CSG disc is a fan of long slivers
	// with no interior vertices). Cells no triangle edge passes through are
	// whole quads, kept or dropped by their center; cells an edge touches are
	// clipped against each overlapping triangle. A quad cell's edges are
	// never crossed by a triangle edge, so they carry no extra vertices and
	// meet their clipped neighbors without T-junctions.
	const real_t side = MAX(p_bounds.size.x, p_bounds.size.y);
	const real_t cs = side / (real_t)MAX(p_cells, 1);
	const int nx = MAX((int)Math::ceil(p_bounds.size.x / cs), 1);
	const int nz = MAX((int)Math::ceil(p_bounds.size.y / cs), 1);
	const Vector2 origin = p_bounds.position;

	const int tri_count = p_tris.size() / 3;
	LocalVector<LocalVector<int>> cell_tris;
	cell_tris.resize(nx * nz);
	LocalVector<uint8_t> edge_cell;
	edge_cell.resize(nx * nz);
	memset(edge_cell.ptr(), 0, edge_cell.size());
	const real_t eps = cs * (real_t)1e-4;
	auto cell_range = [&](const Vector2 &p_lo, const Vector2 &p_hi, int &r_x0, int &r_x1, int &r_z0, int &r_z1) {
		r_x0 = CLAMP((int)Math::floor((p_lo.x - eps - origin.x) / cs), 0, nx - 1);
		r_x1 = CLAMP((int)Math::floor((p_hi.x + eps - origin.x) / cs), 0, nx - 1);
		r_z0 = CLAMP((int)Math::floor((p_lo.y - eps - origin.y) / cs), 0, nz - 1);
		r_z1 = CLAMP((int)Math::floor((p_hi.y + eps - origin.y) / cs), 0, nz - 1);
	};
	for (int t = 0; t < tri_count; t++) {
		const Vector2 v[3] = { p_tris[t * 3], p_tris[t * 3 + 1], p_tris[t * 3 + 2] };
		int x0, x1, z0, z1;
		cell_range(v[0].min(v[1]).min(v[2]), v[0].max(v[1]).max(v[2]), x0, x1, z0, z1);
		for (int z = z0; z <= z1; z++) {
			for (int x = x0; x <= x1; x++) {
				cell_tris[z * nx + x].push_back(t);
			}
		}
		for (int e = 0; e < 3; e++) {
			const Vector2 p0 = v[e], p1 = v[(e + 1) % 3];
			cell_range(p0.min(p1), p0.max(p1), x0, x1, z0, z1);
			for (int z = z0; z <= z1; z++) {
				for (int x = x0; x <= x1; x++) {
					const Vector2 bmin = origin + Vector2(x, z) * cs - Vector2(eps, eps);
					const Vector2 bmax = origin + Vector2(x + 1, z + 1) * cs + Vector2(eps, eps);
					if (segment_touches_box(p0, p1, bmin, bmax)) {
						edge_cell[z * nx + x] = 1;
					}
				}
			}
		}
	}

	const real_t plane_winding = plane_mesh_xz_winding();

	PackedVector3Array verts;
	PackedVector3Array normals;
	PackedVector2Array uvs;
	PackedInt32Array indices;
	HashMap<Vector2i, int> vertex_ids;
	auto vertex_id = [&](const Vector2 &p_v) {
		const Vector2i key((int)Math::round(p_v.x * 10000.0f), (int)Math::round(p_v.y * 10000.0f));
		if (const int *found = vertex_ids.getptr(key)) {
			return *found;
		}
		const int id = verts.size();
		verts.push_back(Vector3(p_v.x, 0.0f, p_v.y));
		normals.push_back(Vector3(0, 1, 0));
		uvs.push_back((p_v - origin) / side);
		vertex_ids.insert(key, id);
		return id;
	};
	auto add_triangle = [&](const Vector2 &p_a, Vector2 p_b, Vector2 p_c) {
		const real_t area = (p_b - p_a).cross(p_c - p_a);
		if (Math::abs(area) < cs * cs * (real_t)1e-6) {
			return;
		}
		if ((area < 0) != (plane_winding < 0)) {
			SWAP(p_b, p_c);
		}
		indices.push_back(vertex_id(p_a));
		indices.push_back(vertex_id(p_b));
		indices.push_back(vertex_id(p_c));
	};

	LocalVector<Vector2> poly;
	for (int z = 0; z < nz; z++) {
		for (int x = 0; x < nx; x++) {
			const int ci = z * nx + x;
			const Vector2 c00 = origin + Vector2(x, z) * cs;
			const Vector2 c10 = origin + Vector2(x + 1, z) * cs;
			const Vector2 c11 = origin + Vector2(x + 1, z + 1) * cs;
			const Vector2 c01 = origin + Vector2(x, z + 1) * cs;
			if (!edge_cell[ci]) {
				const Vector2 center = (c00 + c11) * 0.5f;
				bool inside = false;
				for (int t : cell_tris[ci]) {
					if (point_in_triangle(center, p_tris[t * 3], p_tris[t * 3 + 1], p_tris[t * 3 + 2])) {
						inside = true;
						break;
					}
				}
				if (inside) {
					add_triangle(c00, c10, c11);
					add_triangle(c00, c11, c01);
				}
				continue;
			}
			for (int t : cell_tris[ci]) {
				Vector2 tri[3] = { p_tris[t * 3], p_tris[t * 3 + 1], p_tris[t * 3 + 2] };
				if ((tri[1] - tri[0]).cross(tri[2] - tri[0]) < 0) {
					SWAP(tri[1], tri[2]);
				}
				poly.clear();
				poly.push_back(c00);
				poly.push_back(c10);
				poly.push_back(c11);
				poly.push_back(c01);
				clip_to_triangle(poly, tri);
				for (uint32_t i = 1; i + 1 < poly.size(); i++) {
					add_triangle(poly[0], poly[i], poly[i + 1]);
				}
			}
		}
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	if (indices.is_empty()) {
		return mesh;
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

void PhysXWaterSurface3D::submit_sphere(int p_owner, Vector3 p_world_pos, float p_radius, float p_strength) {
	solver.submit_sphere((uint64_t)p_owner, p_world_pos, p_radius, p_strength);
}

void PhysXWaterSurface3D::clear_sphere(int p_owner) {
	solver.clear_sphere((uint64_t)p_owner);
}

void PhysXWaterSurface3D::submit_impulse(Vector3 p_world_pos, float p_radius, float p_strength) {
	solver.submit_impulse(p_world_pos, p_radius, p_strength);
}

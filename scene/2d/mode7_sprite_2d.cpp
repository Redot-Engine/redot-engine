/**************************************************************************/
/*  mode7_sprite_2d.cpp                                                   */
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

/**
 * @file mode7_sprite2d.cpp
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "mode7_sprite_2d.h"
#include <cmath>

#include "scene/2d/mode7_scanline_override.h"
#include "scene/2d/mode7_sprite_2d.h"
#include "scene/main/node.h"
#include "scene/main/viewport.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"

// Embedded Mode 7 canvas_item shader.
// The scanline table is a 3-wide, N-tall RGBAF texture:
//   column x=0.1667: (col0.x, col1.x, col0.y, col1.y) — the 2x2 affine matrix
//   column x=0.5:    (tx, ty, pivot_x, pivot_y) — translation & pivot
//   column x=0.8333: (r, g, b, a) — per-scanline modulate
// UV.y (0..1) is used to index the row, so each row maps to a horizontal
// band of the output sprite.
//
// All Mode 7 transforms operate in region-local [0,1]×[0,1] space. When no
// region is active, REGION_RECT defaults to (0,0,1,1) and the math is a no-op,
// so the full-texture behavior is preserved. This ensures pivots like (0.5,0.5)
// always refer to the center of the visible area, not the center of the atlas.
static const char *MODE7_SHADER_CODE = R"(
shader_type canvas_item;

// Preserve the incoming vertex color (e.g., Sprite2D self modulate) so the
// fragment shader can tint the sampled texture with it.
varying vec4 vc_vertex_color;

void vertex() {
    vc_vertex_color = COLOR;
}

// Per-scanline affine parameters (3×N texture: col0=transform, col1=offset/pivot, col2=modulate)
// This "texture" is really a hack to get data into a format for GPUs/shaders
// This stores the per-scanline information to give us the same control as the Super Nintendo.
// Each "row" is 3 pixels wide, each is a "Color," but we're simply after the vec4/data that's passed in.
// Think of this as 3 columns of data in a table
// Transform | Offset/Pivot | Modulate/Color/Bloom
uniform sampler2D mode7_scanline_table : filter_nearest, repeat_disable;

// Tiling: false=transparent out-of-bounds, true=repeat texture
uniform bool mode7_tiling = false;

// Global parameters applied after all scanline transforms
uniform float mode7_global_rotation = 0.0;
uniform vec2  mode7_global_pivot    = vec2(0.5, 0.5);
uniform vec2  mode7_global_offset   = vec2(0.0, 0.0);

// Horizon: angle of the horizon line for top and bottom regions.
// Each tilt rotates the UV space around the center like an aircraft
// attitude indicator. Amount controls how much of each region is masked;
// tilt is always computed independently.
// Masking is evaluated in raw screen-space (before per-scanline projection)
// so that mask_amount maps directly to a visible-screen percentage regardless
// of how extreme the projection interpolation transforms are.
uniform float mode7_top_horizon_mask_amount  = 0.0;
uniform float mode7_top_horizon_tilt         = 0.0;
uniform float mode7_bottom_horizon_mask_amount = 0.0;
uniform float mode7_bottom_horizon_tilt      = 0.0;

// When true, global rotation and horizon tilts are computed as if the
// active Region Rect were square (then re-mapped to fill the actual
// region), so non-square regions don't skew rotations into shears.
uniform bool mode7_override_region_aspect = true;

// Builds a rotation matrix pre/post scaled to compensate for a non-square
// region aspect ratio, so the visual rotation stays angle-preserving.
// aspect = region_pixel_width / region_pixel_height.
mat2 aspect_rotate(float angle, float aspect) {
    float cr = cos(angle);
    float sr = sin(angle);
    return mat2(vec2(cr, sr * aspect), vec2(-sr / aspect, cr));
}

void fragment() {
    // When Region is used, we want the shader to apply to the resulting visible section,
    // not the entire image.
    // When no region is set, REGION_RECT = (0,0,1,1) and this is a no-op.
    vec2 uv = (UV - REGION_RECT.xy) / REGION_RECT.zw;

    // Compute the region's aspect ratio in actual texture pixels so that
    // rotations can be corrected.
    // This option is because, when only showing/transforming the selected region,
    // the UVs here will be stretched if it's not square.  This enables keeping the transformation
    // as expected, but still allowing any aspect for the region.
    vec2 region_px = REGION_RECT.zw / TEXTURE_PIXEL_SIZE;
    float region_aspect = mode7_override_region_aspect ? (region_px.x / region_px.y) : 1.0;

    // Get the data we need from the scanline "table" above
    vec4 transform_data   = texture(mode7_scanline_table, vec2(0.1667, uv.y));
    vec4 offset_pivot_data     = texture(mode7_scanline_table, vec2(0.5, uv.y));
    vec4 mod    = texture(mode7_scanline_table, vec2(0.8333, uv.y));

    // Reconstruct 2×2 affine matrix (column-major: col0, col1)
    // NOTE: left untouched by the aspect fix — this comes from the
    // scanline table's own inverse-depth math and must not be altered.
    mat2 matrix_transformed   = mat2(vec2(transform_data.r, transform_data.b), vec2(transform_data.g, transform_data.a));
    vec2 offset = vec2(offset_pivot_data.r, offset_pivot_data.g);
    vec2 pivot = vec2(offset_pivot_data.b, offset_pivot_data.a);

    // Horizon: compute the tilted UV space for both top and bottom horizons.
    // Masking uses raw screen-space uv (pre per-scanline transform) so that
    // mask_amount maps directly to a visible-screen percentage regardless of
    // how much projection interpolation stretches the projected UVs.
    // The tilt rotates around center like an aircraft attitude indicator.
    float alpha_mult = 1.0;
    vec2 uv_screen_space = (UV - REGION_RECT.xy) / REGION_RECT.zw;
    vec2 uv_top_tilted  = aspect_rotate(mode7_top_horizon_tilt, region_aspect) * (uv_screen_space - vec2(0.5)) + vec2(0.5);
    vec2 uv_bottom_tilted = aspect_rotate(mode7_bottom_horizon_tilt, region_aspect) * (uv_screen_space - vec2(0.5)) + vec2(0.5);

    float horizon_line = 0.0;
    // Top mask: make a region at the top transparent
    horizon_line = mode7_top_horizon_mask_amount;
    alpha_mult *= step(horizon_line, uv_top_tilted.y);

    // Bottom mask: make a region at the bottom transparent
    horizon_line = 1.0 - mode7_bottom_horizon_mask_amount;
    alpha_mult *= 1.0 - step(horizon_line, uv_bottom_tilted.y);

    // Apply per-scanline transform relative to pivot (always in local [0,1])
    uv = matrix_transformed * (uv - pivot) + pivot + offset;

    // Apply global rotation (post-transform, around global pivot),
    // aspect-corrected so a non-square region doesn't shear the rotation.
    mat2 matrix_global = aspect_rotate(mode7_global_rotation, region_aspect);
    uv = matrix_global * (uv - mode7_global_pivot) + mode7_global_pivot;

    // Apply global offset uniformly across the final image.
    // We rotate the offset by the same angle so it shifts in the global (screen)
    // frame rather than the warped UV frame -- this gives a uniform screen-space
    // translation regardless of per-scanline scaling.
    uv += matrix_global * mode7_global_offset;

    // Denormalize back to full-texture coordinates before wrapping. Wrapping
    // here instead of on the region-local uv is what lets an out-of-region
    // UV reveal neighboring texture content instead of re-tiling the crop.
    vec2 uv_full = uv * REGION_RECT.zw + REGION_RECT.xy;

    bool out_of_bounds = !mode7_tiling &&
        (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0);

    if (out_of_bounds) {
        discard;
    } else {
        // Tiling wraps against the whole texture, not just the region, so it
        // works the same whether a region is set or not.
        vec2 uv_sample = mode7_tiling ? fract(uv_full) : uv_full;
        COLOR = texture(TEXTURE, uv_sample) * vc_vertex_color;
    }

    // Apply per-scanline modulate (color tint + alpha falloff)
    COLOR *= mod;
    COLOR.a *= alpha_mult;
}
)";

PackedStringArray Mode7Sprite2D::get_configuration_warnings() const {
	PackedStringArray warnings = Sprite2D::get_configuration_warnings();

	if (!mode7_region_follow_target.is_empty()) {
		Node *target = has_node(mode7_region_follow_target) ? get_node(mode7_region_follow_target) : nullptr;
		if (!target || !Object::cast_to<Node2D>(target)) {
			warnings.push_back(RTR("Region follow target path must point to a valid Node2D node to work."));
		} else if (target == this) {
			warnings.push_back(RTR("Region follow target cannot be this node."));
		}
	}

	return warnings;
}

void Mode7Sprite2D::set_mode7_saved_material(const Ref<Material> &p_material) {
	_saved_material = p_material;
}
Ref<Material> Mode7Sprite2D::get_mode7_saved_material() const {
	return _saved_material;
}

void Mode7Sprite2D::_mode7_rebuild_material() {
	if (_mode7_material.is_null()) {
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code(MODE7_SHADER_CODE);

		_mode7_material.instantiate();
		_mode7_material->set_shader(shader);
	}

	// Set the shader parameters (will be passed to the uniforms)
	_mode7_material->set_shader_parameter("mode7_global_rotation", mode7_global_rotation);
	_mode7_material->set_shader_parameter("mode7_global_pivot", mode7_global_pivot);
	_mode7_material->set_shader_parameter("mode7_global_offset", mode7_global_offset);
	_mode7_material->set_shader_parameter("mode7_top_horizon_mask_amount", mode7_top_horizon_mask_amount);
	_mode7_material->set_shader_parameter("mode7_top_horizon_tilt", mode7_top_horizon_tilt);
	_mode7_material->set_shader_parameter("mode7_bottom_horizon_mask_amount", mode7_bottom_horizon_mask_amount);
	_mode7_material->set_shader_parameter("mode7_bottom_horizon_tilt", mode7_bottom_horizon_tilt);

	_mode7_rebuild_scanline_texture();
	if (_mode7_scanline_tex.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_scanline_table",
				_mode7_scanline_tex);
	}

	_mode7_material->set_shader_parameter("mode7_tiling", mode7_tiling);
	_mode7_material->set_shader_parameter("mode7_override_region_aspect", mode7_override_region_aspect);
}

void Mode7Sprite2D::_mode7_rebuild_scanline_texture() {
	const int interpolate_resolution = 1024;
	int num_overrides = mode7_scanline_overrides.size();

	Ref<Image> img = Image::create_empty(3, interpolate_resolution, false, Image::FORMAT_RGBAF);

	for (int y = 0; y < interpolate_resolution; y++) {
		float uv_y = (y + 0.5f) / (float)interpolate_resolution;

		if (num_overrides == 0) {
			img->set_pixel(0, y, Color(1.0f, 0.0f, 0.0f, 1.0f));
			img->set_pixel(1, y, Color(0.0f, 0.0f, 0.5f, 0.5f));
			img->set_pixel(2, y, Color(1.0f, 1.0f, 1.0f, 1.0f));
			continue;
		}

		Transform2D result_transform;
		Vector2 pivot;
		Color mod;
		_mode7_compute_scanline_data((real_t)uv_y, result_transform, pivot, mod);

		img->set_pixel(0, y, Color(result_transform.columns[0].x, result_transform.columns[1].x, result_transform.columns[0].y, result_transform.columns[1].y));
		img->set_pixel(1, y, Color(result_transform.columns[2].x, result_transform.columns[2].y, pivot.x, pivot.y));
		img->set_pixel(2, y, mod);
	}

	if (_mode7_scanline_tex.is_null() || _mode7_scanline_tex->get_height() != interpolate_resolution) {
		_mode7_scanline_tex = ImageTexture::create_from_image(img);
	} else {
		_mode7_scanline_tex->update(img);
	}
}

void Mode7Sprite2D::_mode7_compute_scanline_data(real_t p_uv_y, Transform2D &r_transform, Vector2 &r_pivot, Color &r_modulate) const {
	Mode7Sprite2D::Mode7InterpolationMode interp_mode = mode7_interpolation;
	int num_overrides = mode7_scanline_overrides.size();

	if (num_overrides == 0) {
		r_transform = Transform2D();
		r_pivot = Vector2(0.5f, 0.5f);
		r_modulate = Color(1.0f, 1.0f, 1.0f, 1.0f);
		return;
	}

	if (interp_mode == Mode7Sprite2D::INTERPOLATION_PROJECTION && num_overrides >= 2) {
		auto safe_transform = [&](int i) { Ref<Mode7ScanlineOverride> s = mode7_scanline_overrides[i]; return s.is_valid() ? s->get_transform() : Transform2D(); };
		auto safe_pivot = [&](int i) { Ref<Mode7ScanlineOverride> s = mode7_scanline_overrides[i]; return s.is_valid() ? s->get_pivot() : Vector2(0.5f, 0.5f); };
		auto safe_modulate = [&](int i) { Ref<Mode7ScanlineOverride> s = mode7_scanline_overrides[i]; return s.is_valid() ? s->get_modulate() : Color(1.0f, 1.0f, 1.0f, 1.0f); };

		Transform2D transform_top = safe_transform(0);
		Transform2D transform_bottom = safe_transform(num_overrides - 1);
		Vector2 pivot_top = safe_pivot(0);
		Vector2 pivot_bottom = safe_pivot(num_overrides - 1);
		Color modulate_top = safe_modulate(0);
		Color modulate_bottom = safe_modulate(num_overrides - 1);

		Vector2 s_top = transform_top.get_scale();
		Vector2 s_bot = transform_bottom.get_scale();
		real_t scale_top = 1.0f / MAX(s_top.x, 0.0001f);
		real_t scale_bottom = 1.0f / MAX(s_bot.x, 0.0001f);
		real_t rotation_top = transform_top.get_rotation();
		real_t rotation_bottom = transform_bottom.get_rotation();

		real_t t = p_uv_y;

		real_t inv_s_top = 1.0f / MAX(scale_top, 0.0001f);
		real_t inv_s_bot = 1.0f / MAX(scale_bottom, 0.0001f);

		real_t t_pa = (t * mode7_projection_pixel_aspect) / (t * mode7_projection_pixel_aspect + (1.0f - t));
		real_t t_g = Math::pow(t_pa, mode7_projection_gamma);

		real_t inv_s_cur = inv_s_top + (inv_s_bot - inv_s_top) * t_g;
		real_t S = 1.0f / MAX(inv_s_cur, 0.0001f);

		real_t theta = rotation_top + (rotation_bottom - rotation_top) * t;

		if (mode7_projection_strength < 1.0f) {
			real_t S_flat = (scale_top + scale_bottom) * 0.5f;
			real_t theta_flat = (rotation_top + rotation_bottom) * 0.5f;
			S = S_flat + (S - S_flat) * mode7_projection_strength;
			theta = theta_flat + (theta - theta_flat) * mode7_projection_strength;
		}

		real_t Sx = S * mode7_projection_aspect_ratio;
		real_t Sy = S;

		real_t cos_t = Math::cos(theta);
		real_t sin_t = Math::sin(theta);

		Vector2 col0(cos_t * Sx, -sin_t * Sy);
		Vector2 col1(sin_t * Sx, cos_t * Sy);

		r_pivot = pivot_top.lerp(pivot_bottom, (real_t)t);
		r_modulate = modulate_top.lerp(modulate_bottom, (real_t)t);

		Vector2 off_raw = transform_top.columns[2].lerp(transform_bottom.columns[2], (real_t)t);

		real_t s_linear = scale_top + (scale_bottom - scale_top) * t;
		if (mode7_projection_strength < 1.0f) {
			const real_t s_flat = (scale_top + scale_bottom) * 0.5f;
			s_linear = s_flat + (s_linear - s_flat) * mode7_projection_strength;
		}
		real_t depth_factor = (s_linear > 0.001f) ? (S / s_linear - 1.0f) : 0.0f;

		off_raw += (transform_bottom.columns[2] - transform_top.columns[2]) * depth_factor;

		r_transform = Transform2D(col0, col1, off_raw);
	} else { // Lerp or no interpolation
		float idx_f = (num_overrides == 1) ? 0.0f : p_uv_y * (num_overrides - 1);
		int idx_lo = CLAMP((int)idx_f, 0, num_overrides - 1);
		int idx_hi = CLAMP(idx_lo + 1, 0, num_overrides - 1);
		float frac = idx_f - (float)idx_lo;

		Ref<Mode7ScanlineOverride> entry_lo = mode7_scanline_overrides[idx_lo];
		Transform2D xf_lo = entry_lo.is_valid() ? entry_lo->get_transform() : Transform2D();
		Vector2 pivot_lo = entry_lo.is_valid() ? entry_lo->get_pivot() : Vector2(0.5f, 0.5f);
		Color modulate_lo = entry_lo.is_valid() ? entry_lo->get_modulate() : Color(1.0f, 1.0f, 1.0f, 1.0f);
		bool do_lerp = (interp_mode == Mode7Sprite2D::INTERPOLATION_LERP);

		if (do_lerp && idx_hi != idx_lo && frac > 0.0f) {
			Ref<Mode7ScanlineOverride> entry_hi = mode7_scanline_overrides[idx_hi];
			Transform2D xf_hi = entry_hi.is_valid() ? entry_hi->get_transform() : Transform2D();
			Vector2 pivot_hi = entry_hi.is_valid() ? entry_hi->get_pivot() : Vector2(0.5f, 0.5f);
			Color modulate_hi = entry_hi.is_valid() ? entry_hi->get_modulate() : Color(1.0f, 1.0f, 1.0f, 1.0f);
			r_transform = xf_lo.interpolate_with(xf_hi, frac);
			r_pivot = pivot_lo.lerp(pivot_hi, frac);
			r_modulate = modulate_lo.lerp(modulate_hi, frac);
		} else {
			int idx_nearest = CLAMP((int)roundf(idx_f), 0, num_overrides - 1);
			Ref<Mode7ScanlineOverride> entry_nearest = mode7_scanline_overrides[idx_nearest];
			r_transform = entry_nearest.is_valid() ? entry_nearest->get_transform() : Transform2D();
			r_pivot = entry_nearest.is_valid() ? entry_nearest->get_pivot() : Vector2(0.5f, 0.5f);
			r_modulate = entry_nearest.is_valid() ? entry_nearest->get_modulate() : Color(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}
}

Transform2D Mode7Sprite2D::_mode7_aspect_rotate(real_t p_angle, real_t p_aspect) {
	real_t cr = Math::cos(p_angle);
	real_t sr = Math::sin(p_angle);
	// Mirrors the shader's mat2(vec2(cr, sr*aspect), vec2(-sr/aspect, cr)):
	// col0 = (cr, sr*aspect), col1 = (-sr/aspect, cr). Origin unused by caller.
	return Transform2D(Vector2(cr, sr * p_aspect), Vector2(-sr / p_aspect, cr), Vector2());
}

void Mode7Sprite2D::_mode7_get_full_rects(Rect2 &r_src_rect, Rect2 &r_dst_rect) const {
	// Mirrors Sprite2D::_get_rects() exactly, except base_rect is always the
	// full texture, never the cropped region_rect -- this is the "virtual"
	// full-image placement that point-space conversion needs, so that a
	// point far outside the currently-visible region crop still maps
	// correctly instead of being linearly extrapolated from a tiny quad.
	Ref<Texture2D> tex = get_texture();
	Rect2 base_rect = Rect2(0, 0, tex->get_width(), tex->get_height());

	Size2 frame_size = base_rect.size / Size2(get_hframes(), get_vframes());
	Point2 frame_offset = Point2(get_frame() % get_hframes(), get_frame() / get_hframes());
	frame_offset *= frame_size;

	r_src_rect.size = frame_size;
	r_src_rect.position = base_rect.position + frame_offset;

	Point2 dest_offset = get_offset();
	if (is_centered()) {
		dest_offset -= frame_size / 2;
	}

	if (get_viewport() && get_viewport()->is_snap_2d_transforms_to_pixel_enabled()) {
		dest_offset = (dest_offset + Point2(0.5, 0.5)).floor();
	}

	r_dst_rect = Rect2(dest_offset, frame_size);

	if (is_flipped_h()) {
		r_dst_rect.size.x = -r_dst_rect.size.x;
	}
	if (is_flipped_v()) {
		r_dst_rect.size.y = -r_dst_rect.size.y;
	}
}

Variant Mode7Sprite2D::mode7_transform_point(const Vector2 &p_point, bool p_visible_area_only) const {
	// CPU-side inverse of the Mode7 fragment shader's per-pixel sampling math.
	//
	// FORWARD (the shader), per dest pixel v in the drawn region quad (region-local [0,1]^2):
	//     uv      = M(r)*(v - p(r)) + p(r) + o(r)      // per-row affine, r = v.y
	//     uv      = G*(uv - gp) + gp + G*go            // global rotation about gp, then offset
	//     uv_full = uv*R.zw + R.xy                     // denormalize to full-texture UV
	//     sampled = tiling ? fract(uv_full) : uv_full  // R = REGION_RECT (normalized)
	//     (discarded if !tiling && uv outside [0,1]^2; horizon-masked alpha per v)
	//
	// INVERSE (this function): given a target texel T (full-texture UV of the input point),
	// find the dest pixel v where the sprite displays T. Everything is exact algebra except the
	// row r = v.y, which appears both as the self-consistency condition and inside M(r), p(r),
	// o(r). That leaves a single 1-D equation H(r) = v(r).y - r = 0 (r in [0,1]), solved by a
	// deterministic grid scan + local refinement. This is robust across NONE / LERP (any number
	// of overrides) / PROJECTION and needs no global-monotonicity assumption (the old single
	// bisection assumed one sign change and fell back to a wrong endpoint, causing the
	// "spiral"/"jump" with LERP/NONE and region+projection).
	//
	// With tiling, fract(T(v)) = fract(P) has candidates T = P + n (n integer). Each candidate has
	// at most one valid dest (a self-consistent root that is NOT a degenerate 180-degree fold
	// and that lies inside the quad). The correct one is chosen by the candidate n whose forward
	// map actually equals P + n (a validity gate that eliminates spurious roots), preferring the n
	// nearest the region -- i.e. the tile the input point belongs to. This is a deterministic
	// selection based on where the input point is, not a guess, and does not use visibility.
	//
	// Returns the dest position in the same (parent-local) space p_point was given in, or
	// null when p_visible_area_only is true and the point has no visible (drawn, unmasked)
	// destination.

	// Mode 7 is off, so the sprite draws unwarped and the point does not move.
	if (!mode7_enabled) {
		return p_point;
	}

	Ref<Texture2D> tex = get_texture();
	if (tex.is_null()) {
		return p_point;
	}
	Vector2 tex_size = tex->get_size();
	if (tex_size.x <= 0.0f || tex_size.y <= 0.0f) {
		return p_point;
	}

	// --- 1) p_point (parent-local, a point on the undistorted source artwork)
	//     -> local drawing space -> full-texture UV. Uses the full, uncropped
	//     virtual rects so points outside the visible region crop still map to
	//     the correct texel. (Same as before.) ---
	Rect2 full_src_rect, full_dst_rect;
	_mode7_get_full_rects(full_src_rect, full_dst_rect);

	Vector2 local_point = get_transform().affine_inverse().xform(p_point);
	Vector2 tex_point = full_src_rect.position + (local_point - full_dst_rect.position) * (full_src_rect.size / full_dst_rect.size);
	Vector2 source_full_uv = tex_point / tex_size;

	// --- 2) Active region in normalized full-texture UV (no region == whole texture).
	Rect2 region_px = is_region_enabled() ? get_region_rect() : Rect2(Vector2(), tex_size);
	if (region_px.size.x == 0.0f || region_px.size.y == 0.0f) {
		region_px.size = tex_size;
	}
	Rect2 R(region_px.position / tex_size, region_px.size / tex_size); // REGION_RECT

	// Region aspect for the aspect-corrected global rotation (mirrors the shader).
	real_t region_aspect = 1.0f;
	if (mode7_override_region_aspect && region_px.size.y != 0.0f) {
		region_aspect = region_px.size.x / region_px.size.y;
	}

	// --- 3) The GLOBAL step (rotation about gp, then offset) as one affine map on
	//     region-local uv: v_global(u) = G*u + d.
	Transform2D G = _mode7_aspect_rotate(mode7_global_rotation, region_aspect);
	Vector2 gp = mode7_global_pivot;
	Vector2 go = mode7_global_offset;
	Transform2D G_inv = G.affine_inverse();
	Vector2 d = gp - G.basis_xform(gp) + G.basis_xform(go); // so v_global(u) = G*u + d

	// Per-row inverse solve for a single candidate row dy: given the "after-per-row"
	// target u1 (region-local), find dest v such that v_global(M(r)(v-p(r))+p(r)+o(r)) = u1_global.
	// v = M(r)^-1 * (u1 - p(r) - o(r)) + p(r).
	auto solve_dest_for_v = [&](const Vector2 &p_u1, real_t p_dy, Vector2 &r_dest, bool &r_ok) {
		Transform2D scan_transform;
		Vector2 pivot;
		Color unused_modulate;
		_mode7_compute_scanline_data(p_dy, scan_transform, pivot, unused_modulate);

		Vector2 scan_offset = scan_transform.columns[2];
		Transform2D matrix_transformed(scan_transform.columns[0], scan_transform.columns[1], Vector2());
		if (Math::is_zero_approx(matrix_transformed.determinant())) {
			// Degenerate row (zero scale / collinear columns): no inverse, no valid dest through it.
			r_dest = Vector2(p_u1.x, p_dy);
			r_ok = false;
			return;
		}
		Transform2D matrix_transformed_inv = matrix_transformed.affine_inverse();
		r_dest = matrix_transformed_inv.basis_xform(p_u1 - pivot - scan_offset) + pivot;
		r_ok = true;
	};

	// Returns true if the forward map of dest (at row root_row) actually equals target T,
	// i.e. dest is a genuine preimage of T (not a degenerate fold root). With tiling the
	// equality is modulo 1 (the shader wraps with fract), so the difference may be small
	// or ~1 (across a tile boundary).
	auto forward_maps_to = [&](const Vector2 &p_dest, real_t p_root_row, const Vector2 &p_T) -> bool {
		Transform2D scan_transform;
		Vector2 pivot;
		Color unused_modulate;
		_mode7_compute_scanline_data(p_root_row, scan_transform, pivot, unused_modulate);
		Vector2 fwd = G.basis_xform(scan_transform.basis_xform(p_dest - pivot) + pivot + scan_transform.columns[2] - gp) + gp + G.basis_xform(go);
		Vector2 fwd_full = Vector2(fwd.x * R.size.x + R.position.x, fwd.y * R.size.y + R.position.y);
		real_t dx = Math::abs(fwd_full.x - p_T.x);
		real_t dy = Math::abs(fwd_full.y - p_T.y);
		real_t tol = 0.02f;
		if (mode7_tiling) {
			dx = Math::abs(dx - Math::round(dx)); // distance to the nearest integer
			dx = Math::abs(dx - Math::round(dx));
		}
		return dx <= tol && dy <= tol;
	};

	// Resolves the dest v for one full-texture target T, or the zero vector if none is valid.
	//  - u1 = inverse-global of T (row independent).
	//  - find ALL self-consistent rows r of H(r) = v(r).y - r on [0,1] (grid scan of sign
	//    changes, then bisection-refine each bracket). There can be more than one for LERP/NONE.
	//  - keep the first root whose forward map actually equals T (forward_maps_to): this
	//    eliminates degenerate 180-degree-fold roots that are self-consistent (v.y == r) but
	//    map to the antipodal texel rather than T, and disambiguates multiple roots.
	auto resolve_dest_for = [&](const Vector2 &p_T, bool &r_found) -> Vector2 {
		r_found = false;
		// Inverse of the global step: u2 = (T - R.xy)/R.zw ; u1 = G^-1*(u2 - d).
		Vector2 u2 = Vector2((p_T.x - R.position.x) / R.size.x, (p_T.y - R.position.y) / R.size.y);
		Vector2 u1 = G_inv.basis_xform(u2 - d);

		const int N = 256; // scan resolution across the row interval [0,1]
		int prev_sign = 0;
		int prev_i = -1;
		real_t prev_absH = 1e30f;
		Vector2 best_touch;
		bool have_touch = false;

		for (int i = 0; i <= N; i++) {
			real_t row = (real_t)i / (real_t)N;
			Vector2 v_i;
			bool ok_i = false;
			solve_dest_for_v(u1, row, v_i, ok_i);
			if (!ok_i) {
				prev_sign = 0; // a degenerate row breaks any sign-change bracket
				prev_i = -1;
				continue;
			}
			real_t H_i = v_i.y - row;
			real_t aH = Math::abs(H_i);
			if (aH < 1e-3f && aH < prev_absH) {
				best_touch = v_i;
				have_touch = true;
				prev_absH = aH;
			}
			int s_i = (H_i > 0.0f) ? 1 : (H_i < 0.0f ? -1 : 0);
			if (prev_sign != 0 && s_i != 0 && s_i != prev_sign && prev_i >= 0) {
				// A genuine root is bracketed in (prev_i, i]: refine it.
				real_t r_lo = (real_t)prev_i / (real_t)N;
				real_t r_hi = (real_t)i / (real_t)N;
				Vector2 v_lo;
				bool ok_lo = false;
				solve_dest_for_v(u1, r_lo, v_lo, ok_lo);
				for (int it = 0; it < 40; it++) {
					real_t mid = (r_lo + r_hi) * 0.5f;
					Vector2 v_mid;
					bool ok_mid = false;
					solve_dest_for_v(u1, mid, v_mid, ok_mid);
					if (!ok_mid) {
						break;
					}
					real_t H_lo = (ok_lo ? v_lo.y : 0.0f) - r_lo;
					real_t H_mid = v_mid.y - mid;
					if (H_lo * H_mid <= 0.0f) {
						r_hi = mid;
					} else {
						r_lo = mid;
						v_lo = v_mid;
						ok_lo = true;
					}
				}
				real_t root_row = (r_lo + r_hi) * 0.5f;
				bool ok_root = false;
				Vector2 dest;
				solve_dest_for_v(u1, root_row, dest, ok_root);
				if (ok_root && dest.x >= 0.0f && dest.x <= 1.0f && dest.y >= 0.0f && dest.y <= 1.0f) {
					// Validity gate: the forward map of this dest must equal the target T.
					if (forward_maps_to(dest, root_row, p_T)) {
						r_found = true;
						return dest;
					}
				}
			}
			prev_sign = s_i;
			prev_i = i;
			prev_absH = aH;
		}

		// No sign-change root (or none passed the gate): fall back to a near-touch of H=0
		// (e.g. a fold), if it is inside the quad and maps to T.
		if (have_touch) {
			Vector2 dest = best_touch;
			if (dest.x >= 0.0f && dest.x <= 1.0f && dest.y >= 0.0f && dest.y <= 1.0f) {
				if (forward_maps_to(dest, dest.y, p_T)) {
					r_found = true;
					return dest;
				}
			}
		}
		return Vector2(); // this target is not reached by the forward map at a valid dest
	};

	// --- 4) Resolve the destination for the input point.
	//     The input point source_full_uv is already an exact location in the infinitely-
	//     tilled texture plane: its integer part encodes WHICH tile copy it is in, and its
	//     fractional part encodes WHERE within that copy. resolve_dest_for() inverts the
	//     full forward map (per-row affine, then global step, then the tiling wrap) for
	//     that exact target, so the destination it returns already lands on the correct
	//     tile. There is no ambiguity to resolve here and no need to re-anchor the
	//     candidate on the region center -- doing so (as before) would pick a different
	//     tile copy than the one the input point actually belongs to.
	Vector2 dest;
	bool have_dest = false;

	bool found = false;
	Vector2 dv = resolve_dest_for(source_full_uv, found);
	if (found) {
		dest = dv;
		have_dest = true;
	}

	// --- 5) dest (region-local [0,1]^2) -> full-texture UV -> local drawing space ->
	//     parent-local space. Uses the REAL, region-cropped rects (what the renderer draws).
	Rect2 cropped_src_rect, cropped_dst_rect;
	bool unused_filter_clip = false;
	_get_rects(cropped_src_rect, cropped_dst_rect, unused_filter_clip);

	if (cropped_src_rect.size.x == 0.0f || cropped_src_rect.size.y == 0.0f) {
		// Degenerate region: no quad to map into.
		if (p_visible_area_only) {
			return Variant();
		}
		return p_point;
	}

	Vector2 dest_full_uv = Vector2(dest.x * R.size.x + R.position.x, dest.y * R.size.y + R.position.y);
	Vector2 tex_point_out = dest_full_uv * tex_size;
	Vector2 local_point_out = cropped_dst_rect.position + (tex_point_out - cropped_src_rect.position) * (cropped_dst_rect.size / cropped_src_rect.size);
	Vector2 result = get_transform().xform(local_point_out);

	// visible_area_only: return null unless the point has a real destination that is
	// actually drawn (inside the region quad, i.e. have_dest is set). The point's
	// position itself is always the correct (deterministic) transformed location.
	if (p_visible_area_only && !have_dest) {
		return Variant();
	}
	return result;
}

void Mode7Sprite2D::set_mode7_tiling(bool p_tiling) {
	if (mode7_tiling == p_tiling) {
		return;
	}
	mode7_tiling = p_tiling;
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_tiling", p_tiling);
		RS::get_singleton()->canvas_item_set_default_texture_repeat(
				get_canvas_item(),
				mode7_tiling ? RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED
							 : RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
		queue_redraw();
	}
}

bool Mode7Sprite2D::is_mode7_tiling() const {
	return mode7_tiling;
}

void Mode7Sprite2D::set_mode7_global_rotation(real_t p_degrees) {
	const real_t radians = Math::deg_to_rad(p_degrees);
	if (mode7_global_rotation == radians) {
		return;
	}
	mode7_global_rotation = radians;
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_global_rotation", radians);
		queue_redraw();
	}
}

real_t Mode7Sprite2D::get_mode7_global_rotation() const {
	return Math::rad_to_deg(mode7_global_rotation);
}

void Mode7Sprite2D::set_mode7_global_pivot(const Vector2 &p_pivot) {
	if (mode7_global_pivot == p_pivot) {
		return;
	}
	mode7_global_pivot = p_pivot;
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_global_pivot", p_pivot);
		queue_redraw();
	}
}

Vector2 Mode7Sprite2D::get_mode7_global_pivot() const {
	return mode7_global_pivot;
}

void Mode7Sprite2D::set_mode7_global_offset(const Vector2 &p_offset) {
	if (mode7_global_offset == p_offset) {
		return;
	}
	mode7_global_offset = p_offset;
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_global_offset", p_offset);
		queue_redraw();
	}
}

Vector2 Mode7Sprite2D::get_mode7_global_offset() const {
	return mode7_global_offset;
}

void Mode7Sprite2D::set_mode7_override_region_aspect(bool p_enabled) {
	if (mode7_override_region_aspect == p_enabled) {
		return;
	}
	mode7_override_region_aspect = p_enabled;
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_override_region_aspect", p_enabled);
		queue_redraw();
	}
}

bool Mode7Sprite2D::is_mode7_override_region_aspect() const {
	return mode7_override_region_aspect;
}

void Mode7Sprite2D::set_mode7_top_horizon_mask_amount(real_t p_amount) {
	if (mode7_top_horizon_mask_amount == p_amount) {
		return;
	}
	mode7_top_horizon_mask_amount = CLAMP(p_amount, 0.0f, 1.0f);
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_top_horizon_mask_amount", mode7_top_horizon_mask_amount);
		queue_redraw();
	}
}

real_t Mode7Sprite2D::get_mode7_top_horizon_mask_amount() const {
	return mode7_top_horizon_mask_amount;
}

void Mode7Sprite2D::set_mode7_top_horizon_tilt(real_t p_degrees) {
	const real_t radians = Math::deg_to_rad(p_degrees);
	if (mode7_top_horizon_tilt == radians) {
		return;
	}
	mode7_top_horizon_tilt = radians;
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_top_horizon_tilt", radians);
		queue_redraw();
	}
}

real_t Mode7Sprite2D::get_mode7_top_horizon_tilt() const {
	return Math::rad_to_deg(mode7_top_horizon_tilt);
}

void Mode7Sprite2D::set_mode7_bottom_horizon_mask_amount(real_t p_amount) {
	if (mode7_bottom_horizon_mask_amount == p_amount) {
		return;
	}
	mode7_bottom_horizon_mask_amount = CLAMP(p_amount, 0.0f, 1.0f);
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_bottom_horizon_mask_amount", mode7_bottom_horizon_mask_amount);
		queue_redraw();
	}
}

real_t Mode7Sprite2D::get_mode7_bottom_horizon_mask_amount() const {
	return mode7_bottom_horizon_mask_amount;
}

void Mode7Sprite2D::set_mode7_bottom_horizon_tilt(real_t p_degrees) {
	const real_t radians = Math::deg_to_rad(p_degrees);
	if (mode7_bottom_horizon_tilt == radians) {
		return;
	}
	mode7_bottom_horizon_tilt = radians;
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_bottom_horizon_tilt", radians);
		queue_redraw();
	}
}

real_t Mode7Sprite2D::get_mode7_bottom_horizon_tilt() const {
	return Math::rad_to_deg(mode7_bottom_horizon_tilt);
}

// ── Projection perspective tuning ──────────────────────────────────────────
// All four are consumed by _mode7_rebuild_scanline_texture() when the
// INTERPOLATION_PROJECTION branch is active, so changing any of them
// rebuilds the scanline table (cheap: 1024 rows of scalar math).

void Mode7Sprite2D::_mode7_refresh_projection_table() {
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_rebuild_scanline_texture();
		if (_mode7_scanline_tex.is_valid()) {
			_mode7_material->set_shader_parameter("mode7_scanline_table", _mode7_scanline_tex);
		}
		queue_redraw();
	}
}

void Mode7Sprite2D::set_mode7_projection_gamma(real_t p_value) {
	p_value = CLAMP(p_value, CMP_EPSILON, 10.0f);
	if (mode7_projection_gamma == p_value) {
		return;
	}
	mode7_projection_gamma = p_value;
	_mode7_refresh_projection_table();
}

real_t Mode7Sprite2D::get_mode7_projection_gamma() const {
	return mode7_projection_gamma;
}

void Mode7Sprite2D::set_mode7_projection_strength(real_t p_value) {
	p_value = CLAMP(p_value, 0.0f, 1.0f);
	if (mode7_projection_strength == p_value) {
		return;
	}
	mode7_projection_strength = p_value;
	_mode7_refresh_projection_table();
}

real_t Mode7Sprite2D::get_mode7_projection_strength() const {
	return mode7_projection_strength;
}

void Mode7Sprite2D::set_mode7_projection_aspect_ratio(real_t p_value) {
	// Reject 0.0: Sx = S * aspect_ratio would be 0, zeroing the matrix's first
	// column and collapsing sampled uv.x for every scanline (singular matrix).
	p_value = CLAMP(p_value, CMP_EPSILON, 2.0f);
	if (mode7_projection_aspect_ratio == p_value) {
		return;
	}
	mode7_projection_aspect_ratio = p_value;
	_mode7_refresh_projection_table();
}

real_t Mode7Sprite2D::get_mode7_projection_aspect_ratio() const {
	return mode7_projection_aspect_ratio;
}

void Mode7Sprite2D::set_mode7_projection_pixel_aspect(real_t p_value) {
	p_value = CLAMP(p_value, 0.875f, 1.125f);
	if (mode7_projection_pixel_aspect == p_value) {
		return;
	}
	mode7_projection_pixel_aspect = p_value;
	_mode7_refresh_projection_table();
}

real_t Mode7Sprite2D::get_mode7_projection_pixel_aspect() const {
	return mode7_projection_pixel_aspect;
}

void Mode7Sprite2D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "material" && mode7_enabled) {
		// The active material is always regenerated from mode7_* properties
		// via _mode7_rebuild_material(); never persist the generated
		// ShaderMaterial as if it were the user's original material.
		p_property.usage &= ~PROPERTY_USAGE_STORAGE;
	}

	// The projection tuning parameters only affect the scanline-table math in
	// INTERPOLATION_PROJECTION mode, so lock them when any other mode is active.
	// (Mirrors the Mode7ScanlineOverride::_validate_property pattern for skew.)
	if (mode7_interpolation != INTERPOLATION_PROJECTION) {
		const StringName &name = p_property.name;
		if (name == "mode7_projection_gamma" ||
				name == "mode7_projection_strength" ||
				name == "mode7_projection_aspect_ratio" ||
				name == "mode7_projection_pixel_aspect") {
			p_property.usage |= PROPERTY_USAGE_READ_ONLY;
		}
	}
}

String Mode7Sprite2D::_get_property_warning(const StringName &p_name) const {
	if (p_name == "mode7_scanline_overrides" &&
			mode7_interpolation == INTERPOLATION_PROJECTION &&
			mode7_scanline_overrides.size() > 2) {
		return "PROJECTION interpolation only uses the first and last scanline overrides as anchors; any extra entries are ignored.";
	}
	return String();
}

void Mode7Sprite2D::set_mode7_enabled(bool p_enabled) {
	if (mode7_enabled == p_enabled) {
		return;
	}
	mode7_enabled = p_enabled;
	if (mode7_enabled) {
		// On initialization, we want 1 scanline override.  The user can add more, which allows interpolating between
		// them.  This way, we don't need to do it, literally, like the Super Nintendo and specify each "scanline" individually.
		// However, the scanline override is where the affine matrix, scale, skew, etc... options live, so even if
		// doing the per-scanline transformation is not desired, a single object here will expose those parameters to be manipulated.
		if (mode7_scanline_overrides.is_empty()) {
			Ref<Mode7ScanlineOverride> def;
			def.instantiate();
			def->set_owner_mode7_sprite(this);
			mode7_scanline_overrides.append(def);

			// Cast through Resource to avoid non-virtual inherited method resolution issues.
			Ref<Resource> res = def;
			res->connect_changed(
					callable_mp(this, &Mode7Sprite2D::_on_mode7_override_changed),
					CONNECT_REFERENCE_COUNTED);
		}

		_saved_material = get_material();
		_mode7_rebuild_material();
		set_material(_mode7_material); // only here, once

		// Save the node's current repeat mode so we can restore it on disable,
		// then push the mode7 repeat state to both the sprite's own repeat
		// (so the Sprite2D texture uses the correct GL sampler) and the canvas
		// item default (fallback for custom shader TEXTURE samplers).
		_saved_texture_repeat = (RS::CanvasItemTextureRepeat)get_texture_repeat();
		RS::get_singleton()->canvas_item_set_default_texture_repeat(
				get_canvas_item(),
				mode7_tiling ? RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED
							 : RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	} // if (mode7_enabled)
	else {
		set_material(_saved_material);
		_saved_material = Ref<Material>();

		// Restore whatever repeat mode the node had before mode7 was enabled.
		RS::get_singleton()->canvas_item_set_default_texture_repeat(
				get_canvas_item(), _saved_texture_repeat);
	}
	notify_property_list_changed();
	queue_redraw();
}

bool Mode7Sprite2D::is_mode7_enabled() const {
	return mode7_enabled;
}

void Mode7Sprite2D::set_mode7_interpolation(Mode7InterpolationMode p_mode) {
	if (mode7_interpolation == p_mode) {
		return;
	}
	mode7_interpolation = p_mode;
	for (int i = 0; i < mode7_scanline_overrides.size(); i++) {
		Ref<Mode7ScanlineOverride> entry = mode7_scanline_overrides[i];
		if (entry.is_valid()) {
			entry->notify_property_list_changed();
		}
	}
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_rebuild_material();
	}
	// The projection tuning properties are read-only unless interpolation is
	// PROJECTION, so the editor must re-run _validate_property when the mode
	// changes (otherwise the lock state from the previous mode is cached).
	notify_property_list_changed();
	queue_redraw();
}

Mode7Sprite2D::Mode7InterpolationMode Mode7Sprite2D::get_mode7_interpolation() const {
	return mode7_interpolation;
}

void Mode7Sprite2D::set_mode7_scanline_overrides(const TypedArray<Mode7ScanlineOverride> &p_overrides) {
	// Disconnect from all existing override resources.
	for (int i = 0; i < mode7_scanline_overrides.size(); i++) {
		Ref<Mode7ScanlineOverride> entry = mode7_scanline_overrides[i];
		if (entry.is_valid()) {
			// Detach the owner (entries still present in the new array are reattached below).
			entry->set_owner_mode7_sprite(nullptr);
			Ref<Resource> res = entry;
			res->disconnect_changed(callable_mp(this, &Mode7Sprite2D::_on_mode7_override_changed));
		}
	}

	mode7_scanline_overrides = p_overrides;

	// Connect to all new override resources.
	for (int i = 0; i < mode7_scanline_overrides.size(); i++) {
		Ref<Mode7ScanlineOverride> entry = mode7_scanline_overrides[i];
		if (entry.is_valid()) {
			entry->set_owner_mode7_sprite(this);
			Ref<Resource> res = entry;
			res->connect_changed(
					callable_mp(this, &Mode7Sprite2D::_on_mode7_override_changed),
					CONNECT_REFERENCE_COUNTED);
		}
	}

	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_rebuild_material();
	}
}

void Mode7Sprite2D::_on_mode7_override_changed() {
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_rebuild_material();
	}
}

TypedArray<Mode7ScanlineOverride> Mode7Sprite2D::get_mode7_scanline_overrides() const {
	return mode7_scanline_overrides;
}

// ── Region follow target ───────────────────────────────────────────────

void Mode7Sprite2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_follow_cache();
			if (mode7_follow_cache.is_valid()) {
				set_physics_process(true);
				mode7_follow_physics_active = true;
			} else {
				set_physics_process(false);
				mode7_follow_physics_active = false;
			}
			mode7_follow_initialized = false;
		} break;

		case NOTIFICATION_EXIT_TREE: {
			mode7_follow_cache = ObjectID();
			mode7_follow_initialized = false;
			mode7_follow_physics_active = false;
			set_physics_process(false);
		} break;

		case NOTIFICATION_PHYSICS_PROCESS: {
			// Resolve the live node from the ObjectID cache each frame.
			Node2D *target_2d = ObjectDB::get_instance<Node2D>(mode7_follow_cache);
			if (!target_2d) {
				// The target was freed or the path no longer resolves to a Node2D.
				mode7_follow_cache = ObjectID();
				set_physics_process(false);
				mode7_follow_physics_active = false;
				return;
			} else if (!is_inside_tree()) {
				// This node itself is not in the tree. NOTIFICATION_EXIT_TREE already
				// resets mode7_follow_cache/mode7_follow_initialized/mode7_follow_physics_active
				// and disarms physics_process unconditionally, so don't duplicate or
				// race with that logic here — just bail out for this frame.
				return;
			} else if (!target_2d->is_inside_tree()) {
				// Only the target is temporarily out of the tree (e.g. being
				// re-parented). This is recoverable, so keep polling instead of
				// disarming physics processing — otherwise follow never resumes
				// once the target re-enters the tree. Force a re-snap once it's
				// back, so the region doesn't jump on resume.
				mode7_follow_initialized = false;
				return;
			} else if (!is_region_enabled()) {
				// Skip the update while the region is disabled, but keep
				// physics processing so follow resumes automatically.
				return;
			}

			Vector2 target_global_pos = target_2d->get_global_position();

			if (!mode7_follow_initialized) {
				mode7_follow_initialized = true;
				return;
			}

			Rect2 rr = get_region_rect();

			Vector2 half_size = rr.size * 0.5f;
			const Transform2D global_xform = get_global_transform();
			if (Math::is_zero_approx(global_xform.determinant())) {
				return;
			}
			Vector2 pivot_in_sprite_local = global_xform.affine_inverse().xform(target_global_pos);

			rr.position.x = pivot_in_sprite_local.x - half_size.x;
			rr.position.y = pivot_in_sprite_local.y - half_size.y;
			set_region_rect(rr);
		} break;
	}
}

void Mode7Sprite2D::set_mode7_region_follow_target(const NodePath &p_path) {
	if (mode7_region_follow_target == p_path) {
		return;
	}
	mode7_region_follow_target = p_path;

	if (is_inside_tree()) {
		_update_follow_cache();
		// Deferred call is the rename safety net: when the Scene dock renames
		// the target node, the property rewrite is queued *before* set_name()
		// fires, so a synchronous lookup would still use the old name.  If the
		// immediate update already found the node, the deferred call is a no-op.
		// Route through _ensure_follow_physics so that if the deferred lookup
		// is what finally resolves the target, physics processing is started
		// there too (a valid cache must enable physics, or follow never starts).
		callable_mp(this, &Mode7Sprite2D::_ensure_follow_physics).call_deferred();
		if (mode7_follow_cache.is_valid()) {
			set_physics_process(true);
			mode7_follow_physics_active = true;
		}

		update_configuration_warnings();
	} else if (!p_path.is_empty()) {
		// Node not in tree yet — defer starting physics too, until we're ready.
		callable_mp(this, &Mode7Sprite2D::_ensure_follow_physics).call_deferred();
	}
}

void Mode7Sprite2D::_ensure_follow_physics() {
	_update_follow_cache();
	if (is_inside_tree() && mode7_follow_cache.is_valid()) {
		set_physics_process(true);
		mode7_follow_physics_active = true;
	}
}

NodePath Mode7Sprite2D::get_mode7_region_follow_target() const {
	return mode7_region_follow_target;
}

void Mode7Sprite2D::_update_follow_cache() {
	mode7_follow_cache = ObjectID();
	if (has_node(mode7_region_follow_target)) {
		Node2D *node = Object::cast_to<Node2D>(get_node(mode7_region_follow_target));
		if (node && this != node) {
			// Only reject self; ancestors/descendants are allowed because we only read
			// the target's position (unlike RemoteTransform2D which writes back to it).
			mode7_follow_cache = node->get_instance_id();
		}
	}
}

void Mode7Sprite2D::force_update_follow_cache() {
	_ensure_follow_physics();
}

void Mode7Sprite2D::_bind_methods() {
	// Global bindings ---------------------------------------------------------
	BIND_ENUM_CONSTANT(INTERPOLATION_NONE);
	BIND_ENUM_CONSTANT(INTERPOLATION_LERP);
	BIND_ENUM_CONSTANT(INTERPOLATION_PROJECTION);

	ClassDB::bind_method(D_METHOD("_get_property_warning", "name"), &Mode7Sprite2D::_get_property_warning);

	ClassDB::bind_method(D_METHOD("set_mode7_enabled", "enabled"), &Mode7Sprite2D::set_mode7_enabled);
	ClassDB::bind_method(D_METHOD("is_mode7_enabled"), &Mode7Sprite2D::is_mode7_enabled);

	ClassDB::bind_method(D_METHOD("set_mode7_interpolation", "mode"), &Mode7Sprite2D::set_mode7_interpolation);
	ClassDB::bind_method(D_METHOD("get_mode7_interpolation"), &Mode7Sprite2D::get_mode7_interpolation);

	ClassDB::bind_method(D_METHOD("set_mode7_scanline_overrides", "overrides"), &Mode7Sprite2D::set_mode7_scanline_overrides);
	ClassDB::bind_method(D_METHOD("get_mode7_scanline_overrides"), &Mode7Sprite2D::get_mode7_scanline_overrides);

	ClassDB::bind_method(D_METHOD("set_mode7_tiling", "tiling"), &Mode7Sprite2D::set_mode7_tiling);
	ClassDB::bind_method(D_METHOD("is_mode7_tiling"), &Mode7Sprite2D::is_mode7_tiling);

	ClassDB::bind_method(D_METHOD("set_mode7_global_rotation", "degrees"), &Mode7Sprite2D::set_mode7_global_rotation);
	ClassDB::bind_method(D_METHOD("get_mode7_global_rotation"), &Mode7Sprite2D::get_mode7_global_rotation);
	ClassDB::bind_method(D_METHOD("set_mode7_global_pivot", "pivot"), &Mode7Sprite2D::set_mode7_global_pivot);
	ClassDB::bind_method(D_METHOD("get_mode7_global_pivot"), &Mode7Sprite2D::get_mode7_global_pivot);
	ClassDB::bind_method(D_METHOD("set_mode7_global_offset", "offset"), &Mode7Sprite2D::set_mode7_global_offset);
	ClassDB::bind_method(D_METHOD("get_mode7_global_offset"), &Mode7Sprite2D::get_mode7_global_offset);

	ClassDB::bind_method(D_METHOD("set_mode7_override_region_aspect", "enabled"), &Mode7Sprite2D::set_mode7_override_region_aspect);
	ClassDB::bind_method(D_METHOD("is_mode7_override_region_aspect"), &Mode7Sprite2D::is_mode7_override_region_aspect);

	ClassDB::bind_method(D_METHOD("set_mode7_region_follow_target", "path"), &Mode7Sprite2D::set_mode7_region_follow_target);
	ClassDB::bind_method(D_METHOD("get_mode7_region_follow_target"), &Mode7Sprite2D::get_mode7_region_follow_target);
	ClassDB::bind_method(D_METHOD("force_update_follow_cache"), &Mode7Sprite2D::force_update_follow_cache);

	// Top horizon mask
	ClassDB::bind_method(D_METHOD("set_mode7_top_horizon_mask_amount", "amount"), &Mode7Sprite2D::set_mode7_top_horizon_mask_amount);
	ClassDB::bind_method(D_METHOD("get_mode7_top_horizon_mask_amount"), &Mode7Sprite2D::get_mode7_top_horizon_mask_amount);
	ClassDB::bind_method(D_METHOD("set_mode7_top_horizon_tilt", "degrees"), &Mode7Sprite2D::set_mode7_top_horizon_tilt);
	ClassDB::bind_method(D_METHOD("get_mode7_top_horizon_tilt"), &Mode7Sprite2D::get_mode7_top_horizon_tilt);

	// Bottom horizon mask
	ClassDB::bind_method(D_METHOD("set_mode7_bottom_horizon_mask_amount", "amount"), &Mode7Sprite2D::set_mode7_bottom_horizon_mask_amount);
	ClassDB::bind_method(D_METHOD("get_mode7_bottom_horizon_mask_amount"), &Mode7Sprite2D::get_mode7_bottom_horizon_mask_amount);
	ClassDB::bind_method(D_METHOD("set_mode7_bottom_horizon_tilt", "degrees"), &Mode7Sprite2D::set_mode7_bottom_horizon_tilt);
	ClassDB::bind_method(D_METHOD("get_mode7_bottom_horizon_tilt"), &Mode7Sprite2D::get_mode7_bottom_horizon_tilt);

	// Projection perspective tuning
	ClassDB::bind_method(D_METHOD("set_mode7_projection_gamma", "value"), &Mode7Sprite2D::set_mode7_projection_gamma);
	ClassDB::bind_method(D_METHOD("get_mode7_projection_gamma"), &Mode7Sprite2D::get_mode7_projection_gamma);
	ClassDB::bind_method(D_METHOD("set_mode7_projection_strength", "value"), &Mode7Sprite2D::set_mode7_projection_strength);
	ClassDB::bind_method(D_METHOD("get_mode7_projection_strength"), &Mode7Sprite2D::get_mode7_projection_strength);
	ClassDB::bind_method(D_METHOD("set_mode7_projection_aspect_ratio", "value"), &Mode7Sprite2D::set_mode7_projection_aspect_ratio);
	ClassDB::bind_method(D_METHOD("get_mode7_projection_aspect_ratio"), &Mode7Sprite2D::get_mode7_projection_aspect_ratio);
	ClassDB::bind_method(D_METHOD("set_mode7_projection_pixel_aspect", "value"), &Mode7Sprite2D::set_mode7_projection_pixel_aspect);
	ClassDB::bind_method(D_METHOD("get_mode7_projection_pixel_aspect"), &Mode7Sprite2D::get_mode7_projection_pixel_aspect);

	ClassDB::bind_method(D_METHOD("set_mode7_saved_material", "material"), &Mode7Sprite2D::set_mode7_saved_material);
	ClassDB::bind_method(D_METHOD("get_mode7_saved_material"), &Mode7Sprite2D::get_mode7_saved_material);
	ClassDB::bind_method(D_METHOD("mode7_transform_point", "point", "visible_area_only"), &Mode7Sprite2D::mode7_transform_point, DEFVAL(false));

	// Properties (exposed in the Inspector) -----------------------------------

	// Global (un-grouped)
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mode7_enabled", PROPERTY_HINT_GROUP_ENABLE), "set_mode7_enabled", "is_mode7_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode7_interpolation", PROPERTY_HINT_ENUM,
						 "None,Lerp,Projection"),
			"set_mode7_interpolation", "get_mode7_interpolation");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "mode7_scanline_overrides",
						 PROPERTY_HINT_ARRAY_TYPE, "Mode7ScanlineOverride"),
			"set_mode7_scanline_overrides", "get_mode7_scanline_overrides");

	// Global Parameters group
	ADD_GROUP("Global Parameters", "mode7_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mode7_tiling"), "set_mode7_tiling", "is_mode7_tiling");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_global_rotation",
						 PROPERTY_HINT_RANGE, "-360,360,0.1"),
			"set_mode7_global_rotation", "get_mode7_global_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "mode7_global_pivot"),
			"set_mode7_global_pivot", "get_mode7_global_pivot");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "mode7_global_offset"),
			"set_mode7_global_offset", "get_mode7_global_offset");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "mode7_region_follow_target",
						 PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node2D"),
			"set_mode7_region_follow_target", "get_mode7_region_follow_target");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mode7_override_region_aspect"),
			"set_mode7_override_region_aspect", "is_mode7_override_region_aspect");

	// Horizon group
	ADD_GROUP("Horizon", "mode7_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_top_horizon_mask_amount",
						 PROPERTY_HINT_RANGE, "0,1,0.001"),
			"set_mode7_top_horizon_mask_amount", "get_mode7_top_horizon_mask_amount");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_top_horizon_tilt",
						 PROPERTY_HINT_RANGE, "-360,360,0.1"),
			"set_mode7_top_horizon_tilt", "get_mode7_top_horizon_tilt");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_bottom_horizon_mask_amount",
						 PROPERTY_HINT_RANGE, "0,1,0.001"),
			"set_mode7_bottom_horizon_mask_amount", "get_mode7_bottom_horizon_mask_amount");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_bottom_horizon_tilt",
						 PROPERTY_HINT_RANGE, "-360,360,0.1"),
			"set_mode7_bottom_horizon_tilt", "get_mode7_bottom_horizon_tilt");

	// Projection group (only active when mode7_interpolation is Projection)
	ADD_GROUP("Projection", "mode7_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_projection_gamma",
						 PROPERTY_HINT_RANGE, "0.00001,10.0,0.001"),
			"set_mode7_projection_gamma", "get_mode7_projection_gamma");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_projection_strength",
						 PROPERTY_HINT_RANGE, "0,1,0.001"),
			"set_mode7_projection_strength", "get_mode7_projection_strength");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_projection_aspect_ratio",
						 PROPERTY_HINT_RANGE, "0.00001,2,0.001"),
			"set_mode7_projection_aspect_ratio", "get_mode7_projection_aspect_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_projection_pixel_aspect",
						 PROPERTY_HINT_RANGE, "0.875,1.125,0.001"),
			"set_mode7_projection_pixel_aspect", "get_mode7_projection_pixel_aspect");

	// Internal - for persistence
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mode7_saved_material", PROPERTY_HINT_RESOURCE_TYPE, "Material", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_NO_EDITOR), "set_mode7_saved_material", "get_mode7_saved_material");
}

Mode7Sprite2D::Mode7Sprite2D() {
}

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
	// (Vertical) Resolution for smooth per-scanline interpolation - mainly for modulate (color).
	// Use a high power-of-2 height so nearest-neighbor sampling doesn't produce
	// visible bands in alpha or color channels between adjacent overrides.
	const int interpolate_resolution = 1024;

	// The interpolation mode is a single node-level setting that applies uniformly
	// to the whole override array for this pass.
	Mode7Sprite2D::Mode7InterpolationMode interp_mode = mode7_interpolation;

	int num_overrides = mode7_scanline_overrides.size();

	Ref<Mode7ScanlineOverride> first;
	if (num_overrides > 0) {
		first = mode7_scanline_overrides[0];
	}

	// Make sure we have more than 1 scanline object to interpolate between
	bool has_projection_anchors = false;
	Transform2D transform_top, transform_bottom;
	Vector2 pivot_top, pivot_bottom;
	Color modulate_top, modulate_bottom;
	real_t scale_top = 1.0f, scale_bottom = 1.0f;
	real_t rotation_top = 0.0f, rotation_bottom = 0.0f;
	if (interp_mode == Mode7Sprite2D::INTERPOLATION_PROJECTION && num_overrides >= 2) {
		// auto used here to hopefully inline/avoid heap allocation
		// These just guard against invalid/null values and are reusable below
		auto safe_transform = [&](int i) { Ref<Mode7ScanlineOverride> s = mode7_scanline_overrides[i]; return s.is_valid() ? s->get_transform()    : Transform2D(); };
		auto safe_pivot = [&](int i) { Ref<Mode7ScanlineOverride> s = mode7_scanline_overrides[i]; return s.is_valid() ? s->get_pivot()       : Vector2(0.5f, 0.5f); };
		auto safe_modulate = [&](int i) { Ref<Mode7ScanlineOverride> s = mode7_scanline_overrides[i]; return s.is_valid() ? s->get_modulate()  : Color(1.0f, 1.0f, 1.0f, 1.0f); };

		transform_top = safe_transform(0);
		transform_bottom = safe_transform(num_overrides - 1);
		pivot_top = safe_pivot(0);
		pivot_bottom = safe_pivot(num_overrides - 1);
		modulate_top = safe_modulate(0);
		modulate_bottom = safe_modulate(num_overrides - 1);

		// User-facing scale (already inverted by get_scale()).
		Vector2 s_top = transform_top.get_scale();
		Vector2 s_bot = transform_bottom.get_scale();
		scale_top = 1.0f / MAX(s_top.x, 0.0001f);
		scale_bottom = 1.0f / MAX(s_bot.x, 0.0001f);
		rotation_top = transform_top.get_rotation();
		rotation_bottom = transform_bottom.get_rotation();
		has_projection_anchors = true;
	}

	Ref<Image> img = Image::create_empty(3, interpolate_resolution, false, Image::FORMAT_RGBAF);

	for (int y = 0; y < interpolate_resolution; y++) {
		float uv_y = (y + 0.5f) / (float)interpolate_resolution;
		Transform2D result_transform;
		Vector2 pivot;
		Color mod;

		if (num_overrides == 0) {
			// Identity transform, centered pivot, white modulate.
			img->set_pixel(0, y, Color(1.0f, 0.0f, 0.0f, 1.0f));
			img->set_pixel(1, y, Color(0.0f, 0.0f, 0.5f, 0.5f));
			img->set_pixel(2, y, Color(1.0f, 1.0f, 1.0f, 1.0f));
			continue;
		}

		if (interp_mode == Mode7Sprite2D::INTERPOLATION_PROJECTION && has_projection_anchors) {
			// Per-scanline inverse-depth interpolation
			//
			// In a true perspective projection of a flat plane, texture scale is
			// inversely proportional to depth (S ~ 1/Z).  Therefore the VALUE
			// that varies linearly with screen height is 1/S, not S itself.
			// We interpolate in inverse-scale space, then invert back to get
			// the correct perspective-correct affine matrix for this scanline.
			//
			// first entry     = top / horizon anchor (small scale, far depth)
			// last entry      = bottom / close anchor (large scale, near depth)

			real_t t = uv_y;

			// Global projection tuning (projection mode only):
			// pixel_aspect remaps the vertical coordinate (NTSC non-square pixel
			// compensation), gamma reshapes the curve, strength blends the result
			// toward a flat image, aspect_ratio scales x relative to y.

			// Inverse-depth interpolation of scale.
			real_t inv_s_top = 1.0f / MAX(scale_top, 0.0001f);
			real_t inv_s_bot = 1.0f / MAX(scale_bottom, 0.0001f);

			// Pixel aspect: stretch/compress the vertical progression about the
			// center of the depth ramp so both endpoints stay pinned (f(0)=0,
			// f(1)=1) — the top and bottom scanlines must always show the first
			// and last anchor scales. 1.0 = no-op. Values <1.0 pull the curve
			// toward the far (top) anchor; values >1.0 toward the near (bottom)
			// anchor. (Square-pixel / NTSC 8:7 ≈ 1.125.)
			real_t t_pa = (t * mode7_projection_pixel_aspect) / (t * mode7_projection_pixel_aspect + (1.0f - t));

			// Gamma: reshape the progression (1.0 = linear inverse, <1.0 softens
			// the falloff, >1.0 sharpens it). Math::pow keeps real_t precision
			// instead of narrowing to float as powf would in double builds.
			real_t t_g = Math::pow(t_pa, mode7_projection_gamma);

			real_t inv_s_cur = inv_s_top + (inv_s_bot - inv_s_top) * t_g;
			real_t S = 1.0f / MAX(inv_s_cur, 0.0001f); // Perspective-correct scale.

			// Rotation interpolates linearly with screen height.
			real_t theta = rotation_top + (rotation_bottom - rotation_top) * t;

			// Strength: blend between a flat (uniform) transform and the full
			// perspective result, without altering the curve shape itself.
			if (mode7_projection_strength < 1.0f) {
				real_t S_flat = (scale_top + scale_bottom) * 0.5f;
				real_t theta_flat = (rotation_top + rotation_bottom) * 0.5f;
				S = S_flat + (S - S_flat) * mode7_projection_strength;
				theta = theta_flat + (theta - theta_flat) * mode7_projection_strength;
			}

			// Horizontal/vertical asymmetry: scale x relative to y
			// (1.0 = uniform Mode 7-like, 0.5 = x is half of y, >1.0 reversed).
			real_t Sx = S * mode7_projection_aspect_ratio;
			real_t Sy = S;

			real_t cos_t = Math::cos(theta);
			real_t sin_t = Math::sin(theta);

			// Affine matrix: A=Sx*cos, B=-Sy*sin, C=Sx*sin, D=Sy*cos.
			Vector2 col0(cos_t * Sx, -sin_t * Sy);
			Vector2 col1(sin_t * Sx, cos_t * Sy);

			// Pivot interpolates linearly.
			pivot = pivot_top.lerp(pivot_bottom, (real_t)t);

			// Modulate interpolates linearly between horizon and close anchors.
			mod = modulate_top.lerp(modulate_bottom, (real_t)t);

			// Scroll offset correction
			//
			// Scaling around a fixed pivot with changing per-scanline scale
			// causes the texture to warp unless the translation offset is also
			// adjusted so that world-space coordinates at the screen center
			// remain stable.  We correct the raw offset by adding a depth-
			// proportional shift.

			Vector2 off_raw = transform_top.columns[2].lerp(transform_bottom.columns[2], (real_t)t);

			// Perspective correction: the offset must be shifted in proportion
			// to how much the actual scale deviates from a linear blend of the
			// anchors.  Uses the strength-blended uniform scale S (not Sx) so the
			// correction stays zero at both anchors regardless of
			// mode7_projection_aspect_ratio.
			real_t s_linear = scale_top + (scale_bottom - scale_top) * t;
			if (mode7_projection_strength < 1.0f) {
				const real_t s_flat = (scale_top + scale_bottom) * 0.5f;
				s_linear = s_flat + (s_linear - s_flat) * mode7_projection_strength;
			}
			real_t depth_factor = (s_linear > 0.001f) ? (S / s_linear - 1.0f) : 0.0f;

			off_raw += (transform_bottom.columns[2] - transform_top.columns[2]) * depth_factor;

			result_transform = Transform2D(col0, col1, off_raw);
		} // if we're doing projection
		else { // Lerp or no interpolation
			float idx_f = (num_overrides == 1) ? 0.0f : uv_y * (num_overrides - 1);
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
				result_transform = xf_lo.interpolate_with(xf_hi, frac);
				pivot = pivot_lo.lerp(pivot_hi, frac);
				mod = modulate_lo.lerp(modulate_hi, frac);
			} else {
				int idx_nearest = CLAMP((int)roundf(idx_f), 0, num_overrides - 1);
				Ref<Mode7ScanlineOverride> entry_nearest = mode7_scanline_overrides[idx_nearest];
				result_transform = entry_nearest.is_valid() ? entry_nearest->get_transform() : Transform2D();
				pivot = entry_nearest.is_valid() ? entry_nearest->get_pivot() : Vector2(0.5f, 0.5f);
				mod = entry_nearest.is_valid() ? entry_nearest->get_modulate() : Color(1.0f, 1.0f, 1.0f, 1.0f);
			}
		}

		// These set the pixels for the "row" we're currently on (y)
		// Transform/scale/rotation
		img->set_pixel(0, y, Color(result_transform.columns[0].x, result_transform.columns[1].x, result_transform.columns[0].y, result_transform.columns[1].y));
		// Scroll offset / pivot point
		img->set_pixel(1, y, Color(result_transform.columns[2].x, result_transform.columns[2].y, pivot.x, pivot.y));
		// Per-scanline modulate (RGBA)
		img->set_pixel(2, y, mod);
	} // end of for loop

	if (_mode7_scanline_tex.is_null() || _mode7_scanline_tex->get_height() != interpolate_resolution) {
		_mode7_scanline_tex = ImageTexture::create_from_image(img);
	} else {
		_mode7_scanline_tex->update(img);
	}
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
			} else if (!is_inside_tree() || !target_2d->is_inside_tree()) {
				set_physics_process(false);
				mode7_follow_physics_active = false;
				return;
			} else if (!is_region_enabled()) {
				// Skip the update while the region is disabled, but keep
				// physics processing so follow resumes automatically.
				return;
			}

			if (!is_inside_tree()) {
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
	_update_follow_cache();
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
}

Mode7Sprite2D::Mode7Sprite2D() {
}

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
 * @file sprite_2d.cpp
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "mode7_sprite_2d.h"

#include "core/input/input.h"
#include "scene/2d/mode7_scanline_override.h"
#include "scene/2d/mode7_sprite_2d.h"
#include "scene/main/viewport.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"

// Embedded Mode 7 canvas_item shader.
// The scanline table is a 2-wide, N-tall RGBAF texture:
//   column x=0.25 (left pixel):  (a, b, c, d)  — the 2x2 affine matrix
//   column x=0.75 (right pixel): (tx, ty, pivot_x, pivot_y) — translation & pivot
// UV.y (0..1) is used to index the row, so each row maps to a horizontal
// band of the output sprite.
//
// All Mode 7 transforms operate in region-local [0,1]×[0,1] space. When no
// region is active, REGION_RECT defaults to (0,0,1,1) and the math is a no-op,
// so the full-texture behavior is preserved. This ensures pivots like (0.5,0.5)
// always refer to the center of the visible area, not the center of the atlas.
static const char *MODE7_SHADER_CODE = R"(
shader_type canvas_item;

// Per-scanline affine parameters (2×N texture: col0=M7A-D, col1=offset/pivot)
uniform sampler2D mode7_scanline_table : filter_nearest, repeat_disable;

// Tiling: false=transparent out-of-bounds (SNES default), true=repeat texture
uniform bool mode7_tiling = false;

// Global rotation applied after all scanline transforms
uniform float mode7_global_rotation = 0.0;
uniform vec2  mode7_global_pivot    = vec2(0.5, 0.5);

// Horizon masks: make a region at the top or bottom of the sprite transparent.
// Each amount is 0..1 (fraction to cull). Each tilt rotates the culling boundary
// around the center like an aircraft attitude indicator.
uniform float mode7_top_horizon_mask_amount  = 0.0;
uniform float mode7_top_horizon_mask_tilt    = 0.0;
uniform float mode7_bottom_horizon_mask_amount = 0.0;
uniform float mode7_bottom_horizon_mask_tilt   = 0.0;

void fragment() {
    // Normalize UV to region-local [0,1]×[0,1] space.
    // When no region is set, REGION_RECT = (0,0,1,1) and this is a no-op.
    vec2 uv = (UV - REGION_RECT.xy) / REGION_RECT.zw;

    // Fetch per-scanline matrix (M7A-D) and offset/pivot from table
    vec4 abcd = texture(mode7_scanline_table, vec2(0.25, uv.y));
    vec4 tp   = texture(mode7_scanline_table, vec2(0.75, uv.y));

    // Reconstruct 2×2 affine matrix (column-major: col0, col1)
    mat2 m   = mat2(vec2(abcd.r, abcd.b), vec2(abcd.g, abcd.a));
    vec2 off = vec2(tp.r, tp.g);
    vec2 pivot = vec2(tp.b, tp.a);

    // Apply per-scanline transform relative to pivot (always in local [0,1])
    uv = m * (uv - pivot) + pivot + off;

    // Apply global rotation (post-transform, around global pivot)
    float cr = cos(mode7_global_rotation);
    float sr = sin(mode7_global_rotation);
    mat2 m_global = mat2(vec2(cr, sr), vec2(-sr, cr));
    uv = m_global * (uv - mode7_global_pivot) + mode7_global_pivot;

    // Horizon masking — make a rotated region at the top and/or bottom transparent.
    // Works in local [0,1] space after all other transforms. Positive tilt
    // rotates the horizon clockwise (right side drops), matching an aircraft
    // attitude indicator.  amount = 0 leaves nothing culled; amount = 1
    // hides the entire sprite.
    float alpha_mult = 1.0;

    // Bottom mask: cull from bottom up
    if (mode7_bottom_horizon_mask_amount > 0.0) {
        float ct = cos(mode7_bottom_horizon_mask_tilt);
        float st = sin(mode7_bottom_horizon_mask_tilt);
        mat2 m_h = mat2(vec2(ct, -st), vec2(st, ct));
        vec2 uv_tilted = m_h * (uv - vec2(0.5)) + vec2(0.5);
        float horizon_line = 1.0 - mode7_bottom_horizon_mask_amount;
        alpha_mult *= 1.0 - step(horizon_line, uv_tilted.y);
    }

    // Top mask: cull from top down
    if (mode7_top_horizon_mask_amount > 0.0) {
        float ct = cos(mode7_top_horizon_mask_tilt);
        float st = sin(mode7_top_horizon_mask_tilt);
        mat2 m_h = mat2(vec2(ct, -st), vec2(st, ct));
        vec2 uv_tilted = m_h * (uv - vec2(0.5)) + vec2(0.5);
        float horizon_line = mode7_top_horizon_mask_amount;
        alpha_mult *= step(horizon_line, uv_tilted.y);
    }

    // Denormalize back to full-texture coordinates for sampling and bounds check
    vec2 uv_full = uv * REGION_RECT.zw + REGION_RECT.xy;
    bool out_of_bounds = !mode7_tiling &&
        (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0);
    COLOR = out_of_bounds ? vec4(0.0) : texture(TEXTURE, uv_full);
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

	// --- all set_shader_parameter calls go here ---
	_mode7_material->set_shader_parameter("mode7_use_table", !mode7_scanline_overrides.is_empty());
	_mode7_material->set_shader_parameter("mode7_global_rotation", mode7_global_rotation);
	_mode7_material->set_shader_parameter("mode7_global_pivot", mode7_global_pivot);
	_mode7_material->set_shader_parameter("mode7_top_horizon_mask_amount", mode7_top_horizon_mask_amount);
	_mode7_material->set_shader_parameter("mode7_top_horizon_mask_tilt", mode7_top_horizon_mask_tilt);
	_mode7_material->set_shader_parameter("mode7_bottom_horizon_mask_amount", mode7_bottom_horizon_mask_amount);
	_mode7_material->set_shader_parameter("mode7_bottom_horizon_mask_tilt", mode7_bottom_horizon_mask_tilt);

	_mode7_rebuild_scanline_texture();
	if (_mode7_scanline_tex.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_scanline_table",
				_mode7_scanline_tex);
	}

	_mode7_material->set_shader_parameter("mode7_tiling", mode7_tiling); // ← add here
}

void Mode7Sprite2D::_mode7_rebuild_scanline_texture() {
	int num_overrides = mode7_scanline_overrides.size();
	if (num_overrides == 0) {
		return;
	}

	int tex_h = 256;
	if (get_texture().is_valid()) {
		tex_h = is_region_enabled()
				? MAX(1, (int)get_region_rect().size.y)
				: MAX(1, get_texture()->get_height());
	}

	// Query the interpolation mode from the first entry (applied uniformly to
	// the whole override array for this pass).
	Ref<Mode7ScanlineOverride> first = mode7_scanline_overrides[0];
	Mode7ScanlineOverride::InterpolationMode interp_mode =
			first.is_valid() ? first->get_interpolation()
							 : Mode7ScanlineOverride::INTERPOLATION_NONE;

	// Pre-fetch Projection anchors (entry[0]=top/horizon, entry[last]=bottom/close).
	bool has_projection_anchors = false;
	Transform2D xf_top, xf_bot;
	Vector2 pivot_top, pivot_bot;
	real_t scale_top = 1.0f, scale_bot = 1.0f;
	real_t rot_top = 0.0f, rot_bot = 0.0f;
	if (interp_mode == Mode7ScanlineOverride::INTERPOLATION_PROJECTION && num_overrides >= 2) {
		auto safe_xf = [&](int i) { Ref<Mode7ScanlineOverride> e = mode7_scanline_overrides[i]; return e.is_valid() ? e->get_transform()    : Transform2D(); };
		auto safe_pivot = [&](int i) { Ref<Mode7ScanlineOverride> e = mode7_scanline_overrides[i]; return e.is_valid() ? e->get_pivot()       : Vector2(0.5f, 0.5f); };
		xf_top = safe_xf(0);
		pivot_top = safe_pivot(0);
		xf_bot = safe_xf(num_overrides - 1);
		pivot_bot = safe_pivot(num_overrides - 1);
		// User-facing scale (already inverted by get_scale()).
		Vector2 s_top = xf_top.get_scale();
		Vector2 s_bot = xf_bot.get_scale();
		scale_top = 1.0f / MAX(s_top.x, 0.0001f);
		scale_bot = 1.0f / MAX(s_bot.x, 0.0001f);
		rot_top = xf_top.get_rotation();
		rot_bot = xf_bot.get_rotation();
		has_projection_anchors = true;
	}

	Ref<Image> img = Image::create_empty(2, tex_h, false, Image::FORMAT_RGBAF);

	for (int y = 0; y < tex_h; y++) {
		float uv_y = (y + 0.5f) / (float)tex_h;
		Transform2D xf;
		Vector2 pivot;

		if (interp_mode == Mode7ScanlineOverride::INTERPOLATION_PROJECTION && has_projection_anchors) {
			// ── Projection: per-scanline inverse-depth interpolation ────────
			//
			// In a true perspective projection of a flat plane, texture scale is
			// inversely proportional to depth (S ~ 1/Z).  Therefore the VALUE
			// that varies linearly with screen height is 1/S, not S itself.
			// We interpolate in inverse-scale space, then invert back to get
			// the correct perspective-correct affine matrix for this scanline.
			//
			// entry[0]        = top / horizon anchor (small scale, far depth)
			// entry[last-1]   = bottom / close anchor (large scale, near depth)

			real_t t = uv_y;

			// Inverse-depth interpolation of scale.
			real_t inv_s_top = 1.0f / MAX(scale_top, 0.0001f);
			real_t inv_s_bot = 1.0f / MAX(scale_bot, 0.0001f);
			real_t inv_s_cur = inv_s_top + (inv_s_bot - inv_s_top) * t;
			real_t S = 1.0f / MAX(inv_s_cur, 0.0001f); // Perspective-correct scale.

			// Rotation interpolates linearly with screen height.
			real_t theta = rot_top + (rot_bot - rot_top) * t;
			real_t cos_t = Math::cos(theta);
			real_t sin_t = Math::sin(theta);

			// Affine matrix: A=S*cos, B=S*sin, C=-S*sin, D=S*cos.
			Vector2 col0(cos_t * S, -sin_t * S);
			Vector2 col1(sin_t * S, cos_t * S);

			// Pivot interpolates linearly.
			pivot = pivot_top.lerp(pivot_bot, (real_t)t);

			// ── Scroll offset correction (the critical fix) ───────────────
			//
			// Scaling around a fixed pivot with changing per-scanline scale
			// causes the texture to warp unless the translation offset is also
			// adjusted so that world-space coordinates at the screen center
			// remain stable.  We correct the raw offset by adding a depth-
			// proportional shift.

			Vector2 off_raw =
					(xf_bot.columns[2] - xf_top.columns[2]).lerp(xf_top.columns[2], (real_t)(1.0f - t));

			// Perspective correction: the offset must be shifted in proportion
			// to how much S deviates from a linear blend of the anchors.
			real_t s_linear = scale_top + (scale_bot - scale_top) * t;
			real_t depth_factor = (s_linear > 0.001f) ? (S / s_linear - 1.0f) : 0.0f;

			off_raw += (xf_bot.columns[2] - xf_top.columns[2]) * depth_factor;

			xf = Transform2D(col0, col1, off_raw);
		} else {
			// ── None / Lerp fallback ──────────────────────────────────────
			float idx_f = (num_overrides == 1) ? 0.0f : uv_y * (num_overrides - 1);
			int idx_lo = CLAMP((int)idx_f, 0, num_overrides - 1);
			int idx_hi = CLAMP(idx_lo + 1, 0, num_overrides - 1);
			float frac = idx_f - (float)idx_lo;

			Ref<Mode7ScanlineOverride> entry_lo = mode7_scanline_overrides[idx_lo];
			Transform2D xf_lo = entry_lo.is_valid() ? entry_lo->get_transform() : Transform2D();
			Vector2 pivot_lo = entry_lo.is_valid() ? entry_lo->get_pivot() : Vector2(0.5f, 0.5f);
			bool do_lerp = (interp_mode == Mode7ScanlineOverride::INTERPOLATION_LERP);

			if (do_lerp && idx_hi != idx_lo && frac > 0.0f) {
				Ref<Mode7ScanlineOverride> entry_hi = mode7_scanline_overrides[idx_hi];
				Transform2D xf_hi = entry_hi.is_valid() ? entry_hi->get_transform() : Transform2D();
				Vector2 pivot_hi = entry_hi.is_valid() ? entry_hi->get_pivot() : Vector2(0.5f, 0.5f);
				xf = xf_lo.interpolate_with(xf_hi, frac);
				pivot = pivot_lo.lerp(pivot_hi, frac);
			} else {
				int idx_nearest = CLAMP((int)roundf(idx_f), 0, num_overrides - 1);
				Ref<Mode7ScanlineOverride> entry_nearest = mode7_scanline_overrides[idx_nearest];
				xf = entry_nearest.is_valid() ? entry_nearest->get_transform() : Transform2D();
				pivot = entry_nearest.is_valid() ? entry_nearest->get_pivot() : Vector2(0.5f, 0.5f);
			}
		}

		img->set_pixel(0, y, Color(xf.columns[0].x, xf.columns[1].x, xf.columns[0].y, xf.columns[1].y));
		// tx, ty in r,g — pivot_x, pivot_y in b,a
		img->set_pixel(1, y, Color(xf.columns[2].x, xf.columns[2].y, pivot.x, pivot.y));
	}

	if (_mode7_scanline_tex.is_null() ||
			_mode7_scanline_tex->get_height() != tex_h) {
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
	if (mode7_enabled) {
		RS::get_singleton()->canvas_item_set_default_texture_repeat(
				get_canvas_item(),
				p_tiling ? RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED
						 : RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
		if (_mode7_material.is_valid()) {
			_mode7_material->set_shader_parameter("mode7_tiling", p_tiling);
		}
		queue_redraw();
	}
}

bool Mode7Sprite2D::is_mode7_tiling() const {
	return mode7_tiling;
}

void Mode7Sprite2D::set_mode7_global_rotation(real_t p_radians) {
	if (mode7_global_rotation == p_radians) {
		return;
	}
	mode7_global_rotation = p_radians;
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_global_rotation", p_radians);
		queue_redraw();
	}
}

real_t Mode7Sprite2D::get_mode7_global_rotation() const {
	return mode7_global_rotation;
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

// ── Top horizon mask ────────────────────────────────────────────────

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

void Mode7Sprite2D::set_mode7_top_horizon_mask_tilt(real_t p_radians) {
	if (mode7_top_horizon_mask_tilt == p_radians) {
		return;
	}
	mode7_top_horizon_mask_tilt = p_radians;
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_top_horizon_mask_tilt", p_radians);
		queue_redraw();
	}
}

real_t Mode7Sprite2D::get_mode7_top_horizon_mask_tilt() const {
	return mode7_top_horizon_mask_tilt;
}

// ── Bottom horizon mask ─────────────────────────────────────────────

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

void Mode7Sprite2D::set_mode7_bottom_horizon_mask_tilt(real_t p_radians) {
	if (mode7_bottom_horizon_mask_tilt == p_radians) {
		return;
	}
	mode7_bottom_horizon_mask_tilt = p_radians;
	if (mode7_enabled && _mode7_material.is_valid()) {
		_mode7_material->set_shader_parameter("mode7_bottom_horizon_mask_tilt", p_radians);
		queue_redraw();
	}
}

real_t Mode7Sprite2D::get_mode7_bottom_horizon_mask_tilt() const {
	return mode7_bottom_horizon_mask_tilt;
}

void Mode7Sprite2D::set_mode7_enabled(bool p_enabled) {
	if (mode7_enabled == p_enabled) {
		return;
	}
	mode7_enabled = p_enabled;
	if (mode7_enabled) {
		if (mode7_scanline_overrides.is_empty()) {
			Ref<Mode7ScanlineOverride> def;
			def.instantiate();
			mode7_scanline_overrides.append(def);
			def->connect_changed(callable_mp(this, &Mode7Sprite2D::_on_mode7_override_changed),
					CONNECT_REFERENCE_COUNTED);
		}
		_mode7_rebuild_material();
		set_material(_mode7_material); // ← only here, once

		// Save the node's current repeat mode so we can restore it on disable,
		// then push the mode7 repeat state directly to the RenderingServer.
		// We bypass set_texture_repeat() to avoid its notify_property_list_changed() call.
		_saved_texture_repeat = (RS::CanvasItemTextureRepeat)get_texture_repeat();
		RS::get_singleton()->canvas_item_set_default_texture_repeat(
				get_canvas_item(),
				mode7_tiling ? RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED
							 : RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);
	} else {
		set_material(Ref<Material>());

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

void Mode7Sprite2D::set_mode7_scanline_overrides(const TypedArray<Mode7ScanlineOverride> &p_overrides) {
	// Disconnect from all existing override resources.
	for (int i = 0; i < mode7_scanline_overrides.size(); i++) {
		Ref<Mode7ScanlineOverride> entry = mode7_scanline_overrides[i];
		if (entry.is_valid()) {
			entry->disconnect_changed(callable_mp(this, &Mode7Sprite2D::_on_mode7_override_changed));
		}
	}

	mode7_scanline_overrides = p_overrides;

	// Connect to all new override resources.
	for (int i = 0; i < mode7_scanline_overrides.size(); i++) {
		Ref<Mode7ScanlineOverride> entry = mode7_scanline_overrides[i];
		if (entry.is_valid()) {
			entry->connect_changed(
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

void Mode7Sprite2D::_bind_methods() {
	// Mode 7  -----------------------------------------------------------------------------------------------------------------------------------------
	ClassDB::bind_method(D_METHOD("set_mode7_enabled", "enabled"), &Mode7Sprite2D::set_mode7_enabled);
	ClassDB::bind_method(D_METHOD("is_mode7_enabled"), &Mode7Sprite2D::is_mode7_enabled);

	ClassDB::bind_method(D_METHOD("set_mode7_scanline_overrides", "overrides"), &Mode7Sprite2D::set_mode7_scanline_overrides);
	ClassDB::bind_method(D_METHOD("get_mode7_scanline_overrides"), &Mode7Sprite2D::get_mode7_scanline_overrides);

	ClassDB::bind_method(D_METHOD("set_mode7_tiling", "tiling"), &Mode7Sprite2D::set_mode7_tiling);
	ClassDB::bind_method(D_METHOD("is_mode7_tiling"), &Mode7Sprite2D::is_mode7_tiling);

	ClassDB::bind_method(D_METHOD("set_mode7_global_rotation", "radians"), &Mode7Sprite2D::set_mode7_global_rotation);
	ClassDB::bind_method(D_METHOD("get_mode7_global_rotation"), &Mode7Sprite2D::get_mode7_global_rotation);
	ClassDB::bind_method(D_METHOD("set_mode7_global_pivot", "pivot"), &Mode7Sprite2D::set_mode7_global_pivot);
	ClassDB::bind_method(D_METHOD("get_mode7_global_pivot"), &Mode7Sprite2D::get_mode7_global_pivot);

	// Top horizon mask
	ClassDB::bind_method(D_METHOD("set_mode7_top_horizon_mask_amount", "amount"), &Mode7Sprite2D::set_mode7_top_horizon_mask_amount);
	ClassDB::bind_method(D_METHOD("get_mode7_top_horizon_mask_amount"), &Mode7Sprite2D::get_mode7_top_horizon_mask_amount);
	ClassDB::bind_method(D_METHOD("set_mode7_top_horizon_mask_tilt", "radians"), &Mode7Sprite2D::set_mode7_top_horizon_mask_tilt);
	ClassDB::bind_method(D_METHOD("get_mode7_top_horizon_mask_tilt"), &Mode7Sprite2D::get_mode7_top_horizon_mask_tilt);

	// Bottom horizon mask
	ClassDB::bind_method(D_METHOD("set_mode7_bottom_horizon_mask_amount", "amount"), &Mode7Sprite2D::set_mode7_bottom_horizon_mask_amount);
	ClassDB::bind_method(D_METHOD("get_mode7_bottom_horizon_mask_amount"), &Mode7Sprite2D::get_mode7_bottom_horizon_mask_amount);
	ClassDB::bind_method(D_METHOD("set_mode7_bottom_horizon_mask_tilt", "radians"), &Mode7Sprite2D::set_mode7_bottom_horizon_mask_tilt);
	ClassDB::bind_method(D_METHOD("get_mode7_bottom_horizon_mask_tilt"), &Mode7Sprite2D::get_mode7_bottom_horizon_mask_tilt);

	// -------------------------------------------------------------------------------------------------------------------------------------------------

	ADD_GROUP("Mode 7", "mode7_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mode7_tiling"), "set_mode7_tiling", "is_mode7_tiling");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_global_rotation",
						 PROPERTY_HINT_RANGE, "-360,360,0.1,radians_as_degrees"),
			"set_mode7_global_rotation", "get_mode7_global_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "mode7_global_pivot"),
			"set_mode7_global_pivot", "get_mode7_global_pivot");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_top_horizon_mask_amount",
						 PROPERTY_HINT_RANGE, "0,1,0.001"),
			"set_mode7_top_horizon_mask_amount", "get_mode7_top_horizon_mask_amount");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_top_horizon_mask_tilt",
						 PROPERTY_HINT_RANGE, "-360,360,0.1,radians_as_degrees"),
			"set_mode7_top_horizon_mask_tilt", "get_mode7_top_horizon_mask_tilt");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_bottom_horizon_mask_amount",
						 PROPERTY_HINT_RANGE, "0,1,0.001"),
			"set_mode7_bottom_horizon_mask_amount", "get_mode7_bottom_horizon_mask_amount");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mode7_bottom_horizon_mask_tilt",
						 PROPERTY_HINT_RANGE, "-360,360,0.1,radians_as_degrees"),
			"set_mode7_bottom_horizon_mask_tilt", "get_mode7_bottom_horizon_mask_tilt");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mode7_enabled", PROPERTY_HINT_GROUP_ENABLE), "set_mode7_enabled", "is_mode7_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "mode7_scanline_overrides",
						 PROPERTY_HINT_ARRAY_TYPE, "Mode7ScanlineOverride"),
			"set_mode7_scanline_overrides", "get_mode7_scanline_overrides");
}

Mode7Sprite2D::Mode7Sprite2D() {
}

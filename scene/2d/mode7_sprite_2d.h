/**************************************************************************/
/*  mode7_sprite_2d.h                                                     */
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

#pragma once

/**
 * @file mode7_sprite_2d.h
 *
 * This extends Sprite2D to have parameters specifically designed to replicate
 * the Super Nintendo's Mode 7 graphics, which essentially allowed affine transformations
 * to be controlled per scanline, which allowed creating a unique-looking projection effect.
 * Naturally, "scanline" is used in the code as an intuitive term.
 */

#include "scene/2d/mode7_scanline_override.h"
#include "scene/2d/sprite_2d.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"

class Mode7Sprite2D : public Sprite2D {
	GDCLASS(Mode7Sprite2D, Sprite2D);

public:
	enum Mode7InterpolationMode {
		INTERPOLATION_NONE, ///< Nearest-neighbor: snap each row to the nearest entry's transform, no blending.
		INTERPOLATION_LERP, ///< Linear interpolation between adjacent entries across the sprite height.
		/// Perspective projection via per-scanline inverse-depth interpolation. Uses the first entry as
		/// top/horizon anchor and the last entry as bottom/close anchor. Use this with 2 overrides.
		INTERPOLATION_PROJECTION
	};

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_mode7_enabled(bool p_enabled);
	bool is_mode7_enabled() const;

	void set_mode7_interpolation(Mode7InterpolationMode p_mode);
	Mode7InterpolationMode get_mode7_interpolation() const;

	void set_mode7_scanline_overrides(const TypedArray<Mode7ScanlineOverride> &p_overrides);
	TypedArray<Mode7ScanlineOverride> get_mode7_scanline_overrides() const;

	void set_mode7_tiling(bool p_tiling);
	bool is_mode7_tiling() const;

	void set_mode7_global_rotation(real_t p_degrees);
	real_t get_mode7_global_rotation() const;
	void set_mode7_global_pivot(const Vector2 &p_pivot);
	Vector2 get_mode7_global_pivot() const;
	void set_mode7_global_offset(const Vector2 &p_offset);
	Vector2 get_mode7_global_offset() const;

	/// @name Region Aspect Override
	/// @{
	void set_mode7_override_region_aspect(bool p_enabled);
	bool is_mode7_override_region_aspect() const;
	/// @endGroup

	/// @name Region follow target
	/// @{
	void set_mode7_region_follow_target(const NodePath &p_path);
	NodePath get_mode7_region_follow_target() const;
	/// @}

	/// @name Top horizon mask (culls from top down)
	/// @{
	void set_mode7_top_horizon_mask_amount(real_t p_amount);
	real_t get_mode7_top_horizon_mask_amount() const;
	void set_mode7_top_horizon_tilt(real_t p_degrees);
	real_t get_mode7_top_horizon_tilt() const;
	/// @}
	/// @name Bottom horizon mask (culls from bottom up)
	/// @{
	void set_mode7_bottom_horizon_mask_amount(real_t p_amount);
	real_t get_mode7_bottom_horizon_mask_amount() const;
	void set_mode7_bottom_horizon_tilt(real_t p_degrees);
	real_t get_mode7_bottom_horizon_tilt() const;
	/// @}

	/// @name Projection perspective tuning (projection mode only)
	/// @{
	void set_mode7_projection_gamma(real_t p_value);
	real_t get_mode7_projection_gamma() const;
	void set_mode7_projection_strength(real_t p_value);
	real_t get_mode7_projection_strength() const;
	void set_mode7_projection_aspect_ratio(real_t p_value);
	real_t get_mode7_projection_aspect_ratio() const;
	void set_mode7_projection_pixel_aspect(real_t p_value);
	real_t get_mode7_projection_pixel_aspect() const;
	/// @}

	/// Re-resolve the follow target from mode7_region_follow_target and store it in mode7_follow_cache.
	/// Exposed for manual refresh (e.g., after a scene reload) without waiting for ENTER_TREE or setter calls.
	void force_update_follow_cache();

	Mode7Sprite2D();

private:
	bool mode7_enabled = false;
	Mode7InterpolationMode mode7_interpolation = INTERPOLATION_NONE;
	TypedArray<Mode7ScanlineOverride> mode7_scanline_overrides; ///< Array of Transform2D, one per output row (UV.y band)

	Ref<ShaderMaterial> _mode7_material;
	/// This "texture" is really a hack to send data to the shader in a manner that speaks GPU
	/// It's 3 pixels wide, each pixel stores a "Color"
	/// First pixel is the transform matrix, second pixel is the offset & pivot, third pixel is the modulate (the only one that's "actually" a Color)
	Ref<ImageTexture> _mode7_scanline_tex;

	void _mode7_rebuild_material();
	/// This is the function responsible for creating the "table" that gets passed to the shader
	/// in the form of a texture.
	/// Builds the transform, pivot/offset and color/modulate as 3 "Color" values per row
	/// We're only after an actual color for the modulate value, though.  The rest, we're using the vec4 for data.
	void _mode7_rebuild_scanline_texture();
	/// Shared tail for the projection tuning setters: these four parameters only feed the
	/// scanline table (not the shader uniforms), so when the material already exists we
	/// rebuild just the table (rebinding it to the material) and request a redraw —
	/// skipping the redundant re-setting of every shader uniform in _mode7_rebuild_material().
	void _mode7_refresh_projection_table();
	void _on_mode7_override_changed();

	bool mode7_tiling = false;
	RS::CanvasItemTextureRepeat _saved_texture_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;

	real_t mode7_global_rotation = 0.0f;
	Vector2 mode7_global_pivot = Vector2(0.5f, 0.5f);
	Vector2 mode7_global_offset = Vector2(0.0f, 0.0f);
	bool mode7_override_region_aspect = true;

	void _validate_property(PropertyInfo &p_property) const;
	String _get_property_warning(const StringName &p_name) const;

	/// Resolve mode7_region_follow_target into the ObjectID cache (mode7_follow_cache).
	/// Called on NOTIFICATION_ENTER_TREE and deferred from set_mode7_region_follow_target()
	/// to handle the editor rename timing edge case.
	void _update_follow_cache();

	/// Verify that a valid follow target is cached, and start physics process if so.
	/// Used as a call_deferred target from the setter when this node is not yet in the tree,
	/// ensuring follow starts as soon as the tree is ready.
	void _ensure_follow_physics();

	/// @name Horizon masks
	real_t mode7_top_horizon_mask_amount = 0.0f; ///< 0..1 fraction to make transparent (from top down)
	real_t mode7_top_horizon_tilt = 0.0f; ///< Stored internally in radians; the mode7_top_horizon_tilt property is exposed in degrees.
	real_t mode7_bottom_horizon_mask_amount = 0.0f; ///< 0..1 fraction to make transparent (from bottom up)
	real_t mode7_bottom_horizon_tilt = 0.0f; ///< Stored internally in radians; the mode7_bottom_horizon_tilt property is exposed in degrees.
	/// @}

	/// @name Projection perspective tuning (applied in the scanline-table rebuild)
	real_t mode7_projection_gamma = 1.0f; ///< 0.5..2.0 curve shape of the inverse-depth progression (1.0 = linear inverse)
	real_t mode7_projection_strength = 1.0f; ///< 0.0..1.0 blend between flat image and full perspective
	real_t mode7_projection_aspect_ratio = 1.0f; ///< 0.0..2.0 horizontal-to-vertical scale ratio (1.0 = uniform)
	real_t mode7_projection_pixel_aspect = 1.0f; ///< 0.5..1.125 y-stretch applied before the inverse-depth calc
	/// @}

	/// Shift region_rect each physics frame so the Mode 7 viewport
	/// "follows" another Node2D while preserving its size/aspect/etc.
	/// @name Region follow target
	NodePath mode7_region_follow_target; ///< Exported path to the target Node2D (rewritten automatically by the editor on rename)
	ObjectID mode7_follow_cache; ///< Cached instance ID resolved from mode7_region_follow_target — degrades safely if the target node is freed
	bool mode7_follow_initialized = false; ///< Whether the follow has reached its second physics frame (skips initial offset snap)
	bool mode7_follow_physics_active = false; ///< Whether physics process is currently running for follow tracking
	/// @endGroup

	Ref<Material> _saved_material;
};

VARIANT_ENUM_CAST(Mode7Sprite2D::Mode7InterpolationMode);

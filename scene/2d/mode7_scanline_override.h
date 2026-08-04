/**************************************************************************/
/*  mode7_scanline_override.h                                             */
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
 * @file mode7_scanline_override.h
 *
 * This class is a companion to mode7_sprite_2d.h
 * One or more instances are intended to be used in an array in the Inspector.
 * As the Super Nintendo was able to have a different transformation each scanline,
 * it essentially manipulated each one when doing effects such as projections.
 *
 * The intent of this is to bring a more modern/intuitive tool to produce that effect.
 * Using 2 Mode7ScanlineOverride objects in the array on the Mode7Sprite2D, interpolation
 * can be set to Lerp or Projection.  Projection is designed around 2.
 * Lerp will interpolate all of the values in between however many of these objects are
 * added to the array.
 *
 * The Mode7Sprite2D will always start with just one Mode7ScanlineOverride,
 * which can be used for a straight-up affine transformation.
 */

#include "core/io/resource.h"
#include "core/variant/binder_common.h"

class Mode7ScanlineOverride : public Resource {
	GDCLASS(Mode7ScanlineOverride, Resource);

public:
	enum InterpolationMode {
		INTERPOLATION_NONE, ///< Nearest-neighbor: snap to this entry's transform for its UV band.
		INTERPOLATION_LERP, ///< Linear interpolation between adjacent entries in the override array.
		/// Perspective projection via per-scanline inverse-depth interpolation. Uses the first entry as top/horizon and the last as bottom/close anchor.
		/// TLDR - use this with 2 Mode7ScanlineOverrides.
		INTERPOLATION_PROJECTION
	};

private:
	/// columns[0] and columns[1] are the 2x2 affine matrix
	/// (rotation/scale/skew); columns[2] is the translation offset.
	Transform2D transform;

	Vector2 pivot = Vector2(0.5f, 0.5f);
	InterpolationMode interpolation = INTERPOLATION_NONE;

	/// Exposes color/alpha/intensity (and therefore bloom) as a sort of bonus,
	/// which will be interpolated between multiple Mode7ScanlineOverride objects in
	/// Lerp or Projection mode, along with the transform parameters.
	/// In Projection mode, this property is still just lerped.
	Color modulate = Color(1.0f, 1.0f, 1.0f, 1.0f);

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	/// @name 2x2 affine matrix
	/// @{
	void set_transform(const Transform2D &p_transform);
	Transform2D get_transform() const;
	/// @}

	/// @name Helper Properties (Intuitive parameters that handle affine matrix updates)
	/// @{
	void set_rotation(real_t p_degrees);
	real_t get_rotation() const;

	void set_scale(const Vector2 &p_scale);
	Vector2 get_scale() const;

	void set_skew(real_t p_degrees);
	real_t get_skew() const;
	/// @}

	/// @name Pivot
	/// @{
	void set_pivot(const Vector2 &p_pivot);
	Vector2 get_pivot() const;
	/// @}

	/// @name Interpolation Mode
	/// @{
	void set_interpolation(InterpolationMode p_mode);
	InterpolationMode get_interpolation() const;
	/// @}

	/// @name Modulate
	/// @{
	void set_modulate(const Color &p_color);
	Color get_modulate() const;
	/// @}
};

VARIANT_ENUM_CAST(Mode7ScanlineOverride::InterpolationMode);

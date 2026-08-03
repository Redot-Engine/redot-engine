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

#include "core/io/resource.h"
#include "core/variant/binder_common.h"

class Mode7ScanlineOverride : public Resource {
	GDCLASS(Mode7ScanlineOverride, Resource);

public:
	enum InterpolationMode {
		INTERPOLATION_NONE, // Nearest-neighbor: snap to this entry's transform for its UV band.
		INTERPOLATION_LERP, // Linear interpolation between adjacent entries in the override array.
		INTERPOLATION_PROJECTION // Perspective projection via per-scanline inverse-depth interpolation.
								 // Uses entry[0] as top/horizon and entry[last] as bottom/close anchor.
	};

private:
	// Canonical storage. columns[0] and columns[1] are the 2x2 affine matrix
	// (rotation/scale/skew); columns[2] is the translation offset.
	Transform2D transform;

	// Vanishing point around which the matrix is applied.
	Vector2 pivot = Vector2(0.5f, 0.5f);

	InterpolationMode interpolation = INTERPOLATION_NONE;

protected:
	static void _bind_methods();

public:
	// ── Raw matrix access ────────────────────────────────────────────────────
	void set_transform(const Transform2D &p_transform);
	Transform2D get_transform() const;

	// ── Decomposed convenience properties ────────────────────────────────────
	// All three read/write through `transform` so the matrix stays canonical.
	// Exposing all three (rotation + scale + skew) makes the round-trip exact.
	void set_rotation(real_t p_radians);
	real_t get_rotation() const;

	void set_scale(const Vector2 &p_scale);
	Vector2 get_scale() const;

	void set_skew(real_t p_radians);
	real_t get_skew() const;

	// ── Pivot ────────────────────────────────────────────────────────────────
	void set_pivot(const Vector2 &p_pivot);
	Vector2 get_pivot() const;

	// ── Interpolation mode ───────────────────────────────────────────────────
	void set_interpolation(InterpolationMode p_mode);
	InterpolationMode get_interpolation() const;
};

VARIANT_ENUM_CAST(Mode7ScanlineOverride::InterpolationMode);

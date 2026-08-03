/**************************************************************************/
/*  mode7_scanline_override.cpp                                           */
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

#include "mode7_scanline_override.h"

void Mode7ScanlineOverride::set_transform(const Transform2D &p_transform) {
	transform = p_transform;
	emit_changed();
}
Transform2D Mode7ScanlineOverride::get_transform() const {
	return transform;
}

void Mode7ScanlineOverride::set_rotation(real_t p_radians) {
	transform.set_rotation_scale_and_skew(p_radians, transform.get_scale(), transform.get_skew());
	emit_changed();
}
real_t Mode7ScanlineOverride::get_rotation() const {
	return transform.get_rotation();
}

void Mode7ScanlineOverride::set_scale(const Vector2 &p_scale) {
	// Clamp to a positive minimum so we never divide by zero (NaN corrupts
	// the Transform2D irrecoverably).  We also reject negative values because
	// they flip the UV orientation and create inconsistent state when round-
	// tripping through get_scale().
	const real_t MIN_SCALE = 0.1f;
	Vector2 clamped(MAX(p_scale.x, MIN_SCALE), MAX(p_scale.y, MIN_SCALE));
	Vector2 inv(1.0f / clamped.x, 1.0f / clamped.y);
	transform.set_scale(inv);
	emit_changed();
}

Vector2 Mode7ScanlineOverride::get_scale() const {
	Vector2 s = transform.get_scale();
	// Guard against NaN or zero that may have leaked in from direct
	// Transform2D manipulation.
	if (s.x <= 0.0f || s.y <= 0.0f) {
		return Vector2(1.0f, 1.0f);
	}
	return Vector2(1.0f / s.x, 1.0f / s.y);
}

void Mode7ScanlineOverride::set_skew(real_t p_radians) {
	transform.set_rotation_scale_and_skew(transform.get_rotation(), transform.get_scale(), p_radians);
	emit_changed();
}
real_t Mode7ScanlineOverride::get_skew() const {
	return transform.get_skew();
}

void Mode7ScanlineOverride::set_pivot(const Vector2 &p_pivot) {
	pivot = p_pivot;
	emit_changed();
}
Vector2 Mode7ScanlineOverride::get_pivot() const {
	return pivot;
}

void Mode7ScanlineOverride::set_interpolation(InterpolationMode p_mode) {
	interpolation = p_mode;
	emit_changed();
}
Mode7ScanlineOverride::InterpolationMode Mode7ScanlineOverride::get_interpolation() const {
	return interpolation;
}

void Mode7ScanlineOverride::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &Mode7ScanlineOverride::set_transform);
	ClassDB::bind_method(D_METHOD("get_transform"), &Mode7ScanlineOverride::get_transform);
	ClassDB::bind_method(D_METHOD("set_rotation", "radians"), &Mode7ScanlineOverride::set_rotation);
	ClassDB::bind_method(D_METHOD("get_rotation"), &Mode7ScanlineOverride::get_rotation);
	ClassDB::bind_method(D_METHOD("set_scale", "scale"), &Mode7ScanlineOverride::set_scale);
	ClassDB::bind_method(D_METHOD("get_scale"), &Mode7ScanlineOverride::get_scale);
	ClassDB::bind_method(D_METHOD("set_skew", "radians"), &Mode7ScanlineOverride::set_skew);
	ClassDB::bind_method(D_METHOD("get_skew"), &Mode7ScanlineOverride::get_skew);
	ClassDB::bind_method(D_METHOD("set_pivot", "pivot"), &Mode7ScanlineOverride::set_pivot);
	ClassDB::bind_method(D_METHOD("get_pivot"), &Mode7ScanlineOverride::get_pivot);
	ClassDB::bind_method(D_METHOD("set_interpolation", "mode"), &Mode7ScanlineOverride::set_interpolation);
	ClassDB::bind_method(D_METHOD("get_interpolation"), &Mode7ScanlineOverride::get_interpolation);

	BIND_ENUM_CONSTANT(INTERPOLATION_NONE);
	BIND_ENUM_CONSTANT(INTERPOLATION_LERP);
	BIND_ENUM_CONSTANT(INTERPOLATION_PROJECTION);

	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "transform"), "set_transform", "get_transform");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rotation", PROPERTY_HINT_RANGE,
						 "-360,360,0.1,radians_as_degrees"),
			"set_rotation", "get_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scale"), "set_scale", "get_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "skew", PROPERTY_HINT_RANGE,
						 "-89.9,89.9,0.1,radians_as_degrees"),
			"set_skew", "get_skew");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "pivot"), "set_pivot", "get_pivot");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "interpolation", PROPERTY_HINT_ENUM,
						 "None,Lerp,Projection"),
			"set_interpolation", "get_interpolation");
}

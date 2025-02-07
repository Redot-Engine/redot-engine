/**************************************************************************/
/*  rect2.cpp                                                             */
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

#include "rect2.h"

#include "core/math/rect2i.h"
#include "core/math/transform_2d.h"
#include "core/string/ustring.h"

bool Rect2::is_equal_approx(const Rect2 &p_rect) const {
	return position.is_equal_approx(p_rect.position) && size.is_equal_approx(p_rect.size);
}

bool Rect2::is_finite() const {
	return position.is_finite() && size.is_finite();
}

bool Rect2::intersects_segment(const Point2 &p_from, const Point2 &p_to, Point2 *r_pos, Point2 *r_normal) const {
	#ifdef MATH_CHECKS
	if (unlikely(size.x < 0 || size.y < 0)) {
		ERR_PRINT("Rect2 size is negative, this is not supported. Use Rect2.abs() to get a Rect2 with a positive size.");
	}
	#endif
	real_t t_min = 0.0f;
	real_t t_max = 1.0f;
	int intersecting_axis = -1;
	real_t sign = 0.0f;

	Vector2 segment_dir = p_to - p_from;

	for (int axis = 0; axis < 2; axis++) {
		real_t seg_from = p_from[axis];
		real_t seg_to = p_to[axis];
		real_t rect_min = position[axis];
		real_t rect_max = rect_min + size[axis];
		real_t t_near, t_far;
		real_t axis_sign = 0.0f;

		if (seg_from < seg_to) {
			if (seg_from > rect_max || seg_to < rect_min) {
				return false;
			}
			real_t length = seg_to - seg_from;
			t_near = (seg_from < rect_min) ? ((rect_min - seg_from) / length) : 0.0f;
			t_far = (seg_to > rect_max) ? ((rect_max - seg_from) / length) : 1.0f;
			axis_sign = -1.0f;
		} else {
			if (seg_to > rect_max || seg_from < rect_min) {
				return false;
			}
			real_t length = seg_to - seg_from;
			t_near = (seg_from > rect_max) ? (rect_max - seg_from) / length : 0.0f;
			t_far = (seg_to < rect_min) ? (rect_min - seg_from) / length : 1.0f;
			axis_sign = 1.0f;
		}

		if (t_near > t_min) {
			t_min = t_near;
			intersecting_axis = axis;
			sign = axis_sign;
		}
		if (t_far < t_max) {
			t_max = t_far;
		}
		if (t_min > t_max) {
			return false;
		}
	}

	if (r_normal) {
		*r_normal = Vector2();
		if (intersecting_axis != -1) {
			(*r_normal)[intersecting_axis] = sign;
		}
	}

	if (r_pos) {
		*r_pos = p_from + segment_dir * t_min;
	}

	return true;
}

bool Rect2::intersects_transformed(const Transform2D &p_xform, const Rect2 &p_rect) const {
	#ifdef MATH_CHECKS
	if (unlikely(size.x < 0 || size.y < 0 || p_rect.size.x < 0 || p_rect.size.y < 0)) {
		ERR_PRINT("Rect2 size is negative, this is not supported. Use Rect2.abs() to get a Rect2 with a positive size.");
	}
	#endif
	// Transformed rect points
	Vector2 xf_points[4] = {
		p_xform.xform(p_rect.position),
		p_xform.xform(Vector2(p_rect.position.x + p_rect.size.x, p_rect.position.y)),
		p_xform.xform(Vector2(p_rect.position.x, p_rect.position.y + p_rect.size.y)),
		p_xform.xform(Vector2(p_rect.position.x + p_rect.size.x, p_rect.position.y + p_rect.size.y)),
	};

	// Check against the base rect using AABB quick reject
	{
		// Check top boundary
		bool any_above = false;
		for (const Vector2 &point : xf_points) {
			if (point.y > position.y) {
				any_above = true;
				break;
			}
		}
		if (!any_above) return false;

		// Check bottom boundary
		real_t bottom_limit = position.y + size.y;
		bool any_below = false;
		for (const Vector2 &point : xf_points) {
			if (point.y < bottom_limit) {
				any_below = true;
				break;
			}
		}
		if (!any_below) return false;

		// Check left boundary
		bool any_right = false;
		for (const Vector2 &point : xf_points) {
			if (point.x > position.x) {
				any_right = true;
				break;
			}
		}
		if (!any_right) return false;

		// Check right boundary
		real_t right_limit = position.x + size.x;
		bool any_left = false;
		for (const Vector2 &point : xf_points) {
			if (point.x < right_limit) {
				any_left = true;
				break;
			}
		}
		if (!any_left) return false;
	}

	// SAT implementation using transformed axes
	Vector2 xf_points2[4] = {
		position,
		Vector2(position.x + size.x, position.y),
		Vector2(position.x, position.y + size.y),
		Vector2(position.x + size.x, position.y + size.y),
	};

	for (int axis = 0; axis < 2; axis++) {
		Vector2 axis_vec = p_xform.columns[axis];

		// Project this rect's points onto the axis
		real_t min_a = axis_vec.dot(xf_points2[0]);
		real_t max_a = min_a;
		for (int i = 1; i < 4; i++) {
			real_t proj = axis_vec.dot(xf_points2[i]);
			min_a = MIN(min_a, proj);
			max_a = MAX(max_a, proj);
		}

		// Project the other rect's points onto the axis
		real_t min_b = axis_vec.dot(xf_points[0]);
		real_t max_b = min_b;
		for (int i = 1; i < 4; i++) {
			real_t proj = axis_vec.dot(xf_points[i]);
			min_b = MIN(min_b, proj);
			max_b = MAX(max_b, proj);
		}

		// Check for overlap
		if (min_a > max_b || min_b > max_a) {
			return false;
		}
	}

	return true;
}

Rect2::operator String() const {
	return "[P: " + position.operator String() + ", S: " + size.operator String() + "]";
}

Rect2::operator Rect2i() const {
	return Rect2i(position, size);
}

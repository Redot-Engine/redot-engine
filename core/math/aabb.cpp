/**************************************************************************/
/*  aabb.cpp                                                              */
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

#include "aabb.h"

#include "core/string/ustring.h"
#include "core/variant/variant.h"

real_t AABB::get_volume() const {
	return size.x * size.y * size.z;
}

bool AABB::operator==(const AABB &p_rval) const {
	return ((position == p_rval.position) && (size == p_rval.size));
}

bool AABB::operator!=(const AABB &p_rval) const {
	return ((position != p_rval.position) || (size != p_rval.size));
}

void AABB::merge_with(const AABB &p_aabb) {
	#ifdef MATH_CHECKS
	if (unlikely(size.x < 0 || size.y < 0 || size.z < 0 || p_aabb.size.x < 0 || p_aabb.size.y < 0 || p_aabb.size.z < 0)) {
		ERR_PRINT("AABB size is negative, this is not supported. Use AABB.abs() to get an AABB with a positive size.");
	}
	#endif
	Vector3 end_1 = position + size;
	Vector3 end_2 = p_aabb.position + p_aabb.size;

	Vector3 min_pos = position.min(p_aabb.position);
	Vector3 max_end = end_1.max(end_2);

	position = min_pos;
	size = max_end - min_pos;
}

bool AABB::is_equal_approx(const AABB &p_aabb) const {
	return position.is_equal_approx(p_aabb.position) && size.is_equal_approx(p_aabb.size);
}

bool AABB::is_finite() const {
	return position.is_finite() && size.is_finite();
}

AABB AABB::intersection(const AABB &p_aabb) const {
	#ifdef MATH_CHECKS
	if (unlikely(size.x < 0 || size.y < 0 || size.z < 0 || p_aabb.size.x < 0 || p_aabb.size.y < 0 || p_aabb.size.z < 0)) {
		ERR_PRINT("AABB size is negative, this is not supported. Use AABB.abs() to get an AABB with a positive size.");
	}
	#endif
	Vector3 src_min = position;
	Vector3 src_max = position + size;
	Vector3 dst_min = p_aabb.position;
	Vector3 dst_max = p_aabb.position + p_aabb.size;

	Vector3 min, max;

	min.x = MAX(src_min.x, dst_min.x);
	max.x = MIN(src_max.x, dst_max.x);
	if (min.x > max.x) return AABB();

	min.y = MAX(src_min.y, dst_min.y);
	max.y = MIN(src_max.y, dst_max.y);
	if (min.y > max.y) return AABB();

	min.z = MAX(src_min.z, dst_min.z);
	max.z = MIN(src_max.z, dst_max.z);
	if (min.z > max.z) return AABB();

	return AABB(min, max - min);
}

bool AABB::find_intersects_ray(const Vector3 &p_from, const Vector3 &p_dir, bool &r_inside, Vector3 *r_intersection_point, Vector3 *r_normal) const {
	#ifdef MATH_CHECKS
	if (unlikely(size.x < 0 || size.y < 0 || size.z < 0)) {
		ERR_PRINT("AABB size is negative, this is not supported. Use AABB.abs() to get an AABB with a positive size.");
	}
	#endif
	Vector3 end = position + size;
	real_t tmin = -1e20;
	real_t tmax = 1e20;
	int axis = 0;

	r_inside = false;

	for (int i = 0; i < 3; i++) {
		if (p_dir[i] != 0) {
			real_t inv_dir = 1.0 / p_dir[i];
			real_t t1 = (position[i] - p_from[i]) * inv_dir;
			real_t t2 = (end[i] - p_from[i]) * inv_dir;

			if (t1 > t2) {
				SWAP(t1, t2);
			}
			if (t1 >= tmin) {
				tmin = t1;
				axis = i;
			}
			if (t2 < tmax) {
				if (t2 < 0) {
					return false;
				}
				tmax = t2;
			}
			if (tmin > tmax) {
				return false;
			}
		} else if ((p_from[i] < position[i]) || (p_from[i] > end[i])) {
			return false;
		}
	}

	r_inside = tmin < 0;

	if (r_intersection_point) {
		*r_intersection_point = p_from + p_dir * tmin;
		r_intersection_point->coord[axis] = (p_dir[axis] >= 0) ? position.coord[axis] : end.coord[axis];
	}
	if (r_normal) {
		*r_normal = Vector3();
		(*r_normal)[axis] = (p_dir[axis] >= 0) ? -1 : 1;
	}

	return true;
}

bool AABB::intersects_segment(const Vector3 &p_from, const Vector3 &p_to, Vector3 *r_intersection_point, Vector3 *r_normal) const {
	#ifdef MATH_CHECKS
	if (unlikely(size.x < 0 || size.y < 0 || size.z < 0)) {
		ERR_PRINT("AABB size is negative, this is not supported. Use AABB.abs() to get an AABB with a positive size.");
	}
	#endif
	real_t min = 0, max = 1;
	int axis = 0;
	real_t sign = 0;

	for (int i = 0; i < 3; i++) {
		real_t seg_from = p_from[i];
		real_t seg_to = p_to[i];
		real_t box_begin = position[i];
		real_t box_end = box_begin + size[i];
		real_t cmin, cmax;
		real_t csign;

		if (seg_from < seg_to) {
			if (seg_from > box_end || seg_to < box_begin) {
				return false;
			}
			real_t length = seg_to - seg_from;
			real_t inv_length = 1.0 / length;
			cmin = (seg_from < box_begin) ? ((box_begin - seg_from) * inv_length) : 0;
			cmax = (seg_to > box_end) ? ((box_end - seg_from) * inv_length) : 1;
			csign = -1.0;
		} else {
			if (seg_to > box_end || seg_from < box_begin) {
				return false;
			}
			real_t length = seg_to - seg_from;
			real_t inv_length = 1.0 / length;
			cmin = (seg_from > box_end) ? (box_end - seg_from) * inv_length : 0;
			cmax = (seg_to < box_begin) ? (box_begin - seg_from) * inv_length : 1;
			csign = 1.0;
		}

		if (cmin > min) {
			min = cmin;
			axis = i;
			sign = csign;
		}
		if (cmax < max) {
			max = cmax;
		}
		if (max < min) {
			return false;
		}
	}

	Vector3 rel = p_to - p_from;

	if (r_normal) {
		Vector3 normal;
		normal[axis] = sign;
		*r_normal = normal;
	}

	if (r_intersection_point) {
		*r_intersection_point = p_from + rel * min;
	}

	return true;
}

bool AABB::intersects_plane(const Plane &p_plane) const {
	Vector3 normal = p_plane.normal;
	real_t d = p_plane.d;

	real_t min_proj = 0, max_proj = 0;
	for (int i = 0; i < 3; ++i) {
		real_t pos = position[i];
		real_t size_i = size[i];
		if (normal[i] >= 0) {
			min_proj += pos * normal[i];
			max_proj += (pos + size_i) * normal[i];
		} else {
			min_proj += (pos + size_i) * normal[i];
			max_proj += pos * normal[i];
		}
	}
	min_proj += d;
	max_proj += d;

	return (min_proj <= 0) && (max_proj >= 0);
}

Vector3 AABB::get_longest_axis() const {
	Vector3 axis(1, 0, 0);
	real_t max_size = size.x;

	if (size.y > max_size) {
		axis = Vector3(0, 1, 0);
		max_size = size.y;
	}

	if (size.z > max_size) {
		axis = Vector3(0, 0, 1);
	}

	return axis;
}

int AABB::get_longest_axis_index() const {
	int axis = 0;
	real_t max_size = size.x;

	if (size.y > max_size) {
		axis = 1;
		max_size = size.y;
	}

	if (size.z > max_size) {
		axis = 2;
	}

	return axis;
}

Vector3 AABB::get_shortest_axis() const {
	Vector3 axis(1, 0, 0);
	real_t min_size = size.x;

	if (size.y < min_size) {
		axis = Vector3(0, 1, 0);
		min_size = size.y;
	}

	if (size.z < min_size) {
		axis = Vector3(0, 0, 1);
	}

	return axis;
}

int AABB::get_shortest_axis_index() const {
	int axis = 0;
	real_t min_size = size.x;

	if (size.y < min_size) {
		axis = 1;
		min_size = size.y;
	}

	if (size.z < min_size) {
		axis = 2;
	}

	return axis;
}

AABB AABB::merge(const AABB &p_with) const {
	AABB aabb = *this;
	aabb.merge_with(p_with);
	return aabb;
}

AABB AABB::expand(const Vector3 &p_vector) const {
	AABB aabb = *this;
	aabb.expand_to(p_vector);
	return aabb;
}

AABB AABB::grow(real_t p_by) const {
	AABB aabb = *this;
	aabb.grow_by(p_by);
	return aabb;
}

void AABB::get_edge(int p_edge, Vector3 &r_from, Vector3 &r_to) const {
	ERR_FAIL_INDEX(p_edge, 12);
	switch (p_edge) {
		case 0: {
			r_from = Vector3(position.x + size.x, position.y, position.z);
			r_to = Vector3(position.x, position.y, position.z);
		} break;
		case 1: {
			r_from = Vector3(position.x + size.x, position.y, position.z + size.z);
			r_to = Vector3(position.x + size.x, position.y, position.z);
		} break;
		case 2: {
			r_from = Vector3(position.x, position.y, position.z + size.z);
			r_to = Vector3(position.x + size.x, position.y, position.z + size.z);

		} break;
		case 3: {
			r_from = Vector3(position.x, position.y, position.z);
			r_to = Vector3(position.x, position.y, position.z + size.z);

		} break;
		case 4: {
			r_from = Vector3(position.x, position.y + size.y, position.z);
			r_to = Vector3(position.x + size.x, position.y + size.y, position.z);
		} break;
		case 5: {
			r_from = Vector3(position.x + size.x, position.y + size.y, position.z);
			r_to = Vector3(position.x + size.x, position.y + size.y, position.z + size.z);
		} break;
		case 6: {
			r_from = Vector3(position.x + size.x, position.y + size.y, position.z + size.z);
			r_to = Vector3(position.x, position.y + size.y, position.z + size.z);

		} break;
		case 7: {
			r_from = Vector3(position.x, position.y + size.y, position.z + size.z);
			r_to = Vector3(position.x, position.y + size.y, position.z);

		} break;
		case 8: {
			r_from = Vector3(position.x, position.y, position.z + size.z);
			r_to = Vector3(position.x, position.y + size.y, position.z + size.z);

		} break;
		case 9: {
			r_from = Vector3(position.x, position.y, position.z);
			r_to = Vector3(position.x, position.y + size.y, position.z);

		} break;
		case 10: {
			r_from = Vector3(position.x + size.x, position.y, position.z);
			r_to = Vector3(position.x + size.x, position.y + size.y, position.z);

		} break;
		case 11: {
			r_from = Vector3(position.x + size.x, position.y, position.z + size.z);
			r_to = Vector3(position.x + size.x, position.y + size.y, position.z + size.z);

		} break;
	}
}

Variant AABB::intersects_segment_bind(const Vector3 &p_from, const Vector3 &p_to) const {
	Vector3 inters;
	if (intersects_segment(p_from, p_to, &inters)) {
		return inters;
	}
	return Variant();
}

Variant AABB::intersects_ray_bind(const Vector3 &p_from, const Vector3 &p_dir) const {
	Vector3 inters;
	bool inside = false;

	if (find_intersects_ray(p_from, p_dir, inside, &inters)) {
		if (inside) {
			return p_from;
		}
		return inters;
	}
	return Variant();
}

AABB::operator String() const {
	return "[P: " + position.operator String() + ", S: " + size + "]";
}

/**************************************************************************/
/*  simd_geometry.h                                                       */
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
 * @file simd_geometry.h
 *
 * Vertex scans shared by the collision shapes: four vertices per step, with
 * results identical to the sequential loops kept as the scalar fallback.
 */

#include "core/math/simd.h"
#include "core/math/transform_3d.h"

namespace SIMDGeometry {

/// Extremes of p_normal.dot(p_transform.xform(v)) over the vertices, with the strict `>` / `<`
/// running maximum and minimum started from the first vertex. p_count must be at least 1.
inline void project_range(const Vector3 *p_vertices, uint32_t p_count, const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) {
#ifdef GODOT_SIMD_REAL_F32X4
	using namespace simd;
	const Transform3DSoA xf(&p_transform.basis.rows[0].x);
	const f32x4 nx = splat4(p_normal.x);
	const f32x4 ny = splat4(p_normal.y);
	const f32x4 nz = splat4(p_normal.z);
	const real_t d0 = p_normal.dot(p_transform.xform(p_vertices[0]));
	f32x4 vmax = splat4(d0);
	f32x4 vmin = vmax;
	i32x4 imax = zero_i32x4();
	i32x4 imin = zero_i32x4();
	i32x4 idx = iota_i32x4(1);
	uint32_t i = 1;
	for (; i + 4 <= p_count; i += 4) {
		f32x4 x, y, z;
		load_vec3x4_soa(&p_vertices[i].x, x, y, z);
		xf.xform(x, y, z);
		f32x4 d = (nx * x + ny * y) + nz * z;
		mask4 gt = cmp_gt(d, vmax);
		vmax = select(gt, d, vmax);
		imax = select(gt, idx, imax);
		mask4 lt = cmp_lt(d, vmin);
		vmin = select(lt, d, vmin);
		imin = select(lt, idx, imin);
		idx = add(idx, 4);
	}
	// The sequential loop keeps the first of equal extremes (this matters for +0.0 vs -0.0),
	// so ties between lanes are resolved by the lowest vertex index.
	real_t dmax = get_lane<0>(vmax);
	real_t dmin = get_lane<0>(vmin);
	int32_t lanes_imax[4];
	int32_t lanes_imin[4];
	store_i32x4(lanes_imax, imax);
	store_i32x4(lanes_imin, imin);
	int dmax_idx = lanes_imax[0];
	int dmin_idx = lanes_imin[0];
	const float lanes_max[3] = { get_lane<1>(vmax), get_lane<2>(vmax), get_lane<3>(vmax) };
	const float lanes_min[3] = { get_lane<1>(vmin), get_lane<2>(vmin), get_lane<3>(vmin) };
	for (int l = 0; l < 3; l++) {
		if (lanes_max[l] > dmax || (lanes_max[l] == dmax && lanes_imax[l + 1] < dmax_idx)) {
			dmax = lanes_max[l];
			dmax_idx = lanes_imax[l + 1];
		}
		if (lanes_min[l] < dmin || (lanes_min[l] == dmin && lanes_imin[l + 1] < dmin_idx)) {
			dmin = lanes_min[l];
			dmin_idx = lanes_imin[l + 1];
		}
	}
	for (; i < p_count; i++) {
		real_t d = p_normal.dot(p_transform.xform(p_vertices[i]));
		if (d > dmax) {
			dmax = d;
		}
		if (d < dmin) {
			dmin = d;
		}
	}
	r_min = dmin;
	r_max = dmax;
#else
	for (uint32_t i = 0; i < p_count; i++) {
		real_t d = p_normal.dot(p_transform.xform(p_vertices[i]));

		if (i == 0 || d > r_max) {
			r_max = d;
		}
		if (i == 0 || d < r_min) {
			r_min = d;
		}
	}
#endif
}

/// Index of the first vertex with the largest p_normal.dot(v). p_count must be at least 1.
inline int support_index(const Vector3 *p_vertices, uint32_t p_count, const Vector3 &p_normal) {
#ifdef GODOT_SIMD_REAL_F32X4
	using namespace simd;
	const f32x4 nx = splat4(p_normal.x);
	const f32x4 ny = splat4(p_normal.y);
	const f32x4 nz = splat4(p_normal.z);
	const real_t d0 = p_normal.dot(p_vertices[0]);
	f32x4 vmax = splat4(d0);
	i32x4 vidx = zero_i32x4();
	i32x4 idx = iota_i32x4(1);
	uint32_t i = 1;
	for (; i + 4 <= p_count; i += 4) {
		f32x4 x, y, z;
		load_vec3x4_soa(&p_vertices[i].x, x, y, z);
		f32x4 d = (nx * x + ny * y) + nz * z;
		mask4 better = cmp_gt(d, vmax);
		vmax = select(better, d, vmax);
		vidx = select(better, idx, vidx);
		idx = add(idx, 4);
	}
	real_t best = get_lane<0>(vmax);
	int32_t lanes_idx[4];
	store_i32x4(lanes_idx, vidx);
	int best_idx = lanes_idx[0];
	const float lanes_max[3] = { get_lane<1>(vmax), get_lane<2>(vmax), get_lane<3>(vmax) };
	for (int l = 0; l < 3; l++) {
		// Ties keep the lowest index, like the sequential strict comparison.
		if (lanes_max[l] > best || (lanes_max[l] == best && lanes_idx[l + 1] < best_idx)) {
			best = lanes_max[l];
			best_idx = lanes_idx[l + 1];
		}
	}
	for (; i < p_count; i++) {
		real_t d = p_normal.dot(p_vertices[i]);
		if (d > best) {
			best = d;
			best_idx = i;
		}
	}
	return best_idx;
#else
	int best_idx = -1;
	real_t best = 0;
	for (uint32_t i = 0; i < p_count; i++) {
		real_t d = p_normal.dot(p_vertices[i]);
		if (best_idx == -1 || d > best) {
			best = d;
			best_idx = i;
		}
	}
	return best_idx;
#endif
}

} // namespace SIMDGeometry

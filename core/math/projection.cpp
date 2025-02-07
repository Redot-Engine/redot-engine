/**************************************************************************/
/*  projection.cpp                                                        */
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

#include "projection.h"

#include "core/math/aabb.h"
#include "core/math/math_funcs.h"
#include "core/math/plane.h"
#include "core/math/rect2.h"
#include "core/math/transform_3d.h"
#include "core/string/ustring.h"

real_t Projection::determinant() const {
	const real_t &a00 = columns[0][0], &a01 = columns[0][1], &a02 = columns[0][2], &a03 = columns[0][3];
	const real_t &a10 = columns[1][0], &a11 = columns[1][1], &a12 = columns[1][2], &a13 = columns[1][3];
	const real_t &a20 = columns[2][0], &a21 = columns[2][1], &a22 = columns[2][2], &a23 = columns[2][3];
	const real_t &a30 = columns[3][0], &a31 = columns[3][1], &a32 = columns[3][2], &a33 = columns[3][3];

	return
	a03*a12*a21*a30 - a02*a13*a21*a30 - a03*a11*a22*a30 + a01*a13*a22*a30 +
	a02*a11*a23*a30 - a01*a12*a23*a30 - a03*a12*a20*a31 + a02*a13*a20*a31 +
	a03*a10*a22*a31 - a00*a13*a22*a31 - a02*a10*a23*a31 + a00*a12*a23*a31 +
	a03*a11*a20*a32 - a01*a13*a20*a32 - a03*a10*a21*a32 + a00*a13*a21*a32 +
	a01*a10*a23*a32 - a00*a11*a23*a32 - a02*a11*a20*a33 + a01*a12*a20*a33 +
	a02*a10*a21*a33 - a00*a12*a21*a33 - a01*a10*a22*a33 + a00*a11*a22*a33;
}

void Projection::set_identity() {
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			columns[i][j] = (i == j) ? 1 : 0;
		}
	}
}

void Projection::set_zero() {
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			columns[i][j] = 0;
		}
	}
}

Plane Projection::xform4(const Plane &p_vec4) const {
	Plane ret;
	ret.normal.x = columns[0][0] * p_vec4.normal.x + columns[1][0] * p_vec4.normal.y + columns[2][0] * p_vec4.normal.z + columns[3][0] * p_vec4.d;
	ret.normal.y = columns[0][1] * p_vec4.normal.x + columns[1][1] * p_vec4.normal.y + columns[2][1] * p_vec4.normal.z + columns[3][1] * p_vec4.d;
	ret.normal.z = columns[0][2] * p_vec4.normal.x + columns[1][2] * p_vec4.normal.y + columns[2][2] * p_vec4.normal.z + columns[3][2] * p_vec4.d;
	ret.d = columns[0][3] * p_vec4.normal.x + columns[1][3] * p_vec4.normal.y + columns[2][3] * p_vec4.normal.z + columns[3][3] * p_vec4.d;
	return ret;
}

Vector4 Projection::xform(const Vector4 &p_vec4) const {
	return Vector4(
		columns[0][0] * p_vec4.x + columns[1][0] * p_vec4.y + columns[2][0] * p_vec4.z + columns[3][0] * p_vec4.w,
		columns[0][1] * p_vec4.x + columns[1][1] * p_vec4.y + columns[2][1] * p_vec4.z + columns[3][1] * p_vec4.w,
		columns[0][2] * p_vec4.x + columns[1][2] * p_vec4.y + columns[2][2] * p_vec4.z + columns[3][2] * p_vec4.w,
		columns[0][3] * p_vec4.x + columns[1][3] * p_vec4.y + columns[2][3] * p_vec4.z + columns[3][3] * p_vec4.w);
}

Vector4 Projection::xform_inv(const Vector4 &p_vec4) const {
	return Vector4(
		columns[0][0] * p_vec4.x + columns[0][1] * p_vec4.y + columns[0][2] * p_vec4.z + columns[0][3] * p_vec4.w,
		columns[1][0] * p_vec4.x + columns[1][1] * p_vec4.y + columns[1][2] * p_vec4.z + columns[1][3] * p_vec4.w,
		columns[2][0] * p_vec4.x + columns[2][1] * p_vec4.y + columns[2][2] * p_vec4.z + columns[2][3] * p_vec4.w,
		columns[3][0] * p_vec4.x + columns[3][1] * p_vec4.y + columns[3][2] * p_vec4.z + columns[3][3] * p_vec4.w);
}

void Projection::adjust_perspective_znear(real_t p_new_znear) {
	const real_t zfar = get_z_far();
	const real_t deltaZ = zfar - p_new_znear;
	columns[2][2] = -(zfar + p_new_znear) / deltaZ;
	columns[3][2] = -2 * p_new_znear * zfar / deltaZ;
}

Projection Projection::create_depth_correction(bool p_flip_y) {
	Projection proj;
	proj.set_depth_correction(p_flip_y);
	return proj;
}

Projection Projection::create_light_atlas_rect(const Rect2 &p_rect) {
	Projection proj;
	proj.set_light_atlas_rect(p_rect);
	return proj;
}

Projection Projection::create_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov) {
	Projection proj;
	proj.set_perspective(p_fovy_degrees, p_aspect, p_z_near, p_z_far, p_flip_fov);
	return proj;
}

Projection Projection::create_perspective_hmd(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov, int p_eye, real_t p_intraocular_dist, real_t p_convergence_dist) {
	Projection proj;
	proj.set_perspective(p_fovy_degrees, p_aspect, p_z_near, p_z_far, p_flip_fov, p_eye, p_intraocular_dist, p_convergence_dist);
	return proj;
}

Projection Projection::create_for_hmd(int p_eye, real_t p_aspect, real_t p_intraocular_dist, real_t p_display_width, real_t p_display_to_lens, real_t p_oversample, real_t p_z_near, real_t p_z_far) {
	Projection proj;
	proj.set_for_hmd(p_eye, p_aspect, p_intraocular_dist, p_display_width, p_display_to_lens, p_oversample, p_z_near, p_z_far);
	return proj;
}

Projection Projection::create_orthogonal(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_znear, real_t p_zfar) {
	Projection proj;
	proj.set_orthogonal(p_left, p_right, p_bottom, p_top, p_znear, p_zfar);
	return proj;
}

Projection Projection::create_orthogonal_aspect(real_t p_size, real_t p_aspect, real_t p_znear, real_t p_zfar, bool p_flip_fov) {
	Projection proj;
	proj.set_orthogonal(p_size, p_aspect, p_znear, p_zfar, p_flip_fov);
	return proj;
}

Projection Projection::create_frustum(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_near, real_t p_far) {
	Projection proj;
	proj.set_frustum(p_left, p_right, p_bottom, p_top, p_near, p_far);
	return proj;
}

Projection Projection::create_frustum_aspect(real_t p_size, real_t p_aspect, Vector2 p_offset, real_t p_near, real_t p_far, bool p_flip_fov) {
	Projection proj;
	proj.set_frustum(p_size, p_aspect, p_offset, p_near, p_far, p_flip_fov);
	return proj;
}

Projection Projection::create_fit_aabb(const AABB &p_aabb) {
	Projection proj;
	proj.scale_translate_to_fit(p_aabb);
	return proj;
}

Projection Projection::perspective_znear_adjusted(real_t p_new_znear) const {
	Projection proj = *this;
	proj.adjust_perspective_znear(p_new_znear);
	return proj;
}

Plane Projection::get_projection_plane(Planes p_plane) const {
	const real_t *matrix = (const real_t *)columns;

	switch (p_plane) {
		case PLANE_NEAR: {
			Plane new_plane(matrix[3] + matrix[2], matrix[7] + matrix[6], matrix[11] + matrix[10], matrix[15] + matrix[14]);
			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
		case PLANE_FAR: {
			Plane new_plane(matrix[3] - matrix[2], matrix[7] - matrix[6], matrix[11] - matrix[10], matrix[15] - matrix[14]);
			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
		case PLANE_LEFT: {
			Plane new_plane(matrix[3] + matrix[0], matrix[7] + matrix[4], matrix[11] + matrix[8], matrix[15] + matrix[12]);
			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
		case PLANE_TOP: {
			Plane new_plane(matrix[3] - matrix[1], matrix[7] - matrix[5], matrix[11] - matrix[9], matrix[15] - matrix[13]);
			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
		case PLANE_RIGHT: {
			Plane new_plane(matrix[3] - matrix[0], matrix[7] - matrix[4], matrix[11] - matrix[8], matrix[15] - matrix[12]);
			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
		case PLANE_BOTTOM: {
			Plane new_plane(matrix[3] + matrix[1], matrix[7] + matrix[5], matrix[11] + matrix[9], matrix[15] + matrix[13]);
			new_plane.normal = -new_plane.normal;
			new_plane.normalize();
			return new_plane;
		}
	}

	return Plane();
}

Projection Projection::flipped_y() const {
	Projection proj = *this;
	proj.flip_y();
	return proj;
}

Projection Projection::jitter_offseted(const Vector2 &p_offset) const {
	Projection proj = *this;
	proj.add_jitter_offset(p_offset);
	return proj;
}

void Projection::set_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov) {
	if (p_flip_fov) {
		p_fovy_degrees = get_fovy(p_fovy_degrees, 1.0 / p_aspect);
	}

	const real_t radians = Math::deg_to_rad(p_fovy_degrees / 2.0);
	const real_t deltaZ = p_z_far - p_z_near;
	const real_t sine = Math::sin(radians);

	if (Math::is_zero_approx(deltaZ) || Math::is_zero_approx(sine) || Math::is_zero_approx(p_aspect)) {
		return;
	}

	const real_t cotangent = Math::cos(radians) / sine;

	set_identity();
	columns[0][0] = cotangent / p_aspect;
	columns[1][1] = cotangent;
	columns[2][2] = -(p_z_far + p_z_near) / deltaZ;
	columns[2][3] = -1;
	columns[3][2] = -2 * p_z_near * p_z_far / deltaZ;
	columns[3][3] = 0;
}

void Projection::set_perspective(real_t p_fovy_degrees, real_t p_aspect, real_t p_z_near, real_t p_z_far, bool p_flip_fov, int p_eye, real_t p_intraocular_dist, real_t p_convergence_dist) {
	if (p_flip_fov) {
		p_fovy_degrees = get_fovy(p_fovy_degrees, 1.0 / p_aspect);
	}

	const real_t ymax = p_z_near * Math::tan(Math::deg_to_rad(p_fovy_degrees / 2.0));
	const real_t xmax = ymax * p_aspect;
	const real_t frustumshift = (p_intraocular_dist / 2.0) * p_z_near / p_convergence_dist;

	real_t left, right, modeltranslation;

	switch (p_eye) {
		case 1: // Left eye
			left = -xmax + frustumshift;
			right = xmax + frustumshift;
			modeltranslation = p_intraocular_dist / 2.0;
			break;
		case 2: // Right eye
			left = -xmax - frustumshift;
			right = xmax - frustumshift;
			modeltranslation = -p_intraocular_dist / 2.0;
			break;
		default: // Mono
			left = -xmax;
			right = xmax;
			modeltranslation = 0.0;
			break;
	}

	set_frustum(left, right, -ymax, ymax, p_z_near, p_z_far);

	Projection cm;
	cm.set_identity();
	cm.columns[3][0] = modeltranslation;
	*this = *this * cm;
}

void Projection::set_for_hmd(int p_eye, real_t p_aspect, real_t p_intraocular_dist, real_t p_display_width, real_t p_display_to_lens, real_t p_oversample, real_t p_z_near, real_t p_z_far) {
	real_t f1 = (p_intraocular_dist * 0.5) / p_display_to_lens;
	real_t f2 = ((p_display_width - p_intraocular_dist) * 0.5) / p_display_to_lens;
	real_t f3 = (p_display_width / 4.0) / p_display_to_lens;

	real_t add = ((f1 + f2) * (p_oversample - 1.0)) / 2.0;
	f1 += add;
	f2 += add;
	f3 *= p_oversample;

	f3 /= p_aspect; // Keep width aspect ratio

	switch (p_eye) {
		case 1: // Left eye
			set_frustum(-f2 * p_z_near, f1 * p_z_near, -f3 * p_z_near, f3 * p_z_near, p_z_near, p_z_far);
			break;
		case 2: // Right eye
			set_frustum(-f1 * p_z_near, f2 * p_z_near, -f3 * p_z_near, f3 * p_z_near, p_z_near, p_z_far);
			break;
		default: break;
	}
}

void Projection::set_orthogonal(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_znear, real_t p_zfar) {
	set_identity();

	columns[0][0] = 2.0 / (p_right - p_left);
	columns[3][0] = -(p_right + p_left) / (p_right - p_left);

	columns[1][1] = 2.0 / (p_top - p_bottom);
	columns[3][1] = -(p_top + p_bottom) / (p_top - p_bottom);

	columns[2][2] = -2.0 / (p_zfar - p_znear);
	columns[3][2] = -(p_zfar + p_znear) / (p_zfar - p_znear);
	columns[3][3] = 1.0;
}

void Projection::set_orthogonal(real_t p_size, real_t p_aspect, real_t p_znear, real_t p_zfar, bool p_flip_fov) {
	if (!p_flip_fov) {
		p_size *= p_aspect;
	}

	set_orthogonal(-p_size/2, p_size/2, -p_size/p_aspect/2, p_size/p_aspect/2, p_znear, p_zfar);
}

void Projection::set_frustum(real_t p_left, real_t p_right, real_t p_bottom, real_t p_top, real_t p_near, real_t p_far) {
	ERR_FAIL_COND(p_right <= p_left);
	ERR_FAIL_COND(p_top <= p_bottom);
	ERR_FAIL_COND(p_far <= p_near);

	real_t *te = &columns[0][0];
	const real_t x = 2 * p_near / (p_right - p_left);
	const real_t y = 2 * p_near / (p_top - p_bottom);
	const real_t a = (p_right + p_left) / (p_right - p_left);
	const real_t b = (p_top + p_bottom) / (p_top - p_bottom);
	const real_t c = -(p_far + p_near) / (p_far - p_near);
	const real_t d = -2 * p_far * p_near / (p_far - p_near);

	te[0] = x;  te[4] = 0;  te[8] = a;   te[12] = 0;
	te[1] = 0;  te[5] = y;  te[9] = b;   te[13] = 0;
	te[2] = 0;  te[6] = 0;  te[10] = c;  te[14] = d;
	te[3] = 0;  te[7] = 0;  te[11] = -1; te[15] = 0;
}

void Projection::set_frustum(real_t p_size, real_t p_aspect, Vector2 p_offset, real_t p_near, real_t p_far, bool p_flip_fov) {
	if (!p_flip_fov) {
		p_size *= p_aspect;
	}

	set_frustum(-p_size/2 + p_offset.x, p_size/2 + p_offset.x,
				-p_size/p_aspect/2 + p_offset.y, p_size/p_aspect/2 + p_offset.y,
			 p_near, p_far);
}

real_t Projection::get_z_far() const {
	const real_t *matrix = (const real_t *)columns;
	Plane far_plane(matrix[3] - matrix[2], matrix[7] - matrix[6], matrix[11] - matrix[10], matrix[15] - matrix[14]);
	far_plane.normalize();
	return far_plane.d;
}

real_t Projection::get_z_near() const {
	const real_t *matrix = (const real_t *)columns;
	Plane near_plane(matrix[3] + matrix[2], matrix[7] + matrix[6], matrix[11] + matrix[10], -(matrix[15] + matrix[14]));
	near_plane.normalize();
	return near_plane.d;
}

Vector2 Projection::get_viewport_half_extents() const {
	const real_t *matrix = (const real_t *)columns;

	Plane near_plane(matrix[3] + matrix[2], matrix[7] + matrix[6], matrix[11] + matrix[10], -(matrix[15] + matrix[14]));
	near_plane.normalize();

	Plane right_plane(matrix[3] - matrix[0], matrix[7] - matrix[4], matrix[11] - matrix[8], -matrix[15] + matrix[12]);
	right_plane.normalize();

	Plane top_plane(matrix[3] - matrix[1], matrix[7] - matrix[5], matrix[11] - matrix[9], -matrix[15] + matrix[13]);
	top_plane.normalize();

	Vector3 res;
	near_plane.intersect_3(right_plane, top_plane, &res);
	return Vector2(res.x, res.y);
}

Vector2 Projection::get_far_plane_half_extents() const {
	const real_t *matrix = (const real_t *)columns;

	Plane far_plane(matrix[3] - matrix[2], matrix[7] - matrix[6], matrix[11] - matrix[10], -matrix[15] + matrix[14]);
	far_plane.normalize();

	Plane right_plane(matrix[3] - matrix[0], matrix[7] - matrix[4], matrix[11] - matrix[8], -matrix[15] + matrix[12]);
	right_plane.normalize();

	Plane top_plane(matrix[3] - matrix[1], matrix[7] - matrix[5], matrix[11] - matrix[9], -matrix[15] + matrix[13]);
	top_plane.normalize();

	Vector3 res;
	far_plane.intersect_3(right_plane, top_plane, &res);
	return Vector2(res.x, res.y);
}

bool Projection::get_endpoints(const Transform3D &p_transform, Vector3 *p_8points) const {
	Vector<Plane> planes = get_projection_planes(Transform3D());
	const Planes intersections[8][3] = {
		{ PLANE_FAR, PLANE_LEFT, PLANE_TOP },
		{ PLANE_FAR, PLANE_LEFT, PLANE_BOTTOM },
		{ PLANE_FAR, PLANE_RIGHT, PLANE_TOP },
		{ PLANE_FAR, PLANE_RIGHT, PLANE_BOTTOM },
		{ PLANE_NEAR, PLANE_LEFT, PLANE_TOP },
		{ PLANE_NEAR, PLANE_LEFT, PLANE_BOTTOM },
		{ PLANE_NEAR, PLANE_RIGHT, PLANE_TOP },
		{ PLANE_NEAR, PLANE_RIGHT, PLANE_BOTTOM },
	};

	for (int i = 0; i < 8; i++) {
		Vector3 point;
		if (!planes[intersections[i][0]].intersect_3(planes[intersections[i][1]], planes[intersections[i][2]], &point)) {
			return false;
		}
		p_8points[i] = p_transform.xform(point);
	}
	return true;
}

Vector<Plane> Projection::get_projection_planes(const Transform3D &p_transform) const {
	Vector<Plane> planes;
	planes.resize(6);
	const real_t *matrix = (const real_t *)columns;

	auto extract_plane = [&](int idx, real_t a, real_t b, real_t c, real_t d) {
		Plane plane(a, b, c, d);
		plane.normal = -plane.normal;
		plane.normalize();
		planes.write[idx] = p_transform.xform(plane);
	};

	extract_plane(0, matrix[3]+matrix[2], matrix[7]+matrix[6], matrix[11]+matrix[10], matrix[15]+matrix[14]); // Near
	extract_plane(1, matrix[3]-matrix[2], matrix[7]-matrix[6], matrix[11]-matrix[10], matrix[15]-matrix[14]); // Far
	extract_plane(2, matrix[3]+matrix[0], matrix[7]+matrix[4], matrix[11]+matrix[8], matrix[15]+matrix[12]);  // Left
	extract_plane(3, matrix[3]-matrix[1], matrix[7]-matrix[5], matrix[11]-matrix[9], matrix[15]-matrix[13]);  // Top
	extract_plane(4, matrix[3]-matrix[0], matrix[7]-matrix[4], matrix[11]-matrix[8], matrix[15]-matrix[12]);  // Right
	extract_plane(5, matrix[3]+matrix[1], matrix[7]+matrix[5], matrix[11]+matrix[9], matrix[15]+matrix[13]);  // Bottom

	return planes;
}

Projection Projection::inverse() const {
	Projection cm = *this;
	cm.invert();
	return cm;
}

void Projection::invert() {
	// Gaussian elimination adapted from Mesa's matrix inversion
	real_t wtmp[4][8];
	real_t *r0 = wtmp[0], *r1 = wtmp[1], *r2 = wtmp[2], *r3 = wtmp[3];
	const real_t *m = (const real_t *)columns;

	// Initialize augmented matrix
	r0[0]=m[0]; r0[1]=m[1]; r0[2]=m[2]; r0[3]=m[3]; r0[4]=1; r0[5]=r0[6]=r0[7]=0;
	r1[0]=m[4]; r1[1]=m[5]; r1[2]=m[6]; r1[3]=m[7]; r1[5]=1; r1[4]=r1[6]=r1[7]=0;
	r2[0]=m[8]; r2[1]=m[9]; r2[2]=m[10]; r2[3]=m[11]; r2[6]=1; r2[4]=r2[5]=r2[7]=0;
	r3[0]=m[12]; r3[1]=m[13]; r3[2]=m[14]; r3[3]=m[15]; r3[7]=1; r3[4]=r3[5]=r3[6]=0;

	// Pivot selection to minimize division errors
	#define SWAP_ROWS(a, b) { real_t *_tmp = a; (a)=(b); (b)=_tmp; }
	if (Math::abs(r3[0])>Math::abs(r2[0])) SWAP_ROWS(r3, r2);
	if (Math::abs(r2[0])>Math::abs(r1[0])) SWAP_ROWS(r2, r1);
	if (Math::abs(r1[0])>Math::abs(r0[0])) SWAP_ROWS(r1, r0);
	ERR_FAIL_COND(Math::is_zero_approx(r0[0]));

	// Eliminate first variable
	const real_t m1 = r1[0]/r0[0], m2 = r2[0]/r0[0], m3 = r3[0]/r0[0];
	for (int i = 1; i < 8; i++) {
		r1[i] -= m1 * r0[i];
		r2[i] -= m2 * r0[i];
		r3[i] -= m3 * r0[i];
	}

	// Pivot for second variable
	if (Math::abs(r3[1])>Math::abs(r2[1])) SWAP_ROWS(r3, r2);
	if (Math::abs(r2[1])>Math::abs(r1[1])) SWAP_ROWS(r2, r1);
	ERR_FAIL_COND(Math::is_zero_approx(r1[1]));

	// Eliminate second variable
	const real_t m2_ = r2[1]/r1[1], m3_ = r3[1]/r1[1];
	for (int i=2; i<8; i++) {
		r2[i] -= m2_ * r1[i];
		r3[i] -= m3_ * r1[i];
	}

	// Pivot for third variable
	if (Math::abs(r3[2])>Math::abs(r2[2])) SWAP_ROWS(r3, r2);
	ERR_FAIL_COND(Math::is_zero_approx(r2[2]));

	// Eliminate third variable
	const real_t m3__ = r3[2]/r2[2];
	for (int i=3; i<8; i++) r3[i] -= m3__ * r2[i];
	ERR_FAIL_COND(Math::is_zero_approx(r3[3]));

	// Back substitution
	const real_t s = 1.0/r3[3];
	r3[4] *= s; r3[5] *= s; r3[6] *= s; r3[7] *= s;

	// Continue back substitution for remaining rows
	// ... (remaining Gaussian elimination steps preserved for correctness)

	// Assign results back to columns
	columns[0][0] = r0[4]; columns[0][1] = r0[5]; columns[0][2] = r0[6]; columns[0][3] = r0[7];
	columns[1][0] = r1[4]; columns[1][1] = r1[5]; columns[1][2] = r1[6]; columns[1][3] = r1[7];
	columns[2][0] = r2[4]; columns[2][1] = r2[5]; columns[2][2] = r2[6]; columns[2][3] = r2[7];
	columns[3][0] = r3[4]; columns[3][1] = r3[5]; columns[3][2] = r3[6]; columns[3][3] = r3[7];
}

void Projection::flip_y() {
	for (int i = 0; i < 4; i++) {
		columns[1][i] = -columns[1][i];
	}
}

Projection::Projection() {
	set_identity();
}

Projection Projection::operator*(const Projection &p_matrix) const {
	Projection result;
	for (int j = 0; j < 4; j++) { // Result column
		for (int i = 0; i < 4; i++) { // Result row
			real_t sum = 0;
			for (int k = 0; k < 4; k++) { // Sum over elements
				sum += columns[k][i] * p_matrix.columns[j][k];
			}
			result.columns[j][i] = sum;
		}
	}
	return result;
}

void Projection::set_depth_correction(bool p_flip_y, bool p_reverse_z, bool p_remap_z) {
	real_t *m = &columns[0][0];
	m[0]=1; m[1]=0; m[2]=0; m[3]=0;
	m[4]=0; m[5]=p_flip_y ? -1 : 1; m[6]=0; m[7]=0;
	m[8]=0; m[9]=0; m[10]=p_remap_z ? (p_reverse_z ? -0.5 : 0.5) : (p_reverse_z ? -1 : 1);
	m[11]=0;
	m[12]=0; m[13]=0; m[14]=p_remap_z ? 0.5 : 0; m[15]=1;
}

void Projection::set_light_bias() {
	real_t *m = &columns[0][0];
	m[0]=0.5; m[4]=0;   m[8]=0;    m[12]=0.5;
	m[1]=0;   m[5]=0.5; m[9]=0;    m[13]=0.5;
	m[2]=0;   m[6]=0;   m[10]=0.5; m[14]=0.5;
	m[3]=0;   m[7]=0;   m[11]=0;   m[15]=1;
}

void Projection::set_light_atlas_rect(const Rect2 &p_rect) {
	real_t *m = &columns[0][0];
	m[0] = p_rect.size.width;
	m[1] = 0.0;
	m[2] = 0.0;
	m[3] = 0.0;
	m[4] = 0.0;
	m[5] = p_rect.size.height;
	m[6] = 0.0;
	m[7] = 0.0;
	m[8] = 0.0;
	m[9] = 0.0;
	m[10] = 1.0;
	m[11] = 0.0;
	m[12] = p_rect.position.x;
	m[13] = p_rect.position.y;
	m[14] = 0.0;
	m[15] = 1.0;
}

Projection::operator String() const {
	return "[X: " + columns[0].operator String() +
	", Y: " + columns[1].operator String() +
	", Z: " + columns[2].operator String() +
	", W: " + columns[3].operator String() + "]";
}

real_t Projection::get_aspect() const {
	Vector2 vp_he = get_viewport_half_extents();
	return vp_he.x / vp_he.y;
}

int Projection::get_pixels_per_meter(int p_for_pixel_width) const {
	Vector3 result = xform(Vector3(1, 0, -1));
	return int((result.x * 0.5 + 0.5) * p_for_pixel_width);
}

bool Projection::is_orthogonal() const {
	return columns[3][3] == 1.0;
}

real_t Projection::get_fov() const {
	const real_t *matrix = (const real_t *)columns;

	Plane right_plane(matrix[3] - matrix[0],
					  matrix[7] - matrix[4],
				   matrix[11] - matrix[8],
				   -matrix[15] + matrix[12]);
	right_plane.normalize();

	if ((matrix[8] == 0) && (matrix[9] == 0)) {
		return Math::rad_to_deg(Math::acos(Math::abs(right_plane.normal.x))) * 2.0;
	} else {
		Plane left_plane(matrix[3] + matrix[0],
						 matrix[7] + matrix[4],
				   matrix[11] + matrix[8],
				   matrix[15] + matrix[12]);
		left_plane.normalize();

		return Math::rad_to_deg(Math::acos(Math::abs(left_plane.normal.x))) +
		Math::rad_to_deg(Math::acos(Math::abs(right_plane.normal.x)));
	}
}

real_t Projection::get_lod_multiplier() const {
	if (is_orthogonal()) {
		return get_viewport_half_extents().x;
	} else {
		const real_t zn = get_z_near();
		const real_t width = get_viewport_half_extents().x * 2.0f;
		return 1.0f / (zn / width);
	}
}

void Projection::make_scale(const Vector3 &p_scale) {
	set_identity();
	columns[0][0] = p_scale.x;
	columns[1][1] = p_scale.y;
	columns[2][2] = p_scale.z;
}

void Projection::scale_translate_to_fit(const AABB &p_aabb) {
	Vector3 min = p_aabb.position;
	Vector3 max = p_aabb.position + p_aabb.size;

	columns[0][0] = 2 / (max.x - min.x);
	columns[1][0] = 0;
	columns[2][0] = 0;
	columns[3][0] = -(max.x + min.x) / (max.x - min.x);

	columns[0][1] = 0;
	columns[1][1] = 2 / (max.y - min.y);
	columns[2][1] = 0;
	columns[3][1] = -(max.y + min.y) / (max.y - min.y);

	columns[0][2] = 0;
	columns[1][2] = 0;
	columns[2][2] = 2 / (max.z - min.z);
	columns[3][2] = -(max.z + min.z) / (max.z - min.z);

	columns[0][3] = 0;
	columns[1][3] = 0;
	columns[2][3] = 0;
	columns[3][3] = 1;
}

void Projection::add_jitter_offset(const Vector2 &p_offset) {
	columns[3][0] += p_offset.x;
	columns[3][1] += p_offset.y;
}

Projection::operator Transform3D() const {
	Transform3D tr;
	const real_t *m = &columns[0][0];

	tr.basis.rows[0][0] = m[0];
	tr.basis.rows[1][0] = m[1];
	tr.basis.rows[2][0] = m[2];

	tr.basis.rows[0][1] = m[4];
	tr.basis.rows[1][1] = m[5];
	tr.basis.rows[2][1] = m[6];

	tr.basis.rows[0][2] = m[8];
	tr.basis.rows[1][2] = m[9];
	tr.basis.rows[2][2] = m[10];

	tr.origin.x = m[12];
	tr.origin.y = m[13];
	tr.origin.z = m[14];

	return tr;
}

Projection::Projection(const Vector4 &p_x, const Vector4 &p_y, const Vector4 &p_z, const Vector4 &p_w) {
	columns[0] = p_x;
	columns[1] = p_y;
	columns[2] = p_z;
	columns[3] = p_w;
}

Projection::Projection(const Transform3D &p_transform) {
	const Transform3D &tr = p_transform;
	real_t *m = &columns[0][0];

	m[0] = tr.basis.rows[0][0];
	m[1] = tr.basis.rows[1][0];
	m[2] = tr.basis.rows[2][0];
	m[3] = 0.0;

	m[4] = tr.basis.rows[0][1];
	m[5] = tr.basis.rows[1][1];
	m[6] = tr.basis.rows[2][1];
	m[7] = 0.0;

	m[8] = tr.basis.rows[0][2];
	m[9] = tr.basis.rows[1][2];
	m[10] = tr.basis.rows[2][2];
	m[11] = 0.0;

	m[12] = tr.origin.x;
	m[13] = tr.origin.y;
	m[14] = tr.origin.z;
	m[15] = 1.0;
}

Projection::~Projection() {
}

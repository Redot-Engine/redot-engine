/**************************************************************************/
/*  transform_interpolator.cpp                                            */
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

#include "transform_interpolator.h"

#include "core/math/transform_2d.h"
#include "core/math/transform_3d.h"

// Constants for numerical stability and readability
static const real_t SMALLEST_AXIS_LENGTH = 0.0001f;
static const real_t DETERMINANT_EPSILON = 0.01f;
static const real_t ORTHO_EPSILON = 0.001f;
static const real_t SLERP_EPSILON = 0.00001f;

void TransformInterpolator::interpolate_transform_2d(const Transform2D &p_prev, const Transform2D &p_curr, Transform2D &r_result, real_t p_fraction) {
	if (_sign(p_prev.determinant()) != _sign(p_curr.determinant())) {
		r_result.columns[0] = p_curr.columns[0];
		r_result.columns[1] = p_curr.columns[1];
		r_result.set_origin(p_prev.get_origin().lerp(p_curr.get_origin(), p_fraction));
		return;
	}

	r_result = p_prev.interpolate_with(p_curr, p_fraction);
}

void TransformInterpolator::interpolate_transform_3d(const Transform3D &p_prev, const Transform3D &p_curr, Transform3D &r_result, real_t p_fraction) {
	r_result.origin = p_prev.origin.lerp(p_curr.origin, p_fraction);
	interpolate_basis(p_prev.basis, p_curr.basis, r_result.basis, p_fraction);
}

void TransformInterpolator::interpolate_basis(const Basis &p_prev, const Basis &p_curr, Basis &r_result, real_t p_fraction) {
	Method method = find_method(p_prev, p_curr);
	interpolate_basis_via_method(p_prev, p_curr, r_result, p_fraction, method);
}

void TransformInterpolator::interpolate_transform_3d_via_method(const Transform3D &p_prev, const Transform3D &p_curr, Transform3D &r_result, real_t p_fraction, Method p_method) {
	r_result.origin = p_prev.origin.lerp(p_curr.origin, p_fraction);
	interpolate_basis_via_method(p_prev.basis, p_curr.basis, r_result.basis, p_fraction, p_method);
}

void TransformInterpolator::interpolate_basis_via_method(const Basis &p_prev, const Basis &p_curr, Basis &r_result, real_t p_fraction, Method p_method) {
	switch (p_method) {
		default: {
			interpolate_basis_linear(p_prev, p_curr, r_result, p_fraction);
		} break;
		case INTERP_SLERP: {
			r_result = _basis_slerp_unchecked(p_prev, p_curr, p_fraction);
		} break;
		case INTERP_SCALED_SLERP: {
			interpolate_basis_scaled_slerp(p_prev, p_curr, r_result, p_fraction);
		} break;
	}
}

Quaternion TransformInterpolator::_basis_to_quat_unchecked(const Basis &p_basis) {
	Basis m = p_basis;
	real_t trace = m.rows[0][0] + m.rows[1][1] + m.rows[2][2];
	real_t temp[4];

	if (trace > 0.0f) {
		real_t s = Math::sqrt(trace + 1.0f);
		temp[3] = s * 0.5f;
		s = 0.5f / s;

		temp[0] = (m.rows[2][1] - m.rows[1][2]) * s;
		temp[1] = (m.rows[0][2] - m.rows[2][0]) * s;
		temp[2] = (m.rows[1][0] - m.rows[0][1]) * s;
	} else {
		int i = m.rows[0][0] < m.rows[1][1]
		? (m.rows[1][1] < m.rows[2][2] ? 2 : 1)
		: (m.rows[0][0] < m.rows[2][2] ? 2 : 0);
		int j = (i + 1) % 3;
		int k = (i + 2) % 3;

		real_t s = Math::sqrt(m.rows[i][i] - m.rows[j][j] - m.rows[k][k] + 1.0f);
		temp[i] = s * 0.5f;
		s = 0.5f / s;

		temp[3] = (m.rows[k][j] - m.rows[j][k]) * s;
		temp[j] = (m.rows[j][i] + m.rows[i][j]) * s;
		temp[k] = (m.rows[k][i] + m.rows[i][k]) * s;
	}

	return Quaternion(temp[0], temp[1], temp[2], temp[3]);
}

Quaternion TransformInterpolator::_quat_slerp_unchecked(const Quaternion &p_from, const Quaternion &p_to, real_t p_fraction) {
	Quaternion to1;
	real_t cosom = p_from.dot(p_to);

	if (cosom < 0.0f) {
		cosom = -cosom;
		to1 = -p_to;
	} else {
		to1 = p_to;
	}

	if ((1.0f - cosom) > CMP_EPSILON) {
		real_t omega = Math::acos(cosom);
		real_t sinom = 1.0f / Math::sin(omega);
		real_t scale0 = Math::sin((1.0f - p_fraction) * omega) * sinom;
		real_t scale1 = Math::sin(p_fraction * omega) * sinom;
		return (p_from * scale0) + (to1 * scale1);
	} else {
		// Reverted to original linear interpolation implementation
		return Quaternion(
			p_from.x + (to1.x - p_from.x) * p_fraction,
						  p_from.y + (to1.y - p_from.y) * p_fraction,
						  p_from.z + (to1.z - p_from.z) * p_fraction,
						  p_from.w + (to1.w - p_from.w) * p_fraction
		).normalized();
	}
}

Basis TransformInterpolator::_basis_slerp_unchecked(Basis p_from, Basis p_to, real_t p_fraction) {
	return Basis(_quat_slerp_unchecked(_basis_to_quat_unchecked(p_from), _basis_to_quat_unchecked(p_to), p_fraction));
}

void TransformInterpolator::interpolate_basis_scaled_slerp(Basis p_prev, Basis p_curr, Basis &r_result, real_t p_fraction) {
	Vector3 lengths_prev = _basis_orthonormalize(p_prev);
	Vector3 lengths_curr = _basis_orthonormalize(p_curr);

	r_result = _basis_slerp_unchecked(p_prev, p_curr, p_fraction);

	Vector3 lengths_lerped = lengths_prev.lerp(lengths_curr, p_fraction);
	r_result[0] *= lengths_lerped;
	r_result[1] *= lengths_lerped;
	r_result[2] *= lengths_lerped;
}

void TransformInterpolator::interpolate_basis_linear(const Basis &p_prev, const Basis &p_curr, Basis &r_result, real_t p_fraction) {
	r_result = p_prev.lerp(p_curr, p_fraction);

	const real_t smallest_squared = SMALLEST_AXIS_LENGTH * SMALLEST_AXIS_LENGTH;
	for (int n = 0; n < 3; n++) {
		Vector3 &axis = r_result[n];
		if (axis.length_squared() < smallest_squared) {
			axis[n] = SMALLEST_AXIS_LENGTH;
		}
	}
}

real_t TransformInterpolator::_vec3_normalize(Vector3 &p_vec) {
	real_t lengthsq = p_vec.length_squared();
	if (lengthsq == 0.0f) {
		p_vec = Vector3();
		return 0.0f;
	}
	real_t length = Math::sqrt(lengthsq);
	p_vec /= length;
	return length;
}

Vector3 TransformInterpolator::_basis_orthonormalize(Basis &r_basis) {
	Vector3 x = r_basis.get_column(0);
	Vector3 y = r_basis.get_column(1);
	Vector3 z = r_basis.get_column(2);

	Vector3 lengths;
	lengths.x = _vec3_normalize(x);
	y -= x * x.dot(y);
	lengths.y = _vec3_normalize(y);
	z -= x * x.dot(z) + y * y.dot(z);
	lengths.z = _vec3_normalize(z);

	r_basis.set_column(0, x);
	r_basis.set_column(1, y);
	r_basis.set_column(2, z);
	return lengths;
}

TransformInterpolator::Method TransformInterpolator::_test_basis(Basis p_basis, bool r_needed_normalize, Quaternion &r_quat) {
	Vector3 al(p_basis.get_column(0).length_squared(),
			   p_basis.get_column(1).length_squared(),
			   p_basis.get_column(2).length_squared());

	if (r_needed_normalize || !_vec3_is_equal_approx(al, Vector3(1.0f, 1.0f, 1.0f), ORTHO_EPSILON)) {
		if (al.x < SLERP_EPSILON || al.y < SLERP_EPSILON || al.z < SLERP_EPSILON) {
			return INTERP_LERP;
		}

		al.x = Math::sqrt(al.x);
		al.y = Math::sqrt(al.y);
		al.z = Math::sqrt(al.z);

		p_basis.set_column(0, p_basis.get_column(0) / al.x);
		p_basis.set_column(1, p_basis.get_column(1) / al.y);
		p_basis.set_column(2, p_basis.get_column(2) / al.z);
		r_needed_normalize = true;
	}

	real_t det = p_basis.determinant();
	if (!Math::is_equal_approx(det, 1.0f, DETERMINANT_EPSILON) || !_basis_is_orthogonal(p_basis)) {
		return INTERP_LERP;
	}

	r_quat = _basis_to_quat_unchecked(p_basis);
	return r_quat.is_normalized() ? (r_needed_normalize ? INTERP_SCALED_SLERP : INTERP_SLERP) : INTERP_LERP;
}

bool TransformInterpolator::_basis_is_orthogonal(const Basis &p_basis, real_t p_epsilon) {
	Basis product = p_basis * p_basis.transposed();
	return _vec3_is_equal_approx(product[0], Vector3(1, 0, 0), p_epsilon) &&
	_vec3_is_equal_approx(product[1], Vector3(0, 1, 0), p_epsilon) &&
	_vec3_is_equal_approx(product[2], Vector3(0, 0, 1), p_epsilon);
}

real_t TransformInterpolator::checksum_transform_3d(const Transform3D &p_transform) {
	return _vec3_sum(p_transform.origin)
	- _vec3_sum(p_transform.basis.rows[0])
	+ _vec3_sum(p_transform.basis.rows[1])
	- _vec3_sum(p_transform.basis.rows[2]);
}

TransformInterpolator::Method TransformInterpolator::find_method(const Basis &p_a, const Basis &p_b) {
	bool needed_normalize = false;
	Quaternion q0;

	Method method = _test_basis(p_a, needed_normalize, q0);
	if (method == INTERP_LERP) return method;

	Quaternion q1;
	method = _test_basis(p_b, needed_normalize, q1);
	if (method == INTERP_LERP) return method;

	return (Math::abs(q0.dot(q1)) >= 1.0f - CMP_EPSILON) ? INTERP_LERP : method;
}

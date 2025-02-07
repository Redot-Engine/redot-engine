/**************************************************************************/
/*  quaternion.cpp                                                        */
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

#include "quaternion.h"

#include "core/math/basis.h"
#include "core/string/ustring.h"

real_t Quaternion::angle_to(const Quaternion &p_to) const {
	real_t d = dot(p_to);
	// Calculate the angle using double-angle formula: cos(2θ) = 2d² - 1
	return Math::acos(d * d * 2 - 1);
}

Vector3 Quaternion::get_euler(EulerOrder p_order) const {
	#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!is_normalized(), Vector3(0, 0, 0), "The quaternion " + operator String() + " must be normalized.");
	#endif
	return Basis(*this).get_euler(p_order);
}

void Quaternion::operator*=(const Quaternion &p_q) {
	// Efficient quaternion multiplication using temporary variables to avoid overwriting components prematurely
	real_t xx = w * p_q.x + x * p_q.w + y * p_q.z - z * p_q.y;
	real_t yy = w * p_q.y + y * p_q.w + z * p_q.x - x * p_q.z;
	real_t zz = w * p_q.z + z * p_q.w + x * p_q.y - y * p_q.x;
	w = w * p_q.w - x * p_q.x - y * p_q.y - z * p_q.z;
	x = xx;
	y = yy;
	z = zz;
}

Quaternion Quaternion::operator*(const Quaternion &p_q) const {
	Quaternion r = *this;
	r *= p_q;
	return r;
}

bool Quaternion::is_equal_approx(const Quaternion &p_quaternion) const {
	return Math::is_equal_approx(x, p_quaternion.x) && Math::is_equal_approx(y, p_quaternion.y) && Math::is_equal_approx(z, p_quaternion.z) && Math::is_equal_approx(w, p_quaternion.w);
}

bool Quaternion::is_finite() const {
	return Math::is_finite(x) && Math::is_finite(y) && Math::is_finite(z) && Math::is_finite(w);
}

real_t Quaternion::length() const {
	return Math::sqrt(length_squared());
}

void Quaternion::normalize() {
	*this /= length();
}

Quaternion Quaternion::normalized() const {
	return *this / length();
}

bool Quaternion::is_normalized() const {
	return Math::is_equal_approx(length_squared(), 1, (real_t)UNIT_EPSILON);
}

Quaternion Quaternion::inverse() const {
	#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!is_normalized(), Quaternion(), "The quaternion " + operator String() + " must be normalized.");
	#endif
	// Inverse of a unit quaternion is its conjugate
	return Quaternion(-x, -y, -z, w);
}

Quaternion Quaternion::log() const {
	// Logarithm maps the quaternion to its tangent space (pure quaternion)
	Vector3 src_v = get_axis() * get_angle(); // get_angle() returns 2θ, axis scaled by θ
	return Quaternion(src_v.x, src_v.y, src_v.z, 0);
}

Quaternion Quaternion::exp() const {
	// Exponential maps the tangent space back to the quaternion space
	Vector3 src_v(x, y, z);
	real_t theta = src_v.length();
	src_v = src_v.normalized();
	if (theta < CMP_EPSILON || !src_v.is_normalized()) {
		return Quaternion(0, 0, 0, 1); // Identity quaternion for zero vector
	}
	// Construct quaternion from axis-angle (θ as the angle)
	return Quaternion(src_v, theta);
}

Quaternion Quaternion::slerp(const Quaternion &p_to, real_t p_weight) const {
	#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!is_normalized(), Quaternion(), "The start quaternion " + operator String() + " must be normalized.");
	ERR_FAIL_COND_V_MSG(!p_to.is_normalized(), Quaternion(), "The end quaternion " + p_to.operator String() + " must be normalized.");
	#endif
	Quaternion to1;
	real_t cosom = dot(p_to);

	// Adjust signs for shortest path
	if (cosom < 0.0f) {
		cosom = -cosom;
		to1 = -p_to;
	} else {
		to1 = p_to;
	}

	real_t omega, scale0, scale1;
	if ((1.0f - cosom) > CMP_EPSILON) {
		omega = Math::acos(cosom);
		real_t sinom = 1.0f / Math::sin(omega);
		scale0 = Math::sin((1.0f - p_weight) * omega) * sinom;
		scale1 = Math::sin(p_weight * omega) * sinom;
	} else { // Linear interpolation for near-parallel quaternions
		scale0 = 1.0f - p_weight;
		scale1 = p_weight;
	}

	return Quaternion(
		scale0 * x + scale1 * to1.x,
		scale0 * y + scale1 * to1.y,
		scale0 * z + scale1 * to1.z,
		scale0 * w + scale1 * to1.w);
}

Quaternion Quaternion::slerpni(const Quaternion &p_to, real_t p_weight) const {
	#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!is_normalized(), Quaternion(), "The start quaternion " + operator String() + " must be normalized.");
	ERR_FAIL_COND_V_MSG(!p_to.is_normalized(), Quaternion(), "The end quaternion " + p_to.operator String() + " must be normalized.");
	#endif
	real_t dot = this->dot(p_to);
	if (Math::absf(dot) > 0.9999f) {
		return *this; // Avoid division by near-zero in sinom calculation
	}

	real_t theta = Math::acos(dot);
	real_t sinom = 1.0f / Math::sin(theta);
	real_t new_factor = Math::sin(p_weight * theta) * sinom;
	real_t inv_factor = Math::sin((1.0f - p_weight) * theta) * sinom;

	return Quaternion(
		inv_factor * x + new_factor * p_to.x,
		inv_factor * y + new_factor * p_to.y,
		inv_factor * z + new_factor * p_to.z,
		inv_factor * w + new_factor * p_to.w);
}

Quaternion Quaternion::spherical_cubic_interpolate(const Quaternion &p_b, const Quaternion &p_pre_a, const Quaternion &p_post_b, real_t p_weight) const {
	#ifdef MATH_CHECKS
	ERR_FAIL_COND_V_MSG(!is_normalized(), Quaternion(), "The start quaternion " + operator String() + " must be normalized.");
	ERR_FAIL_COND_V_MSG(!p_b.is_normalized(), Quaternion(), "The end quaternion " + p_b.operator String() + " must be normalized.");
	#endif
	Quaternion from_q = *this;
	Quaternion pre_q = p_pre_a;
	Quaternion to_q = p_b;
	Quaternion post_q = p_post_b;

	// Align phases to ensure shortest path
	from_q = Basis(from_q).get_rotation_quaternion();
	pre_q = Basis(pre_q).get_rotation_quaternion();
	to_q = Basis(to_q).get_rotation_quaternion();
	post_q = Basis(post_q).get_rotation_quaternion();

	// Adjust signs based on dot product to maintain continuity
	bool flip1 = signbit(from_q.dot(pre_q));
	pre_q = flip1 ? -pre_q : pre_q;
	bool flip2 = signbit(from_q.dot(to_q));
	to_q = flip2 ? -to_q : to_q;
	bool flip3 = flip2 ? to_q.dot(post_q) <= 0 : signbit(to_q.dot(post_q));
	post_q = flip3 ? -post_q : post_q;

	// Interpolate in logarithmic space from 'from_q' perspective
	Quaternion ln_to = (from_q.inverse() * to_q).log();
	Quaternion ln_pre = (from_q.inverse() * pre_q).log();
	Quaternion ln_post = (from_q.inverse() * post_q).log();

	Quaternion ln;
	ln.x = Math::cubic_interpolate(0.0f, ln_to.x, ln_pre.x, ln_post.x, p_weight);
	ln.y = Math::cubic_interpolate(0.0f, ln_to.y, ln_pre.y, ln_post.y, p_weight);
	ln.z = Math::cubic_interpolate(0.0f, ln_to.z, ln_pre.z, ln_post.z, p_weight);
	Quaternion q1 = from_q * ln.exp();

	// Interpolate in logarithmic space from 'to_q' perspective
	Quaternion ln_from = (to_q.inverse() * from_q).log();
	ln_pre = (to_q.inverse() * pre_q).log();
	ln_post = (to_q.inverse() * post_q).log();

	ln.x = Math::cubic_interpolate(ln_from.x, 0.0f, ln_pre.x, ln_post.x, p_weight);
	ln.y = Math::cubic_interpolate(ln_from.y, 0.0f, ln_pre.y, ln_post.y, p_weight);
	ln.z = Math::cubic_interpolate(ln_from.z, 0.0f, ln_pre.z, ln_post.z, p_weight);
	Quaternion q2 = to_q * ln.exp();

	// Blend results to minimize errors
	return q1.slerp(q2, p_weight);
}

Quaternion Quaternion::spherical_cubic_interpolate_in_time(const Quaternion &p_b, const Quaternion &p_pre_a, const Quaternion &p_post_b, real_t p_weight,
														   real_t p_b_t, real_t p_pre_a_t, real_t p_post_b_t) const {
															   #ifdef MATH_CHECKS
															   ERR_FAIL_COND_V_MSG(!is_normalized(), Quaternion(), "The start quaternion " + operator String() + " must be normalized.");
															   ERR_FAIL_COND_V_MSG(!p_b.is_normalized(), Quaternion(), "The end quaternion " + p_b.operator String() + " must be normalized.");
															   #endif
															   Quaternion from_q = *this;
															   Quaternion pre_q = p_pre_a;
															   Quaternion to_q = p_b;
															   Quaternion post_q = p_post_b;

															   // Align phases and adjust signs as in the non-timed version
															   from_q = Basis(from_q).get_rotation_quaternion();
															   pre_q = Basis(pre_q).get_rotation_quaternion();
															   to_q = Basis(to_q).get_rotation_quaternion();
															   post_q = Basis(post_q).get_rotation_quaternion();

															   bool flip1 = signbit(from_q.dot(pre_q));
															   pre_q = flip1 ? -pre_q : pre_q;
															   bool flip2 = signbit(from_q.dot(to_q));
															   to_q = flip2 ? -to_q : to_q;
															   bool flip3 = flip2 ? to_q.dot(post_q) <= 0 : signbit(to_q.dot(post_q));
															   post_q = flip3 ? -post_q : post_q;

															   // Interpolation in 'from_q' space with time parameters
															   Quaternion ln_to = (from_q.inverse() * to_q).log();
															   Quaternion ln_pre = (from_q.inverse() * pre_q).log();
															   Quaternion ln_post = (from_q.inverse() * post_q).log();

															   Quaternion ln;
															   ln.x = Math::cubic_interpolate_in_time(0.0f, ln_to.x, ln_pre.x, ln_post.x, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
															   ln.y = Math::cubic_interpolate_in_time(0.0f, ln_to.y, ln_pre.y, ln_post.y, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
															   ln.z = Math::cubic_interpolate_in_time(0.0f, ln_to.z, ln_pre.z, ln_post.z, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
															   Quaternion q1 = from_q * ln.exp();

															   // Interpolation in 'to_q' space with time parameters
															   Quaternion ln_from = (to_q.inverse() * from_q).log();
															   ln_pre = (to_q.inverse() * pre_q).log();
															   ln_post = (to_q.inverse() * post_q).log();

															   ln.x = Math::cubic_interpolate_in_time(ln_from.x, 0.0f, ln_pre.x, ln_post.x, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
															   ln.y = Math::cubic_interpolate_in_time(ln_from.y, 0.0f, ln_pre.y, ln_post.y, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
															   ln.z = Math::cubic_interpolate_in_time(ln_from.z, 0.0f, ln_pre.z, ln_post.z, p_weight, p_b_t, p_pre_a_t, p_post_b_t);
															   Quaternion q2 = to_q * ln.exp();

															   return q1.slerp(q2, p_weight);
														   }

														   Quaternion::operator String() const {
															   return "(" + String::num_real(x, false) + ", " + String::num_real(y, false) + ", " + String::num_real(z, false) + ", " + String::num_real(w, false) + ")";
														   }

														   Vector3 Quaternion::get_axis() const {
															   if (Math::abs(w) > 1 - CMP_EPSILON) {
																   return Vector3(x, y, z); // Near-identity case, axis is arbitrary but normalized
															   }
															   real_t r = 1.0f / Math::sqrt(1 - w * w);
															   return Vector3(x * r, y * r, z * r);
														   }

														   real_t Quaternion::get_angle() const {
															   return 2 * Math::acos(w);
														   }

														   Quaternion::Quaternion(const Vector3 &p_axis, real_t p_angle) {
															   #ifdef MATH_CHECKS
															   ERR_FAIL_COND_MSG(!p_axis.is_normalized(), "The axis Vector3 " + p_axis.operator String() + " must be normalized.");
															   #endif
															   real_t d = p_axis.length();
															   if (d == 0) {
																   // Fallback to invalid quaternion (caller should ensure axis is normalized)
																   x = y = z = w = 0;
															   } else {
																   real_t half_angle = p_angle * 0.5f;
																   real_t sin_angle = Math::sin(half_angle);
																   real_t cos_angle = Math::cos(half_angle);
																   real_t s = sin_angle / d; // d should be 1 if axis is normalized
																   x = p_axis.x * s;
																   y = p_axis.y * s;
																   z = p_axis.z * s;
																   w = cos_angle;
															   }
														   }

														   Quaternion Quaternion::from_euler(const Vector3 &p_euler) {
															   // Convert Euler angles (YXZ order) to quaternion using the NASA reference formula
															   real_t half_yaw = p_euler.y * 0.5f;
															   real_t half_pitch = p_euler.x * 0.5f;
															   real_t half_roll = p_euler.z * 0.5f;

															   real_t cos_yaw = Math::cos(half_yaw);
															   real_t sin_yaw = Math::sin(half_yaw);
															   real_t cos_pitch = Math::cos(half_pitch);
															   real_t sin_pitch = Math::sin(half_pitch);
															   real_t cos_roll = Math::cos(half_roll);
															   real_t sin_roll = Math::sin(half_roll);

															   // Formula derived from Y(a1) * X(a2) * Z(a3) rotation order
															   return Quaternion(
																   sin_yaw * cos_pitch * sin_roll + cos_yaw * sin_pitch * cos_roll,
									sin_yaw * cos_pitch * cos_roll - cos_yaw * sin_pitch * sin_roll,
									cos_yaw * cos_pitch * sin_roll - sin_yaw * sin_pitch * cos_roll,
									sin_yaw * sin_pitch * sin_roll + cos_yaw * cos_pitch * cos_roll);
														   }

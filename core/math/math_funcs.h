/**************************************************************************/
/*  math_funcs.h                                                          */
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
 * @file math_funcs.h
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "core/error/error_macros.h"
#include "core/math/math_defs.h"
#include "core/typedefs.h"

#include <algorithm>
#include <bit>
#include <cfloat>
#include <cmath>
#include <concepts>
#include <limits>

namespace Math {

template <typename T>
_ALWAYS_INLINE_ T sin(T p_x) {
	return std::sin(p_x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T cos(T p_x) {
	return std::cos(p_x);
}

#if !defined(__has_builtin)
#define _MATH_FUNCS_DEFINED_HAS_BUILTIN
#define __has_builtin(x) 0
#endif

template <std::floating_point T, std::floating_point X>
_ALWAYS_INLINE_ void sin_cos(X p_x, T &r_sin, T &r_cos) {
#if __has_builtin(__builtin_sincos)
	double s, c;
	__builtin_sincos(p_x, &s, &c);
	r_sin = static_cast<T>(s);
	r_cos = static_cast<T>(c);
#else
	r_sin = static_cast<T>(Math::sin(p_x));
	r_cos = static_cast<T>(Math::cos(p_x));
#endif
}

#ifdef _MATH_FUNCS_DEFINED_HAS_BUILTIN
#undef _MATH_FUNCS_DEFINED_HAS_BUILTIN
#undef __has_builtin
#endif

template <std::floating_point T>
_ALWAYS_INLINE_ T tan(T p_x) {
	return std::tan(p_x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T sinh(T p_x) {
	return std::sinh(p_x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T sinc(T p_x) {
	return p_x == 0 ? 1 : sin(p_x) / p_x;
}

template <std::floating_point T>
_ALWAYS_INLINE_ T sincn(T p_x) {
	return sinc(PI * p_x);
}

/*
 * Less overhead vs std::clamp due to just comparing values,
 * lowers <algorithm> dependency in this file, too.
 */
template <typename T, typename U, typename V>
constexpr auto clamp(T x, U minv, V maxv) noexcept {
	using R = std::common_type_t<T, U, V>;
	return std::min(std::max(static_cast<R>(x), static_cast<R>(minv)), static_cast<R>(maxv));
}

template <std::floating_point T>
_ALWAYS_INLINE_ T cosh(T x) noexcept {
	return std::cosh(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T tanh(T x) noexcept {
	return std::tanh(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T asin(T x) noexcept {
	x = clamp(x, T(-1), T(1));
	return std::asin(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T acos(T x) noexcept {
	x = clamp(x, T(-1), T(1));
	return std::acos(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T atan(T x) noexcept {
	return std::atan(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T atan2(T y, T x) noexcept {
	return std::atan2(y, x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T asinh(T x) noexcept {
	return std::asinh(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T acosh(T x) noexcept {
	if (x <= T{ 1.f }) {
		return T{ 0.f };
	}
	return std::acosh(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T atanh(T x) noexcept {
	if (x < T{ -1.f + CMP_EPSILON }) {
		return -std::numeric_limits<T>::infinity();
	}
	if (x > T{ 1.f - CMP_EPSILON }) {
		return std::numeric_limits<T>::infinity();
	}
	return std::atanh(x);
}

template <std::floating_point T>
_FORCE_INLINE_ T sqrt(T x) noexcept {
	return std::sqrt(x);
}

template <std::integral T>
_FORCE_INLINE_ double sqrt(T x) noexcept {
	return std::sqrt(static_cast<double>(x));
}

template <std::floating_point T, std::convertible_to<T> U>
_FORCE_INLINE_ T fmod(T x, U y) noexcept {
	return std::fmod(x, y);
}

template <std::floating_point T>
_FORCE_INLINE_ T modf(T x, T y) noexcept {
	return std::modf(x, y);
}

template <std::floating_point T>
_FORCE_INLINE_ T modf_with_ptr(T x, T *y) noexcept {
	return std::modf(x, y);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T floor(T x) noexcept {
	return std::floor(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T ceil(T x) noexcept {
	return std::ceil(x);
}

/*
 * For non negative integer exponents.
 */
template <std::floating_point T>
constexpr T ipow(T base, long long exp) noexcept {
	// TODO: Should we handle negatives?
	T res = T(1);
	T b = base;
	unsigned long long e = static_cast<unsigned long long>(exp);

	if (exp == 0) {
		return res;
	}
	if (exp == 1) {
		return b;
	}
	if (exp == 2) {
		return b * b;
	}
	if (exp == 3) {
		return b * b * b;
	}

	// exponentiation by squaring.
	while (e != 0) {
		if (e & 1ULL) {
			res *= b;
		}
		b *= b;
		e >>= 1ULL;
	}

	return res;
}

template <std::floating_point T>
constexpr T pow(T x, T y) noexcept {
	// Requires both params to be the same floating-type.
	// Cast types manually when needed.
	if (std::is_constant_evaluated()) {
		// std::pow() isn't constexpr until C++23, so this is our workaround.
		if (static_cast<T>(static_cast<long long>(y)) == y) {
			// We need to be sure this is not a fraction.
			return ipow(x, static_cast<long long>(y));
		}
	}
	// Runtime path
	return std::pow(x, y);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T log(T x) noexcept {
	return std::log(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T log1p(T x) noexcept {
	return std::log1p(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T log2(T x) noexcept {
	return std::log2(x);
}

template <std::floating_point T>
_ALWAYS_INLINE_ T exp(T x) noexcept {
	return std::exp(x);
}

template <std::floating_point T>
constexpr bool is_nan(T val) noexcept {
	// Only NaN does not equal itself.
	return val != val;
}

template <std::floating_point T>
constexpr bool is_inf(T val) noexcept {
	// std::isinf(val) isn't constexpr in C++20 [Made constexpr in C++23]
	return val == std::numeric_limits<T>::infinity() || val == -std::numeric_limits<T>::infinity();
}

template <std::floating_point T>
constexpr bool is_finite(T val) noexcept {
	return !is_inf(val) && !is_nan(val);
}

template <typename T>
constexpr T abs(T value) noexcept {
	if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>) {
		// Use std::abs for float/double for hardware optimizations.
		return std::abs(value);
	} else if constexpr (std::is_signed_v<T>) {
		// Manually compute abs for other signed types.
		// Also avoids potential int8_t -> int issues.
		return value < T(0) ? -value : value;
	} else {
		// unsigned is always positive! :^)
		return value;
	}
}

template <unsigned_integral T>
constexpr T division_round_up(T p_num, T p_den) {
	// for unsigned ints
	return (p_num + p_den - 1) / p_den;
}

template <signed_integral T>
constexpr T division_round_up(T p_num, T p_den) {
	// for signed ints.
	T offset = (p_num < 0 && p_den < 0) ? 1 : -1;
	return (p_num + p_den + offset) / p_den;
}

template <std::floating_point T>
constexpr T fposmod(T x, T y) noexcept {
	T value = fmod(x, y);
	// mask based on sign mismatch.
	T mask = T(value != 0 && ((value < T(0))) != (y < T(0)));
	// convert bool to 0.0 or 1.0
	mask = static_cast<T>(mask);
	return value + mask * y;
}

template <std::floating_point T>
constexpr T fposmodp(T x, T y) noexcept {
	T value = fmod(x, y);

	value += (value < T(0)) ? y : T(0);

	return value;
}

// keep overloads and avoid generic templates, otherwise it will slow things down.
constexpr int64_t posmod(int64_t x, int64_t y) noexcept {
	if (y == 0) {
		// Division by zero undefined. Return 0 as fallback.
		return 0;
	}

	int64_t value = x % y;

	value += (value != 0 && ((value < 0) != (y < 0))) ? y : 0;

	return value;
}

constexpr int32_t posmod(int32_t x, int32_t y) noexcept {
	if (y == 0) {
		// Division by zero undefined. Return 0 as fallback.
		return 0;
	}

	int32_t value = x % y;

	value += (value != 0 && ((value < 0) != (y < 0))) ? y : 0;

	return value;
}

template <std::floating_point T>
constexpr T deg_to_rad(T y) noexcept {
	return y * (T(PI) / T(180));
}

template <std::floating_point T>
constexpr T rad_to_deg(T y) {
	return y * (T(180) / T(PI));
}

template <std::floating_point T>
constexpr T lerp(T from, T to, T weight) noexcept {
	// TODO: AVX512 crushes float here via clang++
	// and use scalar for doubles.
	return from + (to - from) * weight;
}

template <std::floating_point T>
constexpr T cubic_interpolate(T p_from, T p_to, T p_pre, T p_post, T p_weight) noexcept {
	// weight squared.
	T w2 = p_weight * p_weight;
	// weight cubed.
	T w3 = p_weight * w2;

	// calculate coefficients.
	T a = -p_pre + p_to;
	T b = T(2) * p_pre - T(5) * p_from + T(4) * p_to - p_post;
	T c = -p_pre + T(3) * p_from - T(3) * p_to + p_post;

	// Catmull-Rom Interpolation:
	// 0.5 * ((2 * p_from) + (a * w) + (b * w^2) + (c * w^3))

	if (std::is_constant_evaluated()) {
		// compile time
		return T(0.5) * (T(2) * p_from) + (a * p_weight) + (b * w2) + (c * w3);
	} else {
		// runtime
		return T(0.5) * std::fma(c, w3, std::fma(b, w2, std::fma(a, p_weight, T(2) * p_from)));
	}
}

template <std::floating_point T>
constexpr T cubic_interpolate_angle(T p_from, T p_to, T p_pre, T p_post, T p_weight) noexcept {
	// Use fposmod instead of fmod to shorten path.
	T from_rot = fposmod(p_from, T(TAU));

	T pre_diff = fposmod(p_pre - from_rot, T(TAU));
	T pre_rot = from_rot + fposmod(T(2) * pre_diff, T(TAU)) - pre_diff;

	T to_diff = fposmod(p_to - from_rot, T(TAU));
	T to_rot = from_rot + fposmod(T(2) * to_diff, T(TAU)) - to_diff;

	T post_diff = fposmod(p_post - to_rot, T(TAU));
	T post_rot = to_rot + fposmod(T(2) * post_diff, T(TAU)) - post_diff;

	return cubic_interpolate(from_rot, to_rot, pre_rot, post_rot, p_weight);
}

template <std::floating_point T>
constexpr T cubic_interpolate_in_time(T p_from, T p_to, T p_pre, T p_post, T p_weight, T p_to_t, T p_pre_t, T p_post_t) noexcept {
	/* Barry-Goldman method */
	T t = lerp(T(0), p_to_t, p_weight);

	// At least try to make this easier to parse for others.
	T pre_scale = (p_pre_t == T(0)) ? T(0.0) : (t - p_pre_t) / -p_pre_t;
	T to_scale = (p_to_t == T(0)) ? T(0.5) : t / p_to_t;
	T post_range = p_post_t - p_to_t;
	T post_scale = (post_range == T(0)) ? T(1) : (t - p_to_t) / post_range;

	// First layer.
	T a1 = lerp(p_pre, p_from, pre_scale);
	T a2 = lerp(p_from, p_to, to_scale);
	T a3 = lerp(p_to, p_post, post_scale);

	// More parsing.
	T mid_range = p_to_t - p_pre_t;
	T from_to_scale = (mid_range == T(0)) ? T(0) : (t - p_pre_t) / mid_range;
	T to_post_scale = (p_post_t == T(0)) ? T(1.0) : t / p_post_t;

	// Second layer.
	T b1 = lerp(a1, a2, from_to_scale);
	T b2 = lerp(a2, a3, to_post_scale);

	// One more for the road.
	T final_range = p_to_t == T(0);
	T final_scale = final_range ? T(0.5) : t / p_to_t;

	return lerp(b1, b2, final_scale);
}

template <std::floating_point T>
constexpr T cubic_interpolate_angle_in_time(T p_from, T p_to, T p_pre, T p_post, T p_weight, T p_to_t, T p_pre_t, T p_post_t) noexcept {
	T from_rot = fposmod(p_from, T(TAU));

	T pre_diff = fposmod(p_pre - from_rot, T(TAU));
	T pre_rot = from_rot + fposmod(T(2.0) * pre_diff, T(TAU)) - pre_diff;

	T to_diff = fposmod(p_to - from_rot, T(TAU));
	T to_rot = from_rot + fposmod(T(2.0) * to_diff, T(TAU)) - to_diff;

	T post_diff = fposmod(p_post - to_rot, T(TAU));
	T post_rot = to_rot + fposmod(T(2.0) * post_diff, T(TAU)) - post_diff;

	return cubic_interpolate_in_time(from_rot, to_rot, pre_rot, post_rot, p_weight, p_to_t, p_pre_t, p_post_t);
}

template <std::floating_point T>
constexpr T bezier_interpolate(T p_start, T p_control_1, T p_control_2, T p_end, T p_t) noexcept {
	/* Formula from Wikipedia article on Bezier curves. */
	// one minus t.
	T omt = T((1) - p_t);
	T omt2 = omt * omt;
	T omt3 = omt2 * omt;
	T t2 = p_t * p_t;
	T t3 = t2 * p_t;

	// B(t) = (1-t)^3 * P_0 + 3(1 - t)^2 * t * P_1 + 3(1 - t) * t^2 * P_2 + t^3 * P_3
	T d = p_start * omt3 + p_control_1 * omt2 * p_t * T(3) + p_control_2 * omt * t2 * T(3) + p_end * t3;
	return d;
}

template <std::floating_point T>
constexpr T bezier_derivative(T p_start, T p_control_1, T p_control_2, T p_end, T p_t) noexcept {
	/* Formula from Wikipedia article on Bezier curves. */
	T omt = (T(1) - p_t);
	T omt2 = omt * omt;
	T t2 = p_t * p_t;

	T d = (p_control_1 - p_start) * T(3) * omt2 + (p_control_2 - p_control_1) * T(6) * omt * p_t + (p_end - p_control_2) * T(3) * t2;
	return d;
}

template <std::floating_point T>
constexpr T angle_difference(T p_from, T p_to) noexcept {
	T diff = fmod(p_to - p_from, T(TAU));
	return std::fmod(T(2) * diff, T(TAU)) - diff;
}

template <std::floating_point T>
constexpr T lerp_angle(T p_from, T p_to, T p_weight) noexcept {
	return p_from + angle_difference(p_from, p_to) * p_weight;
}

template <std::floating_point T>
constexpr T inverse_lerp(T p_from, T p_to, T p_value) noexcept {
	// Could divide by zero here..?
	return (p_value - p_from) / (p_to - p_from);
}

template <std::floating_point T>
constexpr T remap(T p_value, T p_istart, T p_istop, T p_ostart, T p_ostop) noexcept {
	return lerp(p_ostart, p_ostop, inverse_lerp(p_istart, p_istop, p_value));
}

template <arithmetic T>
constexpr bool is_equal_approx(T p_left, T p_right, T p_tolerance) noexcept {
	// Check for exact equality first, required to handle "infinity" values.
	if (p_left == p_right) {
		return true;
	}
	// Then check for approximate equality.
	return abs(p_left - p_right) <= p_tolerance;
}

template <std::floating_point T>
constexpr bool is_equal_approx(T p_left, T p_right) noexcept {
	// Check for exact equality first, required to handle "infinity" values.
	if (p_left == p_right) {
		return true;
	}

	// Compute tolerance by magnitude.
	const T tolerance = (T(CMP_EPSILON) * abs(p_left) < T(CMP_EPSILON)) ? T(CMP_EPSILON) : T(CMP_EPSILON) * abs(p_left);

	return abs(p_left - p_right) <= tolerance;
}

template <std::floating_point T, std::convertible_to<T> C>
constexpr bool is_equal_approx(T p_left, C p_right) noexcept {
	return is_equal_approx(p_left, static_cast<T>(p_right));
}

template <std::floating_point T, std::convertible_to<T> C>
constexpr bool is_equal_approx(C p_left, T p_right) noexcept {
	return is_equal_approx(static_cast<T>(p_left), p_right);
}

template <std::integral T1, std::integral T2>
constexpr bool is_equal_approx(T1 p_left, T2 p_right) noexcept {
	return p_left == p_right;
}

template <std::floating_point T>
constexpr bool is_zero_approx(T p_value) noexcept {
	return abs(p_value) < T(CMP_EPSILON);
}

template <std::integral T>
constexpr bool is_zero_approx(T p_value) noexcept {
	return p_value == T{ 0 };
}

template <std::floating_point T>
constexpr bool is_same(T p_left, T p_right) noexcept {
	return (p_left == p_right) || (is_nan(p_left) && is_nan(p_right));
}

template <std::floating_point T>
constexpr T smoothstep(T p_from, T p_to, T p_s) noexcept {
	if (is_equal_approx(p_from, p_to)) {
		// Degenerate case AKA 3small5u.
		return (p_s <= p_from) ? T(0) : T(1);
	}

	// note: CLAMP defined at core/typedefs.h
	T s = CLAMP((p_s - p_from) / (p_to - p_from), T(0), T(1));

	// Hermite interpolation: (3s^2 - 2s^3)
	return s * s * (T(3) - T(2) * s);
}

template <std::floating_point T>
constexpr T faststep(T p_from, T p_to, T p_s) noexcept {
	if (p_from == p_to) {
		// degenerate case.
		return (p_s <= p_from) ? T(0) : T(1);
	}

	// clamp to [0,1]
	T s = CLAMP((p_s - p_from) / (p_to - p_from), T(0), T(1));

	// less smooth Hermite-like interpolation: (1.5s^2 - 0.5s^3)
	return s * s * (T(1.5) - T(0.5) * s);
}

template <std::floating_point T>
constexpr T move_toward(T p_from, T p_to, T p_delta) noexcept {
	return abs(p_to - p_from) <= p_delta ? p_to : p_from + SIGN(p_to - p_from) * p_delta;
}

template <std::floating_point T>
constexpr T rotate_toward(T p_from, T p_to, T p_delta) noexcept {
	T diff = angle_difference(p_from, p_to);
	T abs_diff = abs(diff);
	T clamped = clamp(p_delta, abs_diff - T(PI), abs_diff);

	return p_from + clamped * std::copysign(T(1), diff);
	;
}

template <std::floating_point T>
constexpr T linear_to_db(T p_linear) noexcept {
	// Converting linear gain to decibals.
	return log(p_linear) * T(DB_CONVERSION_GAIN);
}

template <std::floating_point T>
constexpr T db_to_linear(T p_db) noexcept {
	// Converting decibals to linear gain.
	return exp(p_db * T(GAIN_CONVERSION_DB));
}

template <std::floating_point T>
constexpr T round(T p_val) noexcept {
	return p_val >= T{ 0 } ? static_cast<T>(static_cast<long long>(p_val + T{ .5f })) : static_cast<T>(static_cast<long long>(p_val - T{ .5f }));
}

template <std::floating_point T>
constexpr T wrapf(T p_value, T p_min, T p_max) noexcept {
	T range = p_max - p_min;

	if (is_zero_approx(range)) {
		return p_min;
	}

	T offset = p_value - p_min;
	T result = offset - range * floor(offset / range);
	// This is cheaper due to floor blocking.
	result += p_min;

	if (is_equal_approx(result, p_max)) {
		return p_min;
	}

	return result;
}

template <std::integral T>
constexpr T wrapi(T p_value, T p_min, T p_max) noexcept {
	T range = p_max - p_min;

	if (range == 0) {
		return p_min;
	}

	return p_min + ((((p_value - p_min) % range) + range) % range);
}

template <std::floating_point T>
constexpr T fract(T p_value) noexcept {
	return p_value - floor(p_value);
}

template <std::floating_point T>
constexpr T pingpong(T p_value, T p_length) noexcept {
	if (p_length == T(0)) {
		return 0;
	}

	// Calls twice, may as well.
	const T double_len = p_length * T(2);
	// for readability:
	const T fr = fract((p_value - p_length) / double_len);

	return abs(fr * double_len - p_length);
}

// double only, as these functions are mainly used by the editor and not performance-critical,
double ease(double p_x, double p_c) noexcept;
int step_decimals(double p_step) noexcept;
int range_step_decimals(double p_step) noexcept; // For editor use only.
double snapped(double p_value, double p_step) noexcept;

uint32_t larger_prime(uint32_t p_val) noexcept;

void seed(uint64_t p_seed) noexcept;
void randomize() noexcept;
uint32_t rand_from_seed(uint64_t *p_seed) noexcept;
uint32_t rand() noexcept;

_FORCE_INLINE_ double randd() noexcept {
	return static_cast<double>(Math::rand()) * UINT32_MAX_D;
}
_FORCE_INLINE_ float randf() noexcept {
	return static_cast<float>(Math::rand()) * UINT32_MAX_F;
}
double randfn(double p_mean, double p_deviation) noexcept;

// TODO: Template + make this better (later.)
double random(double p_from, double p_to) noexcept;
float random(float p_from, float p_to) noexcept;
int random(int p_from, int p_to) noexcept;

// This function should be as fast as possible and rounding mode should not matter.
constexpr int fast_ftoi(float p_value) noexcept {
	return static_cast<int>(p_value);
}

/*
 * Convert raw 16bit half-precision floating point pattern to 32-bit
 * single-precision pattern.
 *
 * @param p_half - The 16-bit FP.
 *
 * @return - corresponding 32-bit FP.
 */
constexpr uint32_t halfbits_to_floatbits(uint16_t p_half) noexcept {
	// TODO: We should just have one spot for these masks...
	// FP16 bit masks
	// Bit #15 (sign)
	constexpr uint16_t FP16_SIGN_MASK = 0x8000u;
	// Bits #10 to #14 (exponent)
	constexpr uint16_t FP16_EXP_MASK = 0x7C00u;
	// Bits #0 - #9 (fraction)
	constexpr uint16_t FP16_FRAC_MASK = 0x03FFu;
	constexpr uint16_t FP16_EXP_SHIFT = 10;
	// Bit #10 (Normalized number)
	constexpr uint16_t FP16_IMPLICIT_ONE = 0x0400u;
	// Stored exponent bias goes here:
	constexpr uint16_t FP16_EXP_BIAS = 15;

	// FP32 masks
	// Bit #31 (sign)
	//constexpr uint32_t FP32_SIGN_MASK = 0x80000000u;
	// Bit #23 - #30 (exponent)
	constexpr uint32_t FP32_EXP_MASK = 0x7F800000u;
	// Stored expontent bias for FP32
	constexpr int FP32_EXP_BIAS = 127;

	// Bias diff for FP16 - FP32.
	constexpr int FP_EXP_BIAS_DIFF = FP32_EXP_BIAS - FP16_EXP_BIAS;

	// extract sign, exponent, fraction:
	uint32_t sign = static_cast<uint32_t>(p_half & FP16_SIGN_MASK) << 16;
	uint32_t exponent16 = p_half & FP16_EXP_MASK;
	uint32_t fraction16 = p_half & FP16_FRAC_MASK;

	// zero || subnormal
	if (exponent16 == 0) {
		if (fraction16 == 0) {
			// Signed zero: Only returns the sign.
			return sign;
		}

		int shift = 0;

		// normalize fraction:
		while ((fraction16 & FP16_IMPLICIT_ONE) == 0) {
			fraction16 <<= 1;
			shift++;
		}

		// update (remove) implicit leading one
		fraction16 &= FP16_FRAC_MASK;

		// Adjust for float:
		// Align to exponent:
		uint32_t exponent32 = (FP_EXP_BIAS_DIFF - shift + 1) << 23;
		// align fraction bits to float format:
		uint32_t fraction32 = fraction16 << 13;
		return sign | exponent32 | fraction32;
	}

	// infinity/NaN
	if (exponent16 == FP16_EXP_MASK) {
		// Map exponent directly, preserve NaN:
		return sign | FP32_EXP_MASK | (fraction16 << 13);
	}

	// Normalize - Remember what bits 23 and 13 are? :^)
	uint32_t exponent32 = ((exponent16 >> FP16_EXP_SHIFT) + FP_EXP_BIAS_DIFF) << 23;
	uint32_t fraction32 = fraction16 << 13;

	return sign | exponent32 | fraction32;
}

constexpr float halfptr_to_float(const uint16_t *p_half) noexcept {
	uint32_t bits32 = halfbits_to_floatbits(*p_half);
	return std::bit_cast<float>(bits32);
}

constexpr float half_to_float(const uint16_t p_half) noexcept {
	uint32_t bits32 = halfbits_to_floatbits(p_half);
	return std::bit_cast<float>(bits32);
}

constexpr uint16_t make_half_float(float p_value) noexcept {
	// FP32 bits
	// Bit #31
	constexpr uint32_t FP32_SIGN_MASK = 0x80000000u;
	// Bits #23 - #30
	constexpr uint32_t FP32_EXP_MASK = 0x7F800000u;
	// Bits #0 - #22
	constexpr uint32_t FP32_FRAC_MASK = 0x007FFFFFu;
	// FP32 exponent bias
	constexpr int FP32_EXP_BIAS = 127;

	// FP16 bits
	// Bits #10 - #14
	constexpr uint16_t FP16_EXP_MASK = 0x7C00u;
	// Bits #0 - #9
	//constexpr uint16_t FP16_FRAC_MASK = 0x03FFu; // unused?
	// stored exponent bias
	constexpr int FP16_EXP_BIAS = 15;

	// Bias diff FP16-FP32
	constexpr int FP_EXP_BIAS_DIFF = FP32_EXP_BIAS - FP16_EXP_BIAS;

	// change given bits to uint32_t.
	uint32_t bits32 = std::bit_cast<uint32_t>(p_value);

	// Extract sign, exponent, fraction
	uint32_t sign32 = bits32 & FP32_SIGN_MASK;
	uint32_t exponent32 = bits32 & FP32_EXP_MASK;
	uint32_t fraction32 = bits32 & FP32_FRAC_MASK;

	uint16_t bits16;

	if (exponent32 <= ((FP32_EXP_BIAS - FP16_EXP_BIAS) << 23)) {
		// Zero
		bits16 = static_cast<uint16_t>(sign32 >> 16);
	} else if (exponent32 == FP32_EXP_MASK) {
		// Infinity or NaN
		uint32_t frac = fraction32 ? (fraction32 >> 13) : 0;
		bits16 = static_cast<uint16_t>((sign32 >> 16) | FP16_EXP_MASK | frac);
	} else {
		// normalize
		uint32_t half_exponent = ((exponent32 >> 23) - FP_EXP_BIAS_DIFF) << 10;
		uint32_t half_fraction = fraction32 >> 13;
		bits16 = static_cast<uint16_t>((sign32 >> 16) | half_exponent | half_fraction);
	}

	return bits16;
}

template <std::floating_point T>
constexpr T snap_scalar(T p_offset, T p_step, T p_target) noexcept {
	return p_step != T(0.0) ? snapped(p_target - p_offset, p_step) + p_offset : p_target;
}

template <std::floating_point T>
constexpr T snap_scalar_separation(T p_offset, T p_step, T p_target, T p_separation) noexcept {
	if (p_step == T(0)) {
		// No snapping.
		return p_target;
	} else {
		T a = snapped(p_target - p_offset, p_step + p_separation) + p_offset;
		T b = (p_target >= T(0)) ? a - p_separation : a + p_step;
		return (abs(p_target - a) < abs(p_target - b)) ? a : b;
	}
}

template <std::floating_point T>
constexpr T sigmoid_affine(T p_x, T p_amplitude, T p_y_translation) noexcept {
	return p_amplitude / (T(1.0) + exp(-p_x)) + p_y_translation;
}

template <std::floating_point T>
constexpr T sigmoid_affine_approx(T p_x, T p_amplitude, T p_y_translation) noexcept {
	return p_amplitude * (T(0.5) + p_x / (T(4.0) + fabs(p_x))) + p_y_translation;
}

template <std::floating_point T>
constexpr T monotonic_cubic_interpolate(T p_from, T p_to, T p_pre, T p_post, T p_weight) {
	const T d0 = p_from - p_pre;
	const T d1 = p_to - p_from;
	const T d2 = p_post - p_to;

	T m1 = 0.0;
	T m2 = 0.0;

	if (!is_zero_approx(d1)) {
		m1 = (d0 + d1) * (T)0.5;
		m2 = (d1 + d2) * (T)0.5;

		if (m1 * d1 <= 0.0) {
			m1 = 0.0;
		}
		if (m2 * d1 <= 0.0) {
			m2 = 0.0;
		}

		const T a = m1 / d1;
		const T b = m2 / d1;

		const T h = sqrt(a * a + b * b);

		if (h > (T)3.0) {
			const T scale_factor = (T)3.0 / h;
			m1 *= scale_factor;
			m2 *= scale_factor;
		}
	}

	const T t2 = p_weight * p_weight;
	const T t3 = t2 * p_weight;

	const T h00 = 2 * t3 - 3 * t2 + 1;
	const T h10 = t3 - 2 * t2 + p_weight;
	const T h01 = -2 * t3 + 3 * t2;
	const T h11 = t3 - t2;

	return (
			h00 * p_from +
			h10 * m1 +
			h01 * p_to +
			h11 * m2);
}

template <std::floating_point T>
constexpr T monotonic_cubic_interpolate_in_time(T p_from, T p_to, T p_pre, T p_post, T p_weight, T p_to_t, T p_pre_t, T p_post_t) {
	if (is_zero_approx(p_to_t)) {
		return (p_from + p_to) * (T)0.5;
	}

	const T t_from = (T)0;
	const T t_to = p_to_t;
	const T t_pre = p_pre_t;
	const T t_post = p_post_t;

	const T t = p_weight * t_to;

	const T h0 = t_from - t_pre;
	const T h1 = t_to - t_from;
	const T h2 = t_post - t_to;

	const T d0 = is_zero_approx(h0) ? (T)0 : (p_from - p_pre) / h0;
	const T d1 = (p_to - p_from) / h1;
	const T d2 = is_zero_approx(h2) ? (T)0 : (p_post - p_to) / h2;

	// Fritsch–Carlson monotonic tangents
	auto tangent = [](T a, T b, T ha, T hb) -> T {
		if (a * b <= (T)0) {
			return (T)0;
		}

		const T w1 = (T)2 * hb + ha;
		const T w2 = hb + (T)2 * ha;

		return (w1 + w2) / (w1 / a + w2 / b);
	};

	const T m1 = tangent(d0, d1, h0, h1);
	const T m2 = tangent(d1, d2, h1, h2);

	const T s = t / h1;

	const T s2 = s * s;
	const T s3 = s2 * s;

	// Hermite basis
	const T h00 = (T)2 * s3 - (T)3 * s2 + (T)1;
	const T h10 = s3 - (T)2 * s2 + s;
	const T h01 = -(T)2 * s3 + (T)3 * s2;
	const T h11 = s3 - s2;

	return h00 * p_from +
			h10 * h1 * m1 +
			h01 * p_to +
			h11 * h1 * m2;
}

template <std::floating_point T>
constexpr T monotonic_cubic_interpolate_angle(T p_from, T p_to, T p_pre, T p_post, T p_weight) {
	T from_rot = fmod(p_from, (T)TAU);

	T pre_diff = fmod(p_pre - from_rot, (T)TAU);
	T pre_rot = from_rot + fmod(2.0f * pre_diff, (T)TAU) - pre_diff;

	T to_diff = fmod(p_to - from_rot, (T)TAU);
	T to_rot = from_rot + fmod(2.0f * to_diff, (T)TAU) - to_diff;

	T post_diff = fmod(p_post - to_rot, (T)TAU);
	T post_rot = to_rot + fmod(2.0f * post_diff, (T)TAU) - post_diff;

	return monotonic_cubic_interpolate(from_rot, to_rot, pre_rot, post_rot, p_weight);
}

template <std::floating_point T>
constexpr T monotonic_cubic_interpolate_angle_in_time(T p_from, T p_to, T p_pre, T p_post, T p_weight, T p_to_t, T p_pre_t, T p_post_t) {
	T from_rot = fmod(p_from, (T)TAU);

	T pre_diff = fmod(p_pre - from_rot, (T)TAU);
	T pre_rot = from_rot + fmod((T)2 * pre_diff, (T)TAU) - pre_diff;

	T to_diff = fmod(p_to - from_rot, (T)TAU);
	T to_rot = from_rot + fmod((T)2 * to_diff, (T)TAU) - to_diff;

	T post_diff = fmod(p_post - to_rot, (T)TAU);
	T post_rot = to_rot + fmod((T)2 * post_diff, (T)TAU) - post_diff;

	return monotonic_cubic_interpolate_in_time(from_rot, to_rot, pre_rot, post_rot, p_weight, p_to_t, p_pre_t, p_post_t);
}

}; // namespace Math

/**************************************************************************/
/*  simd.h                                                                */
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
 * @file simd.h
 *
 * Portable 4-lane single precision SIMD wrapper plus runtime CPU feature
 * detection for AVX/AVX2 dispatch.
 *
 * Compile-time baselines:
 *  - x86_64: SSE4.1 (the build system passes -msse4.2 / /d2archSSE42).
 *  - x86_32: SSE2 only; SSE4.1 instructions are emulated.
 *  - Everything else (ARM included): GODOT_SIMD_F32X4 stays undefined and
 *    callers keep their original scalar code.
 *
 * Every operation here is specified to produce bit-identical results to the
 * scalar expression documented next to it, so callers can replace scalar
 * code without changing outputs. In particular there is no FMA, no
 * reciprocal approximation, and min/max follow the `a < b ? a : b` form.
 *
 * AVX/AVX2/FMA code must live in .cpp files, in functions tagged with
 * GODOT_SIMD_TARGET_AVX2 (or _AVX2_FMA), and must only be called after
 * checking CPUFeatures at runtime.
 */

#include "core/math/math_defs.h"
#include "core/typedefs.h"

#include <atomic>

#if defined(GODOT_SIMD_DISABLE)
// Escape hatch: keep every scalar path (used to cross-check the SIMD code).
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define GODOT_SIMD_X86
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#else
#include <immintrin.h>
#endif
#if defined(__x86_64__) || defined(_M_X64)
#define GODOT_SIMD_X86_64
#endif
#if defined(__SSE4_1__) || (defined(_MSC_VER) && defined(_M_X64))
#define GODOT_SIMD_SSE41
#endif
#define GODOT_SIMD_F32X4
#if defined(__GNUC__) || defined(__clang__)
#define GODOT_SIMD_TARGET_AVX __attribute__((target("avx")))
#define GODOT_SIMD_TARGET_AVX2 __attribute__((target("avx2")))
#define GODOT_SIMD_TARGET_AVX2_FMA __attribute__((target("avx2,fma")))
#define GODOT_SIMD_TARGET_FMA __attribute__((target("fma")))
#define GODOT_SIMD_TARGET_F16C __attribute__((target("f16c")))
#else
#define GODOT_SIMD_TARGET_AVX
#define GODOT_SIMD_TARGET_AVX2
#define GODOT_SIMD_TARGET_AVX2_FMA
#define GODOT_SIMD_TARGET_FMA
#define GODOT_SIMD_TARGET_F16C
#endif
#define GODOT_SIMD_AVX_DISPATCH
#endif

// SIMD paths for real_t based types are only compiled for single precision builds.
#if defined(GODOT_SIMD_F32X4) && !defined(REAL_T_IS_DOUBLE)
#define GODOT_SIMD_REAL_F32X4
#endif

class CPUFeatures {
	static std::atomic<uint32_t> features;
	static uint32_t _detect();

public:
	enum Feature : uint32_t {
		INITIALIZED = 1u << 0,
		SSE42 = 1u << 1,
		AVX = 1u << 2,
		AVX2 = 1u << 3,
		FMA = 1u << 4,
		F16C = 1u << 5,
	};

	static _ALWAYS_INLINE_ uint32_t get() {
		uint32_t f = features.load(std::memory_order_relaxed);
		if (unlikely(f == 0)) {
			f = _detect();
			features.store(f, std::memory_order_relaxed);
		}
		return f;
	}
	static _ALWAYS_INLINE_ bool has(Feature p_feature) { return (get() & p_feature) != 0; }
	static _ALWAYS_INLINE_ bool has_avx() { return has(AVX); }
	static _ALWAYS_INLINE_ bool has_avx2() { return has(AVX2); }
	static _ALWAYS_INLINE_ bool has_fma() { return has(FMA); }
	static _ALWAYS_INLINE_ bool has_f16c() { return has(F16C); }
	/// Replaces the detected feature set (always intersected with what the CPU supports). Used by tests.
	static void set_override(uint32_t p_mask);
	static void reset_override();
};

#ifdef GODOT_SIMD_F32X4

namespace simd {

struct f32x4 {
	__m128 v;
	_ALWAYS_INLINE_ f32x4(__m128 p_v) :
			v(p_v) {}
	_ALWAYS_INLINE_ f32x4() = default;
};

// Masks are stored in the same register type; each lane is all ones or all zeros.
using mask4 = f32x4;

_ALWAYS_INLINE_ f32x4 zero4() {
	return _mm_setzero_ps();
}
_ALWAYS_INLINE_ f32x4 set4(float p_x, float p_y, float p_z, float p_w) {
	return _mm_set_ps(p_w, p_z, p_y, p_x);
}
_ALWAYS_INLINE_ f32x4 splat4(float p_v) {
	return _mm_set1_ps(p_v);
}
_ALWAYS_INLINE_ f32x4 load4(const float *p_ptr) {
	return _mm_loadu_ps(p_ptr);
}
/// Loads 3 floats, lane 3 is zero. Never reads past p_ptr[2].
_ALWAYS_INLINE_ f32x4 load3(const float *p_ptr) {
	return _mm_movelh_ps(_mm_castsi128_ps(_mm_loadl_epi64((const __m128i *)p_ptr)), _mm_load_ss(p_ptr + 2));
}
/// Loads 2 floats into lanes 0 and 1, lanes 2 and 3 are zero.
_ALWAYS_INLINE_ f32x4 load2(const float *p_ptr) {
	return _mm_castsi128_ps(_mm_loadl_epi64((const __m128i *)p_ptr));
}
/// Loads 2 floats into both halves: (a, b, a, b).
_ALWAYS_INLINE_ f32x4 load2x2(const float *p_ptr) {
	__m128 lo = load2(p_ptr).v;
	return _mm_movelh_ps(lo, lo);
}
_ALWAYS_INLINE_ void store4(float *p_ptr, f32x4 p_v) {
	_mm_storeu_ps(p_ptr, p_v.v);
}
_ALWAYS_INLINE_ void store3(float *p_ptr, f32x4 p_v) {
	_mm_storel_pi((__m64 *)p_ptr, p_v.v);
	_mm_store_ss(p_ptr + 2, _mm_movehl_ps(p_v.v, p_v.v));
}
_ALWAYS_INLINE_ void store2(float *p_ptr, f32x4 p_v) {
	_mm_storel_pi((__m64 *)p_ptr, p_v.v);
}

_ALWAYS_INLINE_ f32x4 operator+(f32x4 p_a, f32x4 p_b) {
	return _mm_add_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ f32x4 operator-(f32x4 p_a, f32x4 p_b) {
	return _mm_sub_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ f32x4 operator*(f32x4 p_a, f32x4 p_b) {
	return _mm_mul_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ f32x4 operator/(f32x4 p_a, f32x4 p_b) {
	return _mm_div_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ f32x4 operator-(f32x4 p_a) {
	return _mm_xor_ps(p_a.v, _mm_set1_ps(-0.0f));
}
_ALWAYS_INLINE_ f32x4 operator&(f32x4 p_a, f32x4 p_b) {
	return _mm_and_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ f32x4 operator|(f32x4 p_a, f32x4 p_b) {
	return _mm_or_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ f32x4 operator^(f32x4 p_a, f32x4 p_b) {
	return _mm_xor_ps(p_a.v, p_b.v);
}

_ALWAYS_INLINE_ f32x4 abs4(f32x4 p_a) {
	return _mm_andnot_ps(_mm_set1_ps(-0.0f), p_a.v);
}
_ALWAYS_INLINE_ f32x4 sqrt4(f32x4 p_a) {
	return _mm_sqrt_ps(p_a.v);
}
/// a < b ? a : b (per lane, matches MIN()).
_ALWAYS_INLINE_ f32x4 min4(f32x4 p_a, f32x4 p_b) {
	return _mm_min_ps(p_a.v, p_b.v);
}
/// a > b ? a : b (per lane, matches MAX()).
_ALWAYS_INLINE_ f32x4 max4(f32x4 p_a, f32x4 p_b) {
	return _mm_max_ps(p_a.v, p_b.v);
}

_ALWAYS_INLINE_ mask4 cmp_lt(f32x4 p_a, f32x4 p_b) {
	return _mm_cmplt_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ mask4 cmp_le(f32x4 p_a, f32x4 p_b) {
	return _mm_cmple_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ mask4 cmp_gt(f32x4 p_a, f32x4 p_b) {
	return _mm_cmpgt_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ mask4 cmp_ge(f32x4 p_a, f32x4 p_b) {
	return _mm_cmpge_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ mask4 cmp_eq(f32x4 p_a, f32x4 p_b) {
	return _mm_cmpeq_ps(p_a.v, p_b.v);
}
_ALWAYS_INLINE_ mask4 cmp_neq(f32x4 p_a, f32x4 p_b) {
	return _mm_cmpneq_ps(p_a.v, p_b.v);
}
/// Bit i is set when lane i has its sign bit set (all ones for masks).
_ALWAYS_INLINE_ int movemask(mask4 p_m) {
	return _mm_movemask_ps(p_m.v);
}

/// mask ? a : b per lane.
_ALWAYS_INLINE_ f32x4 select(mask4 p_mask, f32x4 p_a, f32x4 p_b) {
#ifdef GODOT_SIMD_SSE41
	return _mm_blendv_ps(p_b.v, p_a.v, p_mask.v);
#else
	return _mm_or_ps(_mm_and_ps(p_mask.v, p_a.v), _mm_andnot_ps(p_mask.v, p_b.v));
#endif
}

_ALWAYS_INLINE_ f32x4 floor4(f32x4 p_a) {
#ifdef GODOT_SIMD_SSE41
	return _mm_floor_ps(p_a.v);
#else
	// Values with |x| >= 2^23 are already integers; keep them unchanged.
	__m128 big = _mm_cmpge_ps(_mm_andnot_ps(_mm_set1_ps(-0.0f), p_a.v), _mm_set1_ps(8388608.0f));
	__m128 t = _mm_cvtepi32_ps(_mm_cvttps_epi32(p_a.v));
	t = _mm_sub_ps(t, _mm_and_ps(_mm_cmpgt_ps(t, p_a.v), _mm_set1_ps(1.0f)));
	t = _mm_or_ps(_mm_and_ps(big, p_a.v), _mm_andnot_ps(big, t));
	// Preserve -0.0 and NaN.
	__m128 nan = _mm_cmpunord_ps(p_a.v, p_a.v);
	t = _mm_or_ps(_mm_and_ps(nan, p_a.v), _mm_andnot_ps(nan, t));
	return _mm_or_ps(t, _mm_and_ps(p_a.v, _mm_set1_ps(-0.0f)));
#endif
}
_ALWAYS_INLINE_ f32x4 ceil4(f32x4 p_a) {
#ifdef GODOT_SIMD_SSE41
	return _mm_ceil_ps(p_a.v);
#else
	__m128 big = _mm_cmpge_ps(_mm_andnot_ps(_mm_set1_ps(-0.0f), p_a.v), _mm_set1_ps(8388608.0f));
	__m128 t = _mm_cvtepi32_ps(_mm_cvttps_epi32(p_a.v));
	t = _mm_add_ps(t, _mm_and_ps(_mm_cmplt_ps(t, p_a.v), _mm_set1_ps(1.0f)));
	t = _mm_or_ps(_mm_and_ps(big, p_a.v), _mm_andnot_ps(big, t));
	__m128 nan = _mm_cmpunord_ps(p_a.v, p_a.v);
	t = _mm_or_ps(_mm_and_ps(nan, p_a.v), _mm_andnot_ps(nan, t));
	return _mm_or_ps(t, _mm_and_ps(p_a.v, _mm_set1_ps(-0.0f)));
#endif
}
/// Round toward zero.
_ALWAYS_INLINE_ f32x4 trunc4(f32x4 p_a) {
#ifdef GODOT_SIMD_SSE41
	return _mm_round_ps(p_a.v, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
#else
	__m128 big = _mm_cmpge_ps(_mm_andnot_ps(_mm_set1_ps(-0.0f), p_a.v), _mm_set1_ps(8388608.0f));
	__m128 t = _mm_cvtepi32_ps(_mm_cvttps_epi32(p_a.v));
	t = _mm_or_ps(_mm_and_ps(big, p_a.v), _mm_andnot_ps(big, t));
	__m128 nan = _mm_cmpunord_ps(p_a.v, p_a.v);
	t = _mm_or_ps(_mm_and_ps(nan, p_a.v), _mm_andnot_ps(nan, t));
	return _mm_or_ps(t, _mm_and_ps(p_a.v, _mm_set1_ps(-0.0f)));
#endif
}

template <int A, int B, int C, int D>
_ALWAYS_INLINE_ f32x4 permute(f32x4 p_a) {
	return _mm_shuffle_ps(p_a.v, p_a.v, _MM_SHUFFLE(D, C, B, A));
}
/// (a[A], a[B], b[C], b[D])
template <int A, int B, int C, int D>
_ALWAYS_INLINE_ f32x4 shuffle(f32x4 p_a, f32x4 p_b) {
	return _mm_shuffle_ps(p_a.v, p_b.v, _MM_SHUFFLE(D, C, B, A));
}
template <int I>
_ALWAYS_INLINE_ f32x4 splat_lane(f32x4 p_a) {
	return _mm_shuffle_ps(p_a.v, p_a.v, _MM_SHUFFLE(I, I, I, I));
}
template <int I>
_ALWAYS_INLINE_ float get_lane(f32x4 p_a) {
	if constexpr (I == 0) {
		return _mm_cvtss_f32(p_a.v);
	} else {
		return _mm_cvtss_f32(_mm_shuffle_ps(p_a.v, p_a.v, _MM_SHUFFLE(I, I, I, I)));
	}
}
/// (a0, a1, a2, b3)
_ALWAYS_INLINE_ f32x4 insert_lane3(f32x4 p_a, f32x4 p_b) {
#ifdef GODOT_SIMD_SSE41
	return _mm_blend_ps(p_a.v, p_b.v, 8);
#else
	return _mm_shuffle_ps(p_a.v, _mm_shuffle_ps(p_a.v, p_b.v, _MM_SHUFFLE(3, 3, 2, 2)), _MM_SHUFFLE(2, 0, 1, 0));
#endif
}
/// (a0, a1, b0, b1)
_ALWAYS_INLINE_ f32x4 movelh(f32x4 p_a, f32x4 p_b) {
	return _mm_movelh_ps(p_a.v, p_b.v);
}
/// (b2, b3, a2, a3)
_ALWAYS_INLINE_ f32x4 movehl(f32x4 p_a, f32x4 p_b) {
	return _mm_movehl_ps(p_a.v, p_b.v);
}
/// (a0, b0, a1, b1)
_ALWAYS_INLINE_ f32x4 unpacklo(f32x4 p_a, f32x4 p_b) {
	return _mm_unpacklo_ps(p_a.v, p_b.v);
}
/// (a2, b2, a3, b3)
_ALWAYS_INLINE_ f32x4 unpackhi(f32x4 p_a, f32x4 p_b) {
	return _mm_unpackhi_ps(p_a.v, p_b.v);
}

_ALWAYS_INLINE_ f32x4 &operator+=(f32x4 &p_a, f32x4 p_b) {
	p_a = p_a + p_b;
	return p_a;
}
_ALWAYS_INLINE_ f32x4 &operator-=(f32x4 &p_a, f32x4 p_b) {
	p_a = p_a - p_b;
	return p_a;
}
_ALWAYS_INLINE_ f32x4 &operator*=(f32x4 &p_a, f32x4 p_b) {
	p_a = p_a * p_b;
	return p_a;
}
_ALWAYS_INLINE_ f32x4 &operator/=(f32x4 &p_a, f32x4 p_b) {
	p_a = p_a / p_b;
	return p_a;
}

_ALWAYS_INLINE_ bool all_lanes(mask4 p_m) {
	return movemask(p_m) == 0xF;
}
_ALWAYS_INLINE_ bool any_lane(mask4 p_m) {
	return movemask(p_m) != 0;
}
_ALWAYS_INLINE_ bool all_lanes3(mask4 p_m) {
	return (movemask(p_m) & 0x7) == 0x7;
}
_ALWAYS_INLINE_ bool any_lane3(mask4 p_m) {
	return (movemask(p_m) & 0x7) != 0;
}

// Per-lane int32 counters, exact like the scalar `int` counts they replace.
struct i32x4 {
	__m128i v;
};
_ALWAYS_INLINE_ i32x4 zero_i32x4() {
	return { _mm_setzero_si128() };
}
// Adds one to each lane whose mask is set (a set mask lane is -1 as an integer).
_ALWAYS_INLINE_ i32x4 count_if(i32x4 p_count, mask4 p_m) {
	return { _mm_sub_epi32(p_count.v, _mm_castps_si128(p_m.v)) };
}
_ALWAYS_INLINE_ mask4 cmp_eq(i32x4 p_a, int32_t p_b) {
	return _mm_castsi128_ps(_mm_cmpeq_epi32(p_a.v, _mm_set1_epi32(p_b)));
}
_ALWAYS_INLINE_ i32x4 iota_i32x4(int32_t p_first) {
	return { _mm_add_epi32(_mm_set1_epi32(p_first), _mm_setr_epi32(0, 1, 2, 3)) };
}
_ALWAYS_INLINE_ i32x4 add(i32x4 p_a, int32_t p_b) {
	return { _mm_add_epi32(p_a.v, _mm_set1_epi32(p_b)) };
}
_ALWAYS_INLINE_ i32x4 select(mask4 p_mask, i32x4 p_a, i32x4 p_b) {
	const __m128i m = _mm_castps_si128(p_mask.v);
	return { _mm_or_si128(_mm_and_si128(m, p_a.v), _mm_andnot_si128(m, p_b.v)) };
}
_ALWAYS_INLINE_ void store_i32x4(int32_t *r_ptr, i32x4 p_v) {
	_mm_storeu_si128((__m128i *)r_ptr, p_v.v);
}

/// Matches Math::round(): v >= 0 ? (T)(long long)(v + 0.5) : (T)(long long)(v - 0.5), including what the
/// hardware float -> int64 conversion does for NaN and out of range values on each architecture.
_ALWAYS_INLINE_ f32x4 round4(f32x4 p_a) {
	const f32x4 half = splat4(0.5f);
	f32x4 adj = select(cmp_ge(p_a, zero4()), p_a + half, p_a - half);
	// The integer round trip turns -0.0 into +0.0.
	f32x4 t = trunc4(adj) + zero4();
	const f32x4 limit = splat4(9223372036854775808.0f);
	// cvttss2si returns 0x8000000000000000 for NaN and anything outside the int64 range.
	mask4 invalid = cmp_ge(abs4(adj), limit) | cmp_neq(adj, adj);
	return select(invalid, -limit, t);
}

/// Matches SIGN(): v > 0 ? 1 : (v < 0 ? -1 : 0).
_ALWAYS_INLINE_ f32x4 sign4(f32x4 p_a) {
	f32x4 pos = cmp_gt(p_a, zero4()) & splat4(1.0f);
	f32x4 neg = cmp_lt(p_a, zero4()) & splat4(-1.0f);
	return pos | neg;
}

/// Matches CLAMP(): v < min ? min : (v > max ? max : v). NaN passes through.
_ALWAYS_INLINE_ f32x4 clamp4(f32x4 p_v, f32x4 p_min, f32x4 p_max) {
	f32x4 r = select(cmp_gt(p_v, p_max), p_max, p_v);
	return select(cmp_lt(p_v, p_min), p_min, r);
}

/// from + (to - from) * weight, per lane (matches Math::lerp).
_ALWAYS_INLINE_ f32x4 lerp4(f32x4 p_from, f32x4 p_to, f32x4 p_weight) {
	return p_from + (p_to - p_from) * p_weight;
}

_ALWAYS_INLINE_ float dot3(f32x4 p_a, f32x4 p_b) {
	f32x4 m = p_a * p_b;
	return (get_lane<0>(m) + get_lane<1>(m)) + get_lane<2>(m);
}
_ALWAYS_INLINE_ float dot4(f32x4 p_a, f32x4 p_b) {
	f32x4 m = p_a * p_b;
	return ((get_lane<0>(m) + get_lane<1>(m)) + get_lane<2>(m)) + get_lane<3>(m);
}
/// Vector3::cross: (ay*bz - az*by, az*bx - ax*bz, ax*by - ay*bx). Lane 3 is zero when both inputs have zero in lane 3.
_ALWAYS_INLINE_ f32x4 cross3(f32x4 p_a, f32x4 p_b) {
	f32x4 a_yzx = permute<1, 2, 0, 3>(p_a);
	f32x4 b_zxy = permute<2, 0, 1, 3>(p_b);
	f32x4 a_zxy = permute<2, 0, 1, 3>(p_a);
	f32x4 b_yzx = permute<1, 2, 0, 3>(p_b);
	return a_yzx * b_zxy - a_zxy * b_yzx;
}

/// Full 4x4 transpose.
_ALWAYS_INLINE_ void transpose4(f32x4 &r_r0, f32x4 &r_r1, f32x4 &r_r2, f32x4 &r_r3) {
	f32x4 t0 = unpacklo(r_r0, r_r1);
	f32x4 t1 = unpackhi(r_r0, r_r1);
	f32x4 t2 = unpacklo(r_r2, r_r3);
	f32x4 t3 = unpackhi(r_r2, r_r3);
	r_r0 = movelh(t0, t2);
	r_r1 = movehl(t2, t0);
	r_r2 = movelh(t1, t3);
	r_r3 = movehl(t3, t1);
}

/// Math::is_equal_approx per lane: a == b, or |a - b| < max(EPS * |a|, EPS) with EPS = CMP_EPSILON.
_ALWAYS_INLINE_ mask4 is_equal_approx4(f32x4 p_a, f32x4 p_b) {
	const f32x4 eps = splat4((float)CMP_EPSILON);
	f32x4 t = eps * abs4(p_a);
	f32x4 tol = select(cmp_lt(t, eps), eps, t);
	return cmp_eq(p_a, p_b) | cmp_lt(abs4(p_a - p_b), tol);
}

/// Math::is_zero_approx per lane: |a| < CMP_EPSILON.
_ALWAYS_INLINE_ mask4 is_zero_approx4(f32x4 p_a) {
	return cmp_lt(abs4(p_a), splat4((float)CMP_EPSILON));
}

/// Truncating float -> integer conversion of each lane, keeping the low byte, packed little-endian (lane 0 in the low byte).
_ALWAYS_INLINE_ uint32_t pack_trunc_u8x4(f32x4 p_v) {
	__m128i i = _mm_cvttps_epi32(p_v.v);
	i = _mm_and_si128(i, _mm_set1_epi32(0xFF));
	i = _mm_packs_epi32(i, i);
	i = _mm_packus_epi16(i, i);
	return (uint32_t)_mm_cvtsi128_si32(i);
}
/// Same as pack_trunc_u8x4 but keeping the low 16 bits of each lane.
_ALWAYS_INLINE_ uint64_t pack_trunc_u16x4(f32x4 p_v) {
	__m128i i = _mm_cvttps_epi32(p_v.v);
	i = _mm_and_si128(i, _mm_set1_epi32(0xFFFF));
	i = _mm_sub_epi32(i, _mm_set1_epi32(0x8000));
	i = _mm_packs_epi32(i, i);
	i = _mm_xor_si128(i, _mm_set1_epi16((short)-0x8000));
	uint64_t out;
	_mm_storel_epi64((__m128i *)&out, i);
	return out;
}

/// Math::half_to_float for 4 halves held in the low 16 bits of each 32-bit lane (bit exact, subnormals included).
_ALWAYS_INLINE_ __m128i half_to_float_bits4(__m128i p_half) {
	const __m128i sign = _mm_slli_epi32(_mm_and_si128(p_half, _mm_set1_epi32(0x8000)), 16);
	const __m128i exp = _mm_and_si128(p_half, _mm_set1_epi32(0x7C00));
	const __m128i frac = _mm_and_si128(p_half, _mm_set1_epi32(0x03FF));
	// Normal: ((exp | frac) << 13) + ((127 - 15) << 23).
	__m128i normal = _mm_add_epi32(_mm_slli_epi32(_mm_or_si128(exp, frac), 13), _mm_set1_epi32(112 << 23));
	// Inf/NaN: keep the fraction bits.
	__m128i infnan = _mm_or_si128(_mm_set1_epi32(0x7F800000), _mm_slli_epi32(frac, 13));
	// Subnormal: the value is frac * 2^-24, which converts exactly.
	__m128i subnormal = _mm_castps_si128(_mm_mul_ps(_mm_cvtepi32_ps(frac), _mm_set1_ps(5.9604644775390625e-8f)));
	__m128i is_zero_exp = _mm_cmpeq_epi32(exp, _mm_setzero_si128());
	__m128i is_max_exp = _mm_cmpeq_epi32(exp, _mm_set1_epi32(0x7C00));
	__m128i r = _mm_or_si128(_mm_and_si128(is_max_exp, infnan), _mm_andnot_si128(is_max_exp, normal));
	r = _mm_or_si128(_mm_and_si128(is_zero_exp, subnormal), _mm_andnot_si128(is_zero_exp, r));
	return _mm_or_si128(r, sign);
}
_ALWAYS_INLINE_ f32x4 half_to_float4(const uint16_t *p_half) {
	__m128i h = _mm_loadl_epi64((const __m128i *)p_half);
	h = _mm_unpacklo_epi16(h, _mm_setzero_si128());
	return _mm_castsi128_ps(half_to_float_bits4(h));
}
/// Math::make_half_float for 4 floats: truncating conversion, |x| < 2^-14 becomes signed zero,
/// no overflow handling (the biased exponent wraps into the upper bits like the scalar code).
_ALWAYS_INLINE_ __m128i float_to_half_bits4(f32x4 p_v) {
	__m128i bits = _mm_castps_si128(p_v.v);
	__m128i sign = _mm_srli_epi32(_mm_and_si128(bits, _mm_set1_epi32(INT32_MIN)), 16);
	__m128i exp = _mm_and_si128(bits, _mm_set1_epi32(0x7F800000));
	__m128i abs_bits = _mm_and_si128(bits, _mm_set1_epi32(0x7FFFFFFF));
	__m128i normal = _mm_sub_epi32(_mm_srli_epi32(abs_bits, 13), _mm_set1_epi32(112 << 10));
	__m128i infnan = _mm_or_si128(_mm_set1_epi32(0x7C00), _mm_srli_epi32(_mm_and_si128(bits, _mm_set1_epi32(0x007FFFFF)), 13));
	__m128i is_zero = _mm_cmpgt_epi32(_mm_set1_epi32((112 << 23) + 1), exp); // exp <= 112 << 23
	__m128i is_max_exp = _mm_cmpeq_epi32(exp, _mm_set1_epi32(0x7F800000));
	__m128i r = _mm_or_si128(_mm_and_si128(is_max_exp, infnan), _mm_andnot_si128(is_max_exp, normal));
	r = _mm_andnot_si128(is_zero, r);
	return _mm_and_si128(_mm_or_si128(r, sign), _mm_set1_epi32(0xFFFF));
}
_ALWAYS_INLINE_ void float_to_half4(f32x4 p_v, uint16_t *r_half) {
	__m128i h = float_to_half_bits4(p_v);
	// Values are already masked to 16 bits, so a signed pack after biasing is exact.
	h = _mm_sub_epi32(h, _mm_set1_epi32(0x8000));
	h = _mm_packs_epi32(h, h);
	h = _mm_xor_si128(h, _mm_set1_epi16((short)-0x8000));
	_mm_storel_epi64((__m128i *)r_half, h);
}

/// Math::cubic_interpolate_in_time per lane; the blend factors only depend on the scalar time arguments.
_ALWAYS_INLINE_ f32x4 cubic_interpolate_in_time4(f32x4 p_from, f32x4 p_to, f32x4 p_pre, f32x4 p_post, float p_weight, float p_to_t, float p_pre_t, float p_post_t) {
	float t = 0.0f + (p_to_t - 0.0f) * p_weight;
	float pre_scale = (p_pre_t == 0.0f) ? 0.0f : (t - p_pre_t) / -p_pre_t;
	float to_scale = (p_to_t == 0.0f) ? 0.5f : t / p_to_t;
	float post_range = p_post_t - p_to_t;
	float post_scale = (post_range == 0.0f) ? 1.0f : (t - p_to_t) / post_range;
	f32x4 a1 = lerp4(p_pre, p_from, splat4(pre_scale));
	f32x4 a2 = lerp4(p_from, p_to, splat4(to_scale));
	f32x4 a3 = lerp4(p_to, p_post, splat4(post_scale));
	float mid_range = p_to_t - p_pre_t;
	float from_to_scale = (mid_range == 0.0f) ? 0.0f : (t - p_pre_t) / mid_range;
	float to_post_scale = (p_post_t == 0.0f) ? 1.0f : t / p_post_t;
	f32x4 b1 = lerp4(a1, a2, splat4(from_to_scale));
	f32x4 b2 = lerp4(a2, a3, splat4(to_post_scale));
	float final_scale = (p_to_t == 0.0f) ? 0.5f : t / p_to_t;
	return lerp4(b1, b2, splat4(final_scale));
}

/// Math::cubic_interpolate per lane. The scalar version evaluates a chain of std::fma at runtime, so this
/// needs hardware fused multiply-add to match it: on x86 call it only when has_fused_cubic() is true.
f32x4 cubic_interpolate4_fma(f32x4 p_from, f32x4 p_to, f32x4 p_pre, f32x4 p_post, float p_weight);
_ALWAYS_INLINE_ bool has_fused_cubic() {
	return CPUFeatures::has_fma();
}

/// Loads 4 consecutive Vector3 (12 contiguous floats) into SoA lanes: r_x = (x0, x1, x2, x3) etc.
_ALWAYS_INLINE_ void load_vec3x4_soa(const float *p_v, f32x4 &r_x, f32x4 &r_y, f32x4 &r_z) {
	f32x4 v0 = load4(p_v); // x0 y0 z0 x1
	f32x4 v1 = load4(p_v + 4); // y1 z1 x2 y2
	f32x4 v2 = load4(p_v + 8); // z2 x3 y3 z3
	f32x4 t = shuffle<2, 2, 1, 1>(v1, v2); // x2 x2 x3 x3
	r_x = shuffle<0, 3, 0, 2>(v0, t);
	f32x4 t1 = shuffle<1, 1, 0, 0>(v0, v1); // y0 y0 y1 y1
	f32x4 t2 = shuffle<3, 3, 2, 2>(v1, v2); // y2 y2 y3 y3
	r_y = shuffle<0, 2, 0, 2>(t1, t2);
	f32x4 t3 = shuffle<2, 2, 1, 1>(v0, v1); // z0 z0 z1 z1
	r_z = shuffle<0, 2, 0, 3>(t3, v2);
}

/// Transform3D::xform applied to SoA vertices: ((r00*x + r01*y) + r02*z) + ox per lane.
struct Transform3DSoA {
	f32x4 r00, r01, r02, r10, r11, r12, r20, r21, r22, ox, oy, oz;
	/// p_t points at the 12 floats of a Transform3D (3 basis rows then the origin).
	_ALWAYS_INLINE_ explicit Transform3DSoA(const float *p_t) {
		r00 = splat4(p_t[0]);
		r01 = splat4(p_t[1]);
		r02 = splat4(p_t[2]);
		r10 = splat4(p_t[3]);
		r11 = splat4(p_t[4]);
		r12 = splat4(p_t[5]);
		r20 = splat4(p_t[6]);
		r21 = splat4(p_t[7]);
		r22 = splat4(p_t[8]);
		ox = splat4(p_t[9]);
		oy = splat4(p_t[10]);
		oz = splat4(p_t[11]);
	}
	_ALWAYS_INLINE_ void xform(f32x4 &r_x, f32x4 &r_y, f32x4 &r_z) const {
		f32x4 x = ((r00 * r_x + r01 * r_y) + r02 * r_z) + ox;
		f32x4 y = ((r10 * r_x + r11 * r_y) + r12 * r_z) + oy;
		f32x4 z = ((r20 * r_x + r21 * r_y) + r22 * r_z) + oz;
		r_x = x;
		r_y = y;
		r_z = z;
	}
};

/// Loads the 3 rows of a 3x3 row-major matrix (9 contiguous floats) without reading past the end.
_ALWAYS_INLINE_ void load_mat3(const float *p_m, f32x4 &r_r0, f32x4 &r_r1, f32x4 &r_r2) {
	r_r0 = load4(p_m);
	r_r1 = load4(p_m + 3);
	// 8 + 4 byte loads so a row just written by store3 still forwards from the store buffer.
	r_r2 = load3(p_m + 6);
}

} // namespace simd

#endif // GODOT_SIMD_F32X4

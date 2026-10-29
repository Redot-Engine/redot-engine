/**************************************************************************/
/*  test_simd.h                                                           */
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

// Every function with a SIMD path is compared against a verbatim copy of its original scalar
// implementation (tests/core/math/simd_reference.h) on random and IEEE edge-case inputs, once for
// each runtime dispatch level. Results must match bit for bit; the only tolerance is that any two
// NaNs compare equal, since SIMD and scalar code may legitimately produce different NaN payloads.

#include "core/math/dynamic_bvh.h"
#include "core/math/simd.h"
#include "core/math/simd_geometry.h"
#include "core/string/ustring_simd.h"
#include "servers/audio/audio_mix_kernels.h"
#include "servers/audio/effects/audio_effect_stereo_enhance.h"
#include "tests/core/math/simd_reference.h"
#include "tests/test_macros.h"

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_GODOT_PHYSICS_2D_ENABLED
#include "modules/godot_physics_2d/godot_shape_2d.h"
#endif

#include <algorithm>
#include <cfenv>
#include <cmath>
#include <cstring>

namespace TestSIMD {

using namespace SIMDReference;

#ifdef DEV_ENABLED
constexpr int ITERATIONS = 3000;
#else
constexpr int ITERATIONS = 20000;
#endif

struct Random {
	uint64_t state;

	explicit Random(uint64_t p_seed) :
			state(p_seed * 0x9E3779B97F4A7C15ull + 0x2545F4914F6CDD1Dull) {}

	uint32_t next() {
		state ^= state << 13;
		state ^= state >> 7;
		state ^= state << 17;
		return uint32_t(state >> 11);
	}
	uint32_t range(uint32_t p_n) { return next() % p_n; }
	bool chance(uint32_t p_one_in) { return range(p_one_in) == 0; }

	float edge_float() {
		static const float specials[] = {
			0.0f, -0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 1e-45f, -1e-45f, 1.17549435e-38f, -1.17549435e-38f,
			3.4e38f, -3.4e38f, INFINITY, -INFINITY, NAN, 2.5f, -2.5f, 16777216.0f, 8388608.5f,
			0.49999997f, -0.49999997f, 1e20f, -1e20f, 255.5f, 65504.0f, 65520.0f
		};
		switch (range(16)) {
			case 0:
				return specials[range(sizeof(specials) / sizeof(specials[0]))];
			case 1: {
				uint32_t bits = next();
				float f;
				memcpy(&f, &bits, 4);
				return f;
			}
			case 2:
				return float(int(range(21)) - 10);
			default:
				return (float(range(2000001)) - 1000000.0f) / 977.0f;
		}
	}
	float finite_float() {
		float f;
		do {
			f = edge_float();
		} while (!std::isfinite(f));
		return f;
	}
	float unit() { return float(range(2000001)) / 1000000.0f - 1.0f; }

	Vector2 v2() { return Vector2(edge_float(), edge_float()); }
	Vector3 v3() { return Vector3(edge_float(), edge_float(), edge_float()); }
	Vector3 v3_finite() { return Vector3(finite_float(), finite_float(), finite_float()); }
	Vector4 v4() { return Vector4(edge_float(), edge_float(), edge_float(), edge_float()); }
	Color color() { return Color(edge_float(), edge_float(), edge_float(), edge_float()); }
	Plane plane() { return Plane(edge_float(), edge_float(), edge_float(), edge_float()); }
	Basis basis() { return Basis(v3(), v3(), v3()); }
	Transform3D transform() { return Transform3D(basis(), v3()); }
	Transform2D transform2d() { return Transform2D(v2(), v2(), v2()); }
	Quaternion quaternion() {
		Quaternion q(unit(), unit(), unit(), unit());
		return q.length_squared() > 0.01f ? q.normalized() : Quaternion();
	}
	AABB aabb() {
		Vector3 size = v3();
		if (!chance(4)) {
			size = size.abs();
		}
		return AABB(v3(), size);
	}
	Rect2 rect() {
		Vector2 size = v2();
		if (!chance(4)) {
			size = size.abs();
		}
		return Rect2(v2(), size);
	}
	Projection projection() {
		Projection p;
		for (int i = 0; i < 4; i++) {
			p.columns[i] = v4();
		}
		return p;
	}
};

// Compares two values word by word as W (float or double); identical bits or two NaNs are equal.
template <typename W, typename T>
bool same_words(const T &p_a, const T &p_b) {
	static_assert(sizeof(T) % sizeof(W) == 0);
	constexpr size_t count = sizeof(T) / sizeof(W);
	W a[count];
	W b[count];
	memcpy(a, &p_a, sizeof(T));
	memcpy(b, &p_b, sizeof(T));
	for (size_t i = 0; i < count; i++) {
		if (memcmp(&a[i], &b[i], sizeof(W)) == 0) {
			continue;
		}
		if (a[i] != a[i] && b[i] != b[i]) {
			continue;
		}
		return false;
	}
	return true;
}

inline bool same_float_buffers(const float *p_a, const float *p_b, int64_t p_count) {
	for (int64_t i = 0; i < p_count; i++) {
		if (!same_words<float>(p_a[i], p_b[i])) {
			return false;
		}
	}
	return true;
}

inline bool same_half_buffers(const uint16_t *p_a, const uint16_t *p_b, int64_t p_count) {
	for (int64_t i = 0; i < p_count; i++) {
		if (p_a[i] == p_b[i]) {
			continue;
		}
		// Two NaNs are equal: the payload of a NaN result is not meaningful and depends on codegen.
		const bool nan_a = (p_a[i] & 0x7C00) == 0x7C00 && (p_a[i] & 0x03FF) != 0;
		const bool nan_b = (p_b[i] & 0x7C00) == 0x7C00 && (p_b[i] & 0x03FF) != 0;
		if (nan_a && nan_b) {
			continue;
		}
		return false;
	}
	return true;
}

// Image data compared bit-for-bit, except that any two NaNs (in float or half formats) are equal.
inline bool same_image_data(Image::Format p_format, const Vector<uint8_t> &p_a, const Vector<uint8_t> &p_b) {
	if (p_a.size() != p_b.size()) {
		return false;
	}
	if (p_format >= Image::FORMAT_RF && p_format <= Image::FORMAT_RGBAF) {
		return same_float_buffers((const float *)p_a.ptr(), (const float *)p_b.ptr(), p_a.size() / 4);
	}
	if (p_format >= Image::FORMAT_RH && p_format <= Image::FORMAT_RGBAH) {
		return same_half_buffers((const uint16_t *)p_a.ptr(), (const uint16_t *)p_b.ptr(), p_a.size() / 2);
	}
	return memcmp(p_a.ptr(), p_b.ptr(), p_a.size()) == 0;
}

struct Mismatches {
	int count = 0;
	String first;

	void add(const String &p_what) {
		if (count == 0) {
			first = p_what;
		}
		count++;
	}
};

#define SIMD_EXPECT_SAME_REAL(m_mismatches, m_current, m_reference)                                      \
	{                                                                                                    \
		const auto _current = (m_current);                                                               \
		const auto _reference = (m_reference);                                                           \
		if (!same_words<real_t>(_current, _reference)) {                                                 \
			m_mismatches.add(vformat("%s (iteration %d, dispatch: %s)", #m_current, iteration, p_mode)); \
		}                                                                                                \
	}

#define SIMD_EXPECT_SAME_FLOAT(m_mismatches, m_current, m_reference)                                     \
	{                                                                                                    \
		const auto _current = (m_current);                                                               \
		const auto _reference = (m_reference);                                                           \
		if (!same_words<float>(_current, _reference)) {                                                  \
			m_mismatches.add(vformat("%s (iteration %d, dispatch: %s)", #m_current, iteration, p_mode)); \
		}                                                                                                \
	}

#define SIMD_EXPECT_EQUAL(m_mismatches, m_current, m_reference)                                          \
	{                                                                                                    \
		const auto _current = (m_current);                                                               \
		const auto _reference = (m_reference);                                                           \
		if (!(_current == _reference)) {                                                                 \
			m_mismatches.add(vformat("%s (iteration %d, dispatch: %s)", #m_current, iteration, p_mode)); \
		}                                                                                                \
	}

#define SIMD_REPORT(m_mismatches) \
	CHECK_MESSAGE(m_mismatches.count == 0, vformat("%d mismatches against the scalar reference, first: %s", m_mismatches.count, m_mismatches.first))

struct DispatchMode {
	const char *name;
	uint32_t mask;
};

// Each level is intersected with what the CPU supports, so on CPUs without AVX the levels collapse.
static const DispatchMode DISPATCH_MODES[] = {
	{ "all detected extensions", 0xFFFFFFFFu },
	{ "AVX without AVX2/FMA/F16C", CPUFeatures::SSE42 | CPUFeatures::AVX },
	{ "baseline SSE", CPUFeatures::SSE42 },
};

template <typename F>
void for_each_dispatch(F p_func) {
	// Scope guard: a failing REQUIRE inside p_func throws, and the override must still be cleared so
	// later test cases run with the CPU's real feature set rather than a leftover SSE-only override.
	struct ResetGuard {
		~ResetGuard() { CPUFeatures::reset_override(); }
	} reset_guard;
	for (const DispatchMode &mode : DISPATCH_MODES) {
		CPUFeatures::set_override(mode.mask);
		p_func(mode.name);
	}
}

TEST_CASE("[SIMD] Runtime CPU feature dispatch") {
	CHECK(CPUFeatures::has(CPUFeatures::INITIALIZED));
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#ifdef GODOT_SIMD_DISABLE
	MESSAGE("SIMD is disabled at compile time (GODOT_SIMD_DISABLE), every test below exercises the scalar code.");
#else
#ifndef GODOT_SIMD_F32X4
	FAIL("GODOT_SIMD_F32X4 must be defined on x86.");
#endif
#endif
#endif

	CPUFeatures::set_override(0);
	CHECK(CPUFeatures::has(CPUFeatures::INITIALIZED));
	CHECK_FALSE(CPUFeatures::has_avx());
	CHECK_FALSE(CPUFeatures::has_avx2());
	CHECK_FALSE(CPUFeatures::has_fma());
	CHECK_FALSE(CPUFeatures::has_f16c());

	CPUFeatures::set_override(0xFFFFFFFFu);
	if (CPUFeatures::has_avx2() || CPUFeatures::has_fma() || CPUFeatures::has_f16c()) {
		CHECK_MESSAGE(CPUFeatures::has_avx(), "AVX2, FMA and F16C are only reported when AVX state is enabled.");
	}
	CPUFeatures::reset_override();
}

#ifdef GODOT_SIMD_F32X4
TEST_CASE("[SIMD] Integer lanes count and index exactly past 2^24") {
	// Float lanes stop representing consecutive integers at 2^24; the counters and vertex indices
	// used by the convex-shape and vertex-scan paths must not.
	using namespace simd;
	const mask4 pattern = cmp_gt(set4(1, 0, 1, 1), zero4());
	for (int32_t start : { 16777213, 16777215, 16777216, 16777217, 1 << 30 }) {
		i32x4 index = iota_i32x4(start);
		i32x4 count = add(zero_i32x4(), start);
		i32x4 picked = zero_i32x4();
		for (int step = 0; step < 6; step++) {
			int32_t lanes[4];
			store_i32x4(lanes, index);
			for (int k = 0; k < 4; k++) {
				CHECK(lanes[k] == start + step * 4 + k);
			}
			count = count_if(count, pattern);
			picked = select(pattern, index, picked);
			index = add(index, 4);
		}
		int32_t counts[4];
		int32_t picks[4];
		store_i32x4(counts, count);
		store_i32x4(picks, picked);
		CHECK(counts[0] == start + 6);
		CHECK(counts[1] == start);
		CHECK(counts[2] == start + 6);
		CHECK(counts[3] == start + 6);
		CHECK(picks[0] == start + 20);
		CHECK(picks[1] == 0);
		CHECK(picks[2] == start + 22);
		CHECK(picks[3] == start + 23);
		CHECK(movemask(cmp_eq(count, start + 6)) == 0xD);
	}
}
#endif

TEST_CASE("[SIMD] AABB and Transform3D match the scalar reference") {
	ERR_PRINT_OFF;
	for_each_dispatch([](const char *p_mode) {
		Random rng(11);
		Mismatches mismatches;
		for (int iteration = 0; iteration < ITERATIONS; iteration++) {
			const AABB aabb = rng.aabb();
			const RefAABB ref(aabb);
			Plane planes[9];
			Vector3 points[9];
			for (int k = 0; k < 9; k++) {
				planes[k] = rng.plane();
				points[k] = rng.v3();
			}
			const int plane_count = rng.range(10);
			const int point_count = rng.range(10);
			SIMD_EXPECT_EQUAL(mismatches, aabb.intersects_convex_shape(planes, plane_count, points, point_count), ref.ref_intersects_convex_shape(planes, plane_count, points, point_count));
			SIMD_EXPECT_EQUAL(mismatches, aabb.inside_convex_shape(planes, plane_count), ref.ref_inside_convex_shape(planes, plane_count));
			const Plane plane = rng.plane();
			SIMD_EXPECT_EQUAL(mismatches, aabb.intersects_plane(plane), ref.ref_intersects_plane(plane));
			const Vector3 from = rng.v3();
			const Vector3 dir = rng.v3();
			const real_t t0 = rng.edge_float();
			const real_t t1 = rng.edge_float();
			SIMD_EXPECT_EQUAL(mismatches, aabb.smits_intersect_ray(from, dir, t0, t1), ref.ref_smits_intersect_ray(from, dir, t0, t1));

			const Transform3D xform = rng.transform();
			SIMD_EXPECT_SAME_REAL(mismatches, xform.xform_inv(aabb), RefTransform3D(xform).ref_xform_inv_aabb(aabb));
			const Basis basis = rng.basis();
			SIMD_EXPECT_SAME_REAL(mismatches, basis.get_scale_abs(), RefBasis(basis).ref_get_scale_abs());
		}

		// Geometry aligned exactly with the box: points and planes on its faces or one ulp away,
		// zero and axis-aligned ray directions, and empty point lists.
		for (int iteration = 0; iteration < ITERATIONS / 4; iteration++) {
			const AABB aabb(Vector3(rng.range(9), rng.range(9), rng.range(9)) - Vector3(4, 4, 4), Vector3(rng.range(5), rng.range(5), rng.range(5)));
			const RefAABB ref(aabb);
			const Vector3 lo = aabb.position;
			const Vector3 hi = aabb.position + aabb.size;
			auto coord = [&](int p_axis) -> real_t {
				switch (rng.range(5)) {
					case 0:
						return lo[p_axis];
					case 1:
						return hi[p_axis];
					case 2:
						return std::nextafter(lo[p_axis], -(real_t)INFINITY);
					case 3:
						return std::nextafter(hi[p_axis], (real_t)INFINITY);
					default:
						return (lo[p_axis] + hi[p_axis]) * 0.5f;
				}
			};
			Plane planes[6];
			Vector3 points[8];
			for (int k = 0; k < 6; k++) {
				Vector3 normal;
				normal[rng.range(3)] = rng.chance(2) ? 1 : -1;
				planes[k] = Plane(normal, normal.dot(Vector3(coord(0), coord(1), coord(2))));
			}
			for (int k = 0; k < 8; k++) {
				points[k] = Vector3(coord(0), coord(1), coord(2));
			}
			const int plane_count = rng.range(7);
			const int point_count = rng.range(9);
			SIMD_EXPECT_EQUAL(mismatches, aabb.intersects_convex_shape(planes, plane_count, points, point_count), ref.ref_intersects_convex_shape(planes, plane_count, points, point_count));
			SIMD_EXPECT_EQUAL(mismatches, aabb.inside_convex_shape(planes, plane_count), ref.ref_inside_convex_shape(planes, plane_count));
			SIMD_EXPECT_EQUAL(mismatches, aabb.intersects_plane(planes[0]), ref.ref_intersects_plane(planes[0]));
			Vector3 dir;
			if (!rng.chance(4)) {
				dir[rng.range(3)] = rng.chance(2) ? 1 : -1;
			}
			const Vector3 from(coord(0), coord(1), coord(2));
			SIMD_EXPECT_EQUAL(mismatches, aabb.smits_intersect_ray(from, dir, 0, 10), ref.ref_smits_intersect_ray(from, dir, 0, 10));
			SIMD_EXPECT_EQUAL(mismatches, aabb.smits_intersect_ray(from - dir * 10, dir, 0, 10), ref.ref_smits_intersect_ray(from - dir * 10, dir, 0, 10));
		}
		SIMD_REPORT(mismatches);
	});
	ERR_PRINT_ON;
}

static _NO_INLINE_ bool ray_intersection_for_exception_test(const AABB &p_box, const Vector3 &p_from, const Vector3 &p_dir) {
	return p_box.smits_intersect_ray(p_from, p_dir, 0, 100);
}

TEST_CASE("[SIMD] Ray intersection does not raise exceptions in its unused lane") {
	fenv_t environment;
	REQUIRE(std::feholdexcept(&environment) == 0);
	// A volatile function pointer prevents the compiler from folding the call or moving it before
	// the floating-point environment is saved. All three meaningful divisor lanes are nonzero.
	bool (*volatile intersect)(const AABB &, const Vector3 &, const Vector3 &) = ray_intersection_for_exception_test;
	const bool hit = intersect(AABB(Vector3(), Vector3(4, 4, 4)), Vector3(-1, -1, -1), Vector3(1, 2, 3));
	const int exceptions = std::fetestexcept(FE_INVALID | FE_DIVBYZERO);
	const int restored = std::fesetenv(&environment);
	CHECK(restored == 0);
	CHECK(hit);
	CHECK(exceptions == 0);
}

TEST_CASE("[SIMD] Vector2, Vector3 and Vector4 match the scalar reference") {
	for_each_dispatch([](const char *p_mode) {
		Random rng(12);
		Mismatches mismatches;
		for (int iteration = 0; iteration < ITERATIONS; iteration++) {
			const Vector4 a = rng.v4();
			const Vector4 b = rng.v4();
			const Vector4 c = rng.v4();
			const Vector4 d = rng.v4();
			const RefVector4 ra(a);
			const real_t weight = rng.edge_float();
			const real_t t1 = rng.edge_float();
			const real_t t2 = rng.edge_float();
			const real_t t3 = rng.edge_float();
			SIMD_EXPECT_EQUAL(mismatches, a.is_equal_approx(b), ra.ref_is_equal_approx(b));
			SIMD_EXPECT_EQUAL(mismatches, a.is_equal_approx(a + Vector4(1e-6, 0, -1e-6, 0)), ra.ref_is_equal_approx(a + Vector4(1e-6, 0, -1e-6, 0)));
			SIMD_EXPECT_EQUAL(mismatches, a.is_zero_approx(), ra.ref_is_zero_approx());
			SIMD_EXPECT_EQUAL(mismatches, (a * 1e-6).is_zero_approx(), RefVector4(a * 1e-6).ref_is_zero_approx());
			SIMD_EXPECT_SAME_REAL(mismatches, a.sign(), ra.ref_sign());
			SIMD_EXPECT_SAME_REAL(mismatches, a.floor(), ra.ref_floor());
			SIMD_EXPECT_SAME_REAL(mismatches, a.ceil(), ra.ref_ceil());
			// round() routes through (long long); non-finite or out-of-int64-range inputs are UB in the
			// scalar reference and diverge across compilers, so assert only the defined domain. Plain
			// float comparisons keep the bound compiler-independent and also exclude NaN.
			bool round_defined = true;
			for (int e = 0; e < 4; e++) {
				round_defined = round_defined && a[e] > -9223372036854775808.0 && a[e] < 9223372036854775808.0;
			}
			if (round_defined) {
				SIMD_EXPECT_SAME_REAL(mismatches, a.round(), ra.ref_round());
			}
			SIMD_EXPECT_SAME_REAL(mismatches, a.clamp(b, c), ra.ref_clamp(b, c));
			SIMD_EXPECT_SAME_REAL(mismatches, a.clampf(t1, t2), ra.ref_clampf(t1, t2));
			SIMD_EXPECT_SAME_REAL(mismatches, a.cubic_interpolate(b, c, d, weight), ra.ref_cubic_interpolate(b, c, d, weight));
			SIMD_EXPECT_SAME_REAL(mismatches, a.cubic_interpolate_in_time(b, c, d, weight, t1, t2, t3), ra.ref_cubic_interpolate_in_time(b, c, d, weight, t1, t2, t3));

			const Vector3 a3 = rng.v3();
			const Vector3 b3 = rng.v3();
			const Vector3 c3 = rng.v3();
			const Vector3 d3 = rng.v3();
			SIMD_EXPECT_SAME_REAL(mismatches, a3.cubic_interpolate(b3, c3, d3, weight), RefVector3(a3).ref_cubic_interpolate(b3, c3, d3, weight));
			SIMD_EXPECT_SAME_REAL(mismatches, a3.cubic_interpolate_in_time(b3, c3, d3, weight, t1, t2, t3), RefVector3(a3).ref_cubic_interpolate_in_time(b3, c3, d3, weight, t1, t2, t3));

			const Vector2 a2 = rng.v2();
			const Vector2 b2 = rng.v2();
			const Vector2 c2 = rng.v2();
			const Vector2 d2 = rng.v2();
			SIMD_EXPECT_SAME_REAL(mismatches, a2.cubic_interpolate(b2, c2, d2, weight), RefVector2(a2).ref_cubic_interpolate(b2, c2, d2, weight));
		}
		SIMD_REPORT(mismatches);
	});
}

TEST_CASE("[SIMD] Quaternion, Plane and Projection match the scalar reference") {
	ERR_PRINT_OFF;
	for_each_dispatch([](const char *p_mode) {
		Random rng(13);
		Mismatches mismatches;
		for (int iteration = 0; iteration < ITERATIONS; iteration++) {
			const Quaternion q = rng.chance(8) ? Quaternion(rng.edge_float(), rng.edge_float(), rng.edge_float(), rng.edge_float()) : rng.quaternion();
			const Vector3 v = rng.v3();
			SIMD_EXPECT_SAME_REAL(mismatches, q.xform(v), RefQuaternion(q).ref_xform(v));

			const Plane p0 = rng.plane();
			const Plane p1 = rng.plane();
			const Plane p2 = rng.plane();
			Vector3 result;
			Vector3 ref_result;
			const bool hit = p0.intersect_3(p1, p2, &result);
			const bool ref_hit = RefPlane(p0).ref_intersect_3(p1, p2, &ref_result);
			SIMD_EXPECT_EQUAL(mismatches, hit, ref_hit);
			if (hit && ref_hit) {
				SIMD_EXPECT_SAME_REAL(mismatches, result, ref_result);
			}

			const Projection projection = rng.projection();
			const Vector4 v4 = rng.v4();
			SIMD_EXPECT_SAME_REAL(mismatches, projection.xform_inv(v4), RefProjection(projection).ref_xform_inv(v4));
		}
		SIMD_REPORT(mismatches);
	});
	ERR_PRINT_ON;
}

TEST_CASE("[SIMD] Rect2 and Transform2D match the scalar reference") {
	ERR_PRINT_OFF;
	for_each_dispatch([](const char *p_mode) {
		Random rng(14);
		Mismatches mismatches;
		for (int iteration = 0; iteration < ITERATIONS; iteration++) {
			const Rect2 a = rng.rect();
			const Rect2 b = rng.chance(8) ? a : rng.rect();
			const RefRect2 ra(a);
			SIMD_EXPECT_EQUAL(mismatches, a.intersects(b), ra.ref_intersects(b));
			SIMD_EXPECT_EQUAL(mismatches, a.intersects(b, true), ra.ref_intersects(b, true));
			const Transform2D xform = rng.transform2d();
			SIMD_EXPECT_SAME_REAL(mismatches, xform.xform(a), RefTransform2D(xform).ref_xform_rect(a));
		}
		SIMD_REPORT(mismatches);
	});
	ERR_PRINT_ON;
}

TEST_CASE("[SIMD] Color matches the scalar reference") {
	for_each_dispatch([](const char *p_mode) {
		Random rng(15);
		Mismatches mismatches;
		for (int iteration = 0; iteration < ITERATIONS; iteration++) {
			const Color a = rng.color();
			const Color b = rng.chance(8) ? a : rng.color();
			const Color lo = rng.color();
			const Color hi = rng.color();
			const float amount = rng.edge_float();
			const RefColor ra(a);
			// Packing scales by 255/65535 and casts through (uintN); components outside [0, 1] overflow
			// that cast, which is UB in the scalar reference and diverges across compilers, so pack only
			// well-defined colors. Plain comparisons also exclude NaN.
			const Color packed = rng.chance(2) ? a : Color(rng.range(300) / 255.0f - 0.1f, rng.range(65600) / 65535.0f, rng.unit(), rng.range(256) / 255.0f);
			const RefColor rp(packed);
			const bool pack_defined = packed.r >= 0.0f && packed.r <= 1.0f && packed.g >= 0.0f && packed.g <= 1.0f &&
					packed.b >= 0.0f && packed.b <= 1.0f && packed.a >= 0.0f && packed.a <= 1.0f;
			if (pack_defined) {
				SIMD_EXPECT_EQUAL(mismatches, packed.to_rgba32(), rp.ref_to_rgba32());
				SIMD_EXPECT_EQUAL(mismatches, packed.to_argb32(), rp.ref_to_argb32());
				SIMD_EXPECT_EQUAL(mismatches, packed.to_abgr32(), rp.ref_to_abgr32());
				SIMD_EXPECT_EQUAL(mismatches, packed.to_rgba64(), rp.ref_to_rgba64());
				SIMD_EXPECT_EQUAL(mismatches, packed.to_argb64(), rp.ref_to_argb64());
				SIMD_EXPECT_EQUAL(mismatches, packed.to_abgr64(), rp.ref_to_abgr64());
			}
			SIMD_EXPECT_EQUAL(mismatches, a == b, ra.ref_equals(b));
			SIMD_EXPECT_SAME_FLOAT(mismatches, a.clamp(), ra.ref_clamp());
			SIMD_EXPECT_SAME_FLOAT(mismatches, a.clamp(lo, hi), ra.ref_clamp(lo, hi));
			SIMD_EXPECT_SAME_FLOAT(mismatches, a.darkened(amount), ra.ref_darkened(amount));
			SIMD_EXPECT_SAME_FLOAT(mismatches, a.lightened(amount), ra.ref_lightened(amount));
		}
		SIMD_REPORT(mismatches);
	});
}

TEST_CASE("[SIMD] BVH bounds and frustum culling match the scalar reference") {
	for_each_dispatch([](const char *p_mode) {
		Random rng(16);
		Mismatches mismatches;
		for (int iteration = 0; iteration < ITERATIONS; iteration++) {
			BVH_ABB<AABB, Vector3> a;
			BVH_ABB<AABB, Vector3> b;
			a.set(rng.v3(), rng.v3());
			b.set(rng.v3(), rng.v3());
			const RefBVH_ABB<AABB, Vector3> ra(a);
			SIMD_EXPECT_EQUAL(mismatches, a.intersects(b), ra.ref_intersects(b));
			SIMD_EXPECT_EQUAL(mismatches, a.intersects_swizzled(b), ra.ref_intersects_swizzled(b));
			SIMD_EXPECT_EQUAL(mismatches, a.is_other_within(b), ra.ref_is_other_within(b));

			BVH_ABB<Rect2, Vector2> a2;
			BVH_ABB<Rect2, Vector2> b2;
			a2.set(rng.v2(), rng.v2());
			b2.set(rng.v2(), rng.v2());
			const RefBVH_ABB<Rect2, Vector2> ra2(a2);
			SIMD_EXPECT_EQUAL(mismatches, a2.intersects(b2), ra2.ref_intersects(b2));
			SIMD_EXPECT_EQUAL(mismatches, a2.intersects_swizzled(b2), ra2.ref_intersects_swizzled(b2));
			SIMD_EXPECT_EQUAL(mismatches, a2.is_other_within(b2), ra2.ref_is_other_within(b2));

			Vector<Plane> planes;
			const int plane_count = rng.range(10);
			for (int k = 0; k < plane_count; k++) {
				Plane plane = rng.plane();
				if (rng.chance(2) && plane.normal.length_squared() > 0) {
					plane.normal.normalize();
				}
				planes.push_back(plane);
			}
			const RendererSceneCull::Frustum frustum(planes);
			const RendererSceneCull::Frustum frustum_copy = frustum;
			const RendererSceneCull::InstanceBounds bounds(rng.aabb());
			const RefInstanceBounds ref_bounds(bounds);
			SIMD_EXPECT_EQUAL(mismatches, bounds.in_frustum(frustum), ref_bounds.ref_in_frustum(frustum));
			SIMD_EXPECT_EQUAL(mismatches, bounds.in_frustum(frustum_copy), ref_bounds.ref_in_frustum(frustum_copy));
		}
		SIMD_REPORT(mismatches);
	});
}

struct BVHCollector {
	Vector<int> hits;
	bool operator()(void *p_data) {
		hits.push_back(int(intptr_t(p_data)));
		return false;
	}
};

// The current DynamicBVH and a verbatim copy of the original receive identical operations, so the
// trees and the query results (in traversal order) must be identical.
TEST_CASE("[SIMD] DynamicBVH matches the scalar reference") {
	for_each_dispatch([](const char *p_mode) {
		Random rng(17);
		Mismatches mismatches;
		const int iterations = MAX(ITERATIONS / 50, 20);
		for (int iteration = 0; iteration < iterations; iteration++) {
			DynamicBVH bvh;
			RefDynamicBVH ref_bvh;
			Vector<DynamicBVH::ID> ids;
			Vector<RefDynamicBVH::ID> ref_ids;
			const bool extreme = rng.chance(3);
			const bool grid = !extreme && rng.chance(2);
			auto grid_point = [&]() {
				return Vector3(rng.range(11), rng.range(11), rng.range(11));
			};
			auto random_box = [&]() {
				if (extreme) {
					return AABB(rng.v3_finite(), rng.v3_finite().abs());
				}
				if (grid) {
					return AABB(grid_point(), Vector3(rng.range(4), rng.range(4), rng.range(4)));
				}
				return AABB(Vector3(rng.unit(), rng.unit(), rng.unit()) * 100.0f, Vector3(rng.unit(), rng.unit(), rng.unit()).abs() * 10.0f);
			};
			const int count = 1 + rng.range(200);
			for (int k = 0; k < count; k++) {
				const AABB box = random_box();
				ids.push_back(bvh.insert(box, (void *)intptr_t(k)));
				ref_ids.push_back(ref_bvh.insert(box, (void *)intptr_t(k)));
			}
			for (int k = 0; k < count / 4; k++) {
				const int index = rng.range(count);
				const AABB box = random_box();
				SIMD_EXPECT_EQUAL(mismatches, bvh.update(ids[index], box), ref_bvh.update(ref_ids[index], box));
			}
			if (rng.chance(2) && count > 1) {
				const int index = rng.range(count);
				bvh.remove(ids[index]);
				ref_bvh.remove(ref_ids[index]);
			}
			if (rng.chance(3)) {
				const int passes = 1 + rng.range(4);
				bvh.optimize_incremental(passes);
				ref_bvh.optimize_incremental(passes);
			}
			SIMD_EXPECT_EQUAL(mismatches, bvh.get_leaf_count(), ref_bvh.get_leaf_count());
			SIMD_EXPECT_EQUAL(mismatches, bvh.get_max_depth(), ref_bvh.get_max_depth());

			for (int q = 0; q < 4; q++) {
				const AABB query = random_box();
				BVHCollector collector;
				BVHCollector ref_collector;
				bvh.aabb_query(query, collector);
				ref_bvh.aabb_query(query, ref_collector);
				SIMD_EXPECT_EQUAL(mismatches, collector.hits, ref_collector.hits);

				Plane planes[6];
				Vector3 points[8];
				for (int k = 0; k < 6; k++) {
					if (grid) {
						Vector3 normal;
						normal[rng.range(3)] = rng.chance(2) ? 1 : -1;
						planes[k] = Plane(normal, normal.dot(grid_point()));
					} else {
						planes[k] = extreme ? Plane(rng.v3_finite().normalized(), rng.finite_float()) : Plane(Vector3(rng.unit(), rng.unit(), rng.unit()).normalized(), rng.unit() * 50.0f);
					}
				}
				for (int k = 0; k < 8; k++) {
					if (grid) {
						points[k] = grid_point();
					} else {
						points[k] = extreme ? rng.v3_finite() : Vector3(rng.unit(), rng.unit(), rng.unit()) * 100.0f;
					}
				}
				BVHCollector convex_collector;
				BVHCollector ref_convex_collector;
				bvh.convex_query(planes, 6, points, 8, convex_collector);
				ref_bvh.convex_query(planes, 6, points, 8, ref_convex_collector);
				SIMD_EXPECT_EQUAL(mismatches, convex_collector.hits, ref_convex_collector.hits);

				const Vector3 from = random_box().position;
				const Vector3 to = random_box().position;
				BVHCollector ray_collector;
				BVHCollector ref_ray_collector;
				bvh.ray_query(from, to, ray_collector);
				ref_bvh.ray_query(from, to, ref_ray_collector);
				SIMD_EXPECT_EQUAL(mismatches, ray_collector.hits, ref_ray_collector.hits);
			}
		}
		SIMD_REPORT(mismatches);
	});
}

template <typename T>
bool reference_vector_equals(const Vector<T> &p_a, const Vector<T> &p_b) {
	if (p_a.size() != p_b.size()) {
		return false;
	}
	for (int64_t i = 0; i < p_a.size(); i++) {
		if (p_a[i] != p_b[i]) {
			return false;
		}
	}
	return true;
}

TEST_CASE("[SIMD] Vector equality matches element-wise comparison") {
	for_each_dispatch([](const char *p_mode) {
		Random rng(18);
		Mismatches mismatches;
		for (int iteration = 0; iteration < ITERATIONS / 4; iteration++) {
			const int size = rng.range(70);
			Vector<float> fa;
			Vector<float> fb;
			Vector<int32_t> ia;
			Vector<int32_t> ib;
			Vector<uint8_t> ba;
			Vector<uint8_t> bb;
			for (int k = 0; k < size; k++) {
				const float f = rng.edge_float();
				fa.push_back(f);
				fb.push_back(rng.chance(60) ? rng.edge_float() : (rng.chance(30) ? -f : f));
				const int32_t i = int32_t(rng.next());
				ia.push_back(i);
				ib.push_back(rng.chance(60) ? int32_t(rng.next()) : i);
				const uint8_t b = uint8_t(rng.next());
				ba.push_back(b);
				bb.push_back(rng.chance(60) ? uint8_t(rng.next()) : b);
			}
			if (rng.chance(10)) {
				fb.push_back(0.0f);
			}
			SIMD_EXPECT_EQUAL(mismatches, fa == fb, reference_vector_equals(fa, fb));
			SIMD_EXPECT_EQUAL(mismatches, fa != fb, !reference_vector_equals(fa, fb));
			SIMD_EXPECT_EQUAL(mismatches, fa == fa, reference_vector_equals(fa, fa));
			SIMD_EXPECT_EQUAL(mismatches, ia == ib, reference_vector_equals(ia, ib));
			SIMD_EXPECT_EQUAL(mismatches, ia != ib, !reference_vector_equals(ia, ib));
			SIMD_EXPECT_EQUAL(mismatches, ba == bb, reference_vector_equals(ba, bb));
		}
		SIMD_REPORT(mismatches);
	});
}

inline String random_string(Random &p_rng, int p_length) {
	String s;
	for (int i = 0; i < p_length; i++) {
		const uint32_t kind = p_rng.range(10);
		char32_t c;
		if (kind < 6) {
			c = 0x20 + p_rng.range(0x5F);
		} else if (kind == 6) {
			c = 0x80 + p_rng.range(0x780);
		} else if (kind == 7) {
			c = 0x800 + p_rng.range(0xF000);
			if (c >= 0xD800 && c <= 0xDFFF) {
				c = 'A';
			}
		} else if (kind == 8) {
			c = 0x10000 + p_rng.range(0x100000);
		} else {
			c = 1 + p_rng.range(0x1F);
		}
		s += c;
	}
	return s;
}

TEST_CASE("[SIMD] String hashing, case mapping, comparison and UTF-8 match the scalar reference") {
	ERR_PRINT_OFF;
	for_each_dispatch([](const char *p_mode) {
		Random rng(19);
		Mismatches mismatches;
		for (int iteration = 0; iteration < ITERATIONS / 2; iteration++) {
			// Length is at least 1: an empty s yields an empty CharString from utf8(), whose null-pointer
			// _unref the GCC 15 -O3 inliner miscompiles in this scope (the destructor's guard is dropped).
			const int length = rng.chance(3) ? 1 + rng.range(200) : 1 + rng.range(24);
			const String s = random_string(rng, length);
			const RefString rs(s);
			SIMD_EXPECT_EQUAL(mismatches, s.hash(), rs.ref_hash());
			SIMD_EXPECT_EQUAL(mismatches, s.to_upper(), rs.ref_to_upper());
			SIMD_EXPECT_EQUAL(mismatches, s.to_lower(), rs.ref_to_lower());

			Vector<uint8_t> map;
			Vector<uint8_t> ref_map;
			const CharString utf8 = s.utf8(&map);
			const CharString ref_utf8 = rs.ref_utf8(&ref_map);
			SIMD_EXPECT_EQUAL(mismatches, utf8, ref_utf8);
			SIMD_EXPECT_EQUAL(mismatches, map, ref_map);

			// UTF-8 decoding: valid text, corrupted bytes, carriage returns, embedded NUL and BOM.
			Vector<uint8_t> bytes;
			if (rng.chance(4)) {
				bytes.push_back(0xEF);
				bytes.push_back(0xBB);
				bytes.push_back(0xBF);
			}
			for (int k = 0; k < utf8.length(); k++) {
				uint8_t b = uint8_t(utf8[k]);
				if (rng.chance(40)) {
					b = uint8_t(rng.next());
				} else if (rng.chance(60)) {
					b = '\r';
				}
				bytes.push_back(b);
			}
			bytes.push_back(0);
			const int len = rng.chance(3) ? -1 : int(rng.range(bytes.size()));
			const bool skip_cr = rng.chance(2);
			const String prefix = rng.chance(2) ? String() : random_string(rng, rng.range(8));
			String decoded = prefix;
			RefString ref_decoded(prefix);
			const Error err = decoded.append_utf8((const char *)bytes.ptr(), len, skip_cr);
			const Error ref_err = ref_decoded.ref_append_utf8((const char *)bytes.ptr(), len, skip_cr);
			SIMD_EXPECT_EQUAL(mismatches, err, ref_err);
			SIMD_EXPECT_EQUAL(mismatches, decoded, static_cast<const String &>(ref_decoded));

			// Latin-1 comparison, including bytes >= 0x80 whose sign extension depends on the platform.
			char latin[80];
			const int latin_length = rng.range(70);
			for (int k = 0; k < latin_length; k++) {
				latin[k] = char(1 + rng.range(255));
			}
			latin[latin_length] = 0;
			const String latin_string(latin);
			const String other = rng.chance(2) ? latin_string : random_string(rng, latin_length);
			SIMD_EXPECT_EQUAL(mismatches, latin_string == latin, RefString(latin_string).ref_equals(latin));
			SIMD_EXPECT_EQUAL(mismatches, other == latin, RefString(other).ref_equals(latin));
			SIMD_EXPECT_EQUAL(mismatches, s == latin, rs.ref_equals(latin));
		}
		SIMD_REPORT(mismatches);
	});
	ERR_PRINT_ON;
}

// --- Image ---------------------------------------------------------------------------------------

inline Ref<Image> random_image(Random &p_rng, int p_width, int p_height, Image::Format p_format, bool p_sparse_alpha = false) {
	const int64_t size = Image::get_image_data_size(p_width, p_height, p_format, false);
	Vector<uint8_t> data;
	data.resize(size);
	uint8_t *w = data.ptrw();
	if (p_format >= Image::FORMAT_RF && p_format <= Image::FORMAT_RGBAF) {
		for (int64_t i = 0; i < size / 4; i++) {
			const float f = p_rng.chance(8) ? p_rng.edge_float() : p_rng.unit();
			memcpy(w + i * 4, &f, 4);
		}
	} else if (p_format >= Image::FORMAT_RH && p_format <= Image::FORMAT_RGBAH) {
		for (int64_t i = 0; i < size / 2; i++) {
			const uint16_t h = p_rng.chance(8) ? uint16_t(p_rng.next()) : Math::make_half_float(p_rng.unit());
			memcpy(w + i * 2, &h, 2);
		}
	} else {
		for (int64_t i = 0; i < size; i++) {
			const uint32_t r = p_rng.range(8);
			w[i] = r == 0 ? 0 : (r == 1 ? 255 : uint8_t(p_rng.next()));
		}
	}
	if (p_sparse_alpha) {
		// Mostly zero alpha so that "invisible" and "unused" outcomes both occur.
		const int ps = Image::get_format_pixel_size(p_format);
		const int64_t pixels = int64_t(p_width) * p_height;
		const bool keep_some = p_rng.chance(2);
		for (int64_t i = 0; i < pixels; i++) {
			const bool keep = keep_some && p_rng.chance(40);
			uint8_t *px = w + i * ps;
			switch (p_format) {
				case Image::FORMAT_LA8:
					px[1] = keep ? uint8_t(1 + p_rng.range(255)) : 0;
					break;
				case Image::FORMAT_RGBA8:
					px[3] = keep ? uint8_t(1 + p_rng.range(255)) : 0;
					break;
				case Image::FORMAT_RGBA4444:
					px[0] = keep ? uint8_t(px[0] | 1) : uint8_t(px[0] & 0xF0);
					break;
				case Image::FORMAT_RGBAH: {
					const uint16_t a = keep ? 0x3C00 : (p_rng.chance(2) ? 0x8000 : 0);
					memcpy(px + 6, &a, 2);
				} break;
				case Image::FORMAT_RGBAF: {
					const float a = keep ? 0.5f : (p_rng.chance(2) ? -0.0f : 0.0f);
					memcpy(px + 12, &a, 4);
				} break;
				default:
					break;
			}
		}
	}
	return Image::create_from_data(p_width, p_height, false, p_format, data);
}

inline void reference_bilinear(Image::Format p_format, const uint8_t *p_src, uint8_t *p_dst, uint32_t p_sw, uint32_t p_sh, uint32_t p_dw, uint32_t p_dh) {
	switch (p_format) {
		case Image::FORMAT_L8:
		case Image::FORMAT_R8:
			ref_scale_bilinear<1, uint8_t>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_LA8:
		case Image::FORMAT_RG8:
			ref_scale_bilinear<2, uint8_t>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_RGB8:
			ref_scale_bilinear<3, uint8_t>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_RGBA8:
			ref_scale_bilinear<4, uint8_t>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_RF:
			ref_scale_bilinear<1, float>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_RGF:
			ref_scale_bilinear<2, float>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_RGBF:
			ref_scale_bilinear<3, float>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_RGBAF:
			ref_scale_bilinear<4, float>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_RH:
			ref_scale_bilinear<1, uint16_t>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_RGH:
			ref_scale_bilinear<2, uint16_t>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_RGBH:
			ref_scale_bilinear<3, uint16_t>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		case Image::FORMAT_RGBAH:
			ref_scale_bilinear<4, uint16_t>(p_src, p_dst, p_sw, p_sh, p_dw, p_dh);
			break;
		default:
			break;
	}
}

// Ports of the original Image member functions, written against the public Image API.
inline Vector<uint8_t> reference_premultiply_alpha(const Ref<Image> &p_image) {
	Vector<uint8_t> data = p_image->get_data();
	uint8_t *data_ptr = data.ptrw();
	const int width = p_image->get_width();
	const int height = p_image->get_height();
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			uint8_t *ptr = &data_ptr[(i * width + j) * 4];

			ptr[0] = (uint16_t(ptr[0]) * uint16_t(ptr[3]) + 255U) >> 8;
			ptr[1] = (uint16_t(ptr[1]) * uint16_t(ptr[3]) + 255U) >> 8;
			ptr[2] = (uint16_t(ptr[2]) * uint16_t(ptr[3]) + 255U) >> 8;
		}
	}
	return data;
}

inline Vector<uint8_t> reference_flip(const Ref<Image> &p_image, bool p_flip_x) {
	Vector<uint8_t> data = p_image->get_data();
	uint8_t *w = data.ptrw();
	const int width = p_image->get_width();
	const int height = p_image->get_height();
	const uint32_t pixel_size = Image::get_format_pixel_size(p_image->get_format());
	uint8_t up[16];
	uint8_t down[16];
	auto get_pixelb = [&](int p_x, int p_y, uint8_t *r_pixel) {
		memcpy(r_pixel, &w[(p_y * width + p_x) * pixel_size], pixel_size);
	};
	auto put_pixelb = [&](int p_x, int p_y, const uint8_t *p_pixel) {
		memcpy(&w[(p_y * width + p_x) * pixel_size], p_pixel, pixel_size);
	};
	if (p_flip_x) {
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width / 2; x++) {
				get_pixelb(x, y, up);
				get_pixelb(width - x - 1, y, down);
				put_pixelb(width - x - 1, y, up);
				put_pixelb(x, y, down);
			}
		}
	} else {
		for (int y = 0; y < height / 2; y++) {
			for (int x = 0; x < width; x++) {
				get_pixelb(x, y, up);
				get_pixelb(x, height - y - 1, down);
				put_pixelb(x, height - y - 1, up);
				put_pixelb(x, y, down);
			}
		}
	}
	return data;
}

inline Image::AlphaMode reference_detect_alpha(const Ref<Image> &p_image) {
	const Vector<uint8_t> data = p_image->get_data();
	const int64_t len = int64_t(p_image->get_width()) * p_image->get_height() * Image::get_format_pixel_size(p_image->get_format());
	if (len == 0) {
		return Image::ALPHA_NONE;
	}
	const unsigned char *data_ptr = data.ptr();
	bool bit = false;
	bool detected = false;
	auto detect = [&](uint8_t p_value) {
		if (p_value < 2) {
			bit = true;
		} else if (p_value < 254) {
			detected = true;
		}
		return detected;
	};
	switch (p_image->get_format()) {
		case Image::FORMAT_LA8: {
			for (int i = 0; i < (len >> 1); i++) {
				if (detect(data_ptr[(i << 1) + 1])) {
					break;
				}
			}
		} break;
		case Image::FORMAT_RGBA8: {
			for (int i = 0; i < (len >> 2); i++) {
				if (detect(data_ptr[(i << 2) + 3])) {
					break;
				}
			}
		} break;
		default: {
		}
	}
	if (detected) {
		return Image::ALPHA_BLEND;
	} else if (bit) {
		return Image::ALPHA_BIT;
	}
	return Image::ALPHA_NONE;
}

inline bool reference_is_invisible(const Ref<Image> &p_image) {
	const Vector<uint8_t> data = p_image->get_data();
	const int64_t len = int64_t(p_image->get_width()) * p_image->get_height() * Image::get_format_pixel_size(p_image->get_format());
	if (len == 0) {
		return true;
	}
	switch (p_image->get_format()) {
		case Image::FORMAT_LA8: {
			const int pixel_count = len / 2;
			const uint16_t *pixeldata = reinterpret_cast<const uint16_t *>(data.ptr());
			for (int i = 0; i < pixel_count; i++) {
				if ((pixeldata[i] & 0xFF00) != 0) {
					return false;
				}
			}
		} break;
		case Image::FORMAT_RGBA8: {
			const int pixel_count = len / 4;
			const uint32_t *pixeldata = reinterpret_cast<const uint32_t *>(data.ptr());
			for (int i = 0; i < pixel_count; i++) {
				if ((pixeldata[i] & 0xFF000000) != 0) {
					return false;
				}
			}
		} break;
		case Image::FORMAT_RGBA4444: {
			const int pixel_count = len / 2;
			const uint16_t *pixeldata = reinterpret_cast<const uint16_t *>(data.ptr());
			for (int i = 0; i < pixel_count; i++) {
				if ((pixeldata[i] & 0x000F) != 0) {
					return false;
				}
			}
		} break;
		case Image::FORMAT_RGBAH: {
			const int pixel_count = len / 8;
			const uint16_t *pixeldata = reinterpret_cast<const uint16_t *>(data.ptr());
			for (int i = 0; i < pixel_count; i++) {
				if ((pixeldata[i * 4 + 3] & 0x7FFF) != 0) {
					return false;
				}
			}
		} break;
		case Image::FORMAT_RGBAF: {
			const int pixel_count = len / 16;
			const uint32_t *pixeldata = reinterpret_cast<const uint32_t *>(data.ptr());
			for (int i = 0; i < pixel_count; i++) {
				if ((pixeldata[i * 4 + 3] & 0x7FFFFFFF) != 0) {
					return false;
				}
			}
		} break;
		default: {
			return false;
		}
	}
	return true;
}

inline Rect2i reference_get_used_rect(const Ref<Image> &p_image) {
	const Image::Format format = p_image->get_format();
	const int width = p_image->get_width();
	const int height = p_image->get_height();
	if (format != Image::FORMAT_LA8 && format != Image::FORMAT_RGBA8 && format != Image::FORMAT_RGBAF && format != Image::FORMAT_RGBAH && format != Image::FORMAT_RGBA4444 && format != Image::FORMAT_RGB565) {
		return Rect2i(0, 0, width, height);
	}
	if (p_image->get_data().is_empty()) {
		return Rect2i();
	}
	int minx = 0xFFFFFF, miny = 0xFFFFFFF;
	int maxx = -1, maxy = -1;
	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			if (!(p_image->get_pixel(i, j).a > 0)) {
				continue;
			}
			if (i > maxx) {
				maxx = i;
			}
			if (j > maxy) {
				maxy = j;
			}
			if (i < minx) {
				minx = i;
			}
			if (j < miny) {
				miny = j;
			}
		}
	}
	if (maxx == -1) {
		return Rect2i();
	}
	return Rect2i(minx, miny, maxx - minx + 1, maxy - miny + 1);
}

TEST_CASE("[SIMD] UTF-8 decoding matches the scalar reference across byte and block boundaries") {
	// Test lead and second-byte combinations with edge values after them, placed near the end of a
	// 16-byte block inside text that takes the multi-byte path.
	ERR_PRINT_OFF;
	static const uint8_t tails[] = { 0x00, 0x0D, 0x41, 0x80, 0xBF, 0xC3 };
	const char *p_mode = "default";
#ifdef GODOT_STRING_SIMD
	// Exercise every lead/second-byte pair. A lead at 13 ends a 4-byte sequence 1 byte into the next
	// block, while a lead at 15 crosses by 1 to 3 bytes.
	static const int offsets[] = { 13, 15 };
	constexpr int second_step = 1;
#else
	// Without the SIMD decoder this only exercises the scalar loop, so use a representative sample.
	static const int offsets[] = { 15 };
	constexpr int second_step = 15;
#endif
	Mismatches mismatches;
	for (int offset : offsets) {
		// Bound offset for the optimizer: writes span bytes[offset - 1 .. offset + 3] into bytes[64].
		// Without this, GCC's -Wstringop-overflow cannot prove the range-for stays in [1, 60].
		if (offset < 1 || offset > 60) {
			continue;
		}
		for (int c0 = 0; c0 < 256; c0++) {
			for (int c1 = 0; c1 < 256; c1 += second_step) {
				const int iteration = (offset << 16) | (c0 << 8) | c1;
				const uint8_t c2 = tails[(c0 + c1) % std::size(tails)];
				const uint8_t c3 = tails[(c0 * 7 + c1 * 3) % std::size(tails)];
				uint8_t bytes[64];
				for (int k = 0; k < offset; k++) {
					bytes[k] = k % 2 ? 0xA9 : 0xC3;
				}
				if (offset % 2) {
					bytes[offset - 1] = 'x';
				}
				bytes[offset] = uint8_t(c0);
				bytes[offset + 1] = uint8_t(c1);
				bytes[offset + 2] = c2;
				bytes[offset + 3] = c3;
				for (int k = offset + 4; k < 64; k++) {
					bytes[k] = k % 2 ? 0xA9 : 0xC3;
				}
				const bool skip_cr = (c0 + c1) % 2;
				String decoded;
				RefString ref_decoded;
				const Error err = decoded.append_utf8((const char *)bytes, 64, skip_cr);
				const Error ref_err = ref_decoded.ref_append_utf8((const char *)bytes, 64, skip_cr);
				SIMD_EXPECT_EQUAL(mismatches, err, ref_err);
				SIMD_EXPECT_EQUAL(mismatches, decoded, static_cast<const String &>(ref_decoded));
			}
		}
	}
	SIMD_REPORT(mismatches);
	ERR_PRINT_ON;
}

TEST_CASE("[SIMD] UTF-8 decoding handles every block-end position") {
	ERR_PRINT_OFF;
	// A small boundary matrix complements the exhaustive byte-pair test without doubling its cost.
	static const uint8_t sequences[][4] = {
		{ 0xC2, 0xA2, 'x', 'x' },
		{ 0xE2, 0x82, 0xAC, 'x' },
		{ 0xF0, 0x9F, 0x98, 0x80 },
		{ 0xF4, 0x8F, 0xBF, 0xBF },
		{ 0xE0, 0x80, 0x80, 'x' },
		{ 0xED, 0xA0, 0x80, 'x' },
		{ 0xF4, 0x90, 0x80, 0x80 },
		{ 0xF0, 0x9F, 'x', 0x80 },
	};
	for (int offset : { 12, 13, 14, 15 }) {
		for (const auto &sequence : sequences) {
			for (bool skip_cr : { false, true }) {
				uint8_t bytes[48];
				memset(bytes, 'x', sizeof(bytes));
				memcpy(bytes + offset, sequence, sizeof(sequence));
				bytes[offset + 4] = '\r';
				String decoded("prefix");
				RefString reference(String("prefix"));
				const Error error = decoded.append_utf8(reinterpret_cast<const char *>(bytes), sizeof(bytes), skip_cr);
				const Error expected_error = reference.ref_append_utf8(reinterpret_cast<const char *>(bytes), sizeof(bytes), skip_cr);
				CHECK(error == expected_error);
				CHECK(decoded == static_cast<const String &>(reference));
			}
		}
	}
	ERR_PRINT_ON;
}

#ifdef GODOT_SIMD_F32X4
TEST_CASE("[SIMD] Half-float conversion matches Math for every input class") {
	// Every half value.
	int mismatches = 0;
	for (uint32_t h = 0; h < 0x10000; h += 4) {
		const uint16_t halves[4] = { uint16_t(h), uint16_t(h + 1), uint16_t(h + 2), uint16_t(h + 3) };
		float out[4];
		simd::store4(out, simd::half_to_float4(halves));
		for (int k = 0; k < 4; k++) {
			// Any two NaNs count as equal: on x86_32 the scalar result passes through x87, which quiets signaling NaNs.
			if (!same_words<float>(out[k], Math::half_to_float(halves[k]))) {
				mismatches++;
			}
		}
	}
	CHECK_MESSAGE(mismatches == 0, "half_to_float4 differs from Math::half_to_float for ", mismatches, " values.");

	// Every sign, exponent and top 10 mantissa bits, which is all the conversion looks at, with the
	// discarded low 13 bits at their edges. Covers zeros, subnormals, the 2^-14 cutoff, the 65504 /
	// 65520 overflow edge, infinities and NaN payloads.
	static const uint32_t low_bits[] = { 0x0000, 0x0001, 0x0FFF, 0x1000, 0x1001, 0x1FFF };
	mismatches = 0;
	for (uint32_t sign = 0; sign < 2; sign++) {
		for (uint32_t exponent = 0; exponent < 256; exponent++) {
			for (uint32_t mantissa = 0; mantissa < 1024; mantissa++) {
				uint32_t bits[6];
				for (int k = 0; k < 6; k++) {
					bits[k] = (sign << 31) | (exponent << 23) | (mantissa << 13) | low_bits[k];
				}
				for (int k = 0; k < 6; k += 2) {
					float in[4];
					memcpy(&in[0], &bits[k], 4);
					memcpy(&in[1], &bits[k + 1], 4);
					in[2] = in[0];
					in[3] = in[1];
					uint16_t out[4];
					simd::float_to_half4(simd::load4(in), out);
					for (int lane = 0; lane < 2; lane++) {
						if (out[lane] != Math::make_half_float(in[lane])) {
							mismatches++;
						}
					}
				}
			}
		}
	}
	CHECK_MESSAGE(mismatches == 0, "float_to_half4 differs from Math::make_half_float for ", mismatches, " values.");
}
#endif

TEST_CASE("[SIMD] Image processing matches the scalar reference") {
	static const Image::Format byte_formats[] = { Image::FORMAT_L8, Image::FORMAT_LA8, Image::FORMAT_R8, Image::FORMAT_RG8, Image::FORMAT_RGB8, Image::FORMAT_RGBA8 };
	static const Image::Format resample_formats[] = {
		Image::FORMAT_L8, Image::FORMAT_LA8, Image::FORMAT_R8, Image::FORMAT_RG8, Image::FORMAT_RGB8, Image::FORMAT_RGBA8,
		Image::FORMAT_RF, Image::FORMAT_RGF, Image::FORMAT_RGBF, Image::FORMAT_RGBAF,
		Image::FORMAT_RH, Image::FORMAT_RGH, Image::FORMAT_RGBH, Image::FORMAT_RGBAH
	};
	static const Image::Format alpha_formats[] = { Image::FORMAT_LA8, Image::FORMAT_RGBA8, Image::FORMAT_RGBA4444, Image::FORMAT_RGBAH, Image::FORMAT_RGBAF, Image::FORMAT_RGB565 };

	ERR_PRINT_OFF;
	for_each_dispatch([](const char *p_mode) {
		Random rng(20);
		Mismatches mismatches;
		const int iterations = MAX(ITERATIONS / 100, 12);
		for (int iteration = 0; iteration < iterations; iteration++) {
			const int width = 1 + rng.range(70);
			const int height = 1 + rng.range(40);

			for (Image::Format from : byte_formats) {
				const Ref<Image> source = random_image(rng, width, height, from);
				for (Image::Format to : byte_formats) {
					if (from == to) {
						continue;
					}
					Vector<uint8_t> expected;
					expected.resize(Image::get_image_data_size(width, height, to, false));
					ref_convert_dispatch(from, to, width, height, source->get_data().ptr(), expected.ptrw());
					Ref<Image> converted = source->duplicate();
					converted->convert(to);
					SIMD_EXPECT_EQUAL(mismatches, converted->get_data(), expected);
				}
			}

			for (Image::Format format : resample_formats) {
				const Ref<Image> source = random_image(rng, width, height, format);

				int dst_width = 1 + rng.range(90);
				const int dst_height = 1 + rng.range(50);
				if (dst_width == width && dst_height == height) {
					dst_width++;
				}
				Vector<uint8_t> expected;
				expected.resize(Image::get_image_data_size(dst_width, dst_height, format, false));
				reference_bilinear(format, source->get_data().ptr(), expected.ptrw(), width, height, dst_width, dst_height);
				Ref<Image> resized = source->duplicate();
				resized->resize(dst_width, dst_height, Image::INTERPOLATE_BILINEAR);
				// Bit-for-bit, but any two NaNs are equal: a NaN result's payload is not meaningful
				// and the engine's scalar half/float path can pick a different one than this copy.
				SIMD_EXPECT_EQUAL(mismatches, same_image_data(format, resized->get_data(), expected), true);

				Ref<Image> mipmapped = source->duplicate();
				mipmapped->generate_mipmaps(false);
				Vector<uint8_t> expected_mips;
				expected_mips.resize(Image::get_image_data_size(width, height, format, true));
				memcpy(expected_mips.ptrw(), source->get_data().ptr(), source->get_data().size());
				int64_t prev_ofs = 0;
				int prev_w = width;
				int prev_h = height;
				for (int level = 1; level <= mipmapped->get_mipmap_count(); level++) {
					int w = 0;
					int h = 0;
					const int64_t ofs = Image::get_image_mipmap_offset_and_dimensions(width, height, format, level, w, h);
					ref_generate_mipmap_from_format(format, expected_mips.ptr() + prev_ofs, expected_mips.ptrw() + ofs, prev_w, prev_h, false);
					prev_ofs = ofs;
					prev_w = w;
					prev_h = h;
				}
				SIMD_EXPECT_EQUAL(mismatches, same_image_data(format, mipmapped->get_data(), expected_mips), true);

				Ref<Image> flipped_x = source->duplicate();
				flipped_x->flip_x();
				SIMD_EXPECT_EQUAL(mismatches, flipped_x->get_data(), reference_flip(source, true));
				Ref<Image> flipped_y = source->duplicate();
				flipped_y->flip_y();
				SIMD_EXPECT_EQUAL(mismatches, flipped_y->get_data(), reference_flip(source, false));
			}

			const Ref<Image> rgba = random_image(rng, width, height, Image::FORMAT_RGBA8);
			Ref<Image> premultiplied = rgba->duplicate();
			premultiplied->premultiply_alpha();
			SIMD_EXPECT_EQUAL(mismatches, premultiplied->get_data(), reference_premultiply_alpha(rgba));

			for (Image::Format format : alpha_formats) {
				const Ref<Image> image = random_image(rng, width, height, format, rng.chance(2));
				SIMD_EXPECT_EQUAL(mismatches, image->detect_alpha(), reference_detect_alpha(image));
				SIMD_EXPECT_EQUAL(mismatches, image->is_invisible(), reference_is_invisible(image));
				SIMD_EXPECT_EQUAL(mismatches, image->get_used_rect(), reference_get_used_rect(image));
			}
		}
		SIMD_REPORT(mismatches);
	});
	ERR_PRINT_ON;
}

// --- Audio ---------------------------------------------------------------------------------------

// Ports of the original AudioServer and EQ loops.
inline void reference_convert_stereo_to_int32(const AudioFrame *p_buf, int32_t *p_dest, int p_frames) {
	for (int j = 0; j < p_frames; j++) {
		float l = CLAMP(p_buf[j].left, -1.0, 1.0);
		int32_t vl = l * ((1 << 20) - 1);
		int32_t vl2 = (vl < 0 ? -1 : 1) * (Math::abs(vl) << 11);
		p_dest[j * 2] = vl2;

		float r = CLAMP(p_buf[j].right, -1.0, 1.0);
		int32_t vr = r * ((1 << 20) - 1);
		int32_t vr2 = (vr < 0 ? -1 : 1) * (Math::abs(vr) << 11);
		p_dest[j * 2 + 1] = vr2;
	}
}

inline void reference_apply_volume_and_peak(AudioFrame *p_buf, uint32_t p_frames, float p_volume, AudioFrame &r_peak) {
	AudioFrame peak = AudioFrame(0, 0);
	for (uint32_t j = 0; j < p_frames; j++) {
		p_buf[j] *= p_volume;

		float l = Math::abs(p_buf[j].left);
		if (l > peak.left) {
			peak.left = l;
		}
		float r = Math::abs(p_buf[j].right);
		if (r > peak.right) {
			peak.right = r;
		}
	}
	r_peak = peak;
}

inline void reference_mix_ramped(AudioFrame *p_out_buf, const AudioFrame *p_source_buf, AudioFrame p_vol_start, AudioFrame p_vol_final, uint32_t p_frames) {
	for (unsigned int frame_idx = 0; frame_idx < p_frames; frame_idx++) {
		float lerp_param = (float)frame_idx / p_frames;
		p_out_buf[frame_idx] += (p_vol_final * lerp_param + (1 - lerp_param) * p_vol_start) * p_source_buf[frame_idx];
	}
}

inline void reference_eq(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count, EQ::BandProcess *proc_l, EQ::BandProcess *proc_r, const float *bgain, int band_count) {
	for (int i = 0; i < p_frame_count; i++) {
		AudioFrame src = p_src_frames[i];
		AudioFrame dst = AudioFrame(0, 0);

		for (int j = 0; j < band_count; j++) {
			float l = src.left;
			float r = src.right;

			proc_l[j].process_one(l);
			proc_r[j].process_one(r);

			dst.left += l * bgain[j];
			dst.right += r * bgain[j];
		}

		p_dst_frames[i] = dst;
	}
}

inline AudioFrame random_frame(Random &p_rng) {
	if (p_rng.chance(16)) {
		return AudioFrame(p_rng.edge_float(), p_rng.edge_float());
	}
	return AudioFrame(p_rng.unit() * (p_rng.chance(4) ? 3.0f : 1.0f), p_rng.unit());
}

inline bool same_frames(const AudioFrame *p_a, const AudioFrame *p_b, int p_count) {
	return same_float_buffers(&p_a[0].left, &p_b[0].left, int64_t(p_count) * 2);
}

TEST_CASE("[SIMD] Audio mixing kernels match the scalar reference") {
	for_each_dispatch([](const char *p_mode) {
		Random rng(21);
		Mismatches mismatches;
		const int iterations = MAX(ITERATIONS / 10, 50);
		for (int iteration = 0; iteration < iterations; iteration++) {
			const int frames = 1 + rng.range(1100);
			LocalVector<AudioFrame> a;
			LocalVector<AudioFrame> b;
			LocalVector<AudioFrame> out;
			a.resize(frames);
			b.resize(frames);
			out.resize(frames);
			for (int k = 0; k < frames; k++) {
				a[k] = random_frame(rng);
				b[k] = random_frame(rng);
				out[k] = random_frame(rng);
			}

			LocalVector<int32_t> pcm;
			LocalVector<int32_t> ref_pcm;
			pcm.resize(frames * 2);
			ref_pcm.resize(frames * 2);
			AudioMixKernels::convert_stereo_to_int32(a.ptr(), pcm.ptr(), frames);
			reference_convert_stereo_to_int32(a.ptr(), ref_pcm.ptr(), frames);
			SIMD_EXPECT_EQUAL(mismatches, memcmp(pcm.ptr(), ref_pcm.ptr(), frames * 8), 0);

			LocalVector<AudioFrame> vol_a = a;
			LocalVector<AudioFrame> ref_vol_a = a;
			const float volume = rng.chance(8) ? 0.0f : rng.unit() * 2.0f;
			AudioFrame peak;
			AudioFrame ref_peak;
			AudioMixKernels::apply_volume_and_peak(vol_a.ptr(), frames, volume, peak);
			reference_apply_volume_and_peak(ref_vol_a.ptr(), frames, volume, ref_peak);
			SIMD_EXPECT_EQUAL(mismatches, same_frames(vol_a.ptr(), ref_vol_a.ptr(), frames), true);
			SIMD_EXPECT_SAME_FLOAT(mismatches, peak, ref_peak);

			LocalVector<AudioFrame> sum = out;
			LocalVector<AudioFrame> ref_sum = out;
			AudioMixKernels::add_frames(sum.ptr(), b.ptr(), frames);
			for (int k = 0; k < frames; k++) {
				ref_sum[k] += b[k];
			}
			SIMD_EXPECT_EQUAL(mismatches, same_frames(sum.ptr(), ref_sum.ptr(), frames), true);

			const AudioFrame vol_start = rng.chance(4) ? AudioFrame(0, 0) : random_frame(rng);
			const AudioFrame vol_final = random_frame(rng);
			AudioMixKernels::mix_ramped(sum.ptr(), a.ptr(), vol_start, vol_final, frames);
			reference_mix_ramped(ref_sum.ptr(), a.ptr(), vol_start, vol_final, frames);
			SIMD_EXPECT_EQUAL(mismatches, same_frames(sum.ptr(), ref_sum.ptr(), frames), true);

			EQ eq;
			eq.set_mix_rate(rng.chance(2) ? 44100 : 48000);
			static const EQ::Preset presets[] = { EQ::PRESET_6_BANDS, EQ::PRESET_8_BANDS, EQ::PRESET_10_BANDS, EQ::PRESET_21_BANDS, EQ::PRESET_31_BANDS };
			eq.set_preset_band_mode(presets[rng.range(5)]);
			const int band_count = eq.get_band_count();
			LocalVector<EQ::BandProcess> procs_l;
			LocalVector<EQ::BandProcess> procs_r;
			LocalVector<float> gains;
			for (int k = 0; k < band_count; k++) {
				procs_l.push_back(eq.get_band_processor(k));
				procs_r.push_back(eq.get_band_processor(k));
				gains.push_back(Math::db_to_linear(rng.unit() * 12.0f));
			}
			LocalVector<EQ::BandProcess> ref_procs_l = procs_l;
			LocalVector<EQ::BandProcess> ref_procs_r = procs_r;
			// Several blocks in a row so the filter history carried between calls is compared too.
			for (int block = 0; block < 3; block++) {
				const int block_frames = 1 + rng.range(frames);
				LocalVector<AudioFrame> eq_out;
				LocalVector<AudioFrame> ref_eq_out;
				eq_out.resize(block_frames);
				ref_eq_out.resize(block_frames);
				AudioMixKernels::EQProcess::process(a.ptr(), eq_out.ptr(), block_frames, procs_l.ptr(), procs_r.ptr(), gains.ptr(), band_count);
				reference_eq(a.ptr(), ref_eq_out.ptr(), block_frames, ref_procs_l.ptr(), ref_procs_r.ptr(), gains.ptr(), band_count);
				SIMD_EXPECT_EQUAL(mismatches, same_frames(eq_out.ptr(), ref_eq_out.ptr(), block_frames), true);
			}
		}
		SIMD_REPORT(mismatches);
	});
}

TEST_CASE("[Audio][SIMD] Stereo enhance effect matches the scalar reference") {
	for_each_dispatch([](const char *p_mode) {
		Random rng(23);
		Mismatches mismatches;
		const float mix_rate = AudioServer::get_singleton()->get_mix_rate();
		const int iterations = MAX(ITERATIONS / 100, 10);
		// Delays around the two-frame SIMD step and the ring length (time_pullout is not clamped),
		// each with and without surround, then random ones.
		int ring_size = 1;
		while (ring_size <= int((50 + 2) / 1000.0f * mix_rate)) {
			ring_size *= 2;
		}
		const unsigned int ring_mask = ring_size - 1;
		const unsigned int boundary_delays[] = { 0, 1, 2, 3, ring_mask - 2, ring_mask - 1, ring_mask, ring_mask + 1, ring_mask + 2, 2 * ring_mask + 1 };
		const int boundary_count = 2 * int(std::size(boundary_delays));
		for (int iteration = 0; iteration < boundary_count + iterations; iteration++) {
			// Stereo enhance. Its delay ring buffer starts uninitialized, so a full ring of warm-up
			// frames is processed (and discarded) before outputs are compared.
			Ref<AudioEffectStereoEnhance> enhance;
			enhance.instantiate();
			const float pan_pullout = rng.unit() * 2.0f + 2.0f;
			float time_pullout = float(rng.range(51));
			float surround = rng.chance(2) ? 0.0f : rng.unit() * 0.5f + 0.5f;
			if (iteration < boundary_count) {
				const unsigned int delay = boundary_delays[iteration / 2];
				time_pullout = float((delay + 0.5) * 1000.0 / mix_rate);
				REQUIRE((unsigned int)((time_pullout / 1000.0) * mix_rate) == delay);
				surround = (iteration % 2) ? 0.75f : 0.0f;
			}
			enhance->set_pan_pullout(pan_pullout);
			enhance->set_time_pullout(time_pullout);
			enhance->set_surround(surround);
			Ref<AudioEffectInstance> enhance_instance = enhance->instantiate();
			RefStereoEnhance ref_enhance;
			ref_enhance.params.pan_pullout = pan_pullout;
			ref_enhance.params.time_pullout = time_pullout;
			ref_enhance.params.surround = surround;
			float ring_buffer_max_size = 50 + 2;
			ring_buffer_max_size /= 1000.0;
			ring_buffer_max_size *= mix_rate;
			int ringbuff_size = (int)ring_buffer_max_size;
			int bits = 0;
			while (ringbuff_size > 0) {
				bits++;
				ringbuff_size /= 2;
			}
			ringbuff_size = 1 << bits;
			REQUIRE(ringbuff_size == ring_size);
			LocalVector<float> ring;
			ring.resize(ringbuff_size);
			ref_enhance.delay_ringbuff = ring.ptr();
			ref_enhance.ringbuff_mask = ringbuff_size - 1;
			{
				LocalVector<AudioFrame> warmup;
				LocalVector<AudioFrame> discard;
				warmup.resize(ringbuff_size);
				discard.resize(ringbuff_size);
				for (int k = 0; k < ringbuff_size; k++) {
					warmup[k] = random_frame(rng);
				}
				enhance_instance->process(warmup.ptr(), discard.ptr(), ringbuff_size);
				ref_enhance.process(warmup.ptr(), discard.ptr(), ringbuff_size);
			}
			for (int block = 0; block < 4; block++) {
				const int frames = 1 + rng.range(1024);
				LocalVector<AudioFrame> src;
				LocalVector<AudioFrame> out;
				LocalVector<AudioFrame> ref_out;
				src.resize(frames);
				out.resize(frames);
				ref_out.resize(frames);
				for (int k = 0; k < frames; k++) {
					src[k] = random_frame(rng);
				}
				enhance_instance->process(src.ptr(), out.ptr(), frames);
				ref_enhance.process(src.ptr(), ref_out.ptr(), frames);
				SIMD_EXPECT_EQUAL(mismatches, same_frames(out.ptr(), ref_out.ptr(), frames), true);
			}
		}
		SIMD_REPORT(mismatches);
	});
}

// --- Physics -------------------------------------------------------------------------------------

// The original GodotConcavePolygonShape3D::project_range / get_support and the extreme vertex scan
// in GodotConvexPolygonShape3D::_setup.
inline void reference_project_range(const Vector3 *vptr, int count, const Vector3 &p_normal, const Transform3D &p_transform, real_t &r_min, real_t &r_max) {
	for (int i = 0; i < count; i++) {
		real_t d = p_normal.dot(p_transform.xform(vptr[i]));

		if (i == 0 || d > r_max) {
			r_max = d;
		}
		if (i == 0 || d < r_min) {
			r_min = d;
		}
	}
}

inline int reference_support_index(const Vector3 *vptr, int count, const Vector3 &p_normal) {
	int vert_support_idx = -1;
	real_t support_max = 0;

	for (int i = 0; i < count; i++) {
		real_t d = p_normal.dot(vptr[i]);

		if (i == 0 || d > support_max) {
			support_max = d;
			vert_support_idx = i;
		}
	}
	return vert_support_idx;
}

TEST_CASE("[SIMD] Collision shape vertex scans match the scalar reference") {
	for_each_dispatch([](const char *p_mode) {
		Random rng(24);
		Mismatches mismatches;
		for (int iteration = 0; iteration < ITERATIONS / 4; iteration++) {
			const int count = 1 + rng.range(60);
			Vector<Vector3> vertices;
			for (int k = 0; k < count; k++) {
				Vector3 v = rng.v3();
				if (k > 0 && rng.chance(5)) {
					v = vertices[rng.range(k)];
				}
				vertices.push_back(v);
			}
			Vector3 normal = rng.v3();
			Transform3D xform = rng.transform();
			if (rng.chance(3)) {
				// Signed zero ties: every vertex projects to +0.0 or -0.0.
				normal = Vector3(1, 0, 0);
				xform = Transform3D();
				for (int k = 0; k < count; k++) {
					vertices.write[k] = Vector3(rng.chance(2) ? 0.0f : -0.0f, rng.edge_float(), rng.edge_float());
				}
			}
			real_t mn = 0;
			real_t mx = 0;
			real_t ref_mn = 0;
			real_t ref_mx = 0;
			SIMDGeometry::project_range(vertices.ptr(), count, normal, xform, mn, mx);
			reference_project_range(vertices.ptr(), count, normal, xform, ref_mn, ref_mx);
			SIMD_EXPECT_SAME_REAL(mismatches, mn, ref_mn);
			SIMD_EXPECT_SAME_REAL(mismatches, mx, ref_mx);
			SIMD_EXPECT_EQUAL(mismatches, SIMDGeometry::support_index(vertices.ptr(), count, normal), reference_support_index(vertices.ptr(), count, normal));

#ifdef MODULE_GODOT_PHYSICS_2D_ENABLED
			Vector<Vector2> points;
			for (int k = 0; k < count; k++) {
				points.push_back(rng.chance(16) ? rng.v2() : Vector2(rng.unit(), rng.unit()));
			}
			GodotConvexPolygonShape2D shape;
			shape.set_data(points);
			const Vector2 normal2 = rng.v2();
			const Transform2D xform2 = rng.transform2d();
			real_t mn2 = 0;
			real_t mx2 = 0;
			shape.project_range(normal2, xform2, mn2, mx2);
			real_t ref_mn2 = normal2.dot(xform2.xform(points[0]));
			real_t ref_mx2 = ref_mn2;
			for (int k = 1; k < count; k++) {
				real_t d = normal2.dot(xform2.xform(points[k]));
				if (d > ref_mx2) {
					ref_mx2 = d;
				}
				if (d < ref_mn2) {
					ref_mn2 = d;
				}
			}
			SIMD_EXPECT_SAME_REAL(mismatches, mn2, ref_mn2);
			SIMD_EXPECT_SAME_REAL(mismatches, mx2, ref_mx2);
#endif
		}
		SIMD_REPORT(mismatches);
	});
}

#undef SIMD_EXPECT_SAME_REAL
#undef SIMD_EXPECT_SAME_FLOAT
#undef SIMD_EXPECT_EQUAL
#undef SIMD_REPORT

} // namespace TestSIMD

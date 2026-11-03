#pragma once

#include <atomic>
#include <asm/hwcap.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/auxv.h>

#include "core/math/math_defs.h"

#if defined(GODOT_SIMD_DISABLE)
// Escape hatch: keep every scalar path (used to cross-check the SIMD code).
#elif (defined(__aarch64__) || defined(__arm__)) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
#define GODOT_SIMD_NEON
#define GODOT_SIMD_F32X4
#include <arm_neon.h>
#endif

#if defined(GODOT_SIMD_F32X4) && !defined(REAL_T_IS_DOUBLE)
#define GODOT_SIMD_REAL_F32X4
#endif

#ifdef GODOT_SIMD_F32X4

namespace simd
{

struct f32x4 {
	float32x4_t v;
	_ALWAYS_INLINE_ f32x4(float32x4_t p_v) :
			v(p_v) {}
	_ALWAYS_INLINE_ f32x4() = default;
	_ALWAYS_INLINE_ operator float32x4_t() const
	{
		return v;
	}
};

struct i32x4 {
	int32x4_t v;
	_ALWAYS_INLINE_ i32x4(int32x4_t p_v) :
			v(p_v) {}
	_ALWAYS_INLINE_ i32x4() = default;
	_ALWAYS_INLINE_ operator int32x4_t() const
	{
		return v;
	}
};

using mask4 = f32x4;

_ALWAYS_INLINE_ f32x4 zero4()
{
	return vdupq_n_f32(0);
}

_ALWAYS_INLINE_ f32x4 set4(float p_arr[4])
{
	return vld1q_f32(p_arr);
}

_ALWAYS_INLINE_ f32x4 splat4(float p_v)
{
	return vdupq_n_f32(p_v);
}

_ALWAYS_INLINE_ f32x4 load4(const float *p_ptr)
{
	return vld1q_f32(p_ptr);
}

_ALWAYS_INLINE_ f32x4 load3(const float *p_ptr)
{
	f32x4 ret = vcombine_f32(vld1_f32(p_ptr), vdup_n_f32(0));
	ret = vld1q_lane_f32(p_ptr + 2, ret, 2);
	return ret;
}

_ALWAYS_INLINE_ f32x4 load2(const float *p_ptr)
{
	return vcombine_f32(vld1_f32(p_ptr), vdup_n_f32(0));
}

_ALWAYS_INLINE_ f32x4 load2x2(const float *p_ptr)
{
	return vcombine_f32(vld1_f32(p_ptr), vld1_f32(p_ptr));
}

_ALWAYS_INLINE_ void store4(float *p_ptr, f32x4 p_v)
{
	vst1q_f32(p_ptr, p_v);
}

_ALWAYS_INLINE_ void store3(float *p_ptr, f32x4 p_v)
{
	vst1q_lane_f32(p_ptr + 0, p_v, 0);
	vst1q_lane_f32(p_ptr + 1, p_v, 1);
	vst1q_lane_f32(p_ptr + 2, p_v, 2);
}

_ALWAYS_INLINE_ void store2(float *p_ptr, f32x4 p_v)
{
	vst1q_lane_f32(p_ptr + 0, p_v, 0);
	vst1q_lane_f32(p_ptr + 1, p_v, 1);
}

// _ALWAYS_INLINE_ f32x4 operator+(f32x4 p_a, f32x4 p_b)
// {
// 	return vaddq_f32(p_a, p_b);
// }

// _ALWAYS_INLINE_ f32x4 operator-(f32x4 p_a, f32x4 p_b)
// {
// 	return vsubq_f32(p_a, p_b);
// }
//
// _ALWAYS_INLINE_ f32x4 operator*(f32x4 p_a, f32x4 p_b)
// {
// 	return vmulq_f32(p_a, p_b);
// }
//
// _ALWAYS_INLINE_ f32x4 operator/(f32x4 p_a, f32x4 p_b)
// {
// 	return vdivq_f32(p_a, p_b);
// }

_ALWAYS_INLINE_ f32x4 operator~(f32x4 p_a)
{
	return vreinterpretq_f32_u32(vmvnq_u32(vreinterpretq_u32_f32(p_a)));
}

_ALWAYS_INLINE_ f32x4 operator&(f32x4 p_a, f32x4 p_b)
{
	return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(p_a), vreinterpretq_u32_f32(p_b)));
}

_ALWAYS_INLINE_ f32x4 operator|(f32x4 p_a, f32x4 p_b)
{
	return vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(p_a), vreinterpretq_u32_f32(p_b)));
}

_ALWAYS_INLINE_ f32x4 operator^(f32x4 p_a, f32x4 p_b)
{
	return (p_a & ~p_b) | (~p_a & p_b); // there is no xor instruction
}

_ALWAYS_INLINE_ f32x4 abs4(f32x4 p_a)
{
	return vabsq_f32(p_a);
}

_ALWAYS_INLINE_ f32x4 sqrt4(f32x4 p_a)
{
	return vsqrtq_f32(p_a);
}

_ALWAYS_INLINE_ f32x4 min4(f32x4 p_a, f32x4 p_b)
{
	return vpminnmq_f32(p_a, p_b);
}

_ALWAYS_INLINE_ f32x4 max4(f32x4 p_a, f32x4 p_b)
{
	return vpmaxnmq_f32(p_a, p_b);
}

_ALWAYS_INLINE_ mask4 cmp_lt(f32x4 p_a, f32x4 p_b)
{
	return vreinterpretq_f32_u32(vcltq_f32(p_a, p_b));
}

_ALWAYS_INLINE_ mask4 cmp_le(f32x4 p_a, f32x4 p_b)
{
	return vreinterpretq_f32_u32(vcleq_f32(p_a, p_b));
}

_ALWAYS_INLINE_ mask4 cmp_gt(f32x4 p_a, f32x4 p_b)
{
	return vreinterpretq_f32_u32(vcgtq_f32(p_a, p_b));
}

_ALWAYS_INLINE_ mask4 cmp_ge(f32x4 p_a, f32x4 p_b)
{
	return vreinterpretq_f32_u32(vcgeq_f32(p_a, p_b));
}

_ALWAYS_INLINE_ mask4 cmp_eq(f32x4 p_a, f32x4 p_b)
{
	return vreinterpretq_f32_u32(vceqq_f32(p_a, p_b));
}

_ALWAYS_INLINE_ mask4 cmp_neq(f32x4 p_a, f32x4 p_b)
{
	return ~f32x4(vreinterpretq_f32_u32(vceqq_f32(p_a, p_b)));
}

_ALWAYS_INLINE_ int movemask(mask4 p_m)
{
	return (int)vaddvq_u32(
		vandq_u32(
			vtstq_u32(
				vreinterpretq_u32_f32(p_m),
				vreinterpretq_u32_f32(splat4(*(float*)((uint32_t)0x10000000)))
			),
			vld1q_u32(((uint32_t[4]){ 0x8, 0x4, 0x2, 0x1 }))
		)
	);
}

_ALWAYS_INLINE_ f32x4 select(mask4 p_mask, f32x4 p_a, f32x4 p_b)
{
	return (p_a & p_mask) | (p_b & ~p_mask);
}

_ALWAYS_INLINE_ f32x4 floor4(f32x4 p_a)
{
	return vrndmq_f32(p_a);
}

_ALWAYS_INLINE_ f32x4 ceil4(f32x4 p_a)
{
	return vrndpq_f32(p_a);
}

_ALWAYS_INLINE_ f32x4 trunc4(f32x4 p_a)
{
	return vrndq_f32(p_a);
}

template <int A, int B, int C, int D>
_ALWAYS_INLINE_ f32x4 shuffle(f32x4 p_a, f32x4 p_b)
{
	f32x4 ret;
	ret = vcopyq_laneq_f32(p_a, 0, p_a, A);
	ret = vcopyq_laneq_f32(ret, 1, p_a, B);
	ret = vcopyq_laneq_f32(ret, 2, p_b, C);
	ret = vcopyq_laneq_f32(ret, 3, p_b, D);
	return ret;
}

template <int A, int B, int C, int D>
_ALWAYS_INLINE_ f32x4 permute(f32x4 p_a)
{
	return shuffle<A, B, C, D>(p_a, p_a);
}

template <int I>
_ALWAYS_INLINE_ f32x4 splat_lane(f32x4 p_a)
{
	return vdupq_n_f32(vgetq_lane_f32(p_a, I));
}

template <int I>
_ALWAYS_INLINE_ float get_lane(f32x4 p_a)
{
	return vgetq_lane_f32(p_a, I);
}

_ALWAYS_INLINE_ f32x4 insert_lane3(f32x4 p_a, f32x4 p_b)
{
	return vextq_f32(p_a, vrev64q_f32(p_b), 2);
}

_ALWAYS_INLINE_ f32x4 movelh(f32x4 p_a, f32x4 p_b)
{
	return vextq_f32(p_a, p_b, 2);
}

_ALWAYS_INLINE_ f32x4 movehl(f32x4 p_a, f32x4 p_b)
{
	return vcombine_f32(vget_high_f32(p_b), vget_high_f32(p_a));
}

_ALWAYS_INLINE_ f32x4 unpacklo(f32x4 p_a, f32x4 p_b)
{
	return vzip1q_f32(p_a, p_b);
}

_ALWAYS_INLINE_ f32x4 unpackhi(f32x4 p_a, f32x4 p_b)
{
	return vzip2q_f32(p_a, p_b);
}

_ALWAYS_INLINE_ f32x4 &operator+=(f32x4 &p_a, f32x4 p_b)
{
	p_a = vaddq_f32(p_a, p_b);
	return p_a;
}

_ALWAYS_INLINE_ f32x4 &operator-=(f32x4 &p_a, f32x4 p_b)
{
	p_a = vsubq_f32(p_a, p_b);
	return p_a;
}

_ALWAYS_INLINE_ f32x4 &operator*=(f32x4 &p_a, f32x4 p_b)
{
	p_a = vmulq_f32(p_a, p_b);
	return p_a;
}

_ALWAYS_INLINE_ f32x4 &operator/=(f32x4 &p_a, f32x4 p_b)
{
	p_a = vdivq_f32(p_a, p_b);
	return p_a;
}

_ALWAYS_INLINE_ f32x4 &operator&=(f32x4 &p_a, f32x4 p_b)
{
	p_a = vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(p_a), vreinterpretq_u32_f32(p_b)));
	return p_a;
}

_ALWAYS_INLINE_ f32x4 &operator|=(f32x4 &p_a, f32x4 p_b)
{
	p_a = vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(p_a), vreinterpretq_u32_f32(p_b)));
	return p_a;
}

_ALWAYS_INLINE_ f32x4 &operator^=(f32x4 &p_a, f32x4 p_b)
{
	p_a = p_a ^ p_b;
	return p_a;
}

_ALWAYS_INLINE_ bool all_lanes(mask4 p_m)
{
	return vminnmvq_f32(p_m) != 0;
}

_ALWAYS_INLINE_ bool any_lane(mask4 p_m)
{
	return vmaxnmvq_f32(p_m) != 0;
}

_ALWAYS_INLINE_ bool all_lanes3(mask4 p_m)
{
	p_m = vsetq_lane_f32(1, p_m, 3);
	return vminnmvq_f32(p_m) != 0;
}

_ALWAYS_INLINE_ bool any_lane3(mask4 p_m)
{
	p_m = vsetq_lane_f32(0, p_m, 3);
	return vmaxnmvq_f32(p_m) != 0;
}

_ALWAYS_INLINE_ i32x4 zero_i32x4()
{
	return vdupq_n_s32(0);
}

_ALWAYS_INLINE_ i32x4 set_i32x4(int32_t p_arr[4])
{
	return vld1q_s32(p_arr);
}

_ALWAYS_INLINE_ i32x4 count_if(i32x4 p_count, mask4 p_m)
{
	return vsubq_s32(p_count, vreinterpretq_s32_f32(p_m));
}

_ALWAYS_INLINE_ mask4 cmp_eq(i32x4 p_a, int32_t p_b)
{
	return vreinterpretq_f32_u32(vceqq_s32(p_a, vdupq_n_s32(p_b)));
}

_ALWAYS_INLINE_ i32x4 iota_i32x4(int32_t p_first)
{
	i32x4 ret = vdupq_n_s32(p_first);
	i32x4 iota = vld1q_s32(((int32_t[4]){ 0, 1, 2, 3 }));
	return vaddq_s32(ret, iota);
}

_ALWAYS_INLINE_ i32x4 add(i32x4 p_a, int32_t p_b)
{
	return vaddq_s32(p_a, vdupq_n_s32(p_b));
}

_ALWAYS_INLINE_ i32x4 operator+(i32x4 p_a, i32x4 p_b)
{
	return vaddq_s32(p_a, p_b);
}

_ALWAYS_INLINE_ i32x4 operator-(i32x4 p_a, i32x4 p_b)
{
	return vsubq_s32(p_a, p_b);
}

_ALWAYS_INLINE_ i32x4 operator*(i32x4 p_a, i32x4 p_b)
{
	return vmulq_s32(p_a, p_b);
}

_ALWAYS_INLINE_ i32x4 operator~(i32x4 p_a)
{
	return vreinterpretq_s32_u32(vmvnq_u32(vreinterpretq_u32_s32(p_a)));
}

_ALWAYS_INLINE_ i32x4 operator&(i32x4 p_a, i32x4 p_b)
{
	return vandq_s32(p_a, p_b);
}

// _ALWAYS_INLINE_ i32x4 operator&(mask4 p_a, i32x4 p_b)
// {
// 	return vandq_s32(vreinterpretq_s32_f32(p_a), p_b);
// }

_ALWAYS_INLINE_ i32x4 operator&(i32x4 p_a, mask4 p_b)
{
	return vandq_s32(p_a, vreinterpretq_s32_f32(p_b));
}

_ALWAYS_INLINE_ i32x4 operator|(i32x4 p_a, i32x4 p_b)
{
	return vreinterpretq_s32_u32(vorrq_u32(vreinterpretq_u32_s32(p_a), vreinterpretq_u32_s32(p_b)));
}

_ALWAYS_INLINE_ i32x4 operator^(i32x4 p_a, i32x4 p_b)
{
	return (p_a & ~p_b) | (~p_a & p_b); // there is no xor instruction
}

_ALWAYS_INLINE_ i32x4 select_i32x4(mask4 p_mask, i32x4 p_a, i32x4 p_b)
{
	return (p_a & p_mask) | (p_b & ~p_mask);
}

_ALWAYS_INLINE_ void store_i32x4(int32_t *r_ptr, i32x4 p_v)
{
	vst1q_s32(r_ptr, p_v);
}

/// Matches Math::round(): v >= 0 ? (T)(long long)(v + 0.5) : (T)(long long)(v - 0.5), including what the
/// hardware float -> int64 conversion does for NaN and out of range values on each architecture.
_ALWAYS_INLINE_ f32x4 round4(f32x4 p_a) {
	const f32x4 half = splat4(0.5f);
	f32x4 adj = select(cmp_ge(p_a, zero4()), p_a + half, p_a - half);
	// trunc4()'s _mm_round_ps keeps -0.0, but Math::round()'s integer cast yields +0.0. Clear the sign
	// of zero with a bitwise mask; "+ 0.0f" is dropped by some compilers (MSVC), leaving -0.0 behind.
	f32x4 truncated = trunc4(adj);
	f32x4 t = cmp_neq(truncated, zero4()) & truncated;
	const f32x4 limit = splat4(9223372036854775808.0f);
	// cvttss2si returns 0x8000000000000000 for NaN and anything outside the int64 range.
	mask4 invalid = cmp_ge(abs4(adj), limit) | cmp_neq(adj, adj);
	return select(invalid, -limit, t);
}

_ALWAYS_INLINE_ f32x4 sign4(f32x4 p_a)
{
	return vcvtq_f32_u32(cmp_ge(vclsq_s32(vreinterpretq_s32_f32(p_a)), vdupq_n_s32(1)) & vdupq_n_f32(1));
}

_ALWAYS_INLINE_ f32x4 clamp4(f32x4 p_v, f32x4 p_min, f32x4 p_max)
{
	f32x4 r = select(cmp_gt(p_v, p_max), p_max, p_v);
	return select(cmp_lt(p_v, p_min), p_min, r);
}

_ALWAYS_INLINE_ f32x4 lerp4(f32x4 p_from, f32x4 p_to, f32x4 p_weight)
{
	return p_from + (p_to - p_from) * p_weight;
}

_ALWAYS_INLINE_ float dot3(f32x4 p_a, f32x4 p_b)
{
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

_ALWAYS_INLINE_ uint32_t pack_trunc_u8x4(f32x4 p_v)
{
	return vget_lane_u32(
		vreinterpret_u32_s8(
			vmovn_s16(
				vcombine_s16(
					vmovn_s32(vcvtq_s32_f32(p_v)),
					vdup_n_s16(0)
				)
			)
		),
		0
	);
}

_ALWAYS_INLINE_ uint64_t pack_trunc_u16x4(f32x4 p_v)
{
	return vget_lane_u64(
		vreinterpret_u64_s16(
			vmovn_s32(vcvtq_s32_f32(p_v))
		),
		0
	);
}

_ALWAYS_INLINE_ f32x4 half_to_float4(const uint16_t *p_half)
{
	return vcvt_f32_f16(vld1_f16((float16_t*)p_half));
}

_ALWAYS_INLINE_ void float_to_half4(f32x4 p_v, uint16_t *r_half)
{
	vst1_u16(r_half, vreinterpret_u16_f16(vcvt_f16_f32(p_v)));
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
	return true;
}

_ALWAYS_INLINE_ void load_vec3x4_soa(const float *p_v, f32x4 &r_x, f32x4 &r_y, f32x4 &r_z)
{
	float32x4x3_t vecs = vld3q_f32(p_v);
	r_x = vecs.val[0];
	r_y = vecs.val[1];
	r_z = vecs.val[2];
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

_ALWAYS_INLINE_ void load_mat3(const float *p_m, f32x4 &r_r0, f32x4 &r_r1, f32x4 &r_r2)
{
	r_r0 = load4(p_m);
	r_r1 = load4(p_m + 3);
	// 8 + 4 byte loads so a row just written by store3 still forwards from the store buffer.
	r_r2 = load3(p_m + 6);
}

}

#endif

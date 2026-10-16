// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEC_MATH_SSE_H
#define PX_VEC_MATH_SSE_H

// PT: this file contains the SSE code common to the Windows & Unix versions

#if !COMPILE_VECTOR_INTRINSICS
	#error Vector intrinsics should not be included when using scalar implementation.
#endif

#ifdef __SSE4_2__
	#include "smmintrin.h"
#endif

#if !PX_DOXYGEN
namespace physx
{
#endif
namespace aos
{

namespace
{
	const PX_ALIGN(16, PxF32) minus1w[4] = { 0.0f, 0.0f, 0.0f, -1.0f };
}

PX_FORCE_INLINE void QuatGetMat33V(const QuatVArg q, Vec3V& column0, Vec3V& column1, Vec3V& column2)
{
    const __m128 q2 = V4Add(q, q);
    const __m128 qw2 = V4MulAdd(q2, V4GetW(q), _mm_load_ps(minus1w));			// (2wx, 2wy, 2wz, 2ww-1)
    const __m128 nw2 = Vec3V_From_Vec4V(V4Neg(qw2));							// (-2wx, -2wy, -2wz, 0)
    const __m128 v = Vec3V_From_Vec4V(q);

    const __m128 a0 = _mm_shuffle_ps(qw2, nw2, _MM_SHUFFLE(3, 1, 2, 3));		// (2ww-1, 2wz, -2wy, 0)
    column0 = V4MulAdd(v, V4GetX(q2), a0);

    const __m128 a1 = _mm_shuffle_ps(qw2, nw2, _MM_SHUFFLE(3, 2, 0, 3));		// (2ww-1, 2wx, -2wz, 0)
    column1 = V4MulAdd(v, V4GetY(q2), _mm_shuffle_ps(a1, a1, _MM_SHUFFLE(3, 1, 0, 2)));

    const __m128 a2 = _mm_shuffle_ps(qw2, nw2, _MM_SHUFFLE(3, 0, 1, 3));		// (2ww-1, 2wy, -2wx, 0)
    column2 = V4MulAdd(v, V4GetZ(q2), _mm_shuffle_ps(a2, a2, _MM_SHUFFLE(3, 0, 2, 1)));
}

//////////////////////////////////////////////////////////////////////
//Test that Vec3V and FloatV are legal
//////////////////////////////////////////////////////////////////////

PX_FORCE_INLINE bool isValidVec3V(const Vec3V a)
{
	//using _mm_comieq_ss to do the comparison doesn't work for NaN.
	PX_ALIGN(16, PxF32 f[4]);
	V4StoreA(a, f);
	return f[3] == 0.0f;
}

PX_FORCE_INLINE bool isFiniteLength(const Vec3V a)
{
	return !FAllEq(V4LengthSq(a), FZero());
}

PX_FORCE_INLINE bool isAligned16(const void* a)
{
	return (0 == (size_t(a) & 0x0f));
}

//ASSERT_FINITELENGTH is deactivated because there is a lot of code that calls a simd normalisation function with zero length but then ignores the result.

#if PX_DEBUG
	#define ASSERT_ISVALIDVEC3V(a) PX_ASSERT(isValidVec3V(a))
	#define ASSERT_ISVALIDFLOATV(a) PX_ASSERT(isValidFloatV(a))
	#define ASSERT_ISALIGNED16(a) PX_ASSERT(isAligned16(reinterpret_cast<const void*>(a)))
	#define ASSERT_ISFINITELENGTH(a) //PX_ASSERT(isFiniteLength(a))
#else
	#define ASSERT_ISVALIDVEC3V(a)
	#define ASSERT_ISVALIDFLOATV(a) 
	#define ASSERT_ISALIGNED16(a)
	#define ASSERT_ISFINITELENGTH(a)
#endif

namespace internalSimd
{
PX_FORCE_INLINE __m128 m128_I2F(__m128i n)
{
	return _mm_castsi128_ps(n);
}

PX_FORCE_INLINE __m128i m128_F2I(__m128 n)
{
	return _mm_castps_si128(n);
}

PX_FORCE_INLINE PxU32 BAllTrue4_R(const BoolV a)
{
	const PxI32 moveMask = _mm_movemask_ps(a);
	return PxU32(moveMask == 0xf);
}

PX_FORCE_INLINE PxU32 BAllTrue3_R(const BoolV a)
{
	const PxI32 moveMask = _mm_movemask_ps(a);
	return PxU32((moveMask & 0x7) == 0x7);
}

PX_FORCE_INLINE PxU32 BAnyTrue4_R(const BoolV a)
{
	const PxI32 moveMask = _mm_movemask_ps(a);
	return PxU32(moveMask != 0x0);
}

PX_FORCE_INLINE PxU32 BAnyTrue3_R(const BoolV a)
{
	const PxI32 moveMask = _mm_movemask_ps(a);
	return PxU32((moveMask & 0x7) != 0x0);
}

PX_FORCE_INLINE PxU32 FiniteTestEq(const Vec4V a, const Vec4V b)
{
	// This is a bit of a bodge.
	//_mm_comieq_ss returns 1 if either value is nan so we need to re-cast a and b with true encoded as a non-nan
	// number.
	// There must be a better way of doing this in sse.
	const BoolV one = FOne();
	const BoolV zero = FZero();
	const BoolV a1 = V4Sel(a, one, zero);
	const BoolV b1 = V4Sel(b, one, zero);
	return (PxU32(
	    _mm_comieq_ss(a1, b1) &&
	    _mm_comieq_ss(_mm_shuffle_ps(a1, a1, _MM_SHUFFLE(1, 1, 1, 1)), _mm_shuffle_ps(b1, b1, _MM_SHUFFLE(1, 1, 1, 1))) &&
	    _mm_comieq_ss(_mm_shuffle_ps(a1, a1, _MM_SHUFFLE(2, 2, 2, 2)), _mm_shuffle_ps(b1, b1, _MM_SHUFFLE(2, 2, 2, 2))) &&
	    _mm_comieq_ss(_mm_shuffle_ps(a1, a1, _MM_SHUFFLE(3, 3, 3, 3)), _mm_shuffle_ps(b1, b1, _MM_SHUFFLE(3, 3, 3, 3)))));
}

PX_FORCE_INLINE bool hasZeroElementInFloatV(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);
	return _mm_comieq_ss(_mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 0)), FZero()) ? true : false;
}

PX_FORCE_INLINE bool hasZeroElementInVec3V(const Vec3V a)
{
	return (_mm_comieq_ss(_mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 0)), FZero()) ||
			_mm_comieq_ss(_mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1)), FZero()) ||
			_mm_comieq_ss(_mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2)), FZero()));
}

PX_FORCE_INLINE bool hasZeroElementInVec4V(const Vec4V a)
{
	return (_mm_comieq_ss(_mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 0)), FZero()) ||
	        _mm_comieq_ss(_mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1)), FZero()) ||
	        _mm_comieq_ss(_mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2)), FZero()) ||
	        _mm_comieq_ss(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 3, 3, 3)), FZero()));
}

} //internalSimd

namespace vecMathTests
{
// PT: this function returns an invalid Vec3V (W!=0.0f) just for unit-testing 'isValidVec3V'
PX_FORCE_INLINE Vec3V getInvalidVec3V()
{
	const float f = 1.0f;
	return _mm_load1_ps(&f);
}

PX_FORCE_INLINE bool allElementsEqualFloatV(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_comieq_ss(a, b) != 0;
}

PX_FORCE_INLINE bool allElementsEqualVec3V(const Vec3V a, const Vec3V b)
{
	return V3AllEq(a, b) != 0;
}

PX_FORCE_INLINE bool allElementsEqualVec4V(const Vec4V a, const Vec4V b)
{
	return V4AllEq(a, b) != 0;
}

PX_FORCE_INLINE bool allElementsEqualVecU32V(const VecU32V a, const VecU32V b)
{
	return internalSimd::BAllTrue4_R(V4IsEqU32(a, b)) != 0;
}

} //vecMathTests

/////////////////////////////////////////////////////////////////////
////VECTORISED FUNCTION IMPLEMENTATIONS
/////////////////////////////////////////////////////////////////////

PX_FORCE_INLINE BoolV BLoad(const bool f)
{
	const PxU32 i = PxU32(-PxI32(f));
	return _mm_load1_ps(reinterpret_cast<const float*>(&i));
}

PX_FORCE_INLINE FloatV FLoad(const PxF32 f)
{
	return _mm_load1_ps(&f);
}

PX_FORCE_INLINE Vec3V V3Load(const PxF32 f)
{
	return _mm_set_ps(0.0f, f, f, f);
}

PX_FORCE_INLINE Vec4V V4Load(const PxF32 f)
{
	return _mm_load1_ps(&f);
}

PX_FORCE_INLINE Vec3V V3LoadU(const PxVec3& f)
{
	return _mm_set_ps(0.0f, f.z, f.y, f.x);
}

PX_FORCE_INLINE Vec3V V3LoadU(const PxF32* const i)
{
	return _mm_set_ps(0.0f, i[2], i[1], i[0]);
}

PX_FORCE_INLINE Vec3V Vec3V_From_Vec4V(Vec4V v)
{
	return V4ClearW(v);
}

PX_FORCE_INLINE Vec3V Vec3V_From_Vec4V_WUndefined(const Vec4V v)
{
	return v;
}

PX_FORCE_INLINE Vec4V Vec4V_From_PxVec3_WUndefined(const PxVec3& f)
{
	return _mm_set_ps(0.0f, f.z, f.y, f.x);
}

PX_FORCE_INLINE Vec4V Vec4V_From_FloatV(FloatV f)
{
	return f;
}

PX_FORCE_INLINE Vec4V Vec4V_From_Vec3V(Vec3V f)
{
	ASSERT_ISVALIDVEC3V(f);
	return f; // ok if it is implemented as the same type.
}

PX_FORCE_INLINE Vec3V Vec3V_From_FloatV(FloatV f)
{
	ASSERT_ISVALIDFLOATV(f);
	return Vec3V_From_Vec4V(Vec4V_From_FloatV(f));
}

PX_FORCE_INLINE Vec3V Vec3V_From_FloatV_WUndefined(FloatV f)
{
	ASSERT_ISVALIDFLOATV(f);
	return Vec3V_From_Vec4V_WUndefined(Vec4V_From_FloatV(f));
}

PX_FORCE_INLINE Vec4V V4LoadA(const PxF32* const f)
{
	ASSERT_ISALIGNED16(f);
	return _mm_load_ps(f);
}

PX_FORCE_INLINE Vec4V V4LoadU(const PxF32* const f)
{
	return _mm_loadu_ps(f);
}

PX_FORCE_INLINE void V4StoreA(const Vec4V a, PxF32* f)
{
	ASSERT_ISALIGNED16(f);
	_mm_store_ps(f, a);
}

PX_FORCE_INLINE void V4StoreU(const Vec4V a, PxF32* f)
{
	_mm_storeu_ps(f, a);
}

PX_FORCE_INLINE void BStoreA(const BoolV a, PxU32* f)
{
	ASSERT_ISALIGNED16(f);
	_mm_store_ps(reinterpret_cast<PxF32*>(f), a);
}

PX_FORCE_INLINE void U4StoreA(const VecU32V uv, PxU32* u)
{
	ASSERT_ISALIGNED16(u);
	_mm_store_ps(reinterpret_cast<float*>(u), uv);
}

PX_FORCE_INLINE void FStore(const FloatV a, PxF32* PX_RESTRICT f)
{
	ASSERT_ISVALIDFLOATV(a);
	_mm_store_ss(f, a);
}

PX_FORCE_INLINE void Store_From_BoolV(const BoolV b, PxU32* b2)
{
	_mm_store_ss(reinterpret_cast<PxF32*>(b2), b);
}

//////////////////////////////////
// FLOATV
//////////////////////////////////

PX_FORCE_INLINE FloatV FZero()
{
	return _mm_setzero_ps();
}

PX_FORCE_INLINE FloatV FOne()
{
	return FLoad(1.0f);
}

PX_FORCE_INLINE FloatV FHalf()
{
	return FLoad(0.5f);
}

PX_FORCE_INLINE FloatV FEps()
{
	return FLoad(PX_EPS_REAL);
}

PX_FORCE_INLINE FloatV FEps6()
{
	return FLoad(1e-6f);
}

PX_FORCE_INLINE FloatV FMax()
{
	return FLoad(PX_MAX_REAL);
}

PX_FORCE_INLINE FloatV FNegMax()
{
	return FLoad(-PX_MAX_REAL);
}

PX_FORCE_INLINE FloatV IZero()
{
	const PxU32 zero = 0;
	return _mm_load1_ps(reinterpret_cast<const PxF32*>(&zero));
}

PX_FORCE_INLINE FloatV IOne()
{
	const PxU32 one = 1;
	return _mm_load1_ps(reinterpret_cast<const PxF32*>(&one));
}

PX_FORCE_INLINE FloatV ITwo()
{
	const PxU32 two = 2;
	return _mm_load1_ps(reinterpret_cast<const PxF32*>(&two));
}

PX_FORCE_INLINE FloatV IThree()
{
	const PxU32 three = 3;
	return _mm_load1_ps(reinterpret_cast<const PxF32*>(&three));
}

PX_FORCE_INLINE FloatV IFour()
{
	const PxU32 four = 4;
	return _mm_load1_ps(reinterpret_cast<const PxF32*>(&four));
}

PX_FORCE_INLINE FloatV FNeg(const FloatV f)
{
	ASSERT_ISVALIDFLOATV(f);
	return _mm_sub_ps(_mm_setzero_ps(), f);
}

PX_FORCE_INLINE FloatV FAdd(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_add_ps(a, b);
}

PX_FORCE_INLINE FloatV FSub(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_sub_ps(a, b);
}

PX_FORCE_INLINE FloatV FMul(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_mul_ps(a, b);
}

PX_FORCE_INLINE FloatV FDiv(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_div_ps(a, b);
}

PX_FORCE_INLINE FloatV FDivFast(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_mul_ps(a, _mm_rcp_ps(b));
}

PX_FORCE_INLINE FloatV FRecip(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);
	return _mm_div_ps(FOne(), a);
}

PX_FORCE_INLINE FloatV FRecipFast(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);
	return _mm_rcp_ps(a);
}

PX_FORCE_INLINE FloatV FRsqrt(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);
	return _mm_div_ps(FOne(), _mm_sqrt_ps(a));
}

PX_FORCE_INLINE FloatV FSqrt(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);
	return _mm_sqrt_ps(a);
}

PX_FORCE_INLINE FloatV FRsqrtFast(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);
	return _mm_rsqrt_ps(a);
}

PX_FORCE_INLINE FloatV FScaleAdd(const FloatV a, const FloatV b, const FloatV c)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	ASSERT_ISVALIDFLOATV(c);
	return FAdd(FMul(a, b), c);
}

PX_FORCE_INLINE FloatV FNegScaleSub(const FloatV a, const FloatV b, const FloatV c)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	ASSERT_ISVALIDFLOATV(c);
	return FSub(c, FMul(a, b));
}

PX_FORCE_INLINE FloatV FSel(const BoolV c, const FloatV a, const FloatV b)
{
	PX_ASSERT(vecMathTests::allElementsEqualBoolV(c, BTTTT()) ||
			  vecMathTests::allElementsEqualBoolV(c, BFFFF()));
	ASSERT_ISVALIDFLOATV(_mm_or_ps(_mm_andnot_ps(c, b), _mm_and_ps(c, a)));
	return _mm_or_ps(_mm_andnot_ps(c, b), _mm_and_ps(c, a));
}

PX_FORCE_INLINE BoolV FIsGrtr(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_cmpgt_ps(a, b);
}

PX_FORCE_INLINE BoolV FIsGrtrOrEq(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_cmpge_ps(a, b);
}

PX_FORCE_INLINE BoolV FIsEq(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_cmpeq_ps(a, b);
}

PX_FORCE_INLINE FloatV FMax(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_max_ps(a, b);
}

PX_FORCE_INLINE FloatV FMin(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_min_ps(a, b);
}

PX_FORCE_INLINE FloatV FClamp(const FloatV a, const FloatV minV, const FloatV maxV)
{
	ASSERT_ISVALIDFLOATV(minV);
	ASSERT_ISVALIDFLOATV(maxV);
	return _mm_max_ps(_mm_min_ps(a, maxV), minV);
}

PX_FORCE_INLINE PxU32 FAllGrtr(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return PxU32(_mm_comigt_ss(a, b));
}

PX_FORCE_INLINE PxU32 FAllGrtrOrEq(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return PxU32(_mm_comige_ss(a, b));
}

PX_FORCE_INLINE PxU32 FAllEq(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	return PxU32(_mm_comieq_ss(a, b));
}

PX_FORCE_INLINE FloatV FRound(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);
#ifdef __SSE4_2__
	return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
#else
	// return _mm_round_ps(a, 0x0);
	const FloatV half = FLoad(0.5f);
	const __m128 signBit = _mm_cvtepi32_ps(_mm_srli_epi32(_mm_cvtps_epi32(a), 31));
	const FloatV aRound = FSub(FAdd(a, half), signBit);
	__m128i tmp = _mm_cvttps_epi32(aRound);
	return _mm_cvtepi32_ps(tmp);
#endif
}

PX_FORCE_INLINE FloatV FSin(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);

	// Modulo the range of the given angles such that -XM_2PI <= Angles < XM_2PI
	const FloatV recipTwoPi = V4LoadA(g_PXReciprocalTwoPi.f);
	const FloatV twoPi = V4LoadA(g_PXTwoPi.f);
	const FloatV tmp = FMul(a, recipTwoPi);
	const FloatV b = FRound(tmp);
	const FloatV V1 = FNegScaleSub(twoPi, b, a);

	// sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! + V^9 / 9! - V^11 / 11! + V^13 / 13! -
	//           V^15 / 15! + V^17 / 17! - V^19 / 19! + V^21 / 21! - V^23 / 23! (for -PI <= V < PI)
	const FloatV V2 = FMul(V1, V1);
	const FloatV V3 = FMul(V2, V1);
	const FloatV V5 = FMul(V3, V2);
	const FloatV V7 = FMul(V5, V2);
	const FloatV V9 = FMul(V7, V2);
	const FloatV V11 = FMul(V9, V2);
	const FloatV V13 = FMul(V11, V2);
	const FloatV V15 = FMul(V13, V2);
	const FloatV V17 = FMul(V15, V2);
	const FloatV V19 = FMul(V17, V2);
	const FloatV V21 = FMul(V19, V2);
	const FloatV V23 = FMul(V21, V2);

	const Vec4V sinCoefficients0 = V4LoadA(g_PXSinCoefficients0.f);
	const Vec4V sinCoefficients1 = V4LoadA(g_PXSinCoefficients1.f);
	const Vec4V sinCoefficients2 = V4LoadA(g_PXSinCoefficients2.f);

	const FloatV S1 = V4GetY(sinCoefficients0);
	const FloatV S2 = V4GetZ(sinCoefficients0);
	const FloatV S3 = V4GetW(sinCoefficients0);
	const FloatV S4 = V4GetX(sinCoefficients1);
	const FloatV S5 = V4GetY(sinCoefficients1);
	const FloatV S6 = V4GetZ(sinCoefficients1);
	const FloatV S7 = V4GetW(sinCoefficients1);
	const FloatV S8 = V4GetX(sinCoefficients2);
	const FloatV S9 = V4GetY(sinCoefficients2);
	const FloatV S10 = V4GetZ(sinCoefficients2);
	const FloatV S11 = V4GetW(sinCoefficients2);

	FloatV Result;
	Result = FScaleAdd(S1, V3, V1);
	Result = FScaleAdd(S2, V5, Result);
	Result = FScaleAdd(S3, V7, Result);
	Result = FScaleAdd(S4, V9, Result);
	Result = FScaleAdd(S5, V11, Result);
	Result = FScaleAdd(S6, V13, Result);
	Result = FScaleAdd(S7, V15, Result);
	Result = FScaleAdd(S8, V17, Result);
	Result = FScaleAdd(S9, V19, Result);
	Result = FScaleAdd(S10, V21, Result);
	Result = FScaleAdd(S11, V23, Result);

	return Result;
}

PX_FORCE_INLINE FloatV FCos(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);

	// Modulo the range of the given angles such that -XM_2PI <= Angles < XM_2PI
	const FloatV recipTwoPi = V4LoadA(g_PXReciprocalTwoPi.f);
	const FloatV twoPi = V4LoadA(g_PXTwoPi.f);
	const FloatV tmp = FMul(a, recipTwoPi);
	const FloatV b = FRound(tmp);
	const FloatV V1 = FNegScaleSub(twoPi, b, a);

	// cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! + V^8 / 8! - V^10 / 10! + V^12 / 12! -
	//           V^14 / 14! + V^16 / 16! - V^18 / 18! + V^20 / 20! - V^22 / 22! (for -PI <= V < PI)
	const FloatV V2 = FMul(V1, V1);
	const FloatV V4 = FMul(V2, V2);
	const FloatV V6 = FMul(V4, V2);
	const FloatV V8 = FMul(V4, V4);
	const FloatV V10 = FMul(V6, V4);
	const FloatV V12 = FMul(V6, V6);
	const FloatV V14 = FMul(V8, V6);
	const FloatV V16 = FMul(V8, V8);
	const FloatV V18 = FMul(V10, V8);
	const FloatV V20 = FMul(V10, V10);
	const FloatV V22 = FMul(V12, V10);

	const Vec4V cosCoefficients0 = V4LoadA(g_PXCosCoefficients0.f);
	const Vec4V cosCoefficients1 = V4LoadA(g_PXCosCoefficients1.f);
	const Vec4V cosCoefficients2 = V4LoadA(g_PXCosCoefficients2.f);

	const FloatV C1 = V4GetY(cosCoefficients0);
	const FloatV C2 = V4GetZ(cosCoefficients0);
	const FloatV C3 = V4GetW(cosCoefficients0);
	const FloatV C4 = V4GetX(cosCoefficients1);
	const FloatV C5 = V4GetY(cosCoefficients1);
	const FloatV C6 = V4GetZ(cosCoefficients1);
	const FloatV C7 = V4GetW(cosCoefficients1);
	const FloatV C8 = V4GetX(cosCoefficients2);
	const FloatV C9 = V4GetY(cosCoefficients2);
	const FloatV C10 = V4GetZ(cosCoefficients2);
	const FloatV C11 = V4GetW(cosCoefficients2);

	FloatV Result;
	Result = FScaleAdd(C1, V2, V4One());
	Result = FScaleAdd(C2, V4, Result);
	Result = FScaleAdd(C3, V6, Result);
	Result = FScaleAdd(C4, V8, Result);
	Result = FScaleAdd(C5, V10, Result);
	Result = FScaleAdd(C6, V12, Result);
	Result = FScaleAdd(C7, V14, Result);
	Result = FScaleAdd(C8, V16, Result);
	Result = FScaleAdd(C9, V18, Result);
	Result = FScaleAdd(C10, V20, Result);
	Result = FScaleAdd(C11, V22, Result);

	return Result;
}

PX_FORCE_INLINE PxU32 FOutOfBounds(const FloatV a, const FloatV min, const FloatV max)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(min);
	ASSERT_ISVALIDFLOATV(max);
	const BoolV c = BOr(FIsGrtr(a, max), FIsGrtr(min, a));
	return PxU32(!BAllEqFFFF(c));
}

PX_FORCE_INLINE PxU32 FInBounds(const FloatV a, const FloatV min, const FloatV max)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(min);
	ASSERT_ISVALIDFLOATV(max);
	const BoolV c = BAnd(FIsGrtrOrEq(a, min), FIsGrtrOrEq(max, a));
	return BAllEqTTTT(c);
}

PX_FORCE_INLINE PxU32 FOutOfBounds(const FloatV a, const FloatV bounds)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(bounds);
	return FOutOfBounds(a, FNeg(bounds), bounds);
}

PX_FORCE_INLINE PxU32 FInBounds(const FloatV a, const FloatV bounds)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(bounds);
	return FInBounds(a, FNeg(bounds), bounds);
}

//////////////////////////////////
// VEC3V
//////////////////////////////////

PX_FORCE_INLINE Vec3V V3Splat(const FloatV f)
{
	ASSERT_ISVALIDFLOATV(f);
	const __m128 zero = _mm_setzero_ps();
	const __m128 fff0 = _mm_move_ss(f, zero);
	return _mm_shuffle_ps(fff0, fff0, _MM_SHUFFLE(0, 1, 2, 3));
}

PX_FORCE_INLINE Vec3V V3Merge(const FloatVArg x, const FloatVArg y, const FloatVArg z)
{
	ASSERT_ISVALIDFLOATV(x);
	ASSERT_ISVALIDFLOATV(y);
	ASSERT_ISVALIDFLOATV(z);
	// static on zero causes compiler crash on x64 debug_opt
	const __m128 zero = _mm_setzero_ps();
	const __m128 xy = _mm_move_ss(x, y);
	const __m128 z0 = _mm_move_ss(zero, z);

	return _mm_shuffle_ps(xy, z0, _MM_SHUFFLE(1, 0, 0, 1));
}

PX_FORCE_INLINE FloatV V3GetX(const Vec3V f)
{
	ASSERT_ISVALIDVEC3V(f);
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(0, 0, 0, 0));
}

PX_FORCE_INLINE FloatV V3GetY(const Vec3V f)
{
	ASSERT_ISVALIDVEC3V(f);
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(1, 1, 1, 1));
}

PX_FORCE_INLINE FloatV V3GetZ(const Vec3V f)
{
	ASSERT_ISVALIDVEC3V(f);
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(2, 2, 2, 2));
}

PX_FORCE_INLINE Vec3V V3SetX(const Vec3V v, const FloatV f)
{
	ASSERT_ISVALIDVEC3V(v);
	ASSERT_ISVALIDFLOATV(f);
	return V4Sel(BFTTT(), v, f);
}

PX_FORCE_INLINE Vec3V V3SetY(const Vec3V v, const FloatV f)
{
	ASSERT_ISVALIDVEC3V(v);
	ASSERT_ISVALIDFLOATV(f);
	return V4Sel(BTFTT(), v, f);
}

PX_FORCE_INLINE Vec3V V3SetZ(const Vec3V v, const FloatV f)
{
	ASSERT_ISVALIDVEC3V(v);
	ASSERT_ISVALIDFLOATV(f);
	return V4Sel(BTTFT(), v, f);
}

PX_FORCE_INLINE Vec3V V3ColX(const Vec3V a, const Vec3V b, const Vec3V c)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	ASSERT_ISVALIDVEC3V(c);
	const Vec3V r = _mm_shuffle_ps(a, c, _MM_SHUFFLE(3, 0, 3, 0));
	return V3SetY(r, V3GetX(b));
}

PX_FORCE_INLINE Vec3V V3ColY(const Vec3V a, const Vec3V b, const Vec3V c)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	ASSERT_ISVALIDVEC3V(c);
	const Vec3V r = _mm_shuffle_ps(a, c, _MM_SHUFFLE(3, 1, 3, 1));
	return V3SetY(r, V3GetY(b));
}

PX_FORCE_INLINE Vec3V V3ColZ(const Vec3V a, const Vec3V b, const Vec3V c)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	ASSERT_ISVALIDVEC3V(c);
	const Vec3V r = _mm_shuffle_ps(a, c, _MM_SHUFFLE(3, 2, 3, 2));
	return V3SetY(r, V3GetZ(b));
}

PX_FORCE_INLINE Vec3V V3Zero()
{
	return _mm_setzero_ps();
}

PX_FORCE_INLINE Vec3V V3One()
{
	return V3Load(1.0f);
}

PX_FORCE_INLINE Vec3V V3Eps()
{
	return V3Load(PX_EPS_REAL);
}

PX_FORCE_INLINE Vec3V V3Neg(const Vec3V f)
{
	ASSERT_ISVALIDVEC3V(f);
	return _mm_sub_ps(_mm_setzero_ps(), f);
}

PX_FORCE_INLINE Vec3V V3Add(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return _mm_add_ps(a, b);
}

PX_FORCE_INLINE Vec3V V3Sub(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return _mm_sub_ps(a, b);
}

PX_FORCE_INLINE Vec3V V3Scale(const Vec3V a, const FloatV b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_mul_ps(a, b);
}

PX_FORCE_INLINE Vec3V V3Mul(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return _mm_mul_ps(a, b);
}

PX_FORCE_INLINE Vec3V V3ScaleInv(const Vec3V a, const FloatV b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_div_ps(a, b);
}

PX_FORCE_INLINE Vec3V V3Div(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return V4ClearW(_mm_div_ps(a, b));
}

PX_FORCE_INLINE Vec3V V3ScaleInvFast(const Vec3V a, const FloatV b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDFLOATV(b);
	return _mm_mul_ps(a, _mm_rcp_ps(b));
}

PX_FORCE_INLINE Vec3V V3DivFast(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return V4ClearW(_mm_mul_ps(a, _mm_rcp_ps(b)));
}

PX_FORCE_INLINE Vec3V V3Recip(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	const __m128 zero = V3Zero();
	const __m128 tttf = BTTTF();
	const __m128 recipA = _mm_div_ps(V3One(), a);
	return V4Sel(tttf, recipA, zero);
}

PX_FORCE_INLINE Vec3V V3RecipFast(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	const __m128 zero = V3Zero();
	const __m128 tttf = BTTTF();
	const __m128 recipA = _mm_rcp_ps(a);
	return V4Sel(tttf, recipA, zero);
}

PX_FORCE_INLINE Vec3V V3Rsqrt(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	const __m128 zero = V3Zero();
	const __m128 tttf = BTTTF();
	const __m128 recipA = _mm_div_ps(V3One(), _mm_sqrt_ps(a));
	return V4Sel(tttf, recipA, zero);
}

PX_FORCE_INLINE Vec3V V3RsqrtFast(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	const __m128 zero = V3Zero();
	const __m128 tttf = BTTTF();
	const __m128 recipA = _mm_rsqrt_ps(a);
	return V4Sel(tttf, recipA, zero);
}

PX_FORCE_INLINE Vec3V V3ScaleAdd(const Vec3V a, const FloatV b, const Vec3V c)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDFLOATV(b);
	ASSERT_ISVALIDVEC3V(c);
	return V3Add(V3Scale(a, b), c);
}

PX_FORCE_INLINE Vec3V V3NegScaleSub(const Vec3V a, const FloatV b, const Vec3V c)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDFLOATV(b);
	ASSERT_ISVALIDVEC3V(c);
	return V3Sub(c, V3Scale(a, b));
}

PX_FORCE_INLINE Vec3V V3MulAdd(const Vec3V a, const Vec3V b, const Vec3V c)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	ASSERT_ISVALIDVEC3V(c);
	return V3Add(V3Mul(a, b), c);
}

PX_FORCE_INLINE Vec3V V3NegMulSub(const Vec3V a, const Vec3V b, const Vec3V c)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	ASSERT_ISVALIDVEC3V(c);
	return V3Sub(c, V3Mul(a, b));
}

PX_FORCE_INLINE Vec3V V3Abs(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	return V3Max(a, V3Neg(a));
}

PX_FORCE_INLINE FloatV V3Dot(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
#ifdef __SSE4_2__
	return _mm_dp_ps(a, b, 0x7f);
#else
	const __m128 t0 = _mm_mul_ps(a, b);								//	aw*bw | az*bz | ay*by | ax*bx
	const __m128 t1 = _mm_shuffle_ps(t0, t0, _MM_SHUFFLE(1,0,3,2));	//	ay*by | ax*bx | aw*bw | az*bz
	const __m128 t2 = _mm_add_ps(t0, t1);							//	ay*by + aw*bw | ax*bx + az*bz | aw*bw + ay*by | az*bz + ax*bx
	const __m128 t3 = _mm_shuffle_ps(t2, t2, _MM_SHUFFLE(2,3,0,1));	//	ax*bx + az*bz | ay*by + aw*bw | az*bz + ax*bx | aw*bw + ay*by
	return _mm_add_ps(t3, t2);										//	ax*bx + az*bz + ay*by + aw*bw 
																	//	ay*by + aw*bw + ax*bx + az*bz
																	//	az*bz + ax*bx + aw*bw + ay*by
																	//	aw*bw + ay*by + az*bz + ax*bx
#endif
}

PX_FORCE_INLINE Vec3V V3Cross(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
/*	if(0)
	{
		const __m128 r1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2)); // z,x,y,w
		const __m128 r2 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1)); // y,z,x,w
		const __m128 l1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)); // y,z,x,w
		const __m128 l2 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2)); // z,x,y,w
		return _mm_sub_ps(_mm_mul_ps(l1, l2), _mm_mul_ps(r1, r2));
	}
	else*/
	{
		const __m128 b0 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3,0,2,1));
		const __m128 a1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3,0,2,1));
		__m128 v = _mm_mul_ps(a1, b);
		v = _mm_sub_ps(_mm_mul_ps(a, b0), v);
		__m128 res = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3,0,2,1));
		ASSERT_ISVALIDVEC3V(res);
		return res;
	}
}

PX_FORCE_INLINE VecCrossV V3PrepareCross(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	VecCrossV v;
	v.mR1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2)); // z,x,y,w
	v.mL1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)); // y,z,x,w
	return v;
}

PX_FORCE_INLINE Vec3V V3Cross(const VecCrossV& a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(b);
	const __m128 r2 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1)); // y,z,x,w
	const __m128 l2 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2)); // z,x,y,w
	return _mm_sub_ps(_mm_mul_ps(a.mL1, l2), _mm_mul_ps(a.mR1, r2));
}

PX_FORCE_INLINE Vec3V V3Cross(const Vec3V a, const VecCrossV& b)
{
	ASSERT_ISVALIDVEC3V(a);
	const __m128 r2 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)); // y,z,x,w
	const __m128 l2 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2)); // z,x,y,w
	return _mm_sub_ps(_mm_mul_ps(b.mR1, r2), _mm_mul_ps(b.mL1, l2));
}

PX_FORCE_INLINE Vec3V V3Cross(const VecCrossV& a, const VecCrossV& b)
{
	return _mm_sub_ps(_mm_mul_ps(a.mL1, b.mR1), _mm_mul_ps(a.mR1, b.mL1));
}

PX_FORCE_INLINE FloatV V3Length(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	return _mm_sqrt_ps(V3Dot(a, a));
}

PX_FORCE_INLINE FloatV V3LengthSq(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	return V3Dot(a, a);
}

PX_FORCE_INLINE Vec3V V3Normalize(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISFINITELENGTH(a);
	return V3ScaleInv(a, _mm_sqrt_ps(V3Dot(a, a)));
}

PX_FORCE_INLINE Vec3V V3NormalizeFast(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISFINITELENGTH(a);
	return V3Scale(a, _mm_rsqrt_ps(V3Dot(a, a)));
}

PX_FORCE_INLINE Vec3V V3NormalizeSafe(const Vec3V a, const Vec3V unsafeReturnValue)
{
	ASSERT_ISVALIDVEC3V(a);
	const __m128 eps = FEps();
	const __m128 length = V3Length(a);
	const __m128 isGreaterThanZero = FIsGrtr(length, eps);
	return V3Sel(isGreaterThanZero, V3ScaleInv(a, length), unsafeReturnValue);
}

PX_FORCE_INLINE Vec3V V3Sel(const BoolV c, const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(_mm_or_ps(_mm_andnot_ps(c, b), _mm_and_ps(c, a)));
	return _mm_or_ps(_mm_andnot_ps(c, b), _mm_and_ps(c, a));
}

PX_FORCE_INLINE BoolV V3IsGrtr(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return _mm_cmpgt_ps(a, b);
}

PX_FORCE_INLINE BoolV V3IsGrtrOrEq(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return _mm_cmpge_ps(a, b);
}

PX_FORCE_INLINE BoolV V3IsEq(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return _mm_cmpeq_ps(a, b);
}

PX_FORCE_INLINE Vec3V V3Max(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return _mm_max_ps(a, b);
}

PX_FORCE_INLINE Vec3V V3Min(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return _mm_min_ps(a, b);
}

PX_FORCE_INLINE FloatV V3ExtractMax(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	const __m128 shuf1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 0));
	const __m128 shuf2 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1));
	const __m128 shuf3 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2));
	return _mm_max_ps(_mm_max_ps(shuf1, shuf2), shuf3);
}

PX_FORCE_INLINE FloatV V3ExtractMin(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	const __m128 shuf1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 0));
	const __m128 shuf2 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1));
	const __m128 shuf3 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2));
	return _mm_min_ps(_mm_min_ps(shuf1, shuf2), shuf3);
}

//// if(a > 0.0f) return 1.0f; else if a == 0.f return 0.f, else return -1.f;
// PX_FORCE_INLINE Vec3V V3MathSign(const Vec3V a)
//{
//	VECMATHAOS_ASSERT(isValidVec3V(a));
//
//	const __m128i ai = _mm_cvtps_epi32(a);
//	const __m128i bi = _mm_cvtps_epi32(V3Neg(a));
//	const __m128  aa = _mm_cvtepi32_ps(_mm_srai_epi32(ai, 31));
//	const __m128  bb = _mm_cvtepi32_ps(_mm_srai_epi32(bi, 31));
//	return _mm_or_ps(aa, bb);
//}

// return (a >= 0.0f) ? 1.0f : -1.0f;
PX_FORCE_INLINE Vec3V V3Sign(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	const __m128 zero = V3Zero();
	const __m128 one = V3One();
	const __m128 none = V3Neg(one);
	return V3Sel(V3IsGrtrOrEq(a, zero), one, none);
}

PX_FORCE_INLINE Vec3V V3Clamp(const Vec3V a, const Vec3V minV, const Vec3V maxV)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(minV);
	ASSERT_ISVALIDVEC3V(maxV);
	return V3Max(V3Min(a, maxV), minV);
}

PX_FORCE_INLINE PxU32 V3AllGrtr(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return internalSimd::BAllTrue3_R(V4IsGrtr(a, b));
}

PX_FORCE_INLINE PxU32 V3AllGrtrOrEq(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return internalSimd::BAllTrue3_R(V4IsGrtrOrEq(a, b));
}

PX_FORCE_INLINE PxU32 V3AllEq(const Vec3V a, const Vec3V b)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(b);
	return internalSimd::BAllTrue3_R(V4IsEq(a, b));
}

PX_FORCE_INLINE Vec3V V3Round(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
#ifdef __SSE4_2__
	return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
#else
	// return _mm_round_ps(a, 0x0);
	const Vec3V half = V3Load(0.5f);
	const __m128 signBit = _mm_cvtepi32_ps(_mm_srli_epi32(_mm_cvtps_epi32(a), 31));
	const Vec3V aRound = V3Sub(V3Add(a, half), signBit);
	__m128i tmp = _mm_cvttps_epi32(aRound);
	return _mm_cvtepi32_ps(tmp);
#endif
}

PX_FORCE_INLINE Vec3V V3Sin(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);

	// Modulo the range of the given angles such that -XM_2PI <= Angles < XM_2PI
	const Vec4V recipTwoPi = V4LoadA(g_PXReciprocalTwoPi.f);
	const Vec4V twoPi = V4LoadA(g_PXTwoPi.f);
	const Vec3V tmp = V3Scale(a, recipTwoPi);
	const Vec3V b = V3Round(tmp);
	const Vec3V V1 = V3NegScaleSub(b, twoPi, a);

	// sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! + V^9 / 9! - V^11 / 11! + V^13 / 13! -
	//           V^15 / 15! + V^17 / 17! - V^19 / 19! + V^21 / 21! - V^23 / 23! (for -PI <= V < PI)
	const Vec3V V2 = V3Mul(V1, V1);
	const Vec3V V3 = V3Mul(V2, V1);
	const Vec3V V5 = V3Mul(V3, V2);
	const Vec3V V7 = V3Mul(V5, V2);
	const Vec3V V9 = V3Mul(V7, V2);
	const Vec3V V11 = V3Mul(V9, V2);
	const Vec3V V13 = V3Mul(V11, V2);
	const Vec3V V15 = V3Mul(V13, V2);
	const Vec3V V17 = V3Mul(V15, V2);
	const Vec3V V19 = V3Mul(V17, V2);
	const Vec3V V21 = V3Mul(V19, V2);
	const Vec3V V23 = V3Mul(V21, V2);

	const Vec4V sinCoefficients0 = V4LoadA(g_PXSinCoefficients0.f);
	const Vec4V sinCoefficients1 = V4LoadA(g_PXSinCoefficients1.f);
	const Vec4V sinCoefficients2 = V4LoadA(g_PXSinCoefficients2.f);

	const FloatV S1 = V4GetY(sinCoefficients0);
	const FloatV S2 = V4GetZ(sinCoefficients0);
	const FloatV S3 = V4GetW(sinCoefficients0);
	const FloatV S4 = V4GetX(sinCoefficients1);
	const FloatV S5 = V4GetY(sinCoefficients1);
	const FloatV S6 = V4GetZ(sinCoefficients1);
	const FloatV S7 = V4GetW(sinCoefficients1);
	const FloatV S8 = V4GetX(sinCoefficients2);
	const FloatV S9 = V4GetY(sinCoefficients2);
	const FloatV S10 = V4GetZ(sinCoefficients2);
	const FloatV S11 = V4GetW(sinCoefficients2);

	Vec3V Result;
	Result = V3ScaleAdd(V3, S1, V1);
	Result = V3ScaleAdd(V5, S2, Result);
	Result = V3ScaleAdd(V7, S3, Result);
	Result = V3ScaleAdd(V9, S4, Result);
	Result = V3ScaleAdd(V11, S5, Result);
	Result = V3ScaleAdd(V13, S6, Result);
	Result = V3ScaleAdd(V15, S7, Result);
	Result = V3ScaleAdd(V17, S8, Result);
	Result = V3ScaleAdd(V19, S9, Result);
	Result = V3ScaleAdd(V21, S10, Result);
	Result = V3ScaleAdd(V23, S11, Result);

	ASSERT_ISVALIDVEC3V(Result);
	return Result;
}

PX_FORCE_INLINE Vec3V V3Cos(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);

	// Modulo the range of the given angles such that -XM_2PI <= Angles < XM_2PI
	const Vec4V recipTwoPi = V4LoadA(g_PXReciprocalTwoPi.f);
	const Vec4V twoPi = V4LoadA(g_PXTwoPi.f);
	const Vec3V tmp = V3Scale(a, recipTwoPi);
	const Vec3V b = V3Round(tmp);
	const Vec3V V1 = V3NegScaleSub(b, twoPi, a);

	// cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! + V^8 / 8! - V^10 / 10! + V^12 / 12! -
	//           V^14 / 14! + V^16 / 16! - V^18 / 18! + V^20 / 20! - V^22 / 22! (for -PI <= V < PI)
	const Vec3V V2 = V3Mul(V1, V1);
	const Vec3V V4 = V3Mul(V2, V2);
	const Vec3V V6 = V3Mul(V4, V2);
	const Vec3V V8 = V3Mul(V4, V4);
	const Vec3V V10 = V3Mul(V6, V4);
	const Vec3V V12 = V3Mul(V6, V6);
	const Vec3V V14 = V3Mul(V8, V6);
	const Vec3V V16 = V3Mul(V8, V8);
	const Vec3V V18 = V3Mul(V10, V8);
	const Vec3V V20 = V3Mul(V10, V10);
	const Vec3V V22 = V3Mul(V12, V10);

	const Vec4V cosCoefficients0 = V4LoadA(g_PXCosCoefficients0.f);
	const Vec4V cosCoefficients1 = V4LoadA(g_PXCosCoefficients1.f);
	const Vec4V cosCoefficients2 = V4LoadA(g_PXCosCoefficients2.f);

	const FloatV C1 = V4GetY(cosCoefficients0);
	const FloatV C2 = V4GetZ(cosCoefficients0);
	const FloatV C3 = V4GetW(cosCoefficients0);
	const FloatV C4 = V4GetX(cosCoefficients1);
	const FloatV C5 = V4GetY(cosCoefficients1);
	const FloatV C6 = V4GetZ(cosCoefficients1);
	const FloatV C7 = V4GetW(cosCoefficients1);
	const FloatV C8 = V4GetX(cosCoefficients2);
	const FloatV C9 = V4GetY(cosCoefficients2);
	const FloatV C10 = V4GetZ(cosCoefficients2);
	const FloatV C11 = V4GetW(cosCoefficients2);

	Vec3V Result;
	Result = V3ScaleAdd(V2, C1, V3One());
	Result = V3ScaleAdd(V4, C2, Result);
	Result = V3ScaleAdd(V6, C3, Result);
	Result = V3ScaleAdd(V8, C4, Result);
	Result = V3ScaleAdd(V10, C5, Result);
	Result = V3ScaleAdd(V12, C6, Result);
	Result = V3ScaleAdd(V14, C7, Result);
	Result = V3ScaleAdd(V16, C8, Result);
	Result = V3ScaleAdd(V18, C9, Result);
	Result = V3ScaleAdd(V20, C10, Result);
	Result = V3ScaleAdd(V22, C11, Result);

	ASSERT_ISVALIDVEC3V(Result); 
	return Result;
}

PX_FORCE_INLINE Vec3V V3PermYZZ(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 2, 2, 1));
}

PX_FORCE_INLINE Vec3V V3PermXYX(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 1, 0));
}

PX_FORCE_INLINE Vec3V V3PermYZX(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
}

PX_FORCE_INLINE Vec3V V3PermZXY(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2));
}

PX_FORCE_INLINE Vec3V V3PermZZY(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 2, 2));
}

PX_FORCE_INLINE Vec3V V3PermYXX(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 0, 1));
}

PX_FORCE_INLINE Vec3V V3Perm_Zero_1Z_0Y(const Vec3V v0, const Vec3V v1)
{
	ASSERT_ISVALIDVEC3V(v0);
	ASSERT_ISVALIDVEC3V(v1);
	return _mm_shuffle_ps(v1, v0, _MM_SHUFFLE(3, 1, 2, 3));
}

PX_FORCE_INLINE Vec3V V3Perm_0Z_Zero_1X(const Vec3V v0, const Vec3V v1)
{
	ASSERT_ISVALIDVEC3V(v0);
	ASSERT_ISVALIDVEC3V(v1);
	return _mm_shuffle_ps(v0, v1, _MM_SHUFFLE(3, 0, 3, 2));
}

PX_FORCE_INLINE Vec3V V3Perm_1Y_0X_Zero(const Vec3V v0, const Vec3V v1)
{
	ASSERT_ISVALIDVEC3V(v0);
	ASSERT_ISVALIDVEC3V(v1);
	// There must be a better way to do this.
	Vec3V v2 = V3Zero();
	const FloatV y1 = V3GetY(v1);
	const FloatV x0 = V3GetX(v0);
	v2 = V3SetX(v2, y1);
	return V3SetY(v2, x0);
}

PX_FORCE_INLINE FloatV V3SumElems(const Vec3V a)
{
	ASSERT_ISVALIDVEC3V(a);
#ifdef __SSE4_2__
	Vec3V r = _mm_hadd_ps(a, a);
	r = _mm_hadd_ps(r, r);
	return r;
#else
	const __m128 shuf1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 0)); // z,y,x,w
	const __m128 shuf2 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1)); // y,x,w,z
	const __m128 shuf3 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2)); // x,w,z,y
	return _mm_add_ps(_mm_add_ps(shuf1, shuf2), shuf3);
#endif
}

PX_FORCE_INLINE PxU32 V3OutOfBounds(const Vec3V a, const Vec3V min, const Vec3V max)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(min);
	ASSERT_ISVALIDVEC3V(max);
	const BoolV c = BOr(V3IsGrtr(a, max), V3IsGrtr(min, a));
	return PxU32(!BAllEqFFFF(c));
}

PX_FORCE_INLINE PxU32 V3InBounds(const Vec3V a, const Vec3V min, const Vec3V max)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(min);
	ASSERT_ISVALIDVEC3V(max);
	const BoolV c = BAnd(V3IsGrtrOrEq(a, min), V3IsGrtrOrEq(max, a));
	return BAllEqTTTT(c);
}

PX_FORCE_INLINE PxU32 V3OutOfBounds(const Vec3V a, const Vec3V bounds)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(bounds);
	return V3OutOfBounds(a, V3Neg(bounds), bounds);
}

PX_FORCE_INLINE PxU32 V3InBounds(const Vec3V a, const Vec3V bounds)
{
	ASSERT_ISVALIDVEC3V(a);
	ASSERT_ISVALIDVEC3V(bounds);
	return V3InBounds(a, V3Neg(bounds), bounds);
}

PX_FORCE_INLINE void V3Transpose(Vec3V& col0, Vec3V& col1, Vec3V& col2)
{
	ASSERT_ISVALIDVEC3V(col0);
	ASSERT_ISVALIDVEC3V(col1);
	ASSERT_ISVALIDVEC3V(col2);
	const Vec3V col3 = _mm_setzero_ps();
	const Vec3V tmp0 = _mm_unpacklo_ps(col0, col1);
	const Vec3V tmp2 = _mm_unpacklo_ps(col2, col3);
	const Vec3V tmp1 = _mm_unpackhi_ps(col0, col1);
	const Vec3V tmp3 = _mm_unpackhi_ps(col2, col3);
	col0 = _mm_movelh_ps(tmp0, tmp2);
	col1 = _mm_movehl_ps(tmp2, tmp0);
	col2 = _mm_movelh_ps(tmp1, tmp3);
}

//////////////////////////////////
// VEC4V
//////////////////////////////////

PX_FORCE_INLINE Vec4V V4Splat(const FloatV f)
{
	ASSERT_ISVALIDFLOATV(f);
	// return _mm_shuffle_ps(f, f, _MM_SHUFFLE(0,0,0,0));
	return f;
}

PX_FORCE_INLINE Vec4V V4Merge(const FloatV* const floatVArray)
{
	ASSERT_ISVALIDFLOATV(floatVArray[0]);
	ASSERT_ISVALIDFLOATV(floatVArray[1]);
	ASSERT_ISVALIDFLOATV(floatVArray[2]);
	ASSERT_ISVALIDFLOATV(floatVArray[3]);
	const __m128 xw = _mm_move_ss(floatVArray[1], floatVArray[0]); // y, y, y, x
	const __m128 yz = _mm_move_ss(floatVArray[2], floatVArray[3]); // z, z, z, w
	return _mm_shuffle_ps(xw, yz, _MM_SHUFFLE(0, 2, 1, 0));
}

PX_FORCE_INLINE Vec4V V4Merge(const FloatVArg x, const FloatVArg y, const FloatVArg z, const FloatVArg w)
{
	ASSERT_ISVALIDFLOATV(x);
	ASSERT_ISVALIDFLOATV(y);
	ASSERT_ISVALIDFLOATV(z);
	ASSERT_ISVALIDFLOATV(w);
	const __m128 xw = _mm_move_ss(y, x); // y, y, y, x
	const __m128 yz = _mm_move_ss(z, w); // z, z, z, w
	return _mm_shuffle_ps(xw, yz, _MM_SHUFFLE(0, 2, 1, 0));
}

PX_FORCE_INLINE Vec4V V4MergeW(const Vec4VArg x, const Vec4VArg y, const Vec4VArg z, const Vec4VArg w)
{
	const Vec4V xz = _mm_unpackhi_ps(x, z);
	const Vec4V yw = _mm_unpackhi_ps(y, w);
	return _mm_unpackhi_ps(xz, yw);
}

PX_FORCE_INLINE Vec4V V4MergeZ(const Vec4VArg x, const Vec4VArg y, const Vec4VArg z, const Vec4VArg w)
{
	const Vec4V xz = _mm_unpackhi_ps(x, z);
	const Vec4V yw = _mm_unpackhi_ps(y, w);
	return _mm_unpacklo_ps(xz, yw);
}

PX_FORCE_INLINE Vec4V V4MergeY(const Vec4VArg x, const Vec4VArg y, const Vec4VArg z, const Vec4VArg w)
{
	const Vec4V xz = _mm_unpacklo_ps(x, z);
	const Vec4V yw = _mm_unpacklo_ps(y, w);
	return _mm_unpackhi_ps(xz, yw);
}

PX_FORCE_INLINE Vec4V V4MergeX(const Vec4VArg x, const Vec4VArg y, const Vec4VArg z, const Vec4VArg w)
{
	const Vec4V xz = _mm_unpacklo_ps(x, z);
	const Vec4V yw = _mm_unpacklo_ps(y, w);
	return _mm_unpacklo_ps(xz, yw);
}

PX_FORCE_INLINE Vec4V V4UnpackXY(const Vec4VArg a, const Vec4VArg b)
{
	return _mm_unpacklo_ps(a, b);
}

PX_FORCE_INLINE Vec4V V4UnpackZW(const Vec4VArg a, const Vec4VArg b)
{
	return _mm_unpackhi_ps(a, b);
}

PX_FORCE_INLINE Vec4V V4PermYXWZ(const Vec4V a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1));
}

PX_FORCE_INLINE Vec4V V4PermXZXZ(const Vec4V a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 0, 2, 0));
}

PX_FORCE_INLINE Vec4V V4PermYWYW(const Vec4V a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 3, 1));
}

PX_FORCE_INLINE Vec4V V4PermYZXW(const Vec4V a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
}

PX_FORCE_INLINE Vec4V V4PermZWXY(const Vec4V a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 0, 3, 2));
}

template <PxU8 x, PxU8 y, PxU8 z, PxU8 w>
PX_FORCE_INLINE Vec4V V4Perm(const Vec4V a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(w, z, y, x));
}

PX_FORCE_INLINE FloatV V4GetW(const Vec4V f)
{
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(3, 3, 3, 3));
}

PX_FORCE_INLINE FloatV V4GetX(const Vec4V f)
{
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(0, 0, 0, 0));
}

PX_FORCE_INLINE FloatV V4GetY(const Vec4V f)
{
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(1, 1, 1, 1));
}

PX_FORCE_INLINE FloatV V4GetZ(const Vec4V f)
{
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(2, 2, 2, 2));
}

PX_FORCE_INLINE Vec4V V4SetW(const Vec4V v, const FloatV f)
{
	ASSERT_ISVALIDFLOATV(f);
	return V4Sel(BTTTF(), v, f);
}

PX_FORCE_INLINE Vec4V V4SetX(const Vec4V v, const FloatV f)
{
	ASSERT_ISVALIDFLOATV(f);
	return V4Sel(BFTTT(), v, f);
}

PX_FORCE_INLINE Vec4V V4SetY(const Vec4V v, const FloatV f)
{
	ASSERT_ISVALIDFLOATV(f);
	return V4Sel(BTFTT(), v, f);
}

PX_FORCE_INLINE Vec4V V4SetZ(const Vec4V v, const FloatV f)
{
	ASSERT_ISVALIDFLOATV(f);
	return V4Sel(BTTFT(), v, f);
}

PX_FORCE_INLINE Vec4V V4Zero()
{
	return _mm_setzero_ps();
}

PX_FORCE_INLINE Vec4V V4One()
{
	return V4Load(1.0f);
}

PX_FORCE_INLINE Vec4V V4Eps()
{
	return V4Load(PX_EPS_REAL);
}

PX_FORCE_INLINE Vec4V V4Neg(const Vec4V f)
{
	return _mm_sub_ps(_mm_setzero_ps(), f);
}

PX_FORCE_INLINE Vec4V V4Add(const Vec4V a, const Vec4V b)
{
	return _mm_add_ps(a, b);
}

PX_FORCE_INLINE Vec4V V4Sub(const Vec4V a, const Vec4V b)
{
	return _mm_sub_ps(a, b);
}

PX_FORCE_INLINE Vec4V V4Scale(const Vec4V a, const FloatV b)
{
	return _mm_mul_ps(a, b);
}

PX_FORCE_INLINE Vec4V V4Mul(const Vec4V a, const Vec4V b)
{
	return _mm_mul_ps(a, b);
}

PX_FORCE_INLINE Vec4V V4ScaleInv(const Vec4V a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(b);
	return _mm_div_ps(a, b);
}

PX_FORCE_INLINE Vec4V V4Div(const Vec4V a, const Vec4V b)
{
	return _mm_div_ps(a, b);
}

PX_FORCE_INLINE Vec4V V4ScaleInvFast(const Vec4V a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(b);
	return _mm_mul_ps(a, _mm_rcp_ps(b));
}

PX_FORCE_INLINE Vec4V V4DivFast(const Vec4V a, const Vec4V b)
{
	return _mm_mul_ps(a, _mm_rcp_ps(b));
}

PX_FORCE_INLINE Vec4V V4Recip(const Vec4V a)
{
	return _mm_div_ps(V4One(), a);
}

PX_FORCE_INLINE Vec4V V4RecipFast(const Vec4V a)
{
	return _mm_rcp_ps(a);
}

PX_FORCE_INLINE Vec4V V4Rsqrt(const Vec4V a)
{
	return _mm_div_ps(V4One(), _mm_sqrt_ps(a));
}

PX_FORCE_INLINE Vec4V V4RsqrtFast(const Vec4V a)
{
	return _mm_rsqrt_ps(a);
}

PX_FORCE_INLINE Vec4V V4Sqrt(const Vec4V a)
{
	return _mm_sqrt_ps(a);
}

PX_FORCE_INLINE Vec4V V4ScaleAdd(const Vec4V a, const FloatV b, const Vec4V c)
{
	ASSERT_ISVALIDFLOATV(b);
	return V4Add(V4Scale(a, b), c);
}

PX_FORCE_INLINE Vec4V V4NegScaleSub(const Vec4V a, const FloatV b, const Vec4V c)
{
	ASSERT_ISVALIDFLOATV(b);
	return V4Sub(c, V4Scale(a, b));
}

PX_FORCE_INLINE Vec4V V4MulAdd(const Vec4V a, const Vec4V b, const Vec4V c)
{
	return V4Add(V4Mul(a, b), c);
}

PX_FORCE_INLINE Vec4V V4NegMulSub(const Vec4V a, const Vec4V b, const Vec4V c)
{
	return V4Sub(c, V4Mul(a, b));
}

PX_FORCE_INLINE Vec4V V4Abs(const Vec4V a)
{
	return V4Max(a, V4Neg(a));
}

PX_FORCE_INLINE FloatV V4SumElements(const Vec4V a)
{
#ifdef __SSE4_2__
	Vec4V r = _mm_hadd_ps(a, a);
	r = _mm_hadd_ps(r, r);
	return r;
#else
	const Vec4V xy = V4UnpackXY(a, a);	// x,x,y,y
	const Vec4V zw = V4UnpackZW(a, a);	// z,z,w,w
	const Vec4V xz_yw = V4Add(xy, zw);	// x+z,x+z,y+w,y+w
	const FloatV xz = V4GetX(xz_yw);	// x+z
	const FloatV yw = V4GetZ(xz_yw);	// y+w
	return FAdd(xz, yw);				// sum
#endif
}

PX_FORCE_INLINE FloatV V4Dot(const Vec4V a, const Vec4V b)
{
#ifdef __SSE4_2__
	return _mm_dp_ps(a, b, 0xff);
#else
	//const __m128 dot1 = _mm_mul_ps(a, b);                                     // x,y,z,w
	//const __m128 shuf1 = _mm_shuffle_ps(dot1, dot1, _MM_SHUFFLE(2, 1, 0, 3)); // w,x,y,z
	//const __m128 shuf2 = _mm_shuffle_ps(dot1, dot1, _MM_SHUFFLE(1, 0, 3, 2)); // z,w,x,y
	//const __m128 shuf3 = _mm_shuffle_ps(dot1, dot1, _MM_SHUFFLE(0, 3, 2, 1)); // y,z,w,x
	//return _mm_add_ps(_mm_add_ps(shuf2, shuf3), _mm_add_ps(dot1, shuf1));

	// PT: this version has two less instructions but we should check its accuracy
	// aw*bw | az*bz | ay*by | ax*bx
	const __m128 t0 = _mm_mul_ps(a, b);
	// ay*by | ax*bx | aw*bw | az*bz
	const __m128 t1 = _mm_shuffle_ps(t0, t0, _MM_SHUFFLE(1, 0, 3, 2));
	// ay*by + aw*bw | ax*bx + az*bz | aw*bw + ay*by | az*bz + ax*bx
	const __m128 t2 = _mm_add_ps(t0, t1);
	// ax*bx + az*bz | ay*by + aw*bw | az*bz + ax*bx | aw*bw + ay*by
	const __m128 t3 = _mm_shuffle_ps(t2, t2, _MM_SHUFFLE(2, 3, 0, 1));
	// ax*bx + az*bz + ay*by + aw*bw
	return _mm_add_ps(t3, t2);
	// ay*by + aw*bw + ax*bx + az*bz
	// az*bz + ax*bx + aw*bw + ay*by
	// aw*bw + ay*by + az*bz + ax*bx
#endif
}

PX_FORCE_INLINE FloatV V4Dot3(const Vec4V a, const Vec4V b)
{
#ifdef __SSE4_2__
	return _mm_dp_ps(a, b, 0x7f);
#else
	const __m128 dot1 = _mm_mul_ps(a, b);                                     // aw*bw | az*bz | ay*by | ax*bx
	const __m128 shuf1 = _mm_shuffle_ps(dot1, dot1, _MM_SHUFFLE(0, 0, 0, 0)); // ax*bx | ax*bx | ax*bx | ax*bx
	const __m128 shuf2 = _mm_shuffle_ps(dot1, dot1, _MM_SHUFFLE(1, 1, 1, 1)); // ay*by | ay*by | ay*by | ay*by
	const __m128 shuf3 = _mm_shuffle_ps(dot1, dot1, _MM_SHUFFLE(2, 2, 2, 2)); // az*bz | az*bz | az*bz | az*bz
	return _mm_add_ps(_mm_add_ps(shuf1, shuf2), shuf3);                       // ax*bx + ay*by + az*bz in each component
#endif
}

PX_FORCE_INLINE Vec4V V4Cross(const Vec4V a, const Vec4V b)
{
/*	if(0)
	{
		const __m128 r1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2)); // z,x,y,w
		const __m128 r2 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1)); // y,z,x,w
		const __m128 l1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1)); // y,z,x,w
		const __m128 l2 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2)); // z,x,y,w
		return _mm_sub_ps(_mm_mul_ps(l1, l2), _mm_mul_ps(r1, r2));
	}
	else*/
	{
		const __m128 b0 = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3,0,2,1));
		const __m128 a1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3,0,2,1));
		__m128 v = _mm_mul_ps(a1, b);
		v = _mm_sub_ps(_mm_mul_ps(a, b0), v);
		return _mm_shuffle_ps(v, v, _MM_SHUFFLE(3,0,2,1));
	}
}

PX_FORCE_INLINE FloatV V4Length(const Vec4V a)
{
	return _mm_sqrt_ps(V4Dot(a, a));
}

PX_FORCE_INLINE FloatV V4LengthSq(const Vec4V a)
{
	return V4Dot(a, a);
}

PX_FORCE_INLINE Vec4V V4Normalize(const Vec4V a)
{
	ASSERT_ISFINITELENGTH(a);
	return V4ScaleInv(a, _mm_sqrt_ps(V4Dot(a, a)));
}

PX_FORCE_INLINE Vec4V V4NormalizeFast(const Vec4V a)
{
	ASSERT_ISFINITELENGTH(a);
	return V4ScaleInvFast(a, _mm_sqrt_ps(V4Dot(a, a)));
}

PX_FORCE_INLINE Vec4V V4NormalizeSafe(const Vec4V a, const Vec4V unsafeReturnValue)
{
	const __m128 eps = V3Eps();
	const __m128 length = V4Length(a);
	const __m128 isGreaterThanZero = V4IsGrtr(length, eps);
	return V4Sel(isGreaterThanZero, V4ScaleInv(a, length), unsafeReturnValue);
}

PX_FORCE_INLINE Vec4V V4Sel(const BoolV c, const Vec4V a, const Vec4V b)
{
	return _mm_or_ps(_mm_andnot_ps(c, b), _mm_and_ps(c, a));
}

PX_FORCE_INLINE BoolV V4IsGrtr(const Vec4V a, const Vec4V b)
{
	return _mm_cmpgt_ps(a, b);
}

PX_FORCE_INLINE BoolV V4IsGrtrOrEq(const Vec4V a, const Vec4V b)
{
	return _mm_cmpge_ps(a, b);
}

PX_FORCE_INLINE BoolV V4IsEq(const Vec4V a, const Vec4V b)
{
	return _mm_cmpeq_ps(a, b);
}

PX_FORCE_INLINE Vec4V V4Max(const Vec4V a, const Vec4V b)
{
	return _mm_max_ps(a, b);
}

PX_FORCE_INLINE Vec4V V4Min(const Vec4V a, const Vec4V b)
{
	return _mm_min_ps(a, b);
}

PX_FORCE_INLINE FloatV V4ExtractMax(const Vec4V a)
{
	const __m128 shuf1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 1, 0, 3));
	const __m128 shuf2 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 0, 3, 2));
	const __m128 shuf3 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 3, 2, 1));

	return _mm_max_ps(_mm_max_ps(a, shuf1), _mm_max_ps(shuf2, shuf3));
}

PX_FORCE_INLINE FloatV V4ExtractMin(const Vec4V a)
{
	const __m128 shuf1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 1, 0, 3));
	const __m128 shuf2 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 0, 3, 2));
	const __m128 shuf3 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 3, 2, 1));

	return _mm_min_ps(_mm_min_ps(a, shuf1), _mm_min_ps(shuf2, shuf3));
}

PX_FORCE_INLINE Vec4V V4Clamp(const Vec4V a, const Vec4V minV, const Vec4V maxV)
{
	return V4Max(V4Min(a, maxV), minV);
}

PX_FORCE_INLINE PxU32 V4AllGrtr(const Vec4V a, const Vec4V b)
{
	return internalSimd::BAllTrue4_R(V4IsGrtr(a, b));
}

PX_FORCE_INLINE PxU32 V4AllGrtrOrEq(const Vec4V a, const Vec4V b)
{
	return internalSimd::BAllTrue4_R(V4IsGrtrOrEq(a, b));
}

PX_FORCE_INLINE PxU32 V4AllGrtrOrEq3(const Vec4V a, const Vec4V b)
{
	return internalSimd::BAllTrue3_R(V4IsGrtrOrEq(a, b));
}

PX_FORCE_INLINE PxU32 V4AllEq(const Vec4V a, const Vec4V b)
{
	return internalSimd::BAllTrue4_R(V4IsEq(a, b));
}

PX_FORCE_INLINE PxU32 V4AnyGrtr3(const Vec4V a, const Vec4V b)
{
	return internalSimd::BAnyTrue3_R(V4IsGrtr(a, b));
}

PX_FORCE_INLINE Vec4V V4Round(const Vec4V a)
{
#ifdef __SSE4_2__
	return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
#else
	// return _mm_round_ps(a, 0x0);
	const Vec4V half = V4Load(0.5f);
	const __m128 signBit = _mm_cvtepi32_ps(_mm_srli_epi32(_mm_cvtps_epi32(a), 31));
	const Vec4V aRound = V4Sub(V4Add(a, half), signBit);
	const __m128i tmp = _mm_cvttps_epi32(aRound);
	return _mm_cvtepi32_ps(tmp);
#endif
}

PX_FORCE_INLINE Vec4V V4Sin(const Vec4V a)
{
	// PT: TODO: these should be FLoads
	const Vec4V recipTwoPi = V4LoadA(g_PXReciprocalTwoPi.f);
	const Vec4V twoPi = V4LoadA(g_PXTwoPi.f);
	const Vec4V tmp = V4Mul(a, recipTwoPi);
	const Vec4V b = V4Round(tmp);
	const Vec4V V1 = V4NegMulSub(twoPi, b, a);

	// sin(V) ~= V - V^3 / 3! + V^5 / 5! - V^7 / 7! + V^9 / 9! - V^11 / 11! + V^13 / 13! -
	//           V^15 / 15! + V^17 / 17! - V^19 / 19! + V^21 / 21! - V^23 / 23! (for -PI <= V < PI)
	const Vec4V V2 = V4Mul(V1, V1);
	const Vec4V V3 = V4Mul(V2, V1);
	const Vec4V V5 = V4Mul(V3, V2);
	const Vec4V V7 = V4Mul(V5, V2);
	const Vec4V V9 = V4Mul(V7, V2);
	const Vec4V V11 = V4Mul(V9, V2);
	const Vec4V V13 = V4Mul(V11, V2);
	const Vec4V V15 = V4Mul(V13, V2);
	const Vec4V V17 = V4Mul(V15, V2);
	const Vec4V V19 = V4Mul(V17, V2);
	const Vec4V V21 = V4Mul(V19, V2);
	const Vec4V V23 = V4Mul(V21, V2);

	const Vec4V sinCoefficients0 = V4LoadA(g_PXSinCoefficients0.f);
	const Vec4V sinCoefficients1 = V4LoadA(g_PXSinCoefficients1.f);
	const Vec4V sinCoefficients2 = V4LoadA(g_PXSinCoefficients2.f);

	const FloatV S1 = V4GetY(sinCoefficients0);
	const FloatV S2 = V4GetZ(sinCoefficients0);
	const FloatV S3 = V4GetW(sinCoefficients0);
	const FloatV S4 = V4GetX(sinCoefficients1);
	const FloatV S5 = V4GetY(sinCoefficients1);
	const FloatV S6 = V4GetZ(sinCoefficients1);
	const FloatV S7 = V4GetW(sinCoefficients1);
	const FloatV S8 = V4GetX(sinCoefficients2);
	const FloatV S9 = V4GetY(sinCoefficients2);
	const FloatV S10 = V4GetZ(sinCoefficients2);
	const FloatV S11 = V4GetW(sinCoefficients2);

	Vec4V Result;
	Result = V4MulAdd(S1, V3, V1);
	Result = V4MulAdd(S2, V5, Result);
	Result = V4MulAdd(S3, V7, Result);
	Result = V4MulAdd(S4, V9, Result);
	Result = V4MulAdd(S5, V11, Result);
	Result = V4MulAdd(S6, V13, Result);
	Result = V4MulAdd(S7, V15, Result);
	Result = V4MulAdd(S8, V17, Result);
	Result = V4MulAdd(S9, V19, Result);
	Result = V4MulAdd(S10, V21, Result);
	Result = V4MulAdd(S11, V23, Result);

	return Result;
}

PX_FORCE_INLINE Vec4V V4Cos(const Vec4V a)
{
	// PT: TODO: these should be FLoads
	const Vec4V recipTwoPi = V4LoadA(g_PXReciprocalTwoPi.f);
	const FloatV twoPi = V4LoadA(g_PXTwoPi.f);
	const Vec4V tmp = V4Mul(a, recipTwoPi);
	const Vec4V b = V4Round(tmp);
	const Vec4V V1 = V4NegMulSub(twoPi, b, a);

	// cos(V) ~= 1 - V^2 / 2! + V^4 / 4! - V^6 / 6! + V^8 / 8! - V^10 / 10! + V^12 / 12! -
	//           V^14 / 14! + V^16 / 16! - V^18 / 18! + V^20 / 20! - V^22 / 22! (for -PI <= V < PI)
	const Vec4V V2 = V4Mul(V1, V1);
	const Vec4V V4 = V4Mul(V2, V2);
	const Vec4V V6 = V4Mul(V4, V2);
	const Vec4V V8 = V4Mul(V4, V4);
	const Vec4V V10 = V4Mul(V6, V4);
	const Vec4V V12 = V4Mul(V6, V6);
	const Vec4V V14 = V4Mul(V8, V6);
	const Vec4V V16 = V4Mul(V8, V8);
	const Vec4V V18 = V4Mul(V10, V8);
	const Vec4V V20 = V4Mul(V10, V10);
	const Vec4V V22 = V4Mul(V12, V10);

	const Vec4V cosCoefficients0 = V4LoadA(g_PXCosCoefficients0.f);
	const Vec4V cosCoefficients1 = V4LoadA(g_PXCosCoefficients1.f);
	const Vec4V cosCoefficients2 = V4LoadA(g_PXCosCoefficients2.f);

	const FloatV C1 = V4GetY(cosCoefficients0);
	const FloatV C2 = V4GetZ(cosCoefficients0);
	const FloatV C3 = V4GetW(cosCoefficients0);
	const FloatV C4 = V4GetX(cosCoefficients1);
	const FloatV C5 = V4GetY(cosCoefficients1);
	const FloatV C6 = V4GetZ(cosCoefficients1);
	const FloatV C7 = V4GetW(cosCoefficients1);
	const FloatV C8 = V4GetX(cosCoefficients2);
	const FloatV C9 = V4GetY(cosCoefficients2);
	const FloatV C10 = V4GetZ(cosCoefficients2);
	const FloatV C11 = V4GetW(cosCoefficients2);

	Vec4V Result;
	Result = V4MulAdd(C1, V2, V4One());
	Result = V4MulAdd(C2, V4, Result);
	Result = V4MulAdd(C3, V6, Result);
	Result = V4MulAdd(C4, V8, Result);
	Result = V4MulAdd(C5, V10, Result);
	Result = V4MulAdd(C6, V12, Result);
	Result = V4MulAdd(C7, V14, Result);
	Result = V4MulAdd(C8, V16, Result);
	Result = V4MulAdd(C9, V18, Result);
	Result = V4MulAdd(C10, V20, Result);
	Result = V4MulAdd(C11, V22, Result);

	return Result;
}

PX_FORCE_INLINE void V4Transpose(Vec4V& col0, Vec4V& col1, Vec4V& col2, Vec4V& col3)
{
	const Vec4V tmp0 = _mm_unpacklo_ps(col0, col1);
	const Vec4V tmp2 = _mm_unpacklo_ps(col2, col3);
	const Vec4V tmp1 = _mm_unpackhi_ps(col0, col1);
	const Vec4V tmp3 = _mm_unpackhi_ps(col2, col3);
	col0 = _mm_movelh_ps(tmp0, tmp2);
	col1 = _mm_movehl_ps(tmp2, tmp0);
	col2 = _mm_movelh_ps(tmp1, tmp3);
	col3 = _mm_movehl_ps(tmp3, tmp1);
}

PX_FORCE_INLINE BoolV V4IsEqU32(const VecU32V a, const VecU32V b)
{
	return internalSimd::m128_I2F(_mm_cmpeq_epi32(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

//////////////////////////////////
// BoolV
//////////////////////////////////

PX_FORCE_INLINE BoolV BFFFF()
{
	return _mm_setzero_ps();
}

PX_FORCE_INLINE BoolV BFFFT()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0,0,0,0xFFFFFFFF};
	const __m128 ffft=_mm_load_ps((float*)&f);
	return ffft;*/
	return internalSimd::m128_I2F(_mm_set_epi32(-1, 0, 0, 0));
}

PX_FORCE_INLINE BoolV BFFTF()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0,0,0xFFFFFFFF,0};
	const __m128 fftf=_mm_load_ps((float*)&f);
	return fftf;*/
	return internalSimd::m128_I2F(_mm_set_epi32(0, -1, 0, 0));
}

PX_FORCE_INLINE BoolV BFFTT()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0,0,0xFFFFFFFF,0xFFFFFFFF};
	const __m128 fftt=_mm_load_ps((float*)&f);
	return fftt;*/
	return internalSimd::m128_I2F(_mm_set_epi32(-1, -1, 0, 0));
}

PX_FORCE_INLINE BoolV BFTFF()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0,0xFFFFFFFF,0,0};
	const __m128 ftff=_mm_load_ps((float*)&f);
	return ftff;*/
	return internalSimd::m128_I2F(_mm_set_epi32(0, 0, -1, 0));
}

PX_FORCE_INLINE BoolV BFTFT()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0,0xFFFFFFFF,0,0xFFFFFFFF};
	const __m128 ftft=_mm_load_ps((float*)&f);
	return ftft;*/
	return internalSimd::m128_I2F(_mm_set_epi32(-1, 0, -1, 0));
}

PX_FORCE_INLINE BoolV BFTTF()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0,0xFFFFFFFF,0xFFFFFFFF,0};
	const __m128 fttf=_mm_load_ps((float*)&f);
	return fttf;*/
	return internalSimd::m128_I2F(_mm_set_epi32(0, -1, -1, 0));
}

PX_FORCE_INLINE BoolV BFTTT()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};
	const __m128 fttt=_mm_load_ps((float*)&f);
	return fttt;*/
	return internalSimd::m128_I2F(_mm_set_epi32(-1, -1, -1, 0));
}

PX_FORCE_INLINE BoolV BTFFF()
{
	// const PX_ALIGN(16, PxU32 f[4])={0xFFFFFFFF,0,0,0};
	// const __m128 tfff=_mm_load_ps((float*)&f);
	// return tfff;
	return internalSimd::m128_I2F(_mm_set_epi32(0, 0, 0, -1));
}

PX_FORCE_INLINE BoolV BTFFT()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0xFFFFFFFF,0,0,0xFFFFFFFF};
	const __m128 tfft=_mm_load_ps((float*)&f);
	return tfft;*/
	return internalSimd::m128_I2F(_mm_set_epi32(-1, 0, 0, -1));
}

PX_FORCE_INLINE BoolV BTFTF()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0xFFFFFFFF,0,0xFFFFFFFF,0};
	const __m128 tftf=_mm_load_ps((float*)&f);
	return tftf;*/
	return internalSimd::m128_I2F(_mm_set_epi32(0, -1, 0, -1));
}

PX_FORCE_INLINE BoolV BTFTT()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0xFFFFFFFF,0,0xFFFFFFFF,0xFFFFFFFF};
	const __m128 tftt=_mm_load_ps((float*)&f);
	return tftt;*/
	return internalSimd::m128_I2F(_mm_set_epi32(-1, -1, 0, -1));
}

PX_FORCE_INLINE BoolV BTTFF()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0xFFFFFFFF,0xFFFFFFFF,0,0};
	const __m128 ttff=_mm_load_ps((float*)&f);
	return ttff;*/
	return internalSimd::m128_I2F(_mm_set_epi32(0, 0, -1, -1));
}

PX_FORCE_INLINE BoolV BTTFT()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0xFFFFFFFF,0xFFFFFFFF,0,0xFFFFFFFF};
	const __m128 ttft=_mm_load_ps((float*)&f);
	return ttft;*/
	return internalSimd::m128_I2F(_mm_set_epi32(-1, 0, -1, -1));
}

PX_FORCE_INLINE BoolV BTTTF()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0};
	const __m128 tttf=_mm_load_ps((float*)&f);
	return tttf;*/
	return internalSimd::m128_I2F(_mm_set_epi32(0, -1, -1, -1));
}

PX_FORCE_INLINE BoolV BTTTT()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};
	const __m128 tttt=_mm_load_ps((float*)&f);
	return tttt;*/
	return internalSimd::m128_I2F(_mm_set_epi32(-1, -1, -1, -1));
}

PX_FORCE_INLINE BoolV BXMask()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0xFFFFFFFF,0,0,0};
	const __m128 tfff=_mm_load_ps((float*)&f);
	return tfff;*/
	return internalSimd::m128_I2F(_mm_set_epi32(0, 0, 0, -1));
}

PX_FORCE_INLINE BoolV BYMask()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0,0xFFFFFFFF,0,0};
	const __m128 ftff=_mm_load_ps((float*)&f);
	return ftff;*/
	return internalSimd::m128_I2F(_mm_set_epi32(0, 0, -1, 0));
}

PX_FORCE_INLINE BoolV BZMask()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0,0,0xFFFFFFFF,0};
	const __m128 fftf=_mm_load_ps((float*)&f);
	return fftf;*/
	return internalSimd::m128_I2F(_mm_set_epi32(0, -1, 0, 0));
}

PX_FORCE_INLINE BoolV BWMask()
{
	/*const PX_ALIGN(16, PxU32 f[4])={0,0,0,0xFFFFFFFF};
	const __m128 ffft=_mm_load_ps((float*)&f);
	return ffft;*/
	return internalSimd::m128_I2F(_mm_set_epi32(-1, 0, 0, 0));
}

PX_FORCE_INLINE BoolV BGetX(const BoolV f)
{
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(0, 0, 0, 0));
}

PX_FORCE_INLINE BoolV BGetY(const BoolV f)
{
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(1, 1, 1, 1));
}

PX_FORCE_INLINE BoolV BGetZ(const BoolV f)
{
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(2, 2, 2, 2));
}

PX_FORCE_INLINE BoolV BGetW(const BoolV f)
{
	return _mm_shuffle_ps(f, f, _MM_SHUFFLE(3, 3, 3, 3));
}

PX_FORCE_INLINE BoolV BSetX(const BoolV v, const BoolV f)
{
	return V4Sel(BFTTT(), v, f);
}

PX_FORCE_INLINE BoolV BSetY(const BoolV v, const BoolV f)
{
	return V4Sel(BTFTT(), v, f);
}

PX_FORCE_INLINE BoolV BSetZ(const BoolV v, const BoolV f)
{
	return V4Sel(BTTFT(), v, f);
}

PX_FORCE_INLINE BoolV BSetW(const BoolV v, const BoolV f)
{
	return V4Sel(BTTTF(), v, f);
}

PX_FORCE_INLINE BoolV BAnd(const BoolV a, const BoolV b)
{
	return _mm_and_ps(a, b);
}

PX_FORCE_INLINE BoolV BNot(const BoolV a)
{
	const BoolV bAllTrue(BTTTT());
	return _mm_xor_ps(a, bAllTrue);
}

PX_FORCE_INLINE BoolV BAndNot(const BoolV a, const BoolV b)
{
	return _mm_andnot_ps(b, a);
}

PX_FORCE_INLINE BoolV BOr(const BoolV a, const BoolV b)
{
	return _mm_or_ps(a, b);
}

PX_FORCE_INLINE BoolV BAllTrue4(const BoolV a)
{
	const BoolV bTmp = _mm_and_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 1, 0, 1)), _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 2, 3)));
	return _mm_and_ps(_mm_shuffle_ps(bTmp, bTmp, _MM_SHUFFLE(0, 0, 0, 0)),
	                  _mm_shuffle_ps(bTmp, bTmp, _MM_SHUFFLE(1, 1, 1, 1)));
}

PX_FORCE_INLINE BoolV BAnyTrue4(const BoolV a)
{
	const BoolV bTmp = _mm_or_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 1, 0, 1)), _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 2, 3)));
	return _mm_or_ps(_mm_shuffle_ps(bTmp, bTmp, _MM_SHUFFLE(0, 0, 0, 0)),
	                 _mm_shuffle_ps(bTmp, bTmp, _MM_SHUFFLE(1, 1, 1, 1)));
}

PX_FORCE_INLINE BoolV BAllTrue3(const BoolV a)
{
	const BoolV bTmp = _mm_and_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 1, 0, 1)), _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2)));
	return _mm_and_ps(_mm_shuffle_ps(bTmp, bTmp, _MM_SHUFFLE(0, 0, 0, 0)),
	                  _mm_shuffle_ps(bTmp, bTmp, _MM_SHUFFLE(1, 1, 1, 1)));
}

PX_FORCE_INLINE BoolV BAnyTrue3(const BoolV a)
{
	const BoolV bTmp = _mm_or_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 1, 0, 1)), _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2)));
	return _mm_or_ps(_mm_shuffle_ps(bTmp, bTmp, _MM_SHUFFLE(0, 0, 0, 0)),
	                 _mm_shuffle_ps(bTmp, bTmp, _MM_SHUFFLE(1, 1, 1, 1)));
}

PX_FORCE_INLINE PxU32 BAllEq(const BoolV a, const BoolV b)
{
	const BoolV bTest = internalSimd::m128_I2F(_mm_cmpeq_epi32(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
	return internalSimd::BAllTrue4_R(bTest);
}

PX_FORCE_INLINE PxU32 BAllEqTTTT(const BoolV a)
{
	return PxU32(_mm_movemask_ps(a)==15);
}

PX_FORCE_INLINE PxU32 BAllEqFFFF(const BoolV a)
{
	return PxU32(_mm_movemask_ps(a)==0);
}

PX_FORCE_INLINE PxU32 BGetBitMask(const BoolV a)
{
	return PxU32(_mm_movemask_ps(a));
}

//////////////////////////////////
// MAT33V
//////////////////////////////////

PX_FORCE_INLINE Mat33V M33Identity()
{
	return Mat33V(V3UnitX(), V3UnitY(), V3UnitZ());
}

PX_FORCE_INLINE Vec3V M33MulV3(const Mat33V& a, const Vec3V b)
{
	const FloatV x = V3GetX(b);
	const FloatV y = V3GetY(b);
	const FloatV z = V3GetZ(b);
	const Vec3V v0 = V3Scale(a.col0, x);
	const Vec3V v1 = V3Scale(a.col1, y);
	const Vec3V v2 = V3Scale(a.col2, z);
	const Vec3V v0PlusV1 = V3Add(v0, v1);
	return V3Add(v0PlusV1, v2);
}

PX_FORCE_INLINE Vec3V M33MulV3AddV3(const Mat33V& A, const Vec3V b, const Vec3V c)
{
	const FloatV x = V3GetX(b);
	const FloatV y = V3GetY(b);
	const FloatV z = V3GetZ(b);
	Vec3V result = V3ScaleAdd(A.col0, x, c);
	result = V3ScaleAdd(A.col1, y, result);
	return V3ScaleAdd(A.col2, z, result);
}

PX_FORCE_INLINE Mat33V M33MulM33(const Mat33V& a, const Mat33V& b)
{
	return Mat33V(M33MulV3(a, b.col0), M33MulV3(a, b.col1), M33MulV3(a, b.col2));
}

PX_FORCE_INLINE Mat33V M33Add(const Mat33V& a, const Mat33V& b)
{
	return Mat33V(V3Add(a.col0, b.col0), V3Add(a.col1, b.col1), V3Add(a.col2, b.col2));
}

PX_FORCE_INLINE Mat33V M33Scale(const Mat33V& a, const FloatV& b)
{
	return Mat33V(V3Scale(a.col0, b), V3Scale(a.col1, b), V3Scale(a.col2, b));
}

PX_FORCE_INLINE Mat33V M33Sub(const Mat33V& a, const Mat33V& b)
{
	return Mat33V(V3Sub(a.col0, b.col0), V3Sub(a.col1, b.col1), V3Sub(a.col2, b.col2));
}

PX_FORCE_INLINE Mat33V M33Neg(const Mat33V& a)
{
	return Mat33V(V3Neg(a.col0), V3Neg(a.col1), V3Neg(a.col2));
}

PX_FORCE_INLINE Mat33V M33Abs(const Mat33V& a)
{
	return Mat33V(V3Abs(a.col0), V3Abs(a.col1), V3Abs(a.col2));
}

PX_FORCE_INLINE Mat33V M33Inverse(const Mat33V& a)
{
	const BoolV tfft = BTFFT();
	const BoolV tttf = BTTTF();
	const FloatV zero = FZero();
	const Vec3V cross01 = V3Cross(a.col0, a.col1);
	const Vec3V cross12 = V3Cross(a.col1, a.col2);
	const Vec3V cross20 = V3Cross(a.col2, a.col0);
	const FloatV dot = V3Dot(cross01, a.col2);
	const FloatV invDet = _mm_rcp_ps(dot);
	const Vec3V mergeh = _mm_unpacklo_ps(cross12, cross01);
	const Vec3V mergel = _mm_unpackhi_ps(cross12, cross01);
	Vec3V colInv0 = _mm_unpacklo_ps(mergeh, cross20);
	colInv0 = _mm_or_ps(_mm_andnot_ps(tttf, zero), _mm_and_ps(tttf, colInv0));
	const Vec3V zppd = _mm_shuffle_ps(mergeh, cross20, _MM_SHUFFLE(3, 0, 0, 2));
	const Vec3V pbwp = _mm_shuffle_ps(cross20, mergeh, _MM_SHUFFLE(3, 3, 1, 0));
	const Vec3V colInv1 = _mm_or_ps(_mm_andnot_ps(BTFFT(), pbwp), _mm_and_ps(BTFFT(), zppd));
	const Vec3V xppd = _mm_shuffle_ps(mergel, cross20, _MM_SHUFFLE(3, 0, 0, 0));
	const Vec3V pcyp = _mm_shuffle_ps(cross20, mergel, _MM_SHUFFLE(3, 1, 2, 0));
	const Vec3V colInv2 = _mm_or_ps(_mm_andnot_ps(tfft, pcyp), _mm_and_ps(tfft, xppd));

	return Mat33V(_mm_mul_ps(colInv0, invDet), _mm_mul_ps(colInv1, invDet), _mm_mul_ps(colInv2, invDet));
}

PX_FORCE_INLINE Mat33V M33Diagonal(const Vec3VArg d)
{
	const FloatV x = V3Mul(V3UnitX(), d);
	const FloatV y = V3Mul(V3UnitY(), d);
	const FloatV z = V3Mul(V3UnitZ(), d);
	return Mat33V(x, y, z);
}

PX_FORCE_INLINE Mat33V Mat33V_From_PxMat33(const PxMat33& m)
{
	return Mat33V(V3LoadU(m.column0), V3LoadU(m.column1), V3LoadU(m.column2));
}

PX_FORCE_INLINE void PxMat33_From_Mat33V(const Mat33V& m, PxMat33& out)
{
	V3StoreU(m.col0, out.column0);
	V3StoreU(m.col1, out.column1);
	V3StoreU(m.col2, out.column2);
}

//////////////////////////////////
// MAT34V
//////////////////////////////////

PX_FORCE_INLINE Vec3V M34MulV3(const Mat34V& a, const Vec3V b)
{
	const FloatV x = V3GetX(b);
	const FloatV y = V3GetY(b);
	const FloatV z = V3GetZ(b);
	const Vec3V v0 = V3Scale(a.col0, x);
	const Vec3V v1 = V3Scale(a.col1, y);
	const Vec3V v2 = V3Scale(a.col2, z);
	const Vec3V v0PlusV1 = V3Add(v0, v1);
	const Vec3V v0PlusV1Plusv2 = V3Add(v0PlusV1, v2);
	return V3Add(v0PlusV1Plusv2, a.col3);
}

PX_FORCE_INLINE Vec3V M34Mul33V3(const Mat34V& a, const Vec3V b)
{
	const FloatV x = V3GetX(b);
	const FloatV y = V3GetY(b);
	const FloatV z = V3GetZ(b);
	const Vec3V v0 = V3Scale(a.col0, x);
	const Vec3V v1 = V3Scale(a.col1, y);
	const Vec3V v2 = V3Scale(a.col2, z);
	const Vec3V v0PlusV1 = V3Add(v0, v1);
	return V3Add(v0PlusV1, v2);
}

PX_FORCE_INLINE Mat34V M34MulM34(const Mat34V& a, const Mat34V& b)
{
	return Mat34V(M34Mul33V3(a, b.col0), M34Mul33V3(a, b.col1), M34Mul33V3(a, b.col2), M34MulV3(a, b.col3));
}

PX_FORCE_INLINE Mat33V M34MulM33(const Mat34V& a, const Mat33V& b)
{
	return Mat33V(M34Mul33V3(a, b.col0), M34Mul33V3(a, b.col1), M34Mul33V3(a, b.col2));
}

PX_FORCE_INLINE Mat33V M34Mul33MM34(const Mat34V& a, const Mat34V& b)
{
	return Mat33V(M34Mul33V3(a, b.col0), M34Mul33V3(a, b.col1), M34Mul33V3(a, b.col2));
}

PX_FORCE_INLINE Mat34V M34Add(const Mat34V& a, const Mat34V& b)
{
	return Mat34V(V3Add(a.col0, b.col0), V3Add(a.col1, b.col1), V3Add(a.col2, b.col2), V3Add(a.col3, b.col3));
}

//////////////////////////////////
// MAT44V
//////////////////////////////////

PX_FORCE_INLINE Vec4V M44MulV4(const Mat44V& a, const Vec4V b)
{
	const FloatV x = V4GetX(b);
	const FloatV y = V4GetY(b);
	const FloatV z = V4GetZ(b);
	const FloatV w = V4GetW(b);

	const Vec4V v0 = V4Scale(a.col0, x);
	const Vec4V v1 = V4Scale(a.col1, y);
	const Vec4V v2 = V4Scale(a.col2, z);
	const Vec4V v3 = V4Scale(a.col3, w);
	const Vec4V v0PlusV1 = V4Add(v0, v1);
	const Vec4V v0PlusV1Plusv2 = V4Add(v0PlusV1, v2);
	return V4Add(v0PlusV1Plusv2, v3);
}

PX_FORCE_INLINE Mat44V M44MulM44(const Mat44V& a, const Mat44V& b)
{
	return Mat44V(M44MulV4(a, b.col0), M44MulV4(a, b.col1), M44MulV4(a, b.col2), M44MulV4(a, b.col3));
}

PX_FORCE_INLINE Mat44V M44Add(const Mat44V& a, const Mat44V& b)
{
	return Mat44V(V4Add(a.col0, b.col0), V4Add(a.col1, b.col1), V4Add(a.col2, b.col2), V4Add(a.col3, b.col3));
}

PX_FORCE_INLINE Mat44V M44Trnsps(const Mat44V& a);

PX_FORCE_INLINE Mat44V M44Inverse(const Mat44V& a)
{
	__m128 minor0, minor1, minor2, minor3;
	__m128 row0, row1, row2, row3;
	__m128 det, tmp1;

	tmp1 = V4Zero();
	row1 = V4Zero();
	row3 = V4Zero();

	row0 = a.col0;
	row1 = _mm_shuffle_ps(a.col1, a.col1, _MM_SHUFFLE(1, 0, 3, 2));
	row2 = a.col2;
	row3 = _mm_shuffle_ps(a.col3, a.col3, _MM_SHUFFLE(1, 0, 3, 2));

	tmp1 = _mm_mul_ps(row2, row3);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
	minor0 = _mm_mul_ps(row1, tmp1);
	minor1 = _mm_mul_ps(row0, tmp1);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
	minor0 = _mm_sub_ps(_mm_mul_ps(row1, tmp1), minor0);
	minor1 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor1);
	minor1 = _mm_shuffle_ps(minor1, minor1, 0x4E);

	tmp1 = _mm_mul_ps(row1, row2);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
	minor0 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor0);
	minor3 = _mm_mul_ps(row0, tmp1);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
	minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row3, tmp1));
	minor3 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor3);
	minor3 = _mm_shuffle_ps(minor3, minor3, 0x4E);

	tmp1 = _mm_mul_ps(_mm_shuffle_ps(row1, row1, 0x4E), row3);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
	row2 = _mm_shuffle_ps(row2, row2, 0x4E);
	minor0 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor0);
	minor2 = _mm_mul_ps(row0, tmp1);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
	minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row2, tmp1));
	minor2 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor2);
	minor2 = _mm_shuffle_ps(minor2, minor2, 0x4E);

	tmp1 = _mm_mul_ps(row0, row1);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
	minor2 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor2);
	minor3 = _mm_sub_ps(_mm_mul_ps(row2, tmp1), minor3);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
	minor2 = _mm_sub_ps(_mm_mul_ps(row3, tmp1), minor2);
	minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row2, tmp1));

	tmp1 = _mm_mul_ps(row0, row3);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
	minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row2, tmp1));
	minor2 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor2);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
	minor1 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor1);
	minor2 = _mm_sub_ps(minor2, _mm_mul_ps(row1, tmp1));

	tmp1 = _mm_mul_ps(row0, row2);
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
	minor1 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor1);
	minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row1, tmp1));
	tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
	minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row3, tmp1));
	minor3 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor3);

	det = _mm_mul_ps(row0, minor0);
	det = _mm_add_ps(_mm_shuffle_ps(det, det, 0x4E), det);
	det = _mm_add_ss(_mm_shuffle_ps(det, det, 0xB1), det);
	tmp1 = _mm_rcp_ss(det);
#if 0
	det = _mm_sub_ss(_mm_add_ss(tmp1, tmp1), _mm_mul_ss(det, _mm_mul_ss(tmp1, tmp1)));
	det = _mm_shuffle_ps(det, det, 0x00);
#else
	det = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(0, 0, 0, 0));
#endif

	minor0 = _mm_mul_ps(det, minor0);
	minor1 = _mm_mul_ps(det, minor1);
	minor2 = _mm_mul_ps(det, minor2);
	minor3 = _mm_mul_ps(det, minor3);
	Mat44V invTrans(minor0, minor1, minor2, minor3);
	return M44Trnsps(invTrans);
}

//////////////////////////////////
// Misc
//////////////////////////////////

// PT: TODO: seems to be in the wrong section
PX_FORCE_INLINE Vec4V V4LoadXYZW(PxF32 x, PxF32 y, PxF32 z, PxF32 w)
{
	return _mm_set_ps(w, z, y, x);
}

PX_FORCE_INLINE VecU32V V4U32Sel(const BoolV c, const VecU32V a, const VecU32V b)
{
	return internalSimd::m128_I2F(_mm_or_si128(_mm_andnot_si128(internalSimd::m128_F2I(c), internalSimd::m128_F2I(b)), _mm_and_si128(internalSimd::m128_F2I(c), internalSimd::m128_F2I(a))));
}

PX_FORCE_INLINE VecU32V V4U32or(VecU32V a, VecU32V b)
{
	return internalSimd::m128_I2F(_mm_or_si128(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

PX_FORCE_INLINE VecU32V V4U32xor(VecU32V a, VecU32V b)
{
	return internalSimd::m128_I2F(_mm_xor_si128(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

PX_FORCE_INLINE VecU32V V4U32and(VecU32V a, VecU32V b)
{
	return internalSimd::m128_I2F(_mm_and_si128(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

PX_FORCE_INLINE VecU32V V4U32Andc(VecU32V a, VecU32V b)
{
	return internalSimd::m128_I2F(_mm_andnot_si128(internalSimd::m128_F2I(b), internalSimd::m128_F2I(a)));
}

PX_FORCE_INLINE VecU32V U4Load(const PxU32 i)
{
	return _mm_load1_ps(reinterpret_cast<const PxF32*>(&i));
}

PX_FORCE_INLINE VecU32V U4LoadU(const PxU32* i)
{
	return _mm_loadu_ps(reinterpret_cast<const PxF32*>(i));
}

PX_FORCE_INLINE VecU32V U4LoadA(const PxU32* i)
{
	ASSERT_ISALIGNED16(i);
	return _mm_load_ps(reinterpret_cast<const PxF32*>(i));
}

PX_FORCE_INLINE VecI32V VecI32V_One()
{
	return I4Load(1);
}

PX_FORCE_INLINE VecI32V VecI32V_Two()
{
	return I4Load(2);
}

PX_FORCE_INLINE VecI32V VecI32V_MinusOne()
{
	return I4Load(-1);
}

PX_FORCE_INLINE VecU32V U4Zero()
{
	return U4Load(0);
}

PX_FORCE_INLINE VecU32V U4One()
{
	return U4Load(1);
}

PX_FORCE_INLINE VecU32V U4Two()
{
	return U4Load(2);
}

PX_FORCE_INLINE Vec4V V4Andc(const Vec4V a, const VecU32V b)
{
	VecU32V result32(a);
	result32 = V4U32Andc(result32, b);
	return Vec4V(result32);
}

PX_FORCE_INLINE VecU32V V4IsGrtrV32u(const Vec4V a, const Vec4V b)
{
	return V4IsGrtr(a, b);
}

PX_FORCE_INLINE VecU16V V4U16LoadAligned(const VecU16V* addr)
{
	return *addr;
}

PX_FORCE_INLINE VecU16V V4U16LoadUnaligned(const VecU16V* addr)
{
	return *addr;
}

PX_FORCE_INLINE VecU16V V4I16CompareGt(VecU16V a, VecU16V b)
{
	return internalSimd::m128_I2F(_mm_cmpgt_epi16(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

// unsigned compares are not supported on x86
PX_FORCE_INLINE VecU16V V4U16CompareGt(VecU16V a, VecU16V b)
{
	const PxU16 signMask = 0x8000;
	const __m128i signBit = _mm_set1_epi16(PxI16(signMask));

	const __m128i a_i = _mm_xor_si128(_mm_castps_si128(a), signBit);
	const __m128i b_i = _mm_xor_si128(_mm_castps_si128(b), signBit);
	const __m128i result = _mm_cmpgt_epi16(a_i, b_i);
	return _mm_castsi128_ps(result); // Back to __m128
}

PX_FORCE_INLINE Vec4V Vec4V_From_VecU32V(VecU32V a)
{
	// Cast __m128 (actually storing ints) to __m128i, convert to float
	return _mm_cvtepi32_ps(_mm_castps_si128(a));
}

PX_FORCE_INLINE VecU32V U4LoadXYZW(PxU32 x, PxU32 y, PxU32 z, PxU32 w)
{
	const __m128i v = _mm_set_epi32((int)w, (int)z, (int)y, (int)x);
	return _mm_castsi128_ps(v); // Store as __m128 for compatibility
}

} // namespace aos
#if !PX_DOXYGEN
} // namespace physx
#endif


#endif


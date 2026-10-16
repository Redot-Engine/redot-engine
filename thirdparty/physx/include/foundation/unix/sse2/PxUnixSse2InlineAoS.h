// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PXFOUNDATION_PXUNIXSSE2INLINEAOS_H
#define PXFOUNDATION_PXUNIXSSE2INLINEAOS_H

namespace physx
{
namespace aos
{
//////////////////////////////////////////////////////////////////////
//Test that Vec3V and FloatV are legal
//////////////////////////////////////////////////////////////////////

#define FLOAT_COMPONENTS_EQUAL_THRESHOLD 0.01f
PX_FORCE_INLINE static bool isValidFloatV(const FloatV a)
{
	const PxF32 x = V4ReadX(a);
	const PxF32 y = V4ReadY(a);
	const PxF32 z = V4ReadZ(a);
	const PxF32 w = V4ReadW(a);

	return (x == y && x == z && x == w);

 	/*if (
		(PxAbs(x - y) < FLOAT_COMPONENTS_EQUAL_THRESHOLD) &&
		(PxAbs(x - z) < FLOAT_COMPONENTS_EQUAL_THRESHOLD) &&
		(PxAbs(x - w) < FLOAT_COMPONENTS_EQUAL_THRESHOLD)
		)
	{
		return true;
	}

	if (
		(PxAbs((x - y) / x) < FLOAT_COMPONENTS_EQUAL_THRESHOLD) &&
		(PxAbs((x - z) / x) < FLOAT_COMPONENTS_EQUAL_THRESHOLD) &&
		(PxAbs((x - w) / x) < FLOAT_COMPONENTS_EQUAL_THRESHOLD)
		)
	{
		return true;
	}
	return false;*/
}

}
}

#include "../../PxVecMathSSE.h"

namespace physx
{
namespace aos
{

#define PX_FPCLASS_SNAN 0x0001 /* signaling NaN */
#define PX_FPCLASS_QNAN 0x0002 /* quiet NaN */
#define PX_FPCLASS_NINF 0x0004 /* negative infinity */
#define PX_FPCLASS_PINF 0x0200 /* positive infinity */

namespace internalSimd
{
#if !PX_EMSCRIPTEN
#if PX_CLANG
#if PX_LINUX
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
#endif
const PX_ALIGN(16, PxF32 gMaskXYZ[4]) = { physx::PxUnionCast<PxF32>(0xffffffff), physx::PxUnionCast<PxF32>(0xffffffff),
	                                      physx::PxUnionCast<PxF32>(0xffffffff), 0 };
#if PX_CLANG
#if PX_LINUX
#pragma clang diagnostic pop
#endif
#endif
#else
// emscripten doesn't like the PxUnionCast data structure
// the following is what windows and xbox does -- using these for emscripten
const PX_ALIGN(16, PxU32 gMaskXYZ[4]) = { 0xffffffff, 0xffffffff, 0xffffffff, 0 };
#endif
}

namespace vecMathTests
{
PX_FORCE_INLINE bool allElementsEqualBoolV(const BoolV a, const BoolV b)
{
	return internalSimd::BAllTrue4_R(VecI32V_IsEq(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b))) != 0;
}

PX_FORCE_INLINE bool allElementsEqualVecI32V(const VecI32V a, const VecI32V b)
{
	BoolV c = internalSimd::m128_I2F(_mm_cmpeq_epi32(a, b));
	return internalSimd::BAllTrue4_R(c) != 0;
}

#define VECMATH_AOS_EPSILON (1e-3f)

PX_FORCE_INLINE bool allElementsNearEqualFloatV(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	const FloatV c = FSub(a, b);
	const FloatV minError = FLoad(-VECMATH_AOS_EPSILON);
	const FloatV maxError = FLoad(VECMATH_AOS_EPSILON);
	return _mm_comigt_ss(c, minError) && _mm_comilt_ss(c, maxError);
}

PX_FORCE_INLINE bool allElementsNearEqualVec3V(const Vec3V a, const Vec3V b)
{
	const Vec3V c = V3Sub(a, b);
	const Vec3V minError = V3Load(-VECMATH_AOS_EPSILON);
	const Vec3V maxError = V3Load(VECMATH_AOS_EPSILON);
	return (_mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)), minError) &&
	 		_mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)), maxError) &&
	 		_mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1)), minError) &&
	 		_mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1)), maxError) &&
	 		_mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2)), minError) &&
	 		_mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2)), maxError));
}

PX_FORCE_INLINE bool allElementsNearEqualVec4V(const Vec4V a, const Vec4V b)
{
	const Vec4V c = V4Sub(a, b);
	const Vec4V minError = V4Load(-VECMATH_AOS_EPSILON);
	const Vec4V maxError = V4Load(VECMATH_AOS_EPSILON);
	return (_mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)), minError) &&
	        _mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)), maxError) &&
	        _mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1)), minError) &&
	        _mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1)), maxError) &&
	        _mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2)), minError) &&
	        _mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2)), maxError) &&
	        _mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 3, 3, 3)), minError) &&
	        _mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 3, 3, 3)), maxError));
}
} //vecMathTests

/////////////////////////////////////////////////////////////////////
////FUNCTIONS USED ONLY FOR ASSERTS IN VECTORISED IMPLEMENTATIONS
/////////////////////////////////////////////////////////////////////

PX_FORCE_INLINE bool isFiniteFloatV(const FloatV a)
{
	PxF32 badNumber =
	    physx::PxUnionCast<PxF32, PxU32>(PX_FPCLASS_SNAN | PX_FPCLASS_QNAN | PX_FPCLASS_NINF | PX_FPCLASS_PINF);
	const FloatV vBadNum = FLoad(badNumber);
	const BoolV vMask = BAnd(vBadNum, a);
	return internalSimd::FiniteTestEq(vMask, BFFFF()) == 1;
}

PX_FORCE_INLINE bool isFiniteVec3V(const Vec3V a)
{
	PxF32 badNumber =
	    physx::PxUnionCast<PxF32, PxU32>(PX_FPCLASS_SNAN | PX_FPCLASS_QNAN | PX_FPCLASS_NINF | PX_FPCLASS_PINF);
	const Vec3V vBadNum = V3Load(badNumber);
	const BoolV vMask = BAnd(BAnd(vBadNum, a), BTTTF());
	return internalSimd::FiniteTestEq(vMask, BFFFF()) == 1;
}

PX_FORCE_INLINE bool isFiniteVec4V(const Vec4V a)
{
	/*Vec4V a;
	PX_ALIGN(16, PxF32 f[4]);
	F32Array_Aligned_From_Vec4V(a, f);
	return PxIsFinite(f[0])
	        && PxIsFinite(f[1])
	        && PxIsFinite(f[2])
	        && PxIsFinite(f[3]);*/

	PxF32 badNumber =
	    physx::PxUnionCast<PxF32, PxU32>(PX_FPCLASS_SNAN | PX_FPCLASS_QNAN | PX_FPCLASS_NINF | PX_FPCLASS_PINF);
	const Vec4V vBadNum = V4Load(badNumber);
	const BoolV vMask = BAnd(vBadNum, a);

	return internalSimd::FiniteTestEq(vMask, BFFFF()) == 1;
}

/////////////////////////////////////////////////////////////////////
////VECTORISED FUNCTION IMPLEMENTATIONS
/////////////////////////////////////////////////////////////////////

PX_FORCE_INLINE Vec3V V3LoadA(const PxVec3& f)
{
	ASSERT_ISALIGNED16(&f);
#if !PX_EMSCRIPTEN
	return _mm_and_ps(reinterpret_cast<const Vec3V&>(f), V4LoadA(internalSimd::gMaskXYZ));
#else
	return _mm_and_ps((Vec3V&)f, (VecI32V&)internalSimd::gMaskXYZ);
#endif
}

PX_FORCE_INLINE Vec3V V3LoadUnsafeA(const PxVec3& f)
{
	ASSERT_ISALIGNED16(&f);
	return _mm_set_ps(0.0f, f.z, f.y, f.x);
}

PX_FORCE_INLINE Vec3V V3LoadA(const PxF32* const f)
{
	ASSERT_ISALIGNED16(f);
#if !PX_EMSCRIPTEN
	return _mm_and_ps(V4LoadA(f), V4LoadA(internalSimd::gMaskXYZ));
#else
	return _mm_and_ps((Vec3V&)*f, (VecI32V&)internalSimd::gMaskXYZ);
#endif
}

PX_FORCE_INLINE void I4StoreA(const VecI32V iv, PxI32* i)
{
	ASSERT_ISALIGNED16(i);
	_mm_store_ps(reinterpret_cast<float*>(i), internalSimd::m128_I2F(iv));
}

PX_FORCE_INLINE BoolV BLoad(const bool* const f)
{
	const PX_ALIGN(16, PxI32) b[4] = { -PxI32(f[0]), -PxI32(f[1]), -PxI32(f[2]), -PxI32(f[3]) };
	return _mm_load_ps(reinterpret_cast<const float*>(&b));
}

PX_FORCE_INLINE void V3StoreA(const Vec3V a, PxVec3& f)
{
	ASSERT_ISALIGNED16(&f);
	PX_ALIGN(16, PxF32) f2[4];
	_mm_store_ps(f2, a);
	f = PxVec3(f2[0], f2[1], f2[2]);
}

PX_FORCE_INLINE void V3StoreU(const Vec3V a, PxVec3& f)
{
	PX_ALIGN(16, PxF32) f2[4];
	_mm_store_ps(f2, a);
	f = PxVec3(f2[0], f2[1], f2[2]);
}

//////////////////////////////////
// FLOATV
//////////////////////////////////

PX_FORCE_INLINE FloatV FAbs(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);
	PX_ALIGN(16, const PxU32) absMask[4] = { 0x7fFFffFF, 0x7fFFffFF, 0x7fFFffFF, 0x7fFFffFF };
	return _mm_and_ps(a, _mm_load_ps(reinterpret_cast<const PxF32*>(absMask)));
}

//////////////////////////////////
// VEC3V
//////////////////////////////////

PX_FORCE_INLINE Vec3V V3UnitX()
{
	const PX_ALIGN(16, PxF32) x[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	const __m128 x128 = _mm_load_ps(x);
	return x128;
}

PX_FORCE_INLINE Vec3V V3UnitY()
{
	const PX_ALIGN(16, PxF32) y[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
	const __m128 y128 = _mm_load_ps(y);
	return y128;
}

PX_FORCE_INLINE Vec3V V3UnitZ()
{
	const PX_ALIGN(16, PxF32) z[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	const __m128 z128 = _mm_load_ps(z);
	return z128;
}

//////////////////////////////////
// VEC4V
//////////////////////////////////

PX_FORCE_INLINE Vec4V V4UnitW()
{
	const PX_ALIGN(16, PxF32) w[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const __m128 w128 = _mm_load_ps(w);
	return w128;
}

PX_FORCE_INLINE Vec4V V4UnitX()
{
	const PX_ALIGN(16, PxF32) x[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
	const __m128 x128 = _mm_load_ps(x);
	return x128;
}

PX_FORCE_INLINE Vec4V V4UnitY()
{
	const PX_ALIGN(16, PxF32) y[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
	const __m128 y128 = _mm_load_ps(y);
	return y128;
}

PX_FORCE_INLINE Vec4V V4UnitZ()
{
	const PX_ALIGN(16, PxF32) z[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	const __m128 z128 = _mm_load_ps(z);
	return z128;
}

PX_FORCE_INLINE Vec4V V4ClearW(const Vec4V v)
{
#if !PX_EMSCRIPTEN
	return _mm_and_ps(v, V4LoadA(internalSimd::gMaskXYZ));
#else
	return _mm_and_ps(v, (VecI32V&)internalSimd::gMaskXYZ);
#endif
}

//////////////////////////////////
// BoolV
//////////////////////////////////

/*
template<int index> PX_FORCE_INLINE BoolV BSplatElement(BoolV a)
{
    BoolV result;
    result[0] = result[1] = result[2] = result[3] = a[index];
    return result;
}
*/

template <int index>
BoolV BSplatElement(BoolV a)
{
	float* data = reinterpret_cast<float*>(&a);
	return V4Load(data[index]);
}

//////////////////////////////////
// MAT33V
//////////////////////////////////

PX_FORCE_INLINE Vec3V M33TrnspsMulV3(const Mat33V& a, const Vec3V b)
{
	const FloatV x = V3Dot(a.col0, b);
	const FloatV y = V3Dot(a.col1, b);
	const FloatV z = V3Dot(a.col2, b);
	return V3Merge(x, y, z);
}

PX_FORCE_INLINE Mat33V M33Trnsps(const Mat33V& a)
{
	return Mat33V(V3Merge(V3GetX(a.col0), V3GetX(a.col1), V3GetX(a.col2)),
	              V3Merge(V3GetY(a.col0), V3GetY(a.col1), V3GetY(a.col2)),
	              V3Merge(V3GetZ(a.col0), V3GetZ(a.col1), V3GetZ(a.col2)));
}

/*PX_FORCE_INLINE Mat33V PromoteVec3V(const Vec3V v)
{
	const BoolV bTFFF = BTFFF();
	const BoolV bFTFF = BFTFF();
	const BoolV bFFTF = BTFTF();

	const Vec3V zero = V3Zero();

	return Mat33V(V3Sel(bTFFF, v, zero), V3Sel(bFTFF, v, zero), V3Sel(bFFTF, v, zero));
}*/

//////////////////////////////////
// MAT34V
//////////////////////////////////

PX_FORCE_INLINE Vec3V M34TrnspsMul33V3(const Mat34V& a, const Vec3V b)
{
	const FloatV x = V3Dot(a.col0, b);
	const FloatV y = V3Dot(a.col1, b);
	const FloatV z = V3Dot(a.col2, b);
	return V3Merge(x, y, z);
}

PX_FORCE_INLINE Mat33V M34Trnsps33(const Mat34V& a)
{
	return Mat33V(V3Merge(V3GetX(a.col0), V3GetX(a.col1), V3GetX(a.col2)),
	              V3Merge(V3GetY(a.col0), V3GetY(a.col1), V3GetY(a.col2)),
	              V3Merge(V3GetZ(a.col0), V3GetZ(a.col1), V3GetZ(a.col2)));
}

//////////////////////////////////
// MAT44V
//////////////////////////////////

PX_FORCE_INLINE Vec4V M44TrnspsMulV4(const Mat44V& a, const Vec4V b)
{
	PX_ALIGN(16, FloatV) dotProdArray[4] = { V4Dot(a.col0, b), V4Dot(a.col1, b), V4Dot(a.col2, b), V4Dot(a.col3, b) };
	return V4Merge(dotProdArray);
}

PX_FORCE_INLINE Mat44V M44Trnsps(const Mat44V& a)
{
	const Vec4V v0 = _mm_unpacklo_ps(a.col0, a.col2);
	const Vec4V v1 = _mm_unpackhi_ps(a.col0, a.col2);
	const Vec4V v2 = _mm_unpacklo_ps(a.col1, a.col3);
	const Vec4V v3 = _mm_unpackhi_ps(a.col1, a.col3);
	return Mat44V(_mm_unpacklo_ps(v0, v2), _mm_unpackhi_ps(v0, v2), _mm_unpacklo_ps(v1, v3), _mm_unpackhi_ps(v1, v3));
}

//////////////////////////////////
// Misc
//////////////////////////////////

/*
// AP: work in progress - use proper SSE intrinsics where possible
PX_FORCE_INLINE VecU16V V4U32PK(VecU32V a, VecU32V b)
{
    VecU16V result;
    result.m128_u16[0] = PxU16(PxClamp<PxU32>((a).m128_u32[0], 0, 0xFFFF));
    result.m128_u16[1] = PxU16(PxClamp<PxU32>((a).m128_u32[1], 0, 0xFFFF));
    result.m128_u16[2] = PxU16(PxClamp<PxU32>((a).m128_u32[2], 0, 0xFFFF));
    result.m128_u16[3] = PxU16(PxClamp<PxU32>((a).m128_u32[3], 0, 0xFFFF));
    result.m128_u16[4] = PxU16(PxClamp<PxU32>((b).m128_u32[0], 0, 0xFFFF));
    result.m128_u16[5] = PxU16(PxClamp<PxU32>((b).m128_u32[1], 0, 0xFFFF));
    result.m128_u16[6] = PxU16(PxClamp<PxU32>((b).m128_u32[2], 0, 0xFFFF));
    result.m128_u16[7] = PxU16(PxClamp<PxU32>((b).m128_u32[3], 0, 0xFFFF));
    return result;
}
*/

/*
PX_FORCE_INLINE VecU16V V4U16Or(VecU16V a, VecU16V b)
{
    return m128_I2F(_mm_or_si128(m128_F2I(a), m128_F2I(b)));
}
*/

/*
PX_FORCE_INLINE VecU16V V4U16And(VecU16V a, VecU16V b)
{
    return m128_I2F(_mm_and_si128(m128_F2I(a), m128_F2I(b)));
}
*/

/*
PX_FORCE_INLINE VecU16V V4U16Andc(VecU16V a, VecU16V b)
{
    return m128_I2F(_mm_andnot_si128(m128_F2I(b), m128_F2I(a)));
}
*/

PX_FORCE_INLINE VecI32V I4LoadXYZW(const PxI32& x, const PxI32& y, const PxI32& z, const PxI32& w)
{
	return _mm_set_epi32(w, z, y, x);
}

PX_FORCE_INLINE VecI32V I4Load(const PxI32 i)
{
	return internalSimd::m128_F2I(_mm_load1_ps(reinterpret_cast<const PxF32*>(&i)));
}

PX_FORCE_INLINE VecI32V I4LoadU(const PxI32* i)
{
	return internalSimd::m128_F2I(_mm_loadu_ps(reinterpret_cast<const PxF32*>(i)));
}

PX_FORCE_INLINE VecI32V I4LoadA(const PxI32* i)
{
	ASSERT_ISALIGNED16(i);
	return internalSimd::m128_F2I(_mm_load_ps(reinterpret_cast<const PxF32*>(i)));
}

PX_FORCE_INLINE VecI32V VecI32V_Add(const VecI32VArg a, const VecI32VArg b)
{
	return _mm_add_epi32(a, b);
}

PX_FORCE_INLINE VecI32V VecI32V_Sub(const VecI32VArg a, const VecI32VArg b)
{
	return _mm_sub_epi32(a, b);
}

PX_FORCE_INLINE BoolV VecI32V_IsGrtr(const VecI32VArg a, const VecI32VArg b)
{
	return internalSimd::m128_I2F(_mm_cmpgt_epi32(a, b));
}

PX_FORCE_INLINE BoolV VecI32V_IsEq(const VecI32VArg a, const VecI32VArg b)
{
	return internalSimd::m128_I2F(_mm_cmpeq_epi32(a, b));
}

PX_FORCE_INLINE VecI32V V4I32Sel(const BoolV c, const VecI32V a, const VecI32V b)
{
	return _mm_or_si128(_mm_andnot_si128(internalSimd::m128_F2I(c), b), _mm_and_si128(internalSimd::m128_F2I(c), a));
}

PX_FORCE_INLINE VecI32V VecI32V_Zero()
{
	return _mm_setzero_si128();
}

PX_FORCE_INLINE VecI32V VecI32V_Sel(const BoolV c, const VecI32VArg a, const VecI32VArg b)
{
	return _mm_or_si128(_mm_andnot_si128(internalSimd::m128_F2I(c), b), _mm_and_si128(internalSimd::m128_F2I(c), a));
}

PX_FORCE_INLINE VecShiftV VecI32V_PrepareShift(const VecI32VArg shift)
{
	VecShiftV s;
	s.shift = VecI32V_Sel(BTFFF(), shift, VecI32V_Zero());
	return s;
}

PX_FORCE_INLINE VecI32V VecI32V_LeftShift(const VecI32VArg a, const VecShiftVArg count)
{
	return _mm_sll_epi32(a, count.shift);
}

PX_FORCE_INLINE VecI32V VecI32V_RightShift(const VecI32VArg a, const VecShiftVArg count)
{
	return _mm_srl_epi32(a, count.shift);
}

PX_FORCE_INLINE VecI32V VecI32V_LeftShift(const VecI32VArg a, const PxU32 count)
{
	return _mm_slli_epi32(a, PxI32(count));
}

PX_FORCE_INLINE VecI32V VecI32V_RightShift(const VecI32VArg a, const PxU32 count)
{
	return _mm_srai_epi32(a, PxI32(count));
}

PX_FORCE_INLINE VecI32V VecI32V_And(const VecI32VArg a, const VecI32VArg b)
{
	return _mm_and_si128(a, b);
}

PX_FORCE_INLINE VecI32V VecI32V_Or(const VecI32VArg a, const VecI32VArg b)
{
	return _mm_or_si128(a, b);
}

PX_FORCE_INLINE VecI32V VecI32V_GetX(const VecI32VArg a)
{
	return internalSimd::m128_F2I(_mm_shuffle_ps(internalSimd::m128_I2F(a), internalSimd::m128_I2F(a), _MM_SHUFFLE(0, 0, 0, 0)));
}

PX_FORCE_INLINE VecI32V VecI32V_GetY(const VecI32VArg a)
{
	return internalSimd::m128_F2I(_mm_shuffle_ps(internalSimd::m128_I2F(a), internalSimd::m128_I2F(a), _MM_SHUFFLE(1, 1, 1, 1)));
}

PX_FORCE_INLINE VecI32V VecI32V_GetZ(const VecI32VArg a)
{
	return internalSimd::m128_F2I(_mm_shuffle_ps(internalSimd::m128_I2F(a), internalSimd::m128_I2F(a), _MM_SHUFFLE(2, 2, 2, 2)));
}

PX_FORCE_INLINE VecI32V VecI32V_GetW(const VecI32VArg a)
{
	return internalSimd::m128_F2I(_mm_shuffle_ps(internalSimd::m128_I2F(a), internalSimd::m128_I2F(a), _MM_SHUFFLE(3, 3, 3, 3)));
}

PX_FORCE_INLINE void PxI32_From_VecI32V(const VecI32VArg a, PxI32* i)
{
	_mm_store_ss(reinterpret_cast<PxF32*>(i), internalSimd::m128_I2F(a));
}

PX_FORCE_INLINE VecI32V VecI32V_From_BoolV(const BoolVArg a)
{
	return internalSimd::m128_F2I(a);
}

PX_FORCE_INLINE VecU32V VecU32V_From_BoolV(const BoolVArg a)
{
	return a;
}

PX_FORCE_INLINE VecI32V VecI32V_Merge(const VecI32VArg x, const VecI32VArg y, const VecI32VArg z, const VecI32VArg w)
{
	const __m128 xw = _mm_move_ss(internalSimd::m128_I2F(y), internalSimd::m128_I2F(x)); // y, y, y, x
	const __m128 yz = _mm_move_ss(internalSimd::m128_I2F(z), internalSimd::m128_I2F(w)); // z, z, z, w
	return internalSimd::m128_F2I(_mm_shuffle_ps(xw, yz, _MM_SHUFFLE(0, 2, 1, 0)));
}

/*
template<int a> PX_FORCE_INLINE VecI32V V4ISplat()
{
    VecI32V result;
    result.m128_i32[0] = a;
    result.m128_i32[1] = a;
    result.m128_i32[2] = a;
    result.m128_i32[3] = a;
    return result;
}

template<PxU32 a> PX_FORCE_INLINE VecU32V V4USplat()
{
    VecU32V result;
    result.m128_u32[0] = a;
    result.m128_u32[1] = a;
    result.m128_u32[2] = a;
    result.m128_u32[3] = a;
    return result;
}
*/

/*
PX_FORCE_INLINE void V4U16StoreAligned(VecU16V val, VecU16V* address)
{
    *address = val;
}
*/

PX_FORCE_INLINE void V4U32StoreAligned(VecU32V val, VecU32V* address)
{
	*address = val;
}

/*PX_FORCE_INLINE Vec4V V4LoadAligned(Vec4V* addr)
{
	return *addr;
}

PX_FORCE_INLINE Vec4V V4LoadUnaligned(Vec4V* addr)
{
	return V4LoadU(reinterpret_cast<float*>(addr));
}*/

PX_FORCE_INLINE Vec4V Vec4V_From_VecI32V(VecI32V in)
{
	return _mm_cvtepi32_ps(in);
}

PX_FORCE_INLINE VecI32V VecI32V_From_Vec4V(Vec4V a)
{
	return _mm_cvttps_epi32(a);
}

PX_FORCE_INLINE Vec4V Vec4V_ReinterpretFrom_VecU32V(VecU32V a)
{
	return Vec4V(a);
}

PX_FORCE_INLINE Vec4V Vec4V_ReinterpretFrom_VecI32V(VecI32V a)
{
	return internalSimd::m128_I2F(a);
}

PX_FORCE_INLINE VecU32V VecU32V_ReinterpretFrom_Vec4V(Vec4V a)
{
	return VecU32V(a);
}

PX_FORCE_INLINE VecI32V VecI32V_ReinterpretFrom_Vec4V(Vec4V a)
{
	return internalSimd::m128_F2I(a);
}

template <int index>
PX_FORCE_INLINE VecU32V V4U32SplatElement(VecU32V a)
{
	VecU32V result;
	result.m128_u32[0] = result.m128_u32[1] = result.m128_u32[2] = result.m128_u32[3] = a.m128_u32[index];
	return result;
}

template <int index>
PX_FORCE_INLINE Vec4V V4SplatElement(Vec4V a)
{
	float* data = reinterpret_cast<float*>(&a);
	return V4Load(data[index]);
}

/*PX_FORCE_INLINE Vec4V V4Ceil(const Vec4V in)
{
	UnionM128 a(in);
	return V4LoadXYZW(PxCeil(a.m128_f32[0]), PxCeil(a.m128_f32[1]), PxCeil(a.m128_f32[2]), PxCeil(a.m128_f32[3]));
}

PX_FORCE_INLINE Vec4V V4Floor(const Vec4V in)
{
	UnionM128 a(in);
	return V4LoadXYZW(PxFloor(a.m128_f32[0]), PxFloor(a.m128_f32[1]), PxFloor(a.m128_f32[2]), PxFloor(a.m128_f32[3]));
}

PX_FORCE_INLINE VecU32V V4ConvertToU32VSaturate(const Vec4V in, PxU32 power)
{
	PX_ASSERT(power == 0 && "Non-zero power not supported in convertToU32VSaturate");
	PX_UNUSED(power); // prevent warning in release builds
	PxF32 ffffFFFFasFloat = PxF32(0xFFFF0000);
	UnionM128 a(in);
	VecU32V result;
	result.m128_u32[0] = PxU32(PxClamp<PxF32>((a).m128_f32[0], 0.0f, ffffFFFFasFloat));
	result.m128_u32[1] = PxU32(PxClamp<PxF32>((a).m128_f32[1], 0.0f, ffffFFFFasFloat));
	result.m128_u32[2] = PxU32(PxClamp<PxF32>((a).m128_f32[2], 0.0f, ffffFFFFasFloat));
	result.m128_u32[3] = PxU32(PxClamp<PxF32>((a).m128_f32[3], 0.0f, ffffFFFFasFloat));
	return result;
}*/

} // namespace aos
} // namespace physx

#endif // PXFOUNDATION_PXUNIXSSE2INLINEAOS_H

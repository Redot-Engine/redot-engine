// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_WINDOWS_INLINE_AOS_H
#define PX_WINDOWS_INLINE_AOS_H

namespace physx
{
namespace aos
{
//////////////////////////////////////////////////////////////////////
//Test that Vec3V and FloatV are legal
//////////////////////////////////////////////////////////////////////

PX_FORCE_INLINE bool isValidFloatV(const FloatV a)
{
	const PxF32 x = V4ReadX(a);
	const PxF32 y = V4ReadY(a);
	const PxF32 z = V4ReadZ(a);
	const PxF32 w = V4ReadW(a);

	return (!(x != y || x != z || x != w));
}

}
}

#include "../PxVecMathSSE.h"

namespace physx
{
namespace aos
{

/////////////////////////////////////////////////////////////////////
////FUNCTIONS USED ONLY FOR ASSERTS IN VECTORISED IMPLEMENTATIONS
/////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// USED ONLY INTERNALLY
//////////////////////////////////////////////////////////////////////

namespace internalSimd
{
const PX_ALIGN(16, PxU32 gMaskXYZ[4]) = { 0xffffffff, 0xffffffff, 0xffffffff, 0 };
} //internalSimd

namespace vecMathTests
{
PX_FORCE_INLINE bool allElementsEqualBoolV(const BoolV a, const BoolV b)
{
	return internalSimd::BAllTrue4_R(VecI32V_IsEq(a, b)) != 0;
}

PX_FORCE_INLINE bool allElementsEqualVecI32V(const VecI32V a, const VecI32V b)
{
	BoolV c = internalSimd::m128_I2F(_mm_cmpeq_epi32(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
	return internalSimd::BAllTrue4_R(c) != 0;
}

#define VECMATH_AOS_EPSILON (1e-3f)
static const FloatV minFError = FLoad(-VECMATH_AOS_EPSILON);
static const FloatV maxFError = FLoad(VECMATH_AOS_EPSILON);
static const Vec3V minV3Error = V3Load(-VECMATH_AOS_EPSILON);
static const Vec3V maxV3Error = V3Load(VECMATH_AOS_EPSILON);
static const Vec4V minV4Error = V4Load(-VECMATH_AOS_EPSILON);
static const Vec4V maxV4Error = V4Load(VECMATH_AOS_EPSILON);

PX_FORCE_INLINE bool allElementsNearEqualFloatV(const FloatV a, const FloatV b)
{
	ASSERT_ISVALIDFLOATV(a);
	ASSERT_ISVALIDFLOATV(b);
	const FloatV c = FSub(a, b);
	return _mm_comigt_ss(c, minFError) && _mm_comilt_ss(c, maxFError);
}

PX_FORCE_INLINE bool allElementsNearEqualVec3V(const Vec3V a, const Vec3V b)
{
	const Vec3V c = V3Sub(a, b);
	return (_mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)), minV3Error) &&
			_mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)), maxV3Error) &&
			_mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1)), minV3Error) &&
			_mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1)), maxV3Error) &&
			_mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2)), minV3Error) &&
			_mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2)), maxV3Error));
}

PX_FORCE_INLINE bool allElementsNearEqualVec4V(const Vec4V a, const Vec4V b)
{
	const Vec4V c = V4Sub(a, b);
	return (_mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)), minV4Error) &&
	        _mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)), maxV4Error) &&
	        _mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1)), minV4Error) &&
	        _mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1)), maxV4Error) &&
	        _mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2)), minV4Error) &&
	        _mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2)), maxV4Error) &&
	        _mm_comigt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 3, 3, 3)), minV4Error) &&
	        _mm_comilt_ss(_mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 3, 3, 3)), maxV4Error));
}
} //vecMathTests

PX_FORCE_INLINE bool isFiniteFloatV(const FloatV a)
{
	PxF32 f;
	FStore(a, &f);
	return PxIsFinite(f);
	/*
	const PxU32 badNumber = (_FPCLASS_SNAN | _FPCLASS_QNAN | _FPCLASS_NINF | _FPCLASS_PINF);
	const FloatV vBadNum = FloatV_From_F32((PxF32&)badNumber);
	const BoolV vMask = BAnd(vBadNum, a);
	return FiniteTestEq(vMask, BFFFF()) == 1;
	*/
}

PX_FORCE_INLINE bool isFiniteVec3V(const Vec3V a)
{
	PX_ALIGN(16, PxF32 f[4]);
	V4StoreA((Vec4V&)a, f);
	return PxIsFinite(f[0]) && PxIsFinite(f[1]) && PxIsFinite(f[2]);

	/*
	const PxU32 badNumber = (_FPCLASS_SNAN | _FPCLASS_QNAN | _FPCLASS_NINF | _FPCLASS_PINF);
	const Vec3V vBadNum = Vec3V_From_F32((PxF32&)badNumber);
	const BoolV vMask = BAnd(BAnd(vBadNum, a), BTTTF());
	return FiniteTestEq(vMask, BFFFF()) == 1;
	*/
}

PX_FORCE_INLINE bool isFiniteVec4V(const Vec4V a)
{
	PX_ALIGN(16, PxF32 f[4]);
	V4StoreA(a, f);
	return PxIsFinite(f[0]) && PxIsFinite(f[1]) && PxIsFinite(f[2]) && PxIsFinite(f[3]);

	/*
	const PxU32 badNumber = (_FPCLASS_SNAN | _FPCLASS_QNAN | _FPCLASS_NINF | _FPCLASS_PINF);
	const Vec4V vBadNum = Vec4V_From_U32((PxF32&)badNumber);
	const BoolV vMask = BAnd(vBadNum, a);

	return FiniteTestEq(vMask, BFFFF()) == 1;
	*/
}

/////////////////////////////////////////////////////////////////////
////VECTORISED FUNCTION IMPLEMENTATIONS
/////////////////////////////////////////////////////////////////////

PX_FORCE_INLINE Vec3V V3LoadA(const PxVec3& f)
{
	ASSERT_ISALIGNED16(&f);
	return _mm_and_ps(_mm_load_ps(&f.x), reinterpret_cast<const Vec4V&>(internalSimd::gMaskXYZ));
}

// w component of result is undefined
PX_FORCE_INLINE Vec3V V3LoadUnsafeA(const PxVec3& f)
{
	ASSERT_ISALIGNED16(&f);
	return _mm_load_ps(&f.x);
}

PX_FORCE_INLINE Vec3V V3LoadA(const PxF32* const f)
{
	ASSERT_ISALIGNED16(f);
	return V4ClearW(_mm_load_ps(f));
}

PX_FORCE_INLINE void I4StoreA(const VecI32V iv, PxI32* i)
{
	ASSERT_ISALIGNED16(i);
	_mm_store_ps((PxF32*)i, iv);
}

PX_FORCE_INLINE BoolV BLoad(const bool* const f)
{
	const PX_ALIGN(16, PxU32 b[4]) = { PxU32(-(PxI32)f[0]), PxU32(-(PxI32)f[1]),
		                               PxU32(-(PxI32)f[2]), PxU32(-(PxI32)f[3]) };
	return _mm_load_ps((float*)&b);
}

PX_FORCE_INLINE void V3StoreA(const Vec3V a, PxVec3& f)
{
	ASSERT_ISALIGNED16(&f);
	PX_ALIGN(16, PxF32 f2[4]);
	_mm_store_ps(f2, a);
	f = PxVec3(f2[0], f2[1], f2[2]);
}

PX_FORCE_INLINE void V3StoreU(const Vec3V a, PxVec3& f)
{
	PX_ALIGN(16, PxF32 f2[4]);
	_mm_store_ps(f2, a);
	f = PxVec3(f2[0], f2[1], f2[2]);
}

//////////////////////////////////
// FLOATV
//////////////////////////////////

PX_FORCE_INLINE FloatV FAbs(const FloatV a)
{
	ASSERT_ISVALIDFLOATV(a);
	PX_ALIGN(16, const static PxU32 absMask[4]) = { 0x7fFFffFF, 0x7fFFffFF, 0x7fFFffFF, 0x7fFFffFF };
	return _mm_and_ps(a, _mm_load_ps(reinterpret_cast<const PxF32*>(absMask)));
}

//////////////////////////////////
// VEC3V
//////////////////////////////////

PX_FORCE_INLINE Vec3V V3UnitX()
{
	const PX_ALIGN(16, PxF32 x[4]) = { 1.0f, 0.0f, 0.0f, 0.0f };
	const __m128 x128 = _mm_load_ps(x);
	return x128;
}

PX_FORCE_INLINE Vec3V V3UnitY()
{
	const PX_ALIGN(16, PxF32 y[4]) = { 0.0f, 1.0f, 0.0f, 0.0f };
	const __m128 y128 = _mm_load_ps(y);
	return y128;
}

PX_FORCE_INLINE Vec3V V3UnitZ()
{
	const PX_ALIGN(16, PxF32 z[4]) = { 0.0f, 0.0f, 1.0f, 0.0f };
	const __m128 z128 = _mm_load_ps(z);
	return z128;
}

//////////////////////////////////
// VEC4V
//////////////////////////////////

PX_FORCE_INLINE Vec4V V4UnitW()
{
	const PX_ALIGN(16, PxF32 w[4]) = { 0.0f, 0.0f, 0.0f, 1.0f };
	const __m128 w128 = _mm_load_ps(w);
	return w128;
}

PX_FORCE_INLINE Vec4V V4UnitX()
{
	const PX_ALIGN(16, PxF32 x[4]) = { 1.0f, 0.0f, 0.0f, 0.0f };
	const __m128 x128 = _mm_load_ps(x);
	return x128;
}

PX_FORCE_INLINE Vec4V V4UnitY()
{
	const PX_ALIGN(16, PxF32 y[4]) = { 0.0f, 1.0f, 0.0f, 0.0f };
	const __m128 y128 = _mm_load_ps(y);
	return y128;
}

PX_FORCE_INLINE Vec4V V4UnitZ()
{
	const PX_ALIGN(16, PxF32 z[4]) = { 0.0f, 0.0f, 1.0f, 0.0f };
	const __m128 z128 = _mm_load_ps(z);
	return z128;
}

PX_FORCE_INLINE Vec4V V4ClearW(const Vec4V v)
{
	return _mm_and_ps(v, (VecI32V&)internalSimd::gMaskXYZ);
}

//////////////////////////////////
// BoolV
//////////////////////////////////

template <int index>
BoolV BSplatElement(BoolV a)
{
	return internalSimd::m128_I2F(
	    _mm_shuffle_epi32(internalSimd::m128_F2I(a), _MM_SHUFFLE(index, index, index, index)));
}

//////////////////////////////////
// MAT33V
//////////////////////////////////

PX_FORCE_INLINE Vec3V M33TrnspsMulV3(const Mat33V& a, const Vec3V b)
{
	Vec3V v0 = V3Mul(a.col0, b);
	Vec3V v1 = V3Mul(a.col1, b);
	Vec3V v2 = V3Mul(a.col2, b);
	V3Transpose(v0, v1, v2);
	return V3Add(V3Add(v0, v1), v2);
}

PX_FORCE_INLINE Mat33V M33Trnsps(const Mat33V& a)
{
	Vec3V col0 = a.col0, col1 = a.col1, col2 = a.col2;
	V3Transpose(col0, col1, col2);
	return Mat33V(col0, col1, col2);
}

//////////////////////////////////
// MAT34V
//////////////////////////////////

PX_FORCE_INLINE Vec3V M34TrnspsMul33V3(const Mat34V& a, const Vec3V b)
{
	Vec3V v0 = V3Mul(a.col0, b);
	Vec3V v1 = V3Mul(a.col1, b);
	Vec3V v2 = V3Mul(a.col2, b);
	V3Transpose(v0, v1, v2);
	return V3Add(V3Add(v0, v1), v2);
}

PX_FORCE_INLINE Mat33V M34Trnsps33(const Mat34V& a)
{
	Vec3V col0 = a.col0, col1 = a.col1, col2 = a.col2;
	V3Transpose(col0, col1, col2);
	return Mat33V(col0, col1, col2);
}

/*PX_FORCE_INLINE Mat34V M34Inverse(const Mat34V& a)
{
	Mat34V aInv;
	const BoolV tfft = BTFFT();
	const BoolV tttf = BTTTF();
	const FloatV zero = V3Zero();
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
	aInv.col0 = _mm_mul_ps(colInv0, invDet);
	aInv.col1 = _mm_mul_ps(colInv1, invDet);
	aInv.col2 = _mm_mul_ps(colInv2, invDet);
	aInv.col3 = M34Mul33V3(aInv, V3Neg(a.col3));
	return aInv;
}*/

//////////////////////////////////
// MAT44V
//////////////////////////////////

PX_FORCE_INLINE Vec4V M44TrnspsMulV4(const Mat44V& a, const Vec4V b)
{
	Vec4V v0 = V4Mul(a.col0, b);
	Vec4V v1 = V4Mul(a.col1, b);
	Vec4V v2 = V4Mul(a.col2, b);
	Vec4V v3 = V4Mul(a.col3, b);
	V4Transpose(v0, v1, v2, v3);
	return V4Add(V4Add(v0, v1), V4Add(v2, v3));
}

PX_FORCE_INLINE Mat44V M44Trnsps(const Mat44V& a)
{
	Vec4V col0 = a.col0, col1 = a.col1, col2 = a.col2, col3 = a.col3;
	V4Transpose(col0, col1, col2, col3);
	return Mat44V(col0, col1, col2, col3);
}

//////////////////////////////////
// Misc
//////////////////////////////////

PX_FORCE_INLINE VecI32V I4LoadXYZW(const PxI32& x, const PxI32& y, const PxI32& z, const PxI32& w)
{
	return internalSimd::m128_I2F(_mm_set_epi32(w, z, y, x));
}

PX_FORCE_INLINE VecI32V I4Load(const PxI32 i)
{
	return _mm_load1_ps(reinterpret_cast<const PxF32*>(&i));
}

PX_FORCE_INLINE VecI32V I4LoadU(const PxI32* i)
{
	return _mm_loadu_ps(reinterpret_cast<const PxF32*>(i));
}

PX_FORCE_INLINE VecI32V I4LoadA(const PxI32* i)
{
	ASSERT_ISALIGNED16(i);
	return _mm_load_ps(reinterpret_cast<const PxF32*>(i));
}

PX_FORCE_INLINE VecI32V VecI32V_Add(const VecI32VArg a, const VecI32VArg b)
{
	return internalSimd::m128_I2F(_mm_add_epi32(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

PX_FORCE_INLINE VecI32V VecI32V_Sub(const VecI32VArg a, const VecI32VArg b)
{
	return internalSimd::m128_I2F(_mm_sub_epi32(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

PX_FORCE_INLINE BoolV VecI32V_IsGrtr(const VecI32VArg a, const VecI32VArg b)
{
	return internalSimd::m128_I2F(_mm_cmpgt_epi32(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

PX_FORCE_INLINE BoolV VecI32V_IsEq(const VecI32VArg a, const VecI32VArg b)
{
	return internalSimd::m128_I2F(_mm_cmpeq_epi32(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

PX_FORCE_INLINE VecI32V V4I32Sel(const BoolV c, const VecI32V a, const VecI32V b)
{
	return V4U32Sel(c, a, b);
}

PX_FORCE_INLINE VecI32V VecI32V_Zero()
{
	return V4Zero();	// PT: TODO: unify VecI32V / VecU32V on Windows / Linux, not the same types (?!)
}

PX_FORCE_INLINE VecI32V VecI32V_Sel(const BoolV c, const VecI32VArg a, const VecI32VArg b)
{
	PX_ASSERT(vecMathTests::allElementsEqualBoolV(c, BTTTT()) ||
			  vecMathTests::allElementsEqualBoolV(c, BFFFF()));
	return _mm_or_ps(_mm_andnot_ps(c, b), _mm_and_ps(c, a));
}

PX_FORCE_INLINE VecShiftV VecI32V_PrepareShift(const VecI32VArg shift)
{
	VecShiftV preparedShift;
	preparedShift.shift = _mm_or_ps(_mm_andnot_ps(BTFFF(), VecI32V_Zero()), _mm_and_ps(BTFFF(), shift)); 
	return preparedShift;
}

PX_FORCE_INLINE VecI32V VecI32V_LeftShift(const VecI32VArg a, const VecShiftVArg count)
{
	return internalSimd::m128_I2F(_mm_sll_epi32(internalSimd::m128_F2I(a), internalSimd::m128_F2I(count.shift)));
}

PX_FORCE_INLINE VecI32V VecI32V_RightShift(const VecI32VArg a, const VecShiftVArg count)
{
	return internalSimd::m128_I2F(_mm_srl_epi32(internalSimd::m128_F2I(a), internalSimd::m128_F2I(count.shift)));
}

PX_FORCE_INLINE VecI32V VecI32V_LeftShift(const VecI32VArg a, const PxU32 count)
{
	return internalSimd::m128_I2F(_mm_slli_epi32(internalSimd::m128_F2I(a), count));
}

PX_FORCE_INLINE VecI32V VecI32V_RightShift(const VecI32VArg a, const PxU32 count)
{
	return internalSimd::m128_I2F(_mm_srai_epi32(internalSimd::m128_F2I(a), count));
}

PX_FORCE_INLINE VecI32V VecI32V_And(const VecI32VArg a, const VecI32VArg b)
{
	return internalSimd::m128_I2F(_mm_and_si128(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

PX_FORCE_INLINE VecI32V VecI32V_Or(const VecI32VArg a, const VecI32VArg b)
{
	return internalSimd::m128_I2F(_mm_or_si128(internalSimd::m128_F2I(a), internalSimd::m128_F2I(b)));
}

PX_FORCE_INLINE VecI32V VecI32V_GetX(const VecI32VArg a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 0));
}

PX_FORCE_INLINE VecI32V VecI32V_GetY(const VecI32VArg a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1));
}

PX_FORCE_INLINE VecI32V VecI32V_GetZ(const VecI32VArg a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2));
}

PX_FORCE_INLINE VecI32V VecI32V_GetW(const VecI32VArg a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 3, 3, 3));
}

PX_FORCE_INLINE void PxI32_From_VecI32V(const VecI32VArg a, PxI32* i)
{
	_mm_store_ss(reinterpret_cast<PxF32*>(i), a);
}

PX_FORCE_INLINE VecI32V VecI32V_From_BoolV(const BoolVArg a)
{
	return a;
}

PX_FORCE_INLINE VecU32V VecU32V_From_BoolV(const BoolVArg a)
{
	return a;
}

PX_FORCE_INLINE VecI32V VecI32V_Merge(const VecI32VArg a, const VecI32VArg b, const VecI32VArg c, const VecI32VArg d)
{
	const __m128 xw = _mm_move_ss(b, a); // y, y, y, x
	const __m128 yz = _mm_move_ss(c, d); // z, z, z, w
	return _mm_shuffle_ps(xw, yz, _MM_SHUFFLE(0, 2, 1, 0));
}

PX_FORCE_INLINE void V4U32StoreAligned(VecU32V val, VecU32V* address)
{
	*address = val;
}

PX_FORCE_INLINE Vec4V Vec4V_From_VecI32V(VecI32V a)
{
	return _mm_cvtepi32_ps(internalSimd::m128_F2I(a));
}

PX_FORCE_INLINE VecI32V VecI32V_From_Vec4V(Vec4V a)
{
	return internalSimd::m128_I2F(_mm_cvttps_epi32(a));
}

PX_FORCE_INLINE Vec4V Vec4V_ReinterpretFrom_VecU32V(VecU32V a)
{
	return Vec4V(a);
}

PX_FORCE_INLINE Vec4V Vec4V_ReinterpretFrom_VecI32V(VecI32V a)
{
	return Vec4V(a);
}

PX_FORCE_INLINE VecU32V VecU32V_ReinterpretFrom_Vec4V(Vec4V a)
{
	return VecU32V(a);
}

PX_FORCE_INLINE VecI32V VecI32V_ReinterpretFrom_Vec4V(Vec4V a)
{
	return VecI32V(a);
}

template <int index>
PX_FORCE_INLINE VecU32V V4U32SplatElement(VecU32V a)
{
	return internalSimd::m128_I2F(_mm_shuffle_epi32(internalSimd::m128_F2I(a), _MM_SHUFFLE(index, index, index, index)));
}

template <int index>
PX_FORCE_INLINE Vec4V V4SplatElement(Vec4V a)
{
	return internalSimd::m128_I2F(_mm_shuffle_epi32(internalSimd::m128_F2I(a), _MM_SHUFFLE(index, index, index, index)));
}

/*PX_FORCE_INLINE Vec4V V4ConvertFromI32V(const VecI32V in)
{
	return _mm_cvtepi32_ps(internalSimd::m128_F2I(in));
}*/

} // namespace aos
} // namespace physx

#endif


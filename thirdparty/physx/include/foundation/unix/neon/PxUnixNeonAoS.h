// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PXFOUNDATION_PXUNIXNEONAOS_H
#define PXFOUNDATION_PXUNIXNEONAOS_H

// no includes here! this file should be included from PxcVecMath.h only!!!

#if !COMPILE_VECTOR_INTRINSICS
#error Vector intrinsics should not be included when using scalar implementation.
#endif

// only ARM NEON compatible platforms should reach this
#include <arm_neon.h>

namespace physx
{
namespace aos
{

typedef float32x2_t FloatV;
typedef float32x4_t Vec3V;
typedef float32x4_t Vec4V;
typedef uint32x4_t BoolV;
typedef float32x4_t QuatV;

typedef uint32x4_t VecU32V;
typedef int32x4_t VecI32V;
typedef uint16x8_t VecU16V;
typedef int16x8_t VecI16V;
typedef uint8x16_t VecU8V;

#define FloatVArg FloatV &
#define Vec3VArg Vec3V &
#define Vec4VArg Vec4V &
#define BoolVArg BoolV &
#define VecU32VArg VecU32V &
#define VecI32VArg VecI32V &
#define VecU16VArg VecU16V &
#define VecI16VArg VecI16V &
#define VecU8VArg VecU8V &
#define QuatVArg QuatV &

// KS - TODO - make an actual VecCrossV type for NEON
#define VecCrossV Vec3V

typedef VecI32V VecShiftV;
#define VecShiftVArg VecShiftV &

PX_ALIGN_PREFIX(16)
struct Mat33V
{
	Mat33V()
	{
	}
	Mat33V(const Vec3V& c0, const Vec3V& c1, const Vec3V& c2) : col0(c0), col1(c1), col2(c2)
	{
	}
	Vec3V PX_ALIGN(16, col0);
	Vec3V PX_ALIGN(16, col1);
	Vec3V PX_ALIGN(16, col2);
} PX_ALIGN_SUFFIX(16);

PX_ALIGN_PREFIX(16)
struct Mat34V
{
	Mat34V()
	{
	}
	Mat34V(const Vec3V& c0, const Vec3V& c1, const Vec3V& c2, const Vec3V& c3) : col0(c0), col1(c1), col2(c2), col3(c3)
	{
	}
	Vec3V PX_ALIGN(16, col0);
	Vec3V PX_ALIGN(16, col1);
	Vec3V PX_ALIGN(16, col2);
	Vec3V PX_ALIGN(16, col3);
} PX_ALIGN_SUFFIX(16);

PX_ALIGN_PREFIX(16)
struct Mat43V
{
	Mat43V()
	{
	}
	Mat43V(const Vec4V& c0, const Vec4V& c1, const Vec4V& c2) : col0(c0), col1(c1), col2(c2)
	{
	}
	Vec4V PX_ALIGN(16, col0);
	Vec4V PX_ALIGN(16, col1);
	Vec4V PX_ALIGN(16, col2);
} PX_ALIGN_SUFFIX(16);

PX_ALIGN_PREFIX(16)
struct Mat44V
{
	Mat44V()
	{
	}
	Mat44V(const Vec4V& c0, const Vec4V& c1, const Vec4V& c2, const Vec4V& c3) : col0(c0), col1(c1), col2(c2), col3(c3)
	{
	}
	Vec4V PX_ALIGN(16, col0);
	Vec4V PX_ALIGN(16, col1);
	Vec4V PX_ALIGN(16, col2);
	Vec4V PX_ALIGN(16, col3);
} PX_ALIGN_SUFFIX(16);

} // namespace aos
} // namespace physx

#endif // PXFOUNDATION_PXUNIXNEONAOS_H

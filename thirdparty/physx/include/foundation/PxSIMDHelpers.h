// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SIMD_HELPERS_H
#define PX_SIMD_HELPERS_H

#include "foundation/PxMat33.h"
#include "foundation/PxVecMath.h"
#include "foundation/PxTransform.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	//! A padded version of PxMat33, to safely load its data using SIMD
	class PxMat33Padded : public PxMat33
	{
	public:
		explicit PX_FORCE_INLINE PxMat33Padded(const PxQuat& q)
		{
			using namespace aos;
			const QuatV qV = V4LoadU(&q.x);
			Vec3V column0V, column1V, column2V;
			QuatGetMat33V(qV, column0V, column1V, column2V);
// The scalar AoS backend (COMPILE_VECTOR_INTRINSICS == 0, see PxVecMath.h) keeps Vec3V and
// Vec4V as distinct types, so the padded V4 stores below do not compile against it.
#if defined(PX_SIMD_DISABLED) || (PX_LINUX && (PX_ARM || PX_A64)) || !COMPILE_VECTOR_INTRINSICS
			V3StoreU(column0V, column0);
			V3StoreU(column1V, column1);
			V3StoreU(column2V, column2);
#else
			V4StoreU(column0V, &column0.x);
			V4StoreU(column1V, &column1.x);
			V4StoreU(column2V, &column2.x);
#endif
		}
		PX_FORCE_INLINE ~PxMat33Padded()				{}
		PX_FORCE_INLINE void operator=(const PxMat33& other)
		{
			column0 = other.column0;
			column1 = other.column1;
			column2 = other.column2;
		}
		PxU32	padding;
	};

#if !PX_DOXYGEN
namespace aos
{
#endif

	PX_FORCE_INLINE void transformKernelVec4(	const FloatVArg wa, const Vec4VArg va, const Vec4VArg pa, 
												const FloatVArg wb, const Vec4VArg vb, const Vec4VArg pb, 
												FloatV& wo, Vec4V& vo, Vec4V& po)
	{
		wo = FSub(FMul(wa, wb), V4Dot3(va, vb));
		vo = V4ScaleAdd(va, wb, V4ScaleAdd(vb, wa, V4Cross(va, vb)));

		const Vec4V t1 = V4Scale(pb, FScaleAdd(wa, wa, FLoad(-0.5f)));
		const Vec4V t2 = V4ScaleAdd(V4Cross(va, pb), wa, t1);
		const Vec4V t3 = V4ScaleAdd(va, V4Dot3(va, pb), t2);

		po = V4ScaleAdd(t3, FLoad(2.0f), pa);
	}

	// PT: out = a * b
	template<const bool alignedInput, const bool alignedOutput>
	PX_FORCE_INLINE void transformMultiply(PxTransform& out, const PxTransform& a, const PxTransform& b)
	{
		PX_ASSERT(!alignedInput || (size_t(&a)&15) == 0);
		PX_ASSERT(!alignedInput || (size_t(&b)&15) == 0);

		const Vec4V aPos = alignedInput ? V4LoadA(&a.p.x) : V4LoadU(&a.p.x);
		const Vec4V aRot = alignedInput ? V4LoadA(&a.q.x) : V4LoadU(&a.q.x);

		const Vec4V bPos = alignedInput ? V4LoadA(&b.p.x) : V4LoadU(&b.p.x);
		const Vec4V bRot = alignedInput ? V4LoadA(&b.q.x) : V4LoadU(&b.q.x);
	
		Vec4V v, p;
		FloatV w; 
		transformKernelVec4(V4GetW(aRot), aRot, aPos, V4GetW(bRot), bRot, bPos, w, v, p);

		if(alignedOutput)
		{
			PX_ASSERT((size_t(&out)&15) == 0);
			V4StoreA(p, &out.p.x);
			V4StoreA(V4SetW(v,w), &out.q.x);
		}
		else
		{
			V4StoreU(p, &out.p.x);
			V4StoreU(V4SetW(v,w), &out.q.x);
		}
	}

	// PT: out = a * b
	PX_FORCE_INLINE void transformMultiply(PxTransform32& out, const PxTransform32& a, const PxTransform32& b)
	{
		transformMultiply<true, true>(out, a, b);
	}

#if !PX_DOXYGEN
} // namespace aos
#endif

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

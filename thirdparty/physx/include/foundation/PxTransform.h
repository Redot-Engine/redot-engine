// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TRANSFORM_H
#define PX_TRANSFORM_H

#include "foundation/PxQuat.h"

#if !PX_DOXYGEN
namespace physx
{
#endif


template<class Type> class PxMat44T;

/*!
\brief class representing a rigid euclidean transform as a quaternion and a vector
*/

template<class Type>
class PxTransformT
{
  public:
	PxQuatT<Type>	q;
	PxVec3T<Type>	p;

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxTransformT()
	{
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE explicit PxTransformT(PxIDENTITY) : q(PxIdentity), p(PxZero)
	{
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE explicit PxTransformT(const PxVec3T<Type>& position) : q(PxIdentity), p(position)
	{
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE explicit PxTransformT(const PxQuatT<Type>& orientation) : q(orientation), p(Type(0))
	{
		PX_ASSERT(orientation.isSane());
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxTransformT(Type x, Type y, Type z, PxQuatT<Type> aQ = PxQuatT<Type>(PxIdentity)) : q(aQ), p(x, y, z)
	{
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxTransformT(const PxVec3T<Type>& p0, const PxQuatT<Type>& q0) : q(q0), p(p0)
	{
		PX_ASSERT(q0.isSane());
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE explicit PxTransformT(const PxMat44T<Type>& m); // defined in PxMat44.h

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxTransformT(const PxTransformT& other)
	{
		p = other.p;
		q = other.q;
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE void operator=(const PxTransformT& other)
	{
		p = other.p;
		q = other.q;
	}

	/**
	\brief returns true if the two transforms are exactly equal
	*/
	PX_CUDA_CALLABLE PX_INLINE bool operator==(const PxTransformT& t) const
	{
		return p == t.p && q == t.q;
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxTransformT operator*(const PxTransformT& x) const
	{
		PX_ASSERT(x.isSane());
		return transform(x);
	}

	//! Equals matrix multiplication
	PX_CUDA_CALLABLE PX_INLINE PxTransformT& operator*=(const PxTransformT& other)
	{
		*this = *this * other;
		return *this;
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxTransformT getInverse() const
	{
		PX_ASSERT(isFinite());
		return PxTransformT(q.rotateInv(-p), q.getConjugate());
	}

	/**
	\brief return a normalized transform (i.e. one in which the quaternion has unit magnitude)
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxTransformT getNormalized() const
	{
		return PxTransformT(p, q.getNormalized());
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxVec3T<Type> transform(const PxVec3T<Type>& input) const
	{
		PX_ASSERT(isFinite());
		return q.rotate(input) + p;
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxVec3T<Type> transformInv(const PxVec3T<Type>& input) const
	{
		PX_ASSERT(isFinite());
		return q.rotateInv(input - p);
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxVec3T<Type> rotate(const PxVec3T<Type>& input) const
	{
		PX_ASSERT(isFinite());
		return q.rotate(input);
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxVec3T<Type> rotateInv(const PxVec3T<Type>& input) const
	{
		PX_ASSERT(isFinite());
		return q.rotateInv(input);
	}

	//! Transform transform to parent (returns compound transform: first src, then *this)
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxTransformT transform(const PxTransformT& src) const
	{
		PX_ASSERT(src.isSane());
		PX_ASSERT(isSane());
		// src = [srct, srcr] -> [r*srct + t, r*srcr]
		return PxTransformT(q.rotate(src.p) + p, q * src.q);
	}

	//! Transform transform from parent (returns compound transform: first src, then this->inverse)
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxTransformT transformInv(const PxTransformT& src) const
	{
		PX_ASSERT(src.isSane());
		PX_ASSERT(isFinite());
		// src = [srct, srcr] -> [r^-1*(srct-t), r^-1*srcr]
		const PxQuatT<Type> qinv = q.getConjugate();
		return PxTransformT(qinv.rotate(src.p - p), qinv * src.q);
	}

	/**
	\brief returns true if finite and q is a unit quaternion
	*/
	PX_CUDA_CALLABLE bool isValid() const
	{
		return p.isFinite() && q.isFinite() && q.isUnit();
	}

	/**
	\brief returns true if finite and quat magnitude is reasonably close to unit to allow for some accumulation of error
	vs isValid
	*/
	PX_CUDA_CALLABLE bool isSane() const
	{
		return isFinite() && q.isSane();
	}

	/**
	\brief returns true if all elems are finite (not NAN or INF, etc.)
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool isFinite() const
	{
		return p.isFinite() && q.isFinite();
	}
};

typedef PxTransformT<float>		PxTransform;
typedef PxTransformT<double>	PxTransformd;

/*!
\brief	A generic padded & aligned transform class.

This can be used for safe faster loads & stores, and faster address computations
(the default PxTransformT often generating imuls for this otherwise). Padding bytes
can be reused to store useful data if needed.
*/
struct PX_ALIGN_PREFIX(16) PxTransformPadded : PxTransform
{
	PX_FORCE_INLINE PxTransformPadded()
	{
	}

	PX_FORCE_INLINE PxTransformPadded(const PxTransformPadded& other) : PxTransform(other)
	{
	}

	PX_FORCE_INLINE explicit PxTransformPadded(const PxTransform& other) : PxTransform(other)
	{
	}

	PX_FORCE_INLINE explicit PxTransformPadded(PxIDENTITY) : PxTransform(PxIdentity)
	{
	}

	PX_FORCE_INLINE explicit PxTransformPadded(const PxVec3& position) : PxTransform(position)
	{
	}

	PX_FORCE_INLINE explicit PxTransformPadded(const PxQuat& orientation) : PxTransform(orientation)
	{
	}

	PX_FORCE_INLINE PxTransformPadded(const PxVec3& p0, const PxQuat& q0) : PxTransform(p0, q0)
	{
	}

	PX_FORCE_INLINE void operator=(const PxTransformPadded& other)
	{
		p = other.p;
		q = other.q;
	}

	PX_FORCE_INLINE void operator=(const PxTransform& other)
	{
		p = other.p;
		q = other.q;
	}

	PxU32		padding;
}
PX_ALIGN_SUFFIX(16);
PX_COMPILE_TIME_ASSERT(sizeof(PxTransformPadded)==32);

typedef PxTransformPadded	PxTransform32;

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


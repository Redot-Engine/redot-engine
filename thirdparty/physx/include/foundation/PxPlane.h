// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PLANE_H
#define PX_PLANE_H


#include "foundation/PxTransform.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Representation of a plane.

 Plane equation used: n.dot(v) + d = 0
*/
class PxPlane
{
  public:
	/**
	\brief Constructor
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxPlane()
	{
	}

	/**
	\brief Constructor from a normal and a distance
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxPlane(float nx, float ny, float nz, float distance) : n(nx, ny, nz), d(distance)
	{
	}

	/**
	\brief Constructor from a normal and a distance
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxPlane(const PxVec3& normal, float distance) : n(normal), d(distance)
	{
	}

	/**
	\brief Constructor from a point on the plane and a normal
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxPlane(const PxVec3& point, const PxVec3& normal)
	: n(normal), d(-point.dot(n)) // p satisfies normal.dot(p) + d = 0
	{
	}

	/**
	\brief Constructor from three points
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxPlane(const PxVec3& p0, const PxVec3& p1, const PxVec3& p2)
	{
		n = (p1 - p0).cross(p2 - p0).getNormalized();
		d = -p0.dot(n);
	}

	/**
	\brief returns true if the two planes are exactly equal
	*/
	PX_CUDA_CALLABLE PX_INLINE bool operator==(const PxPlane& p) const
	{
		return n == p.n && d == p.d;
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE float distance(const PxVec3& p) const
	{
		return p.dot(n) + d;
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE bool contains(const PxVec3& p) const
	{
		return PxAbs(distance(p)) < (1.0e-7f);
	}

	/**
	\brief projects p into the plane
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxVec3 project(const PxVec3& p) const
	{
		return p - n * distance(p);
	}

	/**
	\brief find an arbitrary point in the plane
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxVec3 pointInPlane() const
	{
		return -n * d;
	}

	/**
	\brief equivalent plane with unit normal
	*/

	PX_CUDA_CALLABLE PX_FORCE_INLINE void normalize()
	{
		float denom = 1.0f / n.magnitude();
		n *= denom;
		d *= denom;
	}

	/**
	\brief transform plane
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxPlane transform(const PxTransform& pose) const
	{
		const PxVec3 transformedNormal = pose.rotate(n);
		return PxPlane(transformedNormal, d - pose.p.dot(transformedNormal));
	}

	/**
	\brief inverse-transform plane
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxPlane inverseTransform(const PxTransform& pose) const
	{
		const PxVec3 transformedNormal = pose.rotateInv(n);
		return PxPlane(transformedNormal, d + pose.p.dot(n));
	}

	PxVec3 n; //!< The normal to the plane
	float d;  //!< The distance from the origin
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


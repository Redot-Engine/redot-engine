// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TRIANGLE_H
#define PX_TRIANGLE_H

#include "common/PxPhysXCommonConfig.h"
#include "foundation/PxVec3.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Triangle class.
*/
class PxTriangle
{
	public:
	/**
	\brief Constructor
	*/
	PX_FORCE_INLINE			PxTriangle() {}

	/**
	\brief Constructor

	\param[in] p0 Point 0
	\param[in] p1 Point 1
	\param[in] p2 Point 2
	*/
	PX_FORCE_INLINE			PxTriangle(const PxVec3& p0, const PxVec3& p1, const PxVec3& p2)
	{
		verts[0] = p0;
		verts[1] = p1;
		verts[2] = p2;
	}

	/**
	\brief Copy constructor

	\param[in] triangle Tri to copy
	*/
	PX_FORCE_INLINE			PxTriangle(const PxTriangle& triangle)
	{
		verts[0] = triangle.verts[0];
		verts[1] = triangle.verts[1];
		verts[2] = triangle.verts[2];
	}

	/**
	\brief Destructor
	*/
	PX_FORCE_INLINE			~PxTriangle() {}

	/**
	\brief Assignment operator
	*/
	PX_FORCE_INLINE void operator=(const PxTriangle& triangle)
	{
		verts[0] = triangle.verts[0];
		verts[1] = triangle.verts[1];
		verts[2] = triangle.verts[2];
	}

	/**
	\brief Compute the normal of the Triangle.

	\param[out] _normal Triangle normal.
	*/
	PX_FORCE_INLINE	void	normal(PxVec3& _normal) const
	{
		_normal = (verts[1]-verts[0]).cross(verts[2]-verts[0]);
		_normal.normalize();
	}

	/**
	\brief Compute the unnormalized normal of the triangle.

	\param[out] _normal Triangle normal (not normalized).
	*/
	PX_FORCE_INLINE	void	denormalizedNormal(PxVec3& _normal) const
	{
		_normal = (verts[1]-verts[0]).cross(verts[2]-verts[0]);
	}

	/**
	\brief Compute the area of the triangle.

	\return Area of the triangle.
	*/
	PX_FORCE_INLINE	PxReal	area() const
	{
		const PxVec3& p0 = verts[0];
		const PxVec3& p1 = verts[1];
		const PxVec3& p2 = verts[2];
		return ((p0 - p1).cross(p0 - p2)).magnitude() * 0.5f;
	}

	/**
	\return Computes a point on the triangle from u and v barycentric coordinates.
	*/
	PX_FORCE_INLINE	PxVec3 pointFromUV(PxReal u, PxReal v)	const
	{
		return (1.0f-u-v)*verts[0] + u*verts[1] + v*verts[2];
	}

	/**
	\brief Array of Vertices.
	*/
	PxVec3		verts[3];
};

//! A padded version of PxTriangle, to safely load its data using SIMD
class PxTrianglePadded : public PxTriangle
{
public:
	PX_FORCE_INLINE PxTrianglePadded()	{}
	PX_FORCE_INLINE ~PxTrianglePadded()	{}
	PxU32	padding;
};

#if !PX_DOXYGEN
}
#endif

#endif

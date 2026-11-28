// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PLANE_GEOMETRY_H
#define PX_PLANE_GEOMETRY_H
#include "geometry/PxGeometry.h"
#include "foundation/PxFoundationConfig.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Class describing a plane geometry.

The plane geometry specifies the half-space volume x<=0. As with other geometry types, 
when used in a PxShape the collision volume is obtained by transforming the halfspace 
by the shape local pose and the actor global pose.

To generate a PxPlane from a PxTransform, transform PxPlane(1,0,0,0).

To generate a PxTransform from a PxPlane, use PxTransformFromPlaneEquation.

\see PxShape.setGeometry() PxShape.getPlaneGeometry() PxTransformFromPlaneEquation 
*/
class PxPlaneGeometry : public PxGeometry 
{
public:
	/**
	\brief Constructor.
	*/
	PX_INLINE PxPlaneGeometry() : PxGeometry(PxGeometryType::ePLANE) {}

	/**
	\brief Copy constructor.

	\param[in] that		Other object
	*/
	PX_INLINE PxPlaneGeometry(const PxPlaneGeometry& that) : PxGeometry(that) {}

	/**
	\brief Assignment operator
	*/
	PX_INLINE void operator=(const PxPlaneGeometry& that)
	{
		mType = that.mType;
	}

	/**
	\brief Returns true if the geometry is valid.

	\return True if the current settings are valid
	*/
	PX_INLINE bool isValid() const;
};

PX_INLINE bool PxPlaneGeometry::isValid() const
{
	if(mType != PxGeometryType::ePLANE)
		return false;

	return true;
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

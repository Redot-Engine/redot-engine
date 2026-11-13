// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CAPSULE_GEOMETRY_H
#define PX_CAPSULE_GEOMETRY_H
#include "geometry/PxGeometry.h"
#include "foundation/PxFoundationConfig.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Class representing the geometry of a capsule.

Capsules are shaped as the union of a cylinder of length 2 * halfHeight and with the 
given radius centered at the origin and extending along the x axis, and two hemispherical ends.
\note The scaling of the capsule is expected to be baked into these values, there is no additional scaling parameter.

The function PxTransformFromSegment is a helper for generating an appropriate transform for the capsule from the capsule's interior line segment.

\see PxTransformFromSegment
*/
class PxCapsuleGeometry : public PxGeometry      
{
public:
	/**
	\brief Constructor, initializes to a capsule with passed radius and half height.
	*/
	PX_INLINE PxCapsuleGeometry(PxReal radius_=0.0f, PxReal halfHeight_=0.0f) :	PxGeometry(PxGeometryType::eCAPSULE), radius(radius_), halfHeight(halfHeight_)	{}

	/**
	\brief Copy constructor.

	\param[in] that		Other object
	*/
	PX_INLINE PxCapsuleGeometry(const PxCapsuleGeometry& that) : PxGeometry(that), radius(that.radius), halfHeight(that.halfHeight)								{}

	/**
	\brief Assignment operator
	*/
	PX_INLINE void operator=(const PxCapsuleGeometry& that)
	{
		mType = that.mType;
		radius = that.radius;
		halfHeight = that.halfHeight;
	}

	/**
	\brief Returns true if the geometry is valid.

	\return True if the current settings are valid.

	\note A valid capsule has radius > 0, halfHeight >= 0.
	It is illegal to call PxPhysics::createShape with a capsule that has zero radius or height.

	\see PxPhysics::createShape
	*/
	PX_INLINE bool isValid() const;

public:
	/**
	\brief The radius of the capsule.
	*/
	PxReal radius;

	/**
	\brief half of the capsule's height, measured between the centers of the hemispherical ends.
	*/
	PxReal halfHeight;
};

PX_INLINE bool PxCapsuleGeometry::isValid() const
{
	if(mType != PxGeometryType::eCAPSULE)
		return false;
	if(!PxIsFinite(radius) || !PxIsFinite(halfHeight))
		return false;
	if(radius <= 0.0f || halfHeight < 0.0f)
		return false;

	return true;
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

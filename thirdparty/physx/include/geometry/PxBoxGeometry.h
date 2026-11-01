// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BOX_GEOMETRY_H
#define PX_BOX_GEOMETRY_H
#include "geometry/PxGeometry.h"
#include "foundation/PxVec3.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Class representing the geometry of a box.  

The geometry of a box can be fully specified by its half extents.  This is the half of its width, height, and depth.
\note The scaling of the box is expected to be baked into these values, there is no additional scaling parameter.
*/
class PxBoxGeometry : public PxGeometry 
{
public:
	/**
	\brief Constructor to initialize half extents from scalar parameters.
	\param hx Initial half extents' x component.
	\param hy Initial half extents' y component.
	\param hz Initial half extents' z component.
	*/
	PX_INLINE PxBoxGeometry(PxReal hx=0.0f, PxReal hy=0.0f, PxReal hz=0.0f) : PxGeometry(PxGeometryType::eBOX), halfExtents(hx, hy, hz)	{}

	/**
	\brief Constructor to initialize half extents from vector parameter.
	\param halfExtents_ Initial half extents.
	*/
	PX_INLINE PxBoxGeometry(PxVec3 halfExtents_) : PxGeometry(PxGeometryType::eBOX), halfExtents(halfExtents_)	{}

	/**
	\brief Copy constructor.

	\param[in] that		Other object
	*/
	PX_INLINE PxBoxGeometry(const PxBoxGeometry& that) : PxGeometry(that), halfExtents(that.halfExtents)		{}

	/**
	\brief Assignment operator
	*/
	PX_INLINE void operator=(const PxBoxGeometry& that)
	{
		mType = that.mType;
		halfExtents = that.halfExtents;
	}

	/**
	\brief Returns true if the geometry is valid.

	\return True if the current settings are valid

	\note A valid box has a positive extent in each direction (halfExtents.x > 0, halfExtents.y > 0, halfExtents.z > 0). 
	It is illegal to call PxPhysics::createShape with a box that has zero extent in any direction.

	\see PxPhysics::createShape
	*/
	PX_INLINE bool isValid() const;

public:
	/**
	\brief Half of the width, height, and depth of the box.
	*/
	PxVec3 halfExtents;
};

PX_INLINE bool PxBoxGeometry::isValid() const
{
	if(mType != PxGeometryType::eBOX)
		return false;
	if(!halfExtents.isFinite())
		return false;
	if(halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
		return false;

	return true;
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

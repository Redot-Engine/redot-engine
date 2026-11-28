// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SPHERE_GEOMETRY_H
#define PX_SPHERE_GEOMETRY_H
#include "geometry/PxGeometry.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief A class representing the geometry of a sphere.

Spheres are defined by their radius.
\note The scaling of the sphere is expected to be baked into this value, there is no additional scaling parameter.
*/
class PxSphereGeometry : public PxGeometry 
{
public:
	/**
	\brief Constructor.
	*/
	PX_INLINE PxSphereGeometry(PxReal ir=0.0f) : PxGeometry(PxGeometryType::eSPHERE), radius(ir)		{}

	/**
	\brief Copy constructor.

	\param[in] that		Other object
	*/
	PX_INLINE PxSphereGeometry(const PxSphereGeometry& that) : PxGeometry(that), radius(that.radius)	{}

	/**
	\brief Assignment operator
	*/
	PX_INLINE void operator=(const PxSphereGeometry& that)
	{
		mType = that.mType;
		radius = that.radius;
	}

	/**
	\brief Returns true if the geometry is valid.

	\return True if the current settings are valid

	\note A valid sphere has radius > 0.  
	It is illegal to call PxPhysics::createShape with a sphere that has zero radius.

	\see PxPhysics::createShape
	*/
	PX_INLINE bool isValid() const;

public:

	/**
	\brief The radius of the sphere.
	*/
	PxReal radius;	
};

PX_INLINE bool PxSphereGeometry::isValid() const
{
	if(mType != PxGeometryType::eSPHERE)
		return false;
	if(!PxIsFinite(radius))
		return false;
	if(radius <= 0.0f)
		return false;

	return true;
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

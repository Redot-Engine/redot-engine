// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SHAPE_EXT_H
#define PX_SHAPE_EXT_H

#include "PxPhysXConfig.h"

#include "PxShape.h"
#include "PxRigidActor.h"
#include "geometry/PxGeometryQuery.h"
#include "PxQueryReport.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief utility functions for use with PxShape

\see PxShape
*/

class PxShapeExt
{
public:
	/**
	\brief Retrieves the world space pose of the shape.

	\param[in] shape The shape for which to get the global pose.
	\param[in] actor The actor to which the shape is attached

	\return Global pose of shape.
	*/
	static PX_INLINE	PxTransform		getGlobalPose(const PxShape& shape, const PxRigidActor& actor)
	{
		// PT:: tag: scalar transform*transform
		return actor.getGlobalPose() * shape.getLocalPose();
	}

	/**
	\brief Raycast test against the shape.

	\param[in] shape the shape
	\param[in] actor the actor to which the shape is attached
	\param[in] rayOrigin The origin of the ray to test the geometry object against
	\param[in] rayDir The direction of the ray to test the geometry object against
	\param[in] maxDist Maximum ray length
	\param[in] hitFlags Specify which properties per hit should be computed and written to result hit array. Combination of #PxHitFlag flags
	\param[in] maxHits max number of returned hits = size of 'rayHits' buffer
	\param[out] rayHits Raycast hits information
	\return Number of hits between the ray and the shape

	\see PxRaycastHit PxTransform
	*/
	static PX_INLINE PxU32				raycast(const PxShape& shape, const PxRigidActor& actor, 
												const PxVec3& rayOrigin, const PxVec3& rayDir, PxReal maxDist, PxHitFlags hitFlags,
												PxU32 maxHits, PxRaycastHit* rayHits)
	{
		return PxGeometryQuery::raycast(
			rayOrigin, rayDir, shape.getGeometry(), getGlobalPose(shape, actor), maxDist, hitFlags, maxHits, rayHits);
	}

	/**
	\brief Test overlap between the shape and a geometry object

	\param[in] shape the shape
	\param[in] actor the actor to which the shape is attached
	\param[in] otherGeom The other geometry object to test overlap with
	\param[in] otherGeomPose Pose of the other geometry object
	\return True if the shape overlaps the geometry object

	\see PxGeometry PxTransform
	*/
	static PX_INLINE bool				overlap(const PxShape& shape, const PxRigidActor& actor, 
												const PxGeometry& otherGeom, const PxTransform& otherGeomPose)
	{
		return PxGeometryQuery::overlap(shape.getGeometry(), getGlobalPose(shape, actor), otherGeom, otherGeomPose);
	}

	/**
	\brief Sweep a geometry object against the shape.

	Currently only box, sphere, capsule and convex mesh shapes are supported, i.e. the swept geometry object must be one of those types.

	\param[in] shape the shape
	\param[in] actor the actor to which the shape is attached
	\param[in] unitDir Normalized direction along which the geometry object should be swept.
	\param[in] distance Sweep distance. Needs to be larger than 0.
	\param[in] otherGeom The geometry object to sweep against the shape
	\param[in] otherGeomPose Pose of the geometry object
	\param[out] sweepHit The sweep hit information. Only valid if this method returns true.
	\param[in] hitFlags Specify which properties per hit should be computed and written to result hit array. Combination of #PxHitFlag flags
	\return True if the swept geometry object hits the shape

	\see PxGeometry PxTransform PxSweepHit
	*/
	static PX_INLINE bool			sweep(const PxShape& shape, const PxRigidActor& actor, 
										  const PxVec3& unitDir, const PxReal distance, const PxGeometry& otherGeom, const PxTransform& otherGeomPose,
										  PxSweepHit& sweepHit, PxHitFlags hitFlags)
	{
		return PxGeometryQuery::sweep(unitDir, distance, otherGeom, otherGeomPose, shape.getGeometry(), getGlobalPose(shape, actor), sweepHit, hitFlags);
	}

	/**
	\brief Retrieves the axis aligned bounding box enclosing the shape.

	\return The shape's bounding box.

	\param[in] shape the shape
	\param[in] actor the actor to which the shape is attached
	\param[in] inflation  Scale factor for computed world bounds. Box extents are multiplied by this value.

	\see PxBounds3
	*/
	static PX_INLINE PxBounds3		getWorldBounds(const PxShape& shape, const PxRigidActor& actor, float inflation=1.01f)
	{
		PxBounds3 bounds;
		PxGeometryQuery::computeGeomBounds(bounds, shape.getGeometry(), getGlobalPose(shape, actor), 0.0f, inflation);
		return bounds;
	}

};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

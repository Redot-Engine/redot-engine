// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_RIGID_ACTOR_EXT_H
#define PX_RIGID_ACTOR_EXT_H

#include "PxPhysXConfig.h"
#include "PxPhysics.h"
#include "PxRigidActor.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxBVH;

/**
\brief utility functions for use with PxRigidActor and subclasses

\see PxRigidActor PxRigidStatic PxRigidBody PxRigidDynamic PxArticulationLink
*/

class PxRigidActorExt
{
public:

	/**
	\brief Creates a new shape with default properties and a list of materials and adds it to the list of shapes of this actor.
	
	This is equivalent to the following

	PxShape* shape(...) = PxGetPhysics().createShape(...);	// reference count is 1
	actor->attachShape(shape);								// increments reference count
	shape->release();										// releases user reference, leaving reference count at 1

	As a consequence, detachShape() will result in the release of the last reference, and the shape will be deleted.

	\note The default shape flags to be set are: eVISUALIZATION, eSIMULATION_SHAPE, eSCENE_QUERY_SHAPE (see #PxShapeFlag).
	Triangle mesh, heightfield or plane geometry shapes configured as eSIMULATION_SHAPE are not supported for 
	non-kinematic PxRigidDynamic instances.

	\note Creating compounds with a very large number of shapes may adversely affect performance and stability.

	<b>Sleeping:</b> Does <b>NOT</b> wake the actor up automatically.

	\param[in] actor the actor to which to attach the shape
	\param[in] geometry	the geometry of the shape
	\param[in] materials a pointer to an array of material pointers
	\param[in] materialCount the count of materials
	\param[in] shapeFlags optional PxShapeFlags

	\return The newly created shape.

	\see PxShape PxShape::release(), PxPhysics::createShape(), PxRigidActor::attachShape()
	*/
	static PxShape* createExclusiveShape(PxRigidActor& actor, const PxGeometry& geometry, PxMaterial*const* materials, PxU16 materialCount, 
								         PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE)
	{
		PxShape* shape = PxGetPhysics().createShape(geometry, materials, materialCount, true, shapeFlags);
		if(shape)
		{
			bool status = actor.attachShape(*shape);	// attach can fail, if e.g. we try and attach a trimesh simulation shape to a dynamic actor
			shape->release();							// if attach fails, we hold the only counted reference, and so this cleans up properly
			if(!status)
				shape = NULL;
		}
		return shape;
	}
	
	/**
	\brief Creates a new shape with default properties and a single material adds it to the list of shapes of this actor.

	This is equivalent to the following

	PxShape* shape(...) = PxGetPhysics().createShape(...);	// reference count is 1
	actor->attachShape(shape);								// increments reference count
	shape->release();										// releases user reference, leaving reference count at 1

	As a consequence, detachShape() will result in the release of the last reference, and the shape will be deleted.

	\note The default shape flags to be set are: eVISUALIZATION, eSIMULATION_SHAPE, eSCENE_QUERY_SHAPE (see #PxShapeFlag).
	Triangle mesh, heightfield or plane geometry shapes configured as eSIMULATION_SHAPE are not supported for 
	non-kinematic PxRigidDynamic instances.

	\note Creating compounds with a very large number of shapes may adversely affect performance and stability.

	<b>Sleeping:</b> Does <b>NOT</b> wake the actor up automatically.

	\param[in] actor the actor to which to attach the shape
	\param[in] geometry	the geometry of the shape
	\param[in] material	the material for the shape
	\param[in] shapeFlags optional PxShapeFlags

	\return The newly created shape.

	\see PxShape PxShape::release(), PxPhysics::createShape(), PxRigidActor::attachShape()
	*/
	static PX_FORCE_INLINE	PxShape*	createExclusiveShape(PxRigidActor& actor, const PxGeometry& geometry, const PxMaterial& material, 
													         PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE)
	{
		PxMaterial* materialPtr = const_cast<PxMaterial*>(&material);
		return createExclusiveShape(actor, geometry, &materialPtr, 1, shapeFlags);
	}

	/**
	\brief Gets a list of bounds based on shapes in rigid actor. This list can be used to cook/create
	bounding volume hierarchy though PxCooking API.

	\param[in] actor The actor from which the bounds list is retrieved.
	\param[out] numBounds Number of bounds in returned list.

	\see PxShape PxBVH PxCooking::createBVH PxCooking::cookBVH
	*/
	static PxBounds3*	getRigidActorShapeLocalBoundsList(const PxRigidActor& actor, PxU32& numBounds);

	/**
	\brief Convenience function to create a PxBVH object from a PxRigidActor.

	The computed PxBVH can then be used in PxScene::addActor() or PxAggregate::addActor().
	After adding the actor & BVH to the scene/aggregate, release the PxBVH object by calling PxBVH::release().

	\param[in] physics	The physics object. The function will retrieve the insertion callback from it.
	\param[in] actor	The actor to compute a PxBVH for.

	\return	The PxBVH for this actor.

	\see PxBVH PxScene::addActor PxAggregate::addActor
	*/
	static PxBVH* createBVHFromActor(PxPhysics& physics, const PxRigidActor& actor);
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

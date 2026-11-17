// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PRUNING_STRUCTURE_H
#define PX_PRUNING_STRUCTURE_H

#include "PxPhysXConfig.h"
#include "common/PxBase.h"

#if !PX_DOXYGEN
namespace physx
{
#endif


/**
\brief A precomputed pruning structure to accelerate scene queries against newly added actors.

The pruning structure can be provided to #PxScene:: addActors() in which case it will get merged
directly into the scene query optimization AABB tree, thus leading to improved performance when
doing queries against the newly added actors. This applies to both static and dynamic actors.

\note PxPruningStructure objects can be added to a collection and get serialized.
\note Adding a PxPruningStructure object to a collection will also add the actors that were used to build the pruning structure.

\note PxPruningStructure must be released before its rigid actors.
\note PxRigidBody objects can be in one PxPruningStructure only.
\note Changing the bounds of PxRigidBody objects assigned to a pruning structure that has not been added to a scene yet will 
invalidate the pruning structure. Same happens if shape scene query flags change or shape gets removed from an actor.

\see PxScene::addActors PxCollection
*/
class PxPruningStructure : public PxBase
{
public:
	/**
	\brief Release this object.
	*/
	virtual void				release() PX_OVERRIDE = 0;

	/**
	\brief Retrieve rigid actors in the pruning structure.

	You can retrieve the number of rigid actor pointers by calling #getNbRigidActors()

	\param[out] userBuffer The buffer to store the actor pointers.
	\param[in] bufferSize Size of provided user buffer.
	\param[in] startIndex Index of first actor pointer to be retrieved
	\return Number of rigid actor pointers written to the buffer.

	\see PxRigidActor
	*/
	virtual PxU32				getRigidActors(PxRigidActor** userBuffer, PxU32 bufferSize, PxU32 startIndex=0) const = 0;

	/**
	\brief Returns the number of rigid actors in the pruning structure.

	You can use #getRigidActors() to retrieve the rigid actor pointers.

	\return Number of rigid actors in the pruning structure.

	\see PxRigidActor
	*/
	virtual PxU32				getNbRigidActors() const = 0;

	/**
	\brief Gets the merge data for static actors

	This is mainly called by the PxSceneQuerySystem::merge() function to merge a PxPruningStructure
	with the internal data-structures of the scene-query system.

	\return Implementation-dependent merge data for static actors.

	\see PxSceneQuerySystem::merge()
	*/
	virtual	const void*			getStaticMergeData()	const	= 0;

	/**
	\brief Gets the merge data for dynamic actors

	This is mainly called by the PxSceneQuerySystem::merge() function to merge a PxPruningStructure
	with the internal data-structures of the scene-query system.

	\return Implementation-dependent merge data for dynamic actors.

	\see PxSceneQuerySystem::merge()
	*/
	virtual	const void*			getDynamicMergeData()	const	= 0;

	virtual	const char*			getConcreteTypeName() const	PX_OVERRIDE	PX_FINAL	{ return "PxPruningStructure";	}
protected:
	PX_INLINE					PxPruningStructure(PxType concreteType, PxBaseFlags baseFlags) : PxBase(concreteType, baseFlags) {}
	PX_INLINE					PxPruningStructure(PxBaseFlags baseFlags) : PxBase(baseFlags) {}
	virtual						~PxPruningStructure()	{}
	virtual		bool			isKindOf(const char* name)	const PX_OVERRIDE		{ PX_IS_KIND_OF(name, "PxPruningStructure", PxBase); }
};


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_AGGREGATE_H
#define PX_AGGREGATE_H

#include "PxPhysXConfig.h"
#include "common/PxBase.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxActor;
class PxBVH;
class PxScene;

	struct PxAggregateType
	{
		enum Enum
		{
			eGENERIC	= 0,	//!< Aggregate will contain various actors of unspecified types
			eSTATIC		= 1,	//!< Aggregate will only contain static actors
			eKINEMATIC	= 2		//!< Aggregate will only contain kinematic actors
		};
	};

	// PxAggregateFilterHint is used for more efficient filtering of aggregates outside of the broadphase.
	// It is a combination of a PxAggregateType and a self-collision bit.
	typedef PxU32	PxAggregateFilterHint;

	PX_CUDA_CALLABLE PX_FORCE_INLINE	PxAggregateFilterHint	PxGetAggregateFilterHint(PxAggregateType::Enum type, bool enableSelfCollision)
	{
		const PxU32 selfCollisionBit = enableSelfCollision ? 1 : 0;
		return PxAggregateFilterHint((PxU32(type)<<1)|selfCollisionBit);
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE	PxU32					PxGetAggregateSelfCollisionBit(PxAggregateFilterHint hint)
	{
		return hint & 1;
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE	PxAggregateType::Enum	PxGetAggregateType(PxAggregateFilterHint hint)
	{
		return PxAggregateType::Enum(hint>>1);
	}

/**
\brief Class to aggregate actors into a single broad-phase entry.

A PxAggregate object is a collection of PxActors, which will exist as a single entry in the
broad-phase structures. This has 3 main benefits:

1) it reduces "broad phase pollution" by allowing a collection of spatially coherent broad-phase 
entries to be replaced by a single aggregated entry (e.g. a ragdoll or a single actor with a 
large number of attached shapes).

2) it reduces broad-phase memory usage

3) filtering can be optimized a lot if self-collisions within an aggregate are not needed. For
   example if you don't need collisions between ragdoll bones, it's faster to simply disable
   filtering once and for all, for the aggregate containing the ragdoll, rather than filtering
   out each bone-bone collision in the filter shader.

\see PxActor, PxPhysics.createAggregate
*/
class PxAggregate : public PxBase
{
public:

	/**
	\brief Deletes the aggregate object.

	Deleting the PxAggregate object does not delete the aggregated actors. If the PxAggregate object
	belongs to a scene, the aggregated actors are automatically re-inserted in that scene. If you intend
	to delete both the PxAggregate and its actors, it is best to release the actors first, then release
	the PxAggregate when it is empty.
	*/
	virtual	void	release()	PX_OVERRIDE	= 0;

	/**
	\brief Adds an actor to the aggregate object.

	A warning is output if the total number of actors is reached, or if the incoming actor already belongs
	to an aggregate.

	If the aggregate belongs to a scene, adding an actor to the aggregate also adds the actor to that scene.

	If the actor already belongs to a scene, a warning is output and the call is ignored. You need to remove
	the actor from the scene first, before adding it to the aggregate.

	\note When a BVH is provided the actor shapes are grouped together. 
	The scene query pruning structure inside PhysX SDK will store/update one
	bound per actor. The scene queries against such an actor will query actor
	bounds and then make a local space query against the provided BVH, which is in actor's local space.

	\param	[in] actor	The actor that should be added to the aggregate
	\param	[in] bvh	BVH for actor shapes.
	return	true if success
	*/
	virtual	bool	addActor(PxActor& actor, const PxBVH* bvh = NULL)	= 0;

	/**
	\brief Removes an actor from the aggregate object.

	A warning is output if the incoming actor does not belong to the aggregate. Otherwise the actor is
	removed from the aggregate. If the aggregate belongs to a scene, the actor is reinserted in that
	scene. If you intend to delete the actor, it is best to call #PxActor::release() directly. That way
	the actor will be automatically removed from its aggregate (if any) and not reinserted in a scene.

	\param	[in] actor The actor that should be removed from the aggregate
	return	true if success
	*/
	virtual	bool	removeActor(PxActor& actor)		= 0;

	/**
	\brief Adds an articulation to the aggregate object.

	A warning is output if the total number of actors is reached (every articulation link counts as an actor), 
	or if the incoming articulation already belongs	to an aggregate.

	If the aggregate belongs to a scene, adding an articulation to the aggregate also adds the articulation to that scene.

	If the articulation already belongs to a scene, a warning is output and the call is ignored. You need to remove
	the articulation from the scene first, before adding it to the aggregate.

	\param	[in] articulation The articulation that should be added to the aggregate
	return	true if success
	*/
	virtual	bool	addArticulation(PxArticulationReducedCoordinate& articulation) = 0;

	/**
	\brief Removes an articulation from the aggregate object.

	A warning is output if the incoming articulation does not belong to the aggregate. Otherwise the articulation is
	removed from the aggregate. If the aggregate belongs to a scene, the articulation is reinserted in that
	scene. If you intend to delete the articulation, it is best to call #PxArticulationReducedCoordinate::release() directly. That way
	the articulation will be automatically removed from its aggregate (if any) and not reinserted in a scene.

	\param	[in] articulation The articulation that should be removed from the aggregate
	return	true if success
	*/
	virtual	bool	removeArticulation(PxArticulationReducedCoordinate& articulation) = 0;

	/**
	\brief Returns the number of actors contained in the aggregate.

	You can use #getActors() to retrieve the actor pointers.

	\return Number of actors contained in the aggregate.

	\see PxActor getActors()
	*/
	virtual PxU32	getNbActors() const = 0;

	/**
	\brief Retrieves max amount of actors that can be contained in the aggregate.

	\return Max actor size. 

	\see PxPhysics::createAggregate()
	*/
	virtual	PxU32	getMaxNbActors() const = 0;

	/**
	\brief Retrieves max amount of shapes that can be contained in the aggregate.

	\return Max shape size.

	\see PxPhysics::createAggregate()
	*/
	virtual	PxU32	getMaxNbShapes() const = 0;

	/**
	\brief Retrieve all actors contained in the aggregate.

	You can retrieve the number of actor pointers by calling #getNbActors()

	\param[out] userBuffer The buffer to store the actor pointers.
	\param[in] bufferSize Size of provided user buffer.
	\param[in] startIndex Index of first actor pointer to be retrieved
	\return Number of actor pointers written to the buffer.

	\see PxShape getNbShapes()
	*/
	virtual PxU32	getActors(PxActor** userBuffer, PxU32 bufferSize, PxU32 startIndex=0) const = 0;

	/**
	\brief Retrieves the scene which this aggregate belongs to.

	\return Owner Scene. NULL if not part of a scene.

	\see PxScene
	*/
	virtual	PxScene*	getScene()	= 0;

	/**
	\brief Retrieves aggregate's self-collision flag.

	\return self-collision flag
	*/
	virtual	bool		getSelfCollision()	const	= 0;

	/**
	\brief Sets the environment ID for this aggregate.

	The environment ID is an extra built-in filter group for the GPU broadphase. Aggregates will only collide with actors or aggregates that
	have the same environment ID.
	
	The default value is PX_INVALID_U32. Aggregates with this ID will collide with other actors or aggregates, regardless of which environment
	they are a part of.

	The environment ID must be set before adding the aggregate to a scene, and cannot change while the aggregate is in the scene.

	If it is not PX_INVALID_U32, the environment ID must be smaller than 1<<24, i.e. the system does not support more than 1<<24 environments.

	Aggregated actors must have a default environment ID (PX_INVALID_U32). The environment ID of the aggregate is used in the broadphase, not
	the environment IDs from aggregated actors.

	<b>Default:</b> PX_INVALID_U32

	\note	This is not available for CPU broadphases.

	\param[in]	envID	 Environment ID for this aggregate.
	\return True if success.

	\see getEnvironmentID()
	*/
	virtual	bool	setEnvironmentID(PxU32 envID)	= 0;

	/**
	\brief Returns the environment ID for this aggregate.

	\return Environment ID for this aggregate.

	\see setEnvironmentID()
	*/
	virtual	PxU32	getEnvironmentID()		const	= 0;

	virtual	const char*	getConcreteTypeName() const	PX_OVERRIDE	PX_FINAL	{ return "PxAggregate"; }

			void*		userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.

protected:
	PX_INLINE			PxAggregate(PxType concreteType, PxBaseFlags baseFlags) : PxBase(concreteType, baseFlags), userData(NULL)  {}
	PX_INLINE			PxAggregate(PxBaseFlags baseFlags) : PxBase(baseFlags) {}
	virtual				~PxAggregate() {}
	virtual	bool		isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxAggregate", PxBase); }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

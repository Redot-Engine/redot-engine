// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_ARTICULATION_LINK_H
#define PX_ARTICULATION_LINK_H

#include "PxPhysXConfig.h"
#include "PxRigidBody.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief A component of an articulation that represents a rigid body.

Articulation links have a restricted subset of the functionality of a PxRigidDynamic:
- They may not be kinematic, and do not support contact-force thresholds.
- Their velocity or global pose cannot be set directly, but must be set via the articulation-root and joint positions/velocities.
- Sleep state and solver iteration counts are properties of the entire articulation rather than the individual links.

\see PxArticulationReducedCoordinate, PxArticulationReducedCoordinate::createLink, PxArticulationJointReducedCoordinate, PxRigidBody
*/

class PxArticulationLink : public PxRigidBody
{
public:
	/**
	\brief Releases the link from the articulation.
	
	\note Only a leaf articulation link can be released.
	\note Releasing a link is not allowed while the articulation link is in a scene. In order to release a link,
	remove and then re-add the corresponding articulation to the scene.

	\see PxArticulationReducedCoordinate::createLink()
	*/
	virtual		void						release() PX_OVERRIDE = 0;

	/**
	\brief Gets the articulation that the link is a part of.

	\return The articulation.

	\see PxArticulationReducedCoordinate
	*/
	virtual		PxArticulationReducedCoordinate&			getArticulation() const = 0;

	/**
	\brief Gets the joint which connects this link to its parent.
	
	\return The joint connecting the link to the parent. NULL for the root link.

	\see PxArticulationJointReducedCoordinate
	*/
	virtual		PxArticulationJointReducedCoordinate*	getInboundJoint() const = 0;

	/**
	\brief Gets the number of degrees of freedom of the joint which connects this link to its parent.

	- The root link DOF-count is defined to be 0 regardless of PxArticulationFlag::eFIX_BASE.
	- The return value is only valid for articulations that are in a scene.

	\return The number of degrees of freedom, or 0xFFFFFFFF if the articulation is not in a scene.

	\see PxArticulationJointReducedCoordinate
	*/
	virtual		PxU32						getInboundJointDof() const = 0;

	/**
	\brief Gets the number of child links.

	\return The number of child links.

	\see getChildren
	*/
	virtual		PxU32						getNbChildren() const = 0;

	/**
	\brief Gets the low-level link index that may be used to index into members of PxArticulationCache.

	The low-level indices are built after an articulation is added to the scene following a breadth-first approach,
	where all the links at the current depth are indexed sequentially before moving to the links at the next depth level.
	The root of the articulation has therefore the index 0.
	Note that the low-level indices may be different from the order in which the links were originally added to the articulation.

	The return value is only valid for articulations that are in a scene.

	\return The low-level index, or 0xFFFFFFFF if the articulation is not in a scene.

	\see PxArticulationCache
	*/
	virtual		PxU32						getLinkIndex() const = 0;

	/**
	\brief Retrieves the child links.

	\param[out] userBuffer The buffer to receive articulation link pointers.
	\param[in] bufferSize The size of the provided user buffer, use getNbChildren() for sizing.
	\param[in] startIndex The index of the first child pointer to be retrieved.

	\return The number of articulation links written to the buffer.

	\see getNbChildren
	*/
	virtual		PxU32						getChildren(PxArticulationLink** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;


	/**
	\brief Set the constraint-force-mixing scale term.

	The cfm scale term is a stabilization term that helps avoid instabilities with over-constrained
	configurations. It should be a small value that is multiplied by 1/mass internally to produce
	an additional bias added to the unit response term in the solver.

	\param[in] cfm The constraint-force-mixing scale term.

	<b>Default:</b> 0.025
	<b>Range:</b> [0, 1]

	\note This call is not allowed while the simulation is running.

	\see getCfmScale
	*/
	virtual		void						setCfmScale(const PxReal cfm) = 0;

	/**
	\brief Get the constraint-force-mixing scale term.
	\return The constraint-force-mixing scale term.

	\see setCfmScale
	*/
	virtual		PxReal						getCfmScale() const = 0;

	/**
	\brief Get the linear velocity of the link.

	- For performance, prefer PxArticulationCache::linkVelocity to get link spatial velocities in a batch query.
	- When the articulation state is updated via non-cache API, use PxArticulationReducedCoordinate::updateKinematic before querying velocity.

	\return The linear velocity of the link.

	\note This call is not allowed while the simulation is running except in a split simulation during #PxScene::collide() and up to #PxScene::advance(),
	and in PxContactModifyCallback or in contact report callbacks.

	\note The linear velocity is reported with respect to the link's center of mass and not the actor frame origin.

	\see PxRigidBody::getCMassLocalPose
	*/
	virtual		PxVec3						getLinearVelocity() const PX_OVERRIDE = 0;

	/**
	\brief Get the angular velocity of the link.

	- For performance, prefer PxArticulationCache::linkVelocity to get link spatial velocities in a batch query.
	- When the articulation state is updated via non-cache API, use PxArticulationReducedCoordinate::updateKinematic before querying velocity.

	\return The angular velocity of the link.

	\note This call is not allowed while the simulation is running except in a split simulation during #PxScene::collide() and up to #PxScene::advance(),
	and in PxContactModifyCallback or in contact report callbacks.
	*/
	virtual		PxVec3						getAngularVelocity() const PX_OVERRIDE = 0;

	/**
	\brief Returns the string name of the dynamic type.

	\return The string name.
	*/
	virtual		const char*					getConcreteTypeName()	const	PX_OVERRIDE	PX_FINAL	{ return "PxArticulationLink";	}

protected:
	PX_INLINE								PxArticulationLink(PxType concreteType, PxBaseFlags baseFlags) : PxRigidBody(concreteType, baseFlags) {}
	PX_INLINE								PxArticulationLink(PxBaseFlags baseFlags) : PxRigidBody(baseFlags)	{}
	virtual									~PxArticulationLink()	{}
	virtual		bool						isKindOf(const char* name)	const PX_OVERRIDE		{ PX_IS_KIND_OF(name, "PxArticulationLink", PxRigidBody);	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_BODY_H
#define PX_DEFORMABLE_BODY_H

#include "PxActor.h"
#include "PxDeformableBodyFlag.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

#if PX_VC
#pragma warning(push)
#pragma warning(disable : 4435)
#endif

class PxCudaContextManager;

/**
\brief Represents a deformable body, a base class for deformable actors
\see PxDeformableSurface, PxDeformableVolume, PxActor
*/
class PxDeformableBody : public PxActor
{
public:

	/**
	\brief Raises or clears a particular deformable body flag.

	See the list of flags #PxDeformableBodyFlag

	<b>Default:</b> No flags are set

	\param[in] flag The PxDeformableBodyFlag to raise(set) or clear. See #PxDeformableBodyFlag.
	\param[in] val The new boolean value for the flag.
	*/
	virtual	void							setDeformableBodyFlag(PxDeformableBodyFlag::Enum flag, bool val) = 0;

	/**
	\brief Sets deformable body flags.

	See the list of flags #PxDeformableBodyFlag

	<b>Default:</b> No flags are set

	\param[in] flags The PxDeformableBodyFlags to set.
	*/
	virtual	void							setDeformableBodyFlags(PxDeformableBodyFlags flags) = 0;

	/**
	\brief Reads the deformable body flags.

	See the list of flags #PxDeformableBodyFlag

	\return The values of the deformable body flags.

	\see setDeformableBodyFlag()
	*/
	virtual	PxDeformableBodyFlags			getDeformableBodyFlags() const = 0;

	/**
	\brief Sets the linear damping parameter.

	After every timestep the velocity is reduced while the magnitude of the
	reduction depends on the linearDamping value.
	\see PxRigidBody.setLinearDamping
	<b>Default:</b> 0.05
	\param[in] linearDamping The linear damping parameter
	*/
	virtual		void						setLinearDamping(const PxReal linearDamping) = 0;

	/**
	\brief Retrieves linear velocity damping parameter.
	\see setLinearDamping

	\return The linear damping parameter
	*/
	virtual		PxReal						getLinearDamping() const = 0;

	/**
	\brief Sets the maximal velocity vertices can reach

	Allows to limit the vertices' maximal velocity to control the maximal distance a vertex can move per frame
	<b>Default:</b> 1.0e32
	\param[in] maxLinearVelocity The maximal linear velocity
	*/
	virtual		void						setMaxLinearVelocity(const PxReal maxLinearVelocity) = 0;

	/**
	\brief Retrieves maximal velocity a vertex can have.

	\return The maximal velocity
	*/
	virtual		PxReal						getMaxLinearVelocity() const = 0;

	/**
	\brief Sets the maximal depenetration velocity vertices can reach

	Allows to limit the vertices' maximal depenetration velocity to avoid that collision responses lead to very high particle velocities
	<b>Default:</b> 1.0e32
	\param[in] maxDepenetrationVelocity The maximal depenetration velocity
	*/
	virtual		void						setMaxDepenetrationVelocity(const PxReal maxDepenetrationVelocity) = 0;

	/**
	\brief Retrieves maximal depenetration velocity a vertex can have.

	\return The maximal depenetration velocity
	*/
	virtual		PxReal						getMaxDepenetrationVelocity() const = 0;

	/**
	\brief Sets the self collision filter distance.

	Penetration distance that needs to be exceeded before contacts for self collision are generated.
	Will only have an effect if self collisions are enabled.
	<b>Default:</b> 0.1

	\param[in] selfCollisionFilterDistance The self collision filter distance
	*/
	virtual		void						setSelfCollisionFilterDistance(const PxReal selfCollisionFilterDistance) = 0;

	/**
	\brief Retrieves the self collision filter distance.
	
	\return The self collision filter distance
	\see setSelfCollisionFilterDistance
	*/
	virtual		PxReal						getSelfCollisionFilterDistance() const = 0;

	/**
	\brief Sets the solver iteration counts for the deformable body.

	The solver iteration count determines how accurately contacts, attachments, and internal
	constraints are resolved. If the deformable body does not converge to its intended stiffness, or
	its contacts and attachments are poorly resolved, setting a higher position iteration count may help.

	Velocity iterations run after the position solve and correct velocities only. Increasing the
	velocity iteration count may reduce the velocity lag inherent in position-based dynamics at
	contacts and attachments, as well as violent depenetration. Internal elastic forces and damping
	are not solved for, biasing the velocity result toward contacts and attachments.

	<b>Default:</b> 4 position iterations, 1 velocity iteration

	\param[in] minPositionIters Number of position iterations the solver should perform for this deformable body. <b>Range:</b> [1,255]
	\param[in] minVelocityIters Number of velocity iterations the solver should perform for this deformable body. <b>Range:</b> [0,255]
	\see getSolverIterationCounts()
	*/
	virtual		void						setSolverIterationCounts(PxU32 minPositionIters, PxU32 minVelocityIters = 1) = 0;

	/**
	\brief Retrieves the solver iteration counts.

	\see setSolverIterationCounts()
	*/
	virtual		void						getSolverIterationCounts(PxU32& minPositionIters, PxU32& minVelocityIters) const = 0;

	/**
	\brief Sets the threshold controlling sleeping of the deformable body.
	
	Threshold that defines the maximal magnitude of the linear motion a deformable body can move in one second
	before it becomes a candidate for sleeping.

	\see PxRigidDynamic.setSleepThreshold
	<b>Default:</b> 0.05

	\param[in] sleepThreshold The sleep threshold
	*/
	virtual		void						setSleepThreshold(const PxReal sleepThreshold) = 0;

	/**
	\brief Retrieves the sleep threshold.
	\see setSleepThreshold

	\return The sleep threshold
	*/
	virtual		PxReal						getSleepThreshold() const = 0;

	/**
	\brief Sets the threshold controlling settling phase before sleeping of the deformable body.

	Threshold that defines the maximal magnitude of the linear motion a deformable body can move 
	in one second before it becomes a candidate for sleeping and settling damping is engaged.
	The settling threshold needs to be higher than the sleep threshold.
	<b>Default:</b> 0.1
	\see setSettlingDamping

	\param[in] settlingThreshold The settling threshold
	*/
	virtual		void						setSettlingThreshold(const PxReal settlingThreshold) = 0;

	/**
	\brief Retrieves the settling threshold.
	\see setSettlingThreshold

	\return The settling threshold
	*/
	virtual		PxReal						getSettlingThreshold() const = 0;

	/**
	\brief Sets the damping parameter used for settling phase.

	If the maximum linear velocity of the deformable body falls below the settling threshold, the deformable body
	enters the settling phase in which the settling damping is applied.
	
	<b>Default:</b> 10.0
	\param[in] settlingDamping The settling damping
	\see setLinearDamping, setSettlingThreshold
	*/
	virtual		void						setSettlingDamping(const PxReal settlingDamping) = 0;

	/**
	\brief Retrieves settling damping parameter.
	\see setSettlingDamping

	\return The settling damping parameter
	*/
	virtual		PxReal						getSettlingDamping() const = 0;

	/**
	\brief Sets the wake counter for the deformable body.

	The wake counter value determines the minimum amount of time until the deformable body can be put to sleep. Please note
	that a deformable body will not be put to sleep if any vertex velocity is above the specified threshold
	or if other awake objects are touching it.

	\note Passing in a positive value will wake the deformable body up automatically.

	<b>Default:</b> 0.4 (which corresponds to 20 frames for a time step of 0.02)

	\param[in] wakeCounterValue Wake counter value. <b>Range:</b> [0, PX_MAX_F32)

	\see isSleeping() getWakeCounter()
	*/
	virtual		void						setWakeCounter(PxReal wakeCounterValue) = 0;

	/**
	\brief Returns the wake counter of the deformable body.

	\return The wake counter of the deformable body.

	\see isSleeping() setWakeCounter()
	*/
	virtual		PxReal						getWakeCounter() const = 0;

	/**
	\brief Returns true if this deformable body is sleeping.

	When an actor does not move for a period of time, it is no longer simulated in order to save time. This state
	is called sleeping. However, because the object automatically wakes up when it is either touched by an awake object,
	or a sleep-affecting property is changed by the user, the entire sleep mechanism should be transparent to the user.

	A deformable volume can only go to sleep if all vertices are ready for sleeping. A deformable body is guaranteed to be awake
	if at least one of the following holds:

	\li The wake counter is positive (\see setWakeCounter()).
	\li The velocity of any vertex is above the sleep threshold.

	If a deformable body is sleeping, the following state is guaranteed:

	\li The wake counter is zero.
	\li The linear velocity of all vertices is zero.

	When a deformable body gets inserted into a scene, it will be considered asleep if all the points above hold, else it will
	be treated as awake.

	\note It is invalid to use this method if the deformable body has not been added to a scene already.

	\return True if the deformable body is sleeping.

	\see isSleeping()
	*/
	virtual		bool						isSleeping() const = 0;

	/**
	\brief Retrieve a shape pointer belonging to the actor.

	\see PxShape getNbShapes() PxShape::release()
	*/

	virtual		PxShape*					getShape() = 0;

	/**
	\brief Attaches a shape

	Attaches the shape to use for collision detection for deformable surfaces and volumes.
	Each deformable needs to have exactly one exclusive shape attached for simulation. If a shape has
	already been attached to a deformable, detachShape needs to be called prior to attaching
	a new shape.

	Deformable surfaces need a shape with triangle mesh geometry, which can be created with
	PxPhysics::createShape(const PxGeometry&, const PxDeformableSurfaceMaterial& material, bool, PxShapeFlags), or
	PxPhysics::createShape(const PxGeometry&, PxDeformableSurfaceMaterial*const*, PxU16, bool, PxShapeFlags)
	Deformable surfaces use the same triangle mesh for collision detection and dynamics computations.

	Deformable volumes need a shape with tetrahedron mesh geometry, which can be created with
	PxPhysics::createShape(const PxGeometry&, const PxDeformableVolumeMaterial& material, bool, PxShapeFlags), or
	PxPhysics::createShape(const PxGeometry&, PxDeformableVolumeMaterial*const*, PxU16, bool, PxShapeFlags)
	Deformable volumes additionally need a separate tetrahedron mesh for dynamics, which can be attached using
	PxDeformbleVolume::attachSimulationMesh.

	\param[in] shape The shape to use for collisions, (and dynamics in case of deformable surfaces)

	\return Returns true if the operation was successful
	*/
	virtual		bool						attachShape(PxShape& shape) = 0;

	/**
	\brief Detaches the shape

	Detaches the shape used for collision detection.

	\see void PxDeformableVolume.detachSimulationMesh()
	*/
	virtual		void						detachShape() = 0;

	/**
	\brief Returns the cuda context manager

	\return The cuda context manager
	*/
	virtual		PxCudaContextManager*		getCudaContextManager() const = 0;

protected:
	PX_INLINE			PxDeformableBody(PxType concreteType, PxBaseFlags baseFlags) : PxActor(concreteType, baseFlags) {}
	PX_INLINE			PxDeformableBody(PxBaseFlags baseFlags) : PxActor(baseFlags) {}
	virtual				~PxDeformableBody() {}
	virtual		bool	isKindOf(const char* name)	const { PX_IS_KIND_OF(name, "PxDeformableBody", PxActor); }
};


#if PX_VC
#pragma warning(pop)
#endif


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_DEFORMABLE_BODY_H

// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_JOINT_H
#define PX_JOINT_H

#include "foundation/PxTransform.h"
#include "PxRigidActor.h"
#include "PxConstraint.h"
#include "common/PxBase.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxRigidActor;
class PxScene;
class PxPhysics;
class PxConstraint;

/**
\brief an enumeration of PhysX' built-in joint types

\see PxJoint
*/
struct PxJointConcreteType
{
	enum Enum
	{
		eSPHERICAL = PxConcreteType::eFIRST_PHYSX_EXTENSION,
		eREVOLUTE,
		ePRISMATIC,
		eFIXED,
		eDISTANCE,
		eD6,
		eGEAR,
		eRACK_AND_PINION,
		eLast
	};
};

PX_DEFINE_TYPEINFO(PxJoint,					PxConcreteType::eUNDEFINED)
PX_DEFINE_TYPEINFO(PxRackAndPinionJoint,	PxJointConcreteType::eRACK_AND_PINION)
PX_DEFINE_TYPEINFO(PxGearJoint,				PxJointConcreteType::eGEAR)
PX_DEFINE_TYPEINFO(PxD6Joint,				PxJointConcreteType::eD6)
PX_DEFINE_TYPEINFO(PxDistanceJoint,			PxJointConcreteType::eDISTANCE)
PX_DEFINE_TYPEINFO(PxFixedJoint,			PxJointConcreteType::eFIXED)
PX_DEFINE_TYPEINFO(PxPrismaticJoint,		PxJointConcreteType::ePRISMATIC)
PX_DEFINE_TYPEINFO(PxRevoluteJoint,			PxJointConcreteType::eREVOLUTE)
PX_DEFINE_TYPEINFO(PxSphericalJoint,		PxJointConcreteType::eSPHERICAL)


/**
\brief an enumeration for specifying one or other of the actors referenced by a joint

\see PxJoint
*/

struct PxJointActorIndex
{
	enum Enum
	{
		eACTOR0,
		eACTOR1,
		COUNT
	};
};

/** 
\brief a base interface providing common functionality for PhysX joints
*/

class PxJoint : public PxBase
{
public:

	/**
	\brief Set the actors for this joint. 
	
	An actor may be NULL to indicate the world frame. At most one of the actors may be NULL.

	\param[in] actor0 the first actor.
	\param[in] actor1 the second actor

	\see getActors()
	*/
	virtual void				setActors(PxRigidActor* actor0, PxRigidActor* actor1)	= 0;

	/**
	\brief Get the actors for this joint. 
	
	\param[out] actor0 the first actor.
	\param[out] actor1 the second actor

	\see setActors()
	*/
	virtual void				getActors(PxRigidActor*& actor0, PxRigidActor*& actor1)	const	= 0;

	/**
	\brief Set the joint local pose for an actor. 
	
	This is the relative pose which locates the joint frame relative to the actor.

	\param[in] actor 0 for the first actor, 1 for the second actor.
	\param[in] localPose the local pose for the actor this joint

	\see getLocalPose()
	*/
	virtual void				setLocalPose(PxJointActorIndex::Enum actor, const PxTransform& localPose) = 0;

	/**
	\brief get the joint local pose for an actor. 
	
	\param[in] actor 0 for the first actor, 1 for the second actor.

	return the local pose for this joint

	\see setLocalPose()
	*/
	virtual PxTransform			getLocalPose(PxJointActorIndex::Enum actor) const = 0;

	/**
	\brief get the relative pose for this joint

	This function returns the pose of the joint frame of actor1 relative to actor0

	*/
	virtual PxTransform			getRelativeTransform()	const	= 0;

	/**
	\brief get the relative linear velocity of the joint

	This function returns the linear velocity of the origin of the constraint frame of actor1, relative to the origin of the constraint
	frame of actor0. The value is returned in the constraint frame of actor0
	*/
	virtual PxVec3				getRelativeLinearVelocity()	const	= 0;

	/**
	\brief get the relative angular velocity of the joint

	This function returns the angular velocity of actor1 relative to actor0. The value is returned in the constraint frame of actor0
	*/
	virtual PxVec3				getRelativeAngularVelocity()	const	= 0;

	/**
	\brief set the break force for this joint. 
	
	if the constraint force or torque on the joint exceeds the specified values, the joint will break, 
	at which point it will not constrain the two actors and the flag PxConstraintFlag::eBROKEN will be set. The
	force and torque are measured in the joint frame of the first actor

	\param[in] force the maximum force the joint can apply before breaking
	\param[in] torque the maximum torque the joint can apply before breaking
	*/
	virtual void				setBreakForce(PxReal force, PxReal torque)	= 0;

	/**
	\brief get the break force for this joint. 
	
	\param[out] force the maximum force the joint can apply before breaking
	\param[out] torque the maximum torque the joint can apply before breaking

	\see setBreakForce() 
	*/
	virtual void				getBreakForce(PxReal& force, PxReal& torque)	const	= 0;

	/**
	\brief set the constraint flags for this joint. 
	
	\param[in] flags the constraint flags

	\see PxConstraintFlag
	*/
	virtual void				setConstraintFlags(PxConstraintFlags flags)	= 0;

	/**
	\brief set a constraint flags for this joint to a specified value. 
	
	\param[in] flag the constraint flag
	\param[in] value the value to which to set the flag

	\see PxConstraintFlag
	*/
	virtual void				setConstraintFlag(PxConstraintFlag::Enum flag, bool value)	= 0;

	/**
	\brief get the constraint flags for this joint. 
	
	\return the constraint flags

	\see PxConstraintFlag
	*/
	virtual PxConstraintFlags	getConstraintFlags()	const	= 0;

	/**
	\brief set the inverse mass scale for actor0.

	\param[in] invMassScale the scale to apply to the inverse mass of actor 0 for resolving this constraint

	\see getInvMassScale0
	*/
	virtual void				setInvMassScale0(PxReal invMassScale)	= 0;

	/**
	\brief get the inverse mass scale for actor0.

	\return inverse mass scale for actor0

	\see setInvMassScale0
	*/
	virtual PxReal				getInvMassScale0()	const	= 0;

	/**
	\brief set the inverse inertia scale for actor0.

	\param[in] invInertiaScale the scale to apply to the inverse inertia of actor0 for resolving this constraint

	\see getInvMassScale0
	*/
	virtual void				setInvInertiaScale0(PxReal invInertiaScale)	= 0;

	/**
	\brief get the inverse inertia scale for actor0.

	\return inverse inertia scale for actor0

	\see setInvInertiaScale0
	*/
	virtual PxReal				getInvInertiaScale0()	const	= 0;

	/**
	\brief set the inverse mass scale for actor1.

	\param[in] invMassScale the scale to apply to the inverse mass of actor 1 for resolving this constraint

	\see getInvMassScale1
	*/
	virtual void				setInvMassScale1(PxReal invMassScale)	= 0;

	/**
	\brief get the inverse mass scale for actor1.

	\return inverse mass scale for actor1

	\see setInvMassScale1
	*/
	virtual PxReal				getInvMassScale1()	const	= 0;

	/**
	\brief set the inverse inertia scale for actor1.

	\param[in] invInertiaScale the scale to apply to the inverse inertia of actor1 for resolving this constraint

	\see getInvInertiaScale1
	*/
	virtual void				setInvInertiaScale1(PxReal invInertiaScale)	= 0;

	/**
	\brief get the inverse inertia scale for actor1.

	\return inverse inertia scale for actor1

	\see setInvInertiaScale1
	*/
	virtual PxReal				getInvInertiaScale1()	const	= 0;

	/**
	\brief Retrieves the PxConstraint corresponding to this joint.
	
	This can be used to determine, among other things, the force applied at the joint.

	\return the constraint
	*/
	virtual PxConstraint*		getConstraint()	const	= 0;

	/**
	\brief Sets a name string for the object that can be retrieved with getName().
	
	This is for debugging and is not used by the SDK. The string is not copied by the SDK, 
	only the pointer is stored.

	\param[in] name String to set the objects name to.

	\see getName()
	*/
	virtual void				setName(const char* name)	= 0;

	/**
	\brief Retrieves the name string set with setName().

	\return Name string associated with object.

	\see setName()
	*/
	virtual const char*			getName()	const	= 0;

	/**
	\brief Deletes the joint.

	\note This call does not wake up the connected rigid bodies.
	*/
	virtual void				release()	= 0;

	/**
	\brief Retrieves the scene which this joint belongs to.

	\return Owner Scene. NULL if not part of a scene.

	\see PxScene
	*/
	virtual PxScene*			getScene()	const	= 0;

	void*						userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.

protected:
	virtual						~PxJoint() {}

	//serialization

	/**
	\brief Constructor
	*/
	PX_INLINE					PxJoint(PxType concreteType, PxBaseFlags baseFlags) : PxBase(concreteType, baseFlags), userData(NULL) {}
	
	/**
	\brief Deserialization constructor
	*/
	PX_INLINE					PxJoint(PxBaseFlags baseFlags)	: PxBase(baseFlags)	{}

	/**
	\brief Returns whether a given type name matches with the type of this instance
	*/
	virtual	bool				isKindOf(const char* name) const { PX_IS_KIND_OF(name, "PxJoint", PxBase); }

	//~serialization
};

class PxSpring
{
public:

	PxReal	stiffness;	//!< the spring strength of the drive: that is, the force proportional to the position error
	PxReal	damping;	//!< the damping strength of the drive: that is, the force proportional to the velocity error

	PxSpring(PxReal stiffness_, PxReal damping_): stiffness(stiffness_), damping(damping_) {}
};


#if !PX_DOXYGEN
} // namespace physx
#endif

/** \brief Helper function to setup a joint's global frame

	This replaces the following functions from previous SDK versions:

	void NxJointDesc::setGlobalAnchor(const NxVec3& wsAnchor);
	void NxJointDesc::setGlobalAxis(const NxVec3& wsAxis);

	The function sets the joint's localPose using world-space input parameters.

	\param[in] wsAnchor Global frame anchor point. <b>Range:</b> position vector
	\param[in] wsAxis Global frame axis. <b>Range:</b> direction vector
	\param[in,out] joint Joint having its global frame set.
*/

PX_C_EXPORT void PX_CALL_CONV PxSetJointGlobalFrame(physx::PxJoint& joint, const physx::PxVec3* wsAnchor, const physx::PxVec3* wsAxis);

#endif

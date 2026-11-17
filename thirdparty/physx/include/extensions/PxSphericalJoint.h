// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SPHERICAL_JOINT_H
#define PX_SPHERICAL_JOINT_H

#include "extensions/PxJoint.h"
#include "extensions/PxJointLimit.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxSphericalJoint;

/**
\brief Create a spherical joint.

 \param[in] physics		The physics SDK
 \param[in] actor0		An actor to which the joint is attached. NULL may be used to attach the joint to a specific point in the world frame
 \param[in] localFrame0	The position and orientation of the joint relative to actor0
 \param[in] actor1		An actor to which the joint is attached. NULL may be used to attach the joint to a specific point in the world frame
 \param[in] localFrame1	The position and orientation of the joint relative to actor1 

\see PxSphericalJoint
*/
PxSphericalJoint*	PxSphericalJointCreate(PxPhysics& physics, PxRigidActor* actor0, const PxTransform& localFrame0, PxRigidActor* actor1, const PxTransform& localFrame1);


/**
\brief Flags specific to the spherical joint.

\see PxSphericalJoint
*/
struct PxSphericalJointFlag
{
	enum Enum
	{
		eLIMIT_ENABLED	= 1<<1	//!< the cone limit for the spherical joint is enabled
	};
};
typedef PxFlags<PxSphericalJointFlag::Enum, PxU16> PxSphericalJointFlags;
PX_FLAGS_OPERATORS(PxSphericalJointFlag::Enum, PxU16)

/**
\brief A joint which behaves in a similar way to a ball and socket.

 A spherical joint removes all linear degrees of freedom from two objects.

 The position of the joint on each actor is specified by the origin of the body's joint frame.
 
 A spherical joint may have a cone limit, to restrict the motion to within a certain range.
 
 Dirve and limits are activated by setting the appropriate flags on the joint.

 \see PxRevoluteJointCreate() PxJoint
*/
class PxSphericalJoint : public PxJoint
{
public:
	
	/**
	\brief Set the limit cone.

	If enabled, the limit cone will constrain the angular movement of the joint to lie
	within an elliptical cone.

	\return the limit cone

	\see PxJointLimitCone setLimit() 
	*/
	virtual PxJointLimitCone	getLimitCone()	const	= 0;

	/**
	\brief Get the limit cone.

	\param[in] limit the limit cone

	\see PxJointLimitCone getLimit() 
	*/
	virtual void				setLimitCone(const PxJointLimitCone& limit)	= 0;

	/**
	\brief get the swing angle of the joint from the Y axis
	*/
	virtual PxReal				getSwingYAngle()	const	= 0;

	/**
	\brief get the swing angle of the joint from the Z axis
	*/
	virtual PxReal				getSwingZAngle()	const	= 0;

	/**
	\brief Set the flags specific to the Spherical Joint.

	<b>Default</b> PxSphericalJointFlags(0)

	\param[in] flags The joint flags.

	\see PxSphericalJointFlag setFlag() getFlags()
	*/
	virtual void				setSphericalJointFlags(PxSphericalJointFlags flags) = 0;

	/**
	\brief Set a single flag specific to a Spherical Joint to true or false.

	\param[in] flag The flag to set or clear.
	\param[in] value the value to which to set the flag

	\see PxSphericalJointFlag, getFlags() setFlags()
	*/
	virtual void				setSphericalJointFlag(PxSphericalJointFlag::Enum flag, bool value) = 0;

	/**
	\brief Get the flags specific to the Spherical Joint.

	\return the joint flags

	\see PxSphericalJoint::flags, PxSphericalJointFlag setFlag() setFlags()
	*/
	virtual PxSphericalJointFlags	getSphericalJointFlags()	const	= 0;

	/**
	\brief Returns string name of PxSphericalJoint, used for serialization
	*/
	virtual	const char*			getConcreteTypeName() const	PX_OVERRIDE	{ return "PxSphericalJoint"; }

protected:

	//serialization

	/**
	\brief Constructor
	*/
	PX_INLINE					PxSphericalJoint(PxType concreteType, PxBaseFlags baseFlags) : PxJoint(concreteType, baseFlags) {}

	/**
	\brief Deserialization constructor
	*/
	PX_INLINE					PxSphericalJoint(PxBaseFlags baseFlags) : PxJoint(baseFlags)	{}

	/**
	\brief Returns whether a given type name matches with the type of this instance
	*/
	virtual	bool				isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxSphericalJoint", PxJoint); }

	//~serialization
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PRISMATIC_JOINT_H
#define PX_PRISMATIC_JOINT_H

#include "extensions/PxJoint.h"
#include "extensions/PxJointLimit.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxPrismaticJoint;

/**
\brief Create a prismatic joint.

 \param[in] physics		The physics SDK
 \param[in] actor0		An actor to which the joint is attached. NULL may be used to attach the joint to a specific point in the world frame
 \param[in] localFrame0	The position and orientation of the joint relative to actor0
 \param[in] actor1		An actor to which the joint is attached. NULL may be used to attach the joint to a specific point in the world frame
 \param[in] localFrame1	The position and orientation of the joint relative to actor1 

\see PxPrismaticJoint
*/
PxPrismaticJoint*	PxPrismaticJointCreate(PxPhysics& physics, PxRigidActor* actor0, const PxTransform& localFrame0, PxRigidActor* actor1, const PxTransform& localFrame1);


/**
\brief Flags specific to the prismatic joint.

\see PxPrismaticJoint
*/
struct PxPrismaticJointFlag
{
	enum Enum
	{
		eLIMIT_ENABLED	= 1<<1
	};
};

typedef PxFlags<PxPrismaticJointFlag::Enum, PxU16> PxPrismaticJointFlags;
PX_FLAGS_OPERATORS(PxPrismaticJointFlag::Enum, PxU16)

/**
 \brief A prismatic joint permits relative translational movement between two bodies along
 an axis, but no relative rotational movement.

 The axis on each body is defined as the line containing the origin of the joint frame and
 extending along the x-axis of that frame.

 \image html prismJoint.png

 \see PxPrismaticJointCreate() PxJoint
*/
class PxPrismaticJoint : public PxJoint
{
public:
	
	/** 
	\brief returns the displacement of the joint along its axis.
	*/
	virtual PxReal			getPosition()	const	= 0;

	/** 
	\brief returns the velocity of the joint along its axis
	*/
	virtual PxReal			getVelocity()	const	= 0;

	/**
	\brief sets the joint limit  parameters.

	The limit range is [-PX_MAX_F32, PX_MAX_F32], but note that the width of the limit (upper-lower) must also be
	a valid float.

	\see PxJointLinearLimitPair getLimit()
	*/
	virtual void			setLimit(const PxJointLinearLimitPair&)		= 0;

	/**
	\brief gets the joint limit  parameters.

	\see PxJointLinearLimit getLimit()
	*/
	virtual PxJointLinearLimitPair getLimit()	const	= 0;

	/**
	\brief Set the flags specific to the Prismatic Joint.

	<b>Default</b> PxPrismaticJointFlags(0)

	\param[in] flags The joint flags.

	\see PxPrismaticJointFlag setFlag() getFlags()
	*/
	virtual void					setPrismaticJointFlags(PxPrismaticJointFlags flags) = 0;

	/**
	\brief Set a single flag specific to a Prismatic Joint to true or false.

	\param[in] flag		The flag to set or clear.
	\param[in] value	The value to which to set the flag

	\see PxPrismaticJointFlag, getFlags() setFlags()
	*/
	virtual void					setPrismaticJointFlag(PxPrismaticJointFlag::Enum flag, bool value) = 0;

	/**
	\brief Get the flags specific to the Prismatic Joint.

	\return the joint flags

	\see PxPrismaticJoint::flags, PxPrismaticJointFlag setFlag() setFlags()
	*/
	virtual PxPrismaticJointFlags	getPrismaticJointFlags()	const	= 0;

	/**
	\brief Returns string name of PxPrismaticJoint, used for serialization
	*/
	virtual	const char*		getConcreteTypeName() const	PX_OVERRIDE	{ return "PxPrismaticJoint"; }

protected:
	//serialization
	
	/**
	\brief Constructor
	*/
	PX_INLINE				PxPrismaticJoint(PxType concreteType, PxBaseFlags baseFlags) : PxJoint(concreteType, baseFlags) {}

	/**
	\brief Deserialization constructor
	*/		
	PX_INLINE				PxPrismaticJoint(PxBaseFlags baseFlags) : PxJoint(baseFlags) {}
	
	/**
	\brief Returns whether a given type name matches with the type of this instance
	*/
	virtual	bool			isKindOf(const char* name) const PX_OVERRIDE {	PX_IS_KIND_OF(name, "PxPrismaticJoint", PxJoint); }
	
	//~serialization
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

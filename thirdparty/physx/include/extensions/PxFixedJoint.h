// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_FIXED_JOINT_H
#define PX_FIXED_JOINT_H

#include "extensions/PxJoint.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxFixedJoint;

/**
\brief Create a fixed joint.

 \param[in] physics		The physics SDK
 \param[in] actor0		An actor to which the joint is attached. NULL may be used to attach the joint to a specific point in the world frame
 \param[in] localFrame0	The position and orientation of the joint relative to actor0
 \param[in] actor1		An actor to which the joint is attached. NULL may be used to attach the joint to a specific point in the world frame
 \param[in] localFrame1	The position and orientation of the joint relative to actor1 

\see PxFixedJoint
*/
PxFixedJoint*	PxFixedJointCreate(PxPhysics& physics, PxRigidActor* actor0, const PxTransform& localFrame0, PxRigidActor* actor1, const PxTransform& localFrame1);

/**
 \brief A fixed joint permits no relative movement between two bodies. ie the bodies are glued together.

 \image html fixedJoint.png

 \see PxFixedJointCreate() PxJoint
*/
class PxFixedJoint : public PxJoint
{
public:
	
	/**
	\brief Returns string name of PxFixedJoint, used for serialization
	*/
	virtual	const char*			getConcreteTypeName() const	PX_OVERRIDE	{ return "PxFixedJoint"; }

protected:

	//serialization

	/**
	\brief Constructor
	*/
	PX_INLINE					PxFixedJoint(PxType concreteType, PxBaseFlags baseFlags) : PxJoint(concreteType, baseFlags) {}

	/**
	\brief Deserialization constructor
	*/
	PX_INLINE					PxFixedJoint(PxBaseFlags baseFlags) : PxJoint(baseFlags)	{}

	/**
	\brief Returns whether a given type name matches with the type of this instance
	*/
	virtual	bool				isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxFixedJoint", PxJoint);	}

	//~serialization
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_RIGID_STATIC_H
#define PX_RIGID_STATIC_H

#include "PxPhysXConfig.h"
#include "PxRigidActor.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief PxRigidStatic represents a static rigid body simulation object in the physics SDK.

PxRigidStatic objects are static rigid physics entities. They shall be used to define solid objects which are fixed in the world.

<h3>Creation</h3>
Instances of this class are created by calling #PxPhysics::createRigidStatic() and deleted with #release().

<h3>Visualizations</h3>
\li #PxVisualizationParameter::eACTOR_AXES

\see PxRigidActor  PxPhysics.createRigidStatic()  release()
*/

class PxRigidStatic : public PxRigidActor
{
public:
	virtual		const char*		getConcreteTypeName() const	PX_OVERRIDE	PX_FINAL	{ return "PxRigidStatic"; }

protected:
	PX_INLINE					PxRigidStatic(PxType concreteType, PxBaseFlags baseFlags) : PxRigidActor(concreteType, baseFlags) {}
	PX_INLINE					PxRigidStatic(PxBaseFlags baseFlags) : PxRigidActor(baseFlags){}
	virtual						~PxRigidStatic() {}
	virtual		bool			isKindOf(const char* name)	const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxRigidStatic", PxRigidActor); }

};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

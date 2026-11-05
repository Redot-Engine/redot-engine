// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_PHYSX_ACTOR_STATES_H
#define PX_VEHICLE_PHYSX_ACTOR_STATES_H

#include "foundation/PxPreprocessor.h"
#include "foundation/PxMemory.h"

#include "PxRigidBody.h"

#include "vehicle/PxVehicleLimits.h"

#if !PX_DOXYGEN
namespace physx
{

class PxShape;

#endif

/**
\brief A description of the PhysX actor and shapes that represent the vehicle in an associated PxScene.
*/
struct PxVehiclePhysXActor
{
	/**
	\brief The PhysX rigid body that represents the vehcle in the associated PhysX scene. 
	\note PxActorFlag::eDISABLE_GRAVITY must be set true on the PxRigidBody
	*/
	PxRigidBody* rigidBody;

	/**
	\brief An array of shapes with one shape pointer (or NULL) for each wheel.
	*/
	PxShape* wheelShapes[PxVehicleLimits::eMAX_NB_WHEELS];	

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehiclePhysXActor));
	}
};

#define PX_VEHICLE_UNSPECIFIED_STEER_STATE PX_MAX_F32

/**
\brief A description of the previous steer command applied to the vehicle.
*/
struct PxVehiclePhysXSteerState
{

	/**
	\brief The steer command that was most previously applied to the vehicle.
	*/
	PxReal previousSteerCommand;

	PX_FORCE_INLINE void setToDefault()
	{
		previousSteerCommand = PX_VEHICLE_UNSPECIFIED_STEER_STATE;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

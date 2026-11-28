// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_RIGIDBODY_FUNCTIONS_H
#define PX_VEHICLE_RIGIDBODY_FUNCTIONS_H

#include "foundation/PxVec3.h"
#include "foundation/PxSimpleTypes.h"
#include "vehicle/PxVehicleParams.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxVehicleRigidBodyParams;
struct PxVehicleSuspensionForce;
struct PxVehicleTireForce;
struct PxVehicleAntiRollTorque;
struct PxVehicleRigidBodyState;

/**
\brief Forward integrate rigid body state.
\param[in] axleDescription is a description of the axles of the vehicle and the wheels on each axle.
\param[in] rigidBodyParams is a description of rigid body mass and moment of inertia.
\param[in] suspensionForces is an array of suspension forces and torques in the world frame to be applied to the rigid body.
\param[in] tireForces is an array of tire forces and torques in the world frame to be applied to the rigid body.
\param[in] antiRollTorque is an optional pointer to a single PxVehicleAntiRollTorque instance that contains the accumulated anti-roll
torque to apply to the rigid body.
\param[in] dt is the timestep of the forward  integration.
\param[in] gravity is gravitational acceleration.
\param[in,out] rigidBodyState is the rigid body state that is to be updated.
\note The suspensionForces array must contain an entry for each wheel listed as an active wheel in axleDescription.
\note The tireForces array must contain an entry for each wheel listed as an active wheel in axleDescription.
\note If antiRollTorque is a null pointer then zero anti-roll torque will be applied to the rigid body.
*/
void PxVehicleRigidBodyUpdate
	(const PxVehicleAxleDescription& axleDescription, const PxVehicleRigidBodyParams& rigidBodyParams, 
	const PxVehicleArrayData<const PxVehicleSuspensionForce>& suspensionForces,
	const PxVehicleArrayData<const PxVehicleTireForce>& tireForces,
	const PxVehicleAntiRollTorque* antiRollTorque,
	const PxReal dt, const PxVec3& gravity,
	PxVehicleRigidBodyState& rigidBodyState);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

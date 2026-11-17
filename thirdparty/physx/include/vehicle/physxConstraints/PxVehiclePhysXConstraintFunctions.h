// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_PHYSX_CONSTRAINT_FUNCTIONS_H
#define PX_VEHICLE_PHYSX_CONSTRAINT_FUNCTIONS_H

#include "foundation/PxVec3.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxVehicleSuspensionParams;
struct PxVehiclePhysXSuspensionLimitConstraintParams;
struct PxVehicleSuspensionState;
struct PxVehicleSuspensionComplianceState;
struct PxVehicleTireDirectionState;
struct PxVehicleTireStickyState;
struct PxVehicleRigidBodyState;
struct PxVehiclePhysXConstraintState;

/**
\brief Read constraint data from the vehicle's internal state for a single wheel and write it to a 
structure that will be read by the associated PxScene and used to impose the constraints during the next 
PxScene::simulate() step.
\param[in] suspensionParams describes the suspension frame.
\param[in] suspensionLimitParams describes the restitution value applied to any constraint triggered by 
the suspension travel limit.
\param[in] suspensionState describes the excess suspension compression beyond the suspension travel limit that will be 
resolved with a constraint.
\param[in] suspensionComplianceState describes the effect of suspension compliance on the effective application point
of the suspension force.
\param[in] groundPlaneNormal The normal direction of the ground plane the wheel is driving on. A normalized vector is
           expected.
\param[in] tireStickyDampingLong The damping coefficient to use in the constraint to approach a zero target velocity
           along the longitudinal tire axis.
\param[in] tireStickyDampingLat Same concept as tireStickyDampingLong but for the lateral tire axis.
\param[in] tireDirectionState describes the longitudinal and lateral directions of the tire in the world frame.
\param[in] tireStickyState describes the low speed state of the tire in the longitudinal and lateral directions.
\param[in] rigidBodyState describes the pose of the rigid body.
\param[out] constraintState is the data structure that will be read by the associated PxScene in the next call to 
PxScene::simulate().
\note Constraints include suspension constraints to account for suspension travel limit and sticky 
tire constraints that bring the vehicle to rest at low longitudinal and lateral speed.
*/
void PxVehiclePhysXConstraintStatesUpdate
(const PxVehicleSuspensionParams& suspensionParams,
 const PxVehiclePhysXSuspensionLimitConstraintParams& suspensionLimitParams,
 const PxVehicleSuspensionState& suspensionState, const PxVehicleSuspensionComplianceState& suspensionComplianceState,
 const PxVec3& groundPlaneNormal,
 const PxReal tireStickyDampingLong, const PxReal tireStickyDampingLat, 
 const PxVehicleTireDirectionState& tireDirectionState, const PxVehicleTireStickyState& tireStickyState,
 const PxVehicleRigidBodyState& rigidBodyState,
 PxVehiclePhysXConstraintState& constraintState);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

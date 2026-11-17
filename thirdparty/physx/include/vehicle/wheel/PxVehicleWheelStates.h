// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_WHEEL_STATES_H
#define PX_VEHICLE_WHEEL_STATES_H

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxTransform.h"
#include "foundation/PxMemory.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief It is useful to know if a brake or drive torque is to be applied to a wheel. 
*/
struct PxVehicleWheelActuationState
{
	bool isBrakeApplied;		//!< True if a brake torque is applied, false if not.
	bool isDriveApplied;		//!< True if a drive torque is applied, false if not.

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleWheelActuationState));
	}
};

struct PxVehicleWheelRigidBody1dState
{
	/**
	\brief The rotation speed of the wheel around the lateral axis.

	<b>Unit:</b> radians / time
	*/
	PxReal rotationSpeed;

	/**
	\brief The corrected rotation speed of the wheel around the lateral axis in radians per second.

	At low forward wheel speed, the wheel rotation speed can get unstable (depending on the tire 
	model used) and, for example, oscillate. To integrate the wheel rotation angle, a (potentially)
	blended rotation speed is used which gets stored in #correctedRotationSpeed.

	<b>Unit:</b> radians / time

	\see PxVehicleSimulationContext::thresholdForwardSpeedForWheelAngleIntegration
	*/
	PxReal correctedRotationSpeed;

	/**
	\brief The accumulated angle of the wheel around the lateral axis in radians in range (-2*Pi,2*Pi)
	*/
	PxReal rotationAngle;

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleWheelRigidBody1dState));
	}
};

struct PxVehicleWheelLocalPose
{
	PxTransform localPose;		//!< The pose of the wheel in the rigid body frame.
	
	PX_FORCE_INLINE void setToDefault()
	{
		localPose = PxTransform(PxIdentity);
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


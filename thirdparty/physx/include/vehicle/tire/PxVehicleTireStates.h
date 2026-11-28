// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_TIRE_STATES_H
#define PX_VEHICLE_TIRE_STATES_H

#include "foundation/PxVec3.h"
#include "foundation/PxMemory.h"

#include "vehicle/PxVehicleParams.h"

#if !PX_DOXYGEN
namespace physx
{
#endif


/**
\brief PxVehicleTireDirectionState stores the world frame lateral and longtidinal axes of the tire after 
projecting the wheel pose in the world frame onto the road geometry plane (also in the world frame).
*/
struct PxVehicleTireDirectionState
{
	PxVec3 directions[PxVehicleTireDirectionModes::eMAX_NB_PLANAR_DIRECTIONS];

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleTireDirectionState));
	}
};


/**
\brief PxVehicleTireSpeedState stores the components of the instantaneous velocity of the rigid body at the tire contact point projected 
along the lateral and longitudinal axes of the tire.
\see PxVehicleTireDirectionState
*/
struct PxVehicleTireSpeedState
{
	PxReal speedStates[PxVehicleTireDirectionModes::eMAX_NB_PLANAR_DIRECTIONS];

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleTireSpeedState));
	}
};

/**
\brief The lateral and longitudinal tire slips.
\see PxVehicleTireSpeedState
*/
struct PxVehicleTireSlipState
{
	PxReal slips[PxVehicleTireDirectionModes::eMAX_NB_PLANAR_DIRECTIONS];

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleTireSlipState));
	}
};

/**
\brief The load and friction experienced by a tire.
*/
struct PxVehicleTireGripState
{
	/**
	\brief The tire load

	<b>Unit:</b> force = mass * length / (time^2)
	*/
	PxReal load;

	/**
	\brief The tire friction is the product of the road geometry friction and a friction response multiplier.
	*/
	PxReal friction;

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleTireGripState));
	}
};

/**
\brief Camber angle of the tire relative to the ground plane.
*/
struct PxVehicleTireCamberAngleState
{
	PxReal camberAngle;

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleTireCamberAngleState));
	}
};

/**
\brief Prolonged low speeds in the lateral and longitudinal directions may be handled with "sticky" velocity constraints that activate after
a speed below a threshold has been recorded for a threshold time.
\see PxVehicleTireStickyParams
\see PxVehicleTireSpeedState
*/
struct PxVehicleTireStickyState
{
	PxReal lowSpeedTime[PxVehicleTireDirectionModes::eMAX_NB_PLANAR_DIRECTIONS];
	bool activeStatus[PxVehicleTireDirectionModes::eMAX_NB_PLANAR_DIRECTIONS];

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleTireStickyState));
	}
};

/**
\brief The longitudinal/lateral forces/torques that develop on the tire.
*/
struct PxVehicleTireForce
{
	/*
	\brief The tire forces that develop along the tire's longitudinal and lateral directions. Specified in the world frame.
	*/
	PxVec3 forces[PxVehicleTireDirectionModes::eMAX_NB_PLANAR_DIRECTIONS];

	/*
	\brief The tire torques that develop around the tire's longitudinal and lateral directions. Specified in the world frame.
	*/
	PxVec3 torques[PxVehicleTireDirectionModes::eMAX_NB_PLANAR_DIRECTIONS];

	/**
	\brief The aligning moment may be propagated to a torque-driven steering controller.
	*/
	PxReal aligningMoment;

	/**
	\brief The torque to apply to the wheel's 1d rigid body.
	*/
	PxReal wheelTorque;

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleTireForce));
	}
};


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

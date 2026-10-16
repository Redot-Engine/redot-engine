// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_COMMAND_STATES_H
#define PX_VEHICLE_COMMAND_STATES_H

#include "foundation/PxPreprocessor.h"
#include "foundation/PxSimpleTypes.h"
#include "foundation/PxMemory.h"
#include "vehicle/PxVehicleLimits.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief A description of the state of commands that are applied to the vehicle
\note brakes[0] and brakes[1] may be used to distinguish brake and handbrake controls.
*/
struct PxVehicleCommandState
{
	PxReal brakes[2];	 //!< The instantaneous state of the brake controllers in range [0,1] with 1 denoting fully pressed and 0 fully depressed.
	PxU32 nbBrakes;		 //|< The number of brake commands.
	PxReal throttle;	 //!< The instantaneous state of the throttle controller in range [0,1] with 1 denoting fully pressed and 0 fully depressed.
	PxReal steer;		 //!< The instantaneous state of the steer controller in range [-1,1].

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleCommandState));
	}
};

/**
\brief A description of the state of transmission-related commands that are applied to a vehicle with direct drive.
*/
struct PxVehicleDirectDriveTransmissionCommandState
{
	/**
	\brief Direct drive vehicles only have reverse, neutral or forward gear.
	*/
	enum Enum
	{
		eREVERSE = 0,
		eNEUTRAL,
		eFORWARD
	};

	Enum gear;	//!< The desired gear of the input gear controller.

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleDirectDriveTransmissionCommandState));
	}
};

/**
\brief A description of the state of transmission-related commands that are applied to a vehicle with engine drive.
*/
struct PxVehicleEngineDriveTransmissionCommandState
{
	enum Enum
	{
		/**
		\brief Special gear value to denote the automatic shift mode (often referred to as DRIVE).
		
		When using automatic transmission, setting this value as target gear will enable automatic 
		gear shifts between first and highest gear. If the current gear is a reverse gear or
		the neutral gear, then this value will trigger a shift to first gear. If this value is
		used even though there is no automatic transmission available, the gear state will remain
		unchanged.
		*/
		eAUTOMATIC_GEAR = 0xff
	};

	PxReal clutch;		//!< The instantaneous state of the clutch controller in range [0,1] with 1 denoting fully pressed and 0 fully depressed.
	PxU32 targetGear;	//!< The desired gear of the input gear controller.

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleEngineDriveTransmissionCommandState));
	}
};

/**
\brief A description of the state of transmission-related commands that are applied to a vehicle with tank drive.
*/
struct PxVehicleTankDriveTransmissionCommandState : public PxVehicleEngineDriveTransmissionCommandState
{
	/**
	\brief The wheels of each tank track are either all connected to thrusts[0] or all connected to thrusts[1].
	\note The thrust commands are used to divert torque from the engine to the wheels of the tank tracks controlled by each thrust.
	\note thrusts[0] and thrusts[1] are in range [-1,1] with the sign dictating whether the thrust will be applied positively or negatively with respect to the gearing ratio.
	*/
	PxReal thrusts[2];

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleTankDriveTransmissionCommandState));
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

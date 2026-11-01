// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_BRAKING_FUNCTIONS_H
#define PX_VEHICLE_BRAKING_FUNCTIONS_H

#include "foundation/PxPreprocessor.h"
#include "foundation/PxMath.h"
#include "PxVehicleBrakingParams.h"
#include "../commands/PxVehicleCommandStates.h"
#include "../commands/PxVehicleCommandHelpers.h"
#include "../drivetrain/PxVehicleDrivetrainParams.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Compute the brake torque response to an array of brake commands.
\param[in] brakeCommands is the array of input brake commands to be applied to the vehicle.
\param[in] nbBrakeCommands is the number of input brake commands to be applied to the vehicle.
\param[in] longitudinalSpeed is the longitudinal speed of the vehicle.
\param[in] wheelId specifies the wheel that is to have its brake response computed.
\param[in] brakeResponseParams specifies the per wheel brake torque response to each brake command as a nonlinear function of brake command and longitudinal speed.
\param[out] brakeResponseState is the brake torque response to the input brake command. 
\note commands.brakes[i] and brakeResponseParams[i] are treated as pairs of brake command and brake command response.
*/
PX_FORCE_INLINE void PxVehicleBrakeCommandResponseUpdate
(const PxReal* brakeCommands, const PxU32 nbBrakeCommands, const PxReal longitudinalSpeed,
 const PxU32 wheelId, const PxVehicleSizedArrayData<const PxVehicleBrakeCommandResponseParams>& brakeResponseParams,
 PxReal& brakeResponseState)
{
	PX_CHECK_AND_RETURN(nbBrakeCommands <= brakeResponseParams.size, "PxVehicleBrakeCommandLinearUpdate: nbBrakes must be less than or equal to brakeResponseParams.size");
	PxReal sum = 0.0f;
	for (PxU32 i = 0; i < nbBrakeCommands; i++)
	{
		sum += PxVehicleNonLinearResponseCompute(brakeCommands[i], longitudinalSpeed, wheelId, brakeResponseParams[i]);
	}
	brakeResponseState = sum;
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

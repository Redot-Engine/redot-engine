// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_COMMAND_HELPERS_H
#define PX_VEHICLE_COMMAND_HELPERS_H

#include "vehicle/PxVehicleParams.h"
#include "PxVehicleCommandParams.h"
#include "PxVehicleCommandStates.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Compute the linear response to a command.
\param[in] command is a normalised command value.
\param[in] wheelId specifies the wheel that is to respond to the command.
\param[in] responseParams specifies the wheel responses for all wheels on a vehicle.
\return The linear response of the specified  wheel to the command.
*/
PX_FORCE_INLINE PxReal PxVehicleLinearResponseCompute
(const PxReal command, const PxU32 wheelId, const PxVehicleCommandResponseParams& responseParams)
{
	return command*responseParams.maxResponse*responseParams.wheelResponseMultipliers[wheelId];
}

/**
\brief Compute the non-linear response to a command.
\param[in] command is a normalised command value.
\param[in] longitudinalSpeed is the longitudional speed of the vehicle. 
\param[in] wheelId specifies the wheel that is to respond to the command.
\param[in] responseParams specifies the wheel responses for all wheels on a vehicle.
\note responseParams is used to compute an interpolated normalized response to the combination of command and longitudinalSpeed. 
The interpolated normalized response is then used in place of the command as input to PxVehicleComputeLinearResponse().
*/
PxReal PxVehicleNonLinearResponseCompute
(const PxReal command, const PxReal longitudinalSpeed, const PxU32 wheelId, const PxVehicleCommandResponseParams& responseParams);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

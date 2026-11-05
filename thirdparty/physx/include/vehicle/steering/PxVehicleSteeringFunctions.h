// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_STEERING_FUNCTIONS_H
#define PX_VEHICLE_STEERING_FUNCTIONS_H

#include "foundation/PxPreprocessor.h"
#include "foundation/PxSimpleTypes.h"

#include "vehicle/PxVehicleParams.h"
#include "PxVehicleSteeringParams.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxVehicleCommandState;

/**
\brief Compute the yaw angle response to a steer command.
\param[in] steer is the input steer command value.
\param[in] longitudinalSpeed is the longitudinal speed of the vehicle.
\param[in] wheelId specifies the wheel to have its steer response computed.
\param[in] steerResponseParams specifies the per wheel yaw angle response to the steer command as a nonlinear function of steer command and longitudinal speed.
\param[out] steerResponseState is the yaw angle response to the input steer command.
*/
void PxVehicleSteerCommandResponseUpdate
(const PxReal steer, const PxReal longitudinalSpeed,
 const PxU32 wheelId, const PxVehicleSteerCommandResponseParams& steerResponseParams,
 PxReal& steerResponseState);

/**
\brief Account for Ackermann correction by modifying the per wheel steer response multipliers to engineer an asymmetric steer response across axles.
\param[in] steer is the input steer command value.
\param[in] steerResponseParams describes the maximum response and a response multiplier per axle.
\param[in] ackermannParams is an array that describes the wheels affected by Ackermann steer correction.
\param[in,out] steerResponseStates contains the corrected per wheel steer response multipliers that take account of Ackermann steer correction.
*/
void  PxVehicleAckermannSteerUpdate
(const PxReal steer, const PxVehicleSteerCommandResponseParams& steerResponseParams,
 const PxVehicleSizedArrayData<const PxVehicleAckermannParams>& ackermannParams,
 PxVehicleArrayData<PxReal>& steerResponseStates);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

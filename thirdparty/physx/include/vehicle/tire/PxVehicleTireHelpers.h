// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_TIRE_HELPERS_H
#define PX_VEHICLE_TIRE_HELPERS_H

#include "vehicle/PxVehicleParams.h"
#include "vehicle/wheel/PxVehicleWheelStates.h"
#include "PxVehicleTireStates.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Compute the intention to accelerate by inspecting the actuation states of the wheels of a powered vehicle.
\param[in] poweredVehicleAxleDesc describes the axles and wheels of a powered vehicle in a jointed ensemble of vehicles.
\param[in] poweredVehicleActuationStates describes the drive state of each wheel of the powered vehicle.
\see PxVehicleTireStickyStateReset
*/
bool PxVehicleAccelerationIntentCompute
(const PxVehicleAxleDescription& poweredVehicleAxleDesc, const PxVehicleArrayData<const PxVehicleWheelActuationState>& poweredVehicleActuationStates);

/**
\brief Reset the sticky tire states of an unpowered vehicle if it is in a jointed ensemble of vehicles with at least one powered vehicle. 
\param[in] poweredVehicleIntentionToAccelerate describes the state of the powered vehicle in an ensemble of jointed vehicles.
\param[in] unpoweredVehicleAxleDesc describes the axles and wheels of an unpowered vehicle towed by a powered vehicle.
\param[out] unpoweredVehicleStickyState is the sticky state of the wheels of an unpowered vehicle towed by a powered vehicle. 
\note If any wheel on the powered vehicle is to receive a drive torque, the sticky tire states of the towed vehicle will be reset to the deactivated state.
\note poweredVehicleIntentionToAccelerate may be computed using PxVehicleAccelerationIntentCompute().
\see PxVehicleAccelerationIntentCompute
*/
void PxVehicleTireStickyStateReset
(const bool poweredVehicleIntentionToAccelerate,
 const PxVehicleAxleDescription& unpoweredVehicleAxleDesc,
 PxVehicleArrayData<PxVehicleTireStickyState>& unpoweredVehicleStickyState);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

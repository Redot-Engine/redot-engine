// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_BRAKING_PARAMS_H
#define PX_VEHICLE_BRAKING_PARAMS_H

#include "foundation/PxFoundation.h"

#include "vehicle/PxVehicleParams.h"

#include "vehicle/commands/PxVehicleCommandParams.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Distribute a brake response to the wheels of a vehicle.
\note The brake torque of each wheel on the ith wheel is brakeCommand * maxResponse * wheelResponseMultipliers[i].
\note A typical use case is to set maxResponse to be the vehicle's maximum achievable brake torque
that occurs when the brake command is equal to 1.0.  The array wheelResponseMultipliers[i] would then be used
to specify the maximum achievable brake torque per wheel as a fractional multiplier of the vehicle's maximum achievable brake torque.
*/
struct PxVehicleBrakeCommandResponseParams : public PxVehicleCommandResponseParams
{
	PX_FORCE_INLINE PxVehicleBrakeCommandResponseParams transformAndScale(
		const PxVehicleFrame& srcFrame, const PxVehicleFrame& trgFrame, const PxVehicleScale& srcScale, const PxVehicleScale& trgScale) const
	{
		PX_UNUSED(srcFrame);
		PX_UNUSED(trgFrame);
		PxVehicleBrakeCommandResponseParams r = *this;
		const PxReal scale = trgScale.scale/srcScale.scale;
		r.maxResponse *= (scale*scale);		//maxResponse is a torque!
		return r;
	}

	PX_FORCE_INLINE bool isValid(const PxVehicleAxleDescription& axleDesc) const
	{
		if (!axleDesc.isValid())
			return false;
		PX_CHECK_AND_RETURN_VAL(maxResponse >= 0.0f, "PxVehicleBrakeCommandResponseParams.maxResponse must be greater than or equal to zero", false);
		for (PxU32 i = 0; i < axleDesc.nbWheels; i++)
		{
			PX_CHECK_AND_RETURN_VAL(wheelResponseMultipliers[axleDesc.wheelIdsInAxleOrder[i]] >= 0.0f, "PxVehicleBrakeCommandResponseParams.wheelResponseMultipliers[i] must be greater than or equal to zero.", false);
		}
		return true;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

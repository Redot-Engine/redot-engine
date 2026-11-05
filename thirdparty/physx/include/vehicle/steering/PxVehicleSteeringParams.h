// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_STEERING_PARAMS_H
#define PX_VEHICLE_STEERING_PARAMS_H

#include "foundation/PxFoundation.h"

#include "vehicle/PxVehicleParams.h"

#include "vehicle/commands/PxVehicleCommandParams.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxVehicleFrame;
struct PxVehicleScale;

/**
\brief Distribute a steer response to the wheels of a vehicle.
\note The steer angle applied to each wheel on the ith wheel is steerCommand * maxResponse * wheelResponseMultipliers[i].
\note A typical use case is to set maxResponse to be the vehicle's maximum achievable steer angle
that occurs when the steer command is equal to 1.0. The array wheelResponseMultipliers[i] would then be used
to specify the maximum achievable steer angle per wheel as a fractional multiplier of the vehicle's maximum achievable steer angle.
*/
struct PxVehicleSteerCommandResponseParams : public PxVehicleCommandResponseParams
{
	PX_FORCE_INLINE PxVehicleSteerCommandResponseParams transformAndScale(
		const PxVehicleFrame& srcFrame, const PxVehicleFrame& trgFrame, const PxVehicleScale& srcScale, const PxVehicleScale& trgScale) const
	{
		PX_UNUSED(srcFrame);
		PX_UNUSED(trgFrame);
		PX_UNUSED(srcScale);
		PX_UNUSED(trgScale);
		return *this;
	}

	PX_FORCE_INLINE bool isValid(const PxVehicleAxleDescription& axleDesc) const
	{
		if (!axleDesc.isValid())
			return false;
		for (PxU32 i = 0; i < axleDesc.nbWheels; i++)
		{
			PX_CHECK_AND_RETURN_VAL(PxAbs(maxResponse*wheelResponseMultipliers[axleDesc.wheelIdsInAxleOrder[i]]) <= PxPi,
					"PxVehicleSteerCommandResponseParams.maxResponse*PxVehicleSteerCommandResponseParams.wheelResponseMultipliers[i] must be in range [-Pi, Pi]", false);
		}
		return true;
	}
};

/**
\brief A description of a single axle that is to be affected by Ackermann steer correction.
*/
struct PxVehicleAckermannParams
{
	PxU32 wheelIds[2];		//!< wheelIds[0] is the id of the wheel that is negative along the lateral axis, wheelIds[1] is the wheel id that is positive along the lateral axis.
	PxReal wheelBase;		//!< wheelBase is the longitudinal distance between the axle that is affected by Ackermann correction and a reference axle.
	PxReal trackWidth;		//!< trackWidth is the width of the axle specified by #wheelIds
	PxReal strength;		//!< is the strength of the correction with 0 denoting no correction and 1 denoting perfect correction.

	PX_FORCE_INLINE bool isValid(const PxVehicleAxleDescription& axleDesc) const
	{
		PX_CHECK_AND_RETURN_VAL(0.0f == strength || wheelIds[0] < axleDesc.getNbWheels(), "PxVehicleAckermannParams.wheelIds[0] must be valid wheel", false);
		PX_CHECK_AND_RETURN_VAL(0.0f == strength || wheelIds[1] < axleDesc.getNbWheels(), "PxVehicleAckermannParams.wheelIds[1] must be a valid wheel", false);
		PX_CHECK_AND_RETURN_VAL(0.0f == strength || wheelIds[0] != wheelIds[1], "PxVehicleAckermannParams.wheelIds[0] and PxVehicleAckermannParams.wheelIds[1] must reference two different wheels", false);
		PX_CHECK_AND_RETURN_VAL(0.0f == strength || wheelBase > 0.0f, "PxVehicleAckermannParams.wheelBase must be greater than zero", false);
		PX_CHECK_AND_RETURN_VAL(0.0f == strength || trackWidth > 0.0f, "PxVehicleAckermannParams.trackWidth must be greater than zero", false);
		PX_CHECK_AND_RETURN_VAL(strength >= 0.0f && strength <= 1.0f, "PxVehicleAckermannParams.strength must be in range [0,1]", false);
		PX_UNUSED(axleDesc);
		return true;
	}

	PX_FORCE_INLINE PxVehicleAckermannParams transformAndScale(
		const PxVehicleFrame& srcFrame, const PxVehicleFrame& trgFrame, const PxVehicleScale& srcScale, const PxVehicleScale& trgScale) const
	{
		PX_UNUSED(srcFrame);
		PX_UNUSED(trgFrame);
		PxVehicleAckermannParams r = *this;
		const PxReal scale = trgScale.scale / srcScale.scale;
		r.wheelBase *= scale;
		r.trackWidth *= scale;
		return r;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

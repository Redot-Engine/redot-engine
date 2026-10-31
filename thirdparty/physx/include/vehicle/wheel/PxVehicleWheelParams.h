// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_WHEEL_PARAMS_H
#define PX_VEHICLE_WHEEL_PARAMS_H

#include "foundation/PxFoundation.h"

#include "vehicle/PxVehicleParams.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxVehicleWheelParams
{
	/**
	\brief Radius of unit that includes metal wheel plus rubber tire.

	<b>Range:</b> (0, inf)<br>
	<b>Unit:</b> length
	*/
	PxReal radius;

	/**
	\brief Half-width of unit that includes wheel plus tire.

	<b>Range:</b> (0, inf)<br>
	<b>Unit:</b> length
	*/
	PxReal halfWidth;

	/**
	\brief Mass of unit that includes wheel plus tire.

	<b>Range:</b> (0, inf)<br>
	<b>Unit:</b> mass
	*/
	PxReal mass;

	/**
	\brief Moment of inertia of unit that includes wheel plus tire about the rolling axis.

	<b>Range:</b> (0, inf)<br>
	<b>Unit:</b> mass * (length^2)
	*/
	PxReal moi;

	/**
	\brief Damping rate applied to wheel.

	<b>Range:</b> [0, inf)<br>
	<b>Unit:</b> torque * time = mass * (length^2) / time
	*/
	PxReal dampingRate;

	PX_FORCE_INLINE PxVehicleWheelParams transformAndScale(
		const PxVehicleFrame& srcFrame, const PxVehicleFrame& trgFrame, const PxVehicleScale& srcScale, const PxVehicleScale& trgScale) const
	{
		PX_UNUSED(srcFrame);
		PX_UNUSED(trgFrame);
		PxVehicleWheelParams r = *this;
		const PxReal scale = trgScale.scale/srcScale.scale;
		r.radius *= scale;
		r.halfWidth *= scale;
		r.moi *= (scale*scale);
		r.dampingRate *= (scale*scale);
		return r;
	}

	PX_FORCE_INLINE bool isValid() const
	{
		PX_CHECK_AND_RETURN_VAL(radius > 0.0f, "PxVehicleWheelParams.radius must be greater than zero", false);
		PX_CHECK_AND_RETURN_VAL(halfWidth > 0.0f, "PxVehicleWheelParams.halfWidth must be greater than zero", false);
		PX_CHECK_AND_RETURN_VAL(mass > 0.0f, "PxVehicleWheelParams.mass must be greater than zero", false);
		PX_CHECK_AND_RETURN_VAL(moi > 0.0f, "PxVehicleWheelParams.moi must be greater than zero", false);
		PX_CHECK_AND_RETURN_VAL(dampingRate >= 0.0f, "PxVehicleWheelParams.dampingRate must be greater than or equal to zero", false);
		return true;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


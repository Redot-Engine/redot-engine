// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_RIGIDBODY_PARAMS_H
#define PX_VEHICLE_RIGIDBODY_PARAMS_H

#include "foundation/PxFoundation.h"
#include "vehicle/PxVehicleParams.h"
#include "vehicle/PxVehicleFunctions.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief The properties of the rigid body.
*/
struct PxVehicleRigidBodyParams
{
	/**
	\brief The mass of the rigid body.

	<b>Range:</b> (0, inf)<br>
	<b>Unit:</b> mass
	*/
	PxReal mass;

	/**
	\brief The moment of inertia of the rigid body.

	<b>Range:</b> (0, inf)<br>
	<b>Unit:</b> mass * (length^2)
	*/
	PxVec3 moi;

	PX_FORCE_INLINE PxVehicleRigidBodyParams transformAndScale(
		const PxVehicleFrame& srcFrame, const PxVehicleFrame& trgFrame, const PxVehicleScale& srcScale, const PxVehicleScale& trgScale) const
	{
		PxVehicleRigidBodyParams r = *this;
		r.moi = PxVehicleTransformFrameToFrame(srcFrame, trgFrame, moi).abs();
		const PxReal scale = trgScale.scale/srcScale.scale;
		r.moi *= (scale*scale);
		return r;
	}

	PX_FORCE_INLINE bool isValid() const
	{
		PX_CHECK_AND_RETURN_VAL(mass > 0.0f, "PxVehicleRigidBodyParams.mass must be greater than zero", false);
		PX_CHECK_AND_RETURN_VAL(moi.x > 0.0f && moi.y > 0.0f && moi.z> 0.0f, "PxVehicleRigidBodyParams.moi must be greater than zero", false);
		return true;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


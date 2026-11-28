// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_RIGIDBODY_STATES_H
#define PX_VEHICLE_RIGIDBODY_STATES_H

#include "foundation/PxTransform.h"
#include "foundation/PxVec3.h"
#include "vehicle/PxVehicleParams.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxVehicleRigidBodyState
{
	PxTransform pose;				//!< the body's pose (in world space)
	PxVec3 linearVelocity;			//!< the body's linear velocity (in world space)
	PxVec3 angularVelocity;			//!< the body's angular velocity (in world space)
	PxVec3 previousLinearVelocity;	//!< the previous linear velocity of the body (in world space)
	PxVec3 previousAngularVelocity;	//!< the previous angular velocity of the body (in world space)
	PxVec3 externalForce;			//!< external force (in world space) affecting the rigid body (usually excluding gravitational force)
	PxVec3 externalTorque;			//!< external torque (in world space) affecting the rigid body

	PX_FORCE_INLINE void setToDefault()
	{
		pose = PxTransform(PxIdentity);
		linearVelocity = PxVec3(PxZero);
		angularVelocity = PxVec3(PxZero);
		externalForce = PxVec3(PxZero);
		externalTorque = PxVec3(PxZero);
	}

	/**
	\brief Compute the vertical speed of the rigid body transformed to the world frame.
	\param[in] frame describes the axes of the vehicle
	*/
	PX_FORCE_INLINE PxReal getVerticalSpeed(const PxVehicleFrame& frame) const
	{
		return linearVelocity.dot(pose.q.rotate(frame.getVrtAxis()));
	}

	/**
	\param[in] frame describes the axes of the vehicle
	\brief Compute the lateral speed of the rigid body transformed to the world frame.
	*/
	PX_FORCE_INLINE PxReal getLateralSpeed(const PxVehicleFrame& frame) const
	{
		return linearVelocity.dot(pose.q.rotate(frame.getLatAxis()));
	}

	/**
	\brief Compute the longitudinal speed of the rigid body transformed to the world frame.
	\param[in] frame describes the axes of the vehicle
	*/
	PX_FORCE_INLINE PxReal getLongitudinalSpeed(const PxVehicleFrame& frame) const
	{
		return linearVelocity.dot(pose.q.rotate(frame.getLngAxis()));
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

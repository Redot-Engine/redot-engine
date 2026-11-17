// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_RIGIDBODY_COMPONENTS_H
#define PX_VEHICLE_RIGIDBODY_COMPONENTS_H

#include "vehicle/PxVehicleParams.h"
#include "vehicle/PxVehicleComponent.h"

#include "PxVehicleRigidBodyFunctions.h"

#include "common/PxProfileZone.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Forward integrate the momentum and pose of the vehicle's rigid body after applying forces and torques 
from the suspension, tires and anti-roll bars.
*/
class PxVehicleRigidBodyComponent : public PxVehicleComponent
{
public:

	PxVehicleRigidBodyComponent() : PxVehicleComponent() {}
	virtual ~PxVehicleRigidBodyComponent() {}

	/**
	\brief Retrieve pointers to the parameter and state data required to update the dynamic state of a rigid body.
	\param[out] axleDescription must  be returned as a non-null pointer to a single PxVehicleAxleDescription instance that describes the wheels and axles
	of the vehicle.
	\param[out] rigidBodyParams must  be returned as a non-null pointer to a single PxVehicleRigidBodyParams instance that describes the mass and moment of 
	inertia of the rigid body. 
	\param[out] suspensionForces must be returned as a non-null pointer to an array of suspension forces and torques in the world frame. 
	The suspension forces and torques will be applied to the rigid body to update *rigidBodyState*.
	\param[out] tireForces must  be returned as a non-null pointer to an array of tire forces and torques in the world frame.
	The tire forces and torques will be applied to the rigid body to update *rigidBodyState*.
	\param[out] antiRollTorque may be returned an optionally non-null pointer to a single PxVehicleAntiRollTorque instance that contains the accumulated anti-roll
	torque to apply to the rigid body.
	\param[out] rigidBodyState imust be returned as a non-null pointer to a single PxVehicleRigidBodyState instance that is to be forward integrated.
	\note The suspensionForces array must contain an entry for each wheel listed as an active wheel in axleDescription.
	\note The tireForces array must contain an entry for each wheel listed as an active wheel in axleDescription.
	\note If antiRollTorque is returned as a null pointer then zero anti-roll torque will be applied to the rigid body.
	*/
	virtual void getDataForRigidBodyComponent(
		const PxVehicleAxleDescription*& axleDescription,
		const PxVehicleRigidBodyParams*& rigidBodyParams,
		PxVehicleArrayData<const PxVehicleSuspensionForce>& suspensionForces,
		PxVehicleArrayData<const PxVehicleTireForce>& tireForces,
		const PxVehicleAntiRollTorque*& antiRollTorque,
		PxVehicleRigidBodyState*& rigidBodyState) = 0;

	virtual bool update(const PxReal dt, const PxVehicleSimulationContext& context)
	{
		PX_PROFILE_ZONE("PxVehicleRigidBodyComponent::update", 0);

		const PxVehicleAxleDescription* axleDescription;
		const PxVehicleRigidBodyParams* rigidBodyParams;
		PxVehicleArrayData<const PxVehicleSuspensionForce> suspensionForces;
		PxVehicleArrayData<const PxVehicleTireForce> tireForces;
		const PxVehicleAntiRollTorque* antiRollTorque;
		PxVehicleRigidBodyState* rigidBodyState;

		getDataForRigidBodyComponent(axleDescription, rigidBodyParams, 
			suspensionForces, tireForces, antiRollTorque,
			rigidBodyState);

		PxVehicleRigidBodyUpdate(
			*axleDescription, *rigidBodyParams, 
			suspensionForces, tireForces, antiRollTorque,
			dt, context.gravity, 
			*rigidBodyState);

		return true;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

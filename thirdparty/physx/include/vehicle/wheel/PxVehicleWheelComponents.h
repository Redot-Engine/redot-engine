// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_WHEEL_COMPONENTS_H
#define PX_VEHICLE_WHEEL_COMPONENTS_H

#include "vehicle/PxVehicleParams.h"
#include "vehicle/PxVehicleComponent.h"

#include "vehicle/commands/PxVehicleCommandHelpers.h"
#include "vehicle/tire/PxVehicleTireStates.h"

#include "PxVehicleWheelFunctions.h"
#include "PxVehicleWheelHelpers.h"

#include "common/PxProfileZone.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxVehicleWheelComponent : public PxVehicleComponent
{
public:

	PxVehicleWheelComponent() : PxVehicleComponent() {}
	virtual ~PxVehicleWheelComponent() {}

	virtual void getDataForWheelComponent(
		const PxVehicleAxleDescription*& axleDescription,
		PxVehicleArrayData<const PxReal>& steerResponseStates,
		PxVehicleArrayData<const PxVehicleWheelParams>& wheelParams,
		PxVehicleArrayData<const PxVehicleSuspensionParams>& suspensionParams,
		PxVehicleArrayData<const PxVehicleWheelActuationState>& actuationStates,
		PxVehicleArrayData<const PxVehicleSuspensionState>& suspensionStates,
		PxVehicleArrayData<const PxVehicleSuspensionComplianceState>& suspensionComplianceStates,
		PxVehicleArrayData<const PxVehicleTireSpeedState>& tireSpeedStates,
		PxVehicleArrayData<PxVehicleWheelRigidBody1dState>& wheelRigidBody1dStates,
  	    PxVehicleArrayData<PxVehicleWheelLocalPose>& wheelLocalPoses) = 0;

	virtual bool update(const PxReal dt, const PxVehicleSimulationContext& context)
	{
		PX_PROFILE_ZONE("PxVehicleWheelComponent::update", 0);

		const PxVehicleAxleDescription* axleDescription;
		PxVehicleArrayData<const PxReal> steerResponseStates;
		PxVehicleArrayData<const PxVehicleWheelParams> wheelParams;
		PxVehicleArrayData<const PxVehicleSuspensionParams> suspensionParams;
		PxVehicleArrayData<const PxVehicleWheelActuationState> actuationStates;
		PxVehicleArrayData<const PxVehicleSuspensionState> suspensionStates;
		PxVehicleArrayData<const PxVehicleSuspensionComplianceState> suspensionComplianceStates;
		PxVehicleArrayData<const PxVehicleTireSpeedState> tireSpeedStates;
		PxVehicleArrayData<PxVehicleWheelRigidBody1dState> wheelRigidBody1dStates;
		PxVehicleArrayData<PxVehicleWheelLocalPose> wheelLocalPoses;

		getDataForWheelComponent(axleDescription, steerResponseStates,
			wheelParams, suspensionParams, actuationStates, suspensionStates,
			suspensionComplianceStates, tireSpeedStates, wheelRigidBody1dStates,
			wheelLocalPoses);

		for (PxU32 i = 0; i < axleDescription->nbWheels; i++)
		{
			const PxU32 wheelId = axleDescription->wheelIdsInAxleOrder[i];

			PxVehicleWheelRotationAngleUpdate(
				wheelParams[wheelId], 
				actuationStates[wheelId], suspensionStates[wheelId], 
				tireSpeedStates[wheelId], 
				context.thresholdForwardSpeedForWheelAngleIntegration, dt, 
				wheelRigidBody1dStates[wheelId]);

			wheelLocalPoses[wheelId].localPose = PxVehicleComputeWheelLocalPose(context.frame, 
				suspensionParams[wheelId],
				suspensionStates[wheelId], 
				suspensionComplianceStates[wheelId],
				steerResponseStates[wheelId],
				wheelRigidBody1dStates[wheelId]);
		}

		return true;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

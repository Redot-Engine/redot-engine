// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_PHYSX_CONSTRAINT_COMPONENTS_H
#define PX_VEHICLE_PHYSX_CONSTRAINT_COMPONENTS_H

#include "vehicle/PxVehicleParams.h"
#include "vehicle/PxVehicleComponent.h"

#include "vehicle/roadGeometry/PxVehicleRoadGeometryState.h"
#include "vehicle/suspension/PxVehicleSuspensionParams.h"
#include "vehicle/suspension/PxVehicleSuspensionStates.h"

#include "PxVehiclePhysXConstraintFunctions.h"
#include "PxVehiclePhysXConstraintHelpers.h"
#include "PxVehiclePhysXConstraintStates.h"
#include "PxVehiclePhysXConstraintParams.h"

#include "common/PxProfileZone.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxVehiclePhysXConstraintComponent : public PxVehicleComponent
{
public:
	PxVehiclePhysXConstraintComponent() : PxVehicleComponent() {}
	virtual ~PxVehiclePhysXConstraintComponent() {}

	virtual void getDataForPhysXConstraintComponent(
		const PxVehicleAxleDescription*& axleDescription,
		const PxVehicleRigidBodyState*& rigidBodyState,
		PxVehicleArrayData<const PxVehicleSuspensionParams>& suspensionParams,
		PxVehicleArrayData<const PxVehiclePhysXSuspensionLimitConstraintParams>& suspensionLimitParams,
		PxVehicleArrayData<const PxVehicleSuspensionState>& suspensionStates,
		PxVehicleArrayData<const PxVehicleSuspensionComplianceState>& suspensionComplianceStates,
		PxVehicleArrayData<const PxVehicleRoadGeometryState>& wheelRoadGeomStates,
		PxVehicleArrayData<const PxVehicleTireDirectionState>& tireDirectionStates,
		PxVehicleArrayData<const PxVehicleTireStickyState>& tireStickyStates,
		PxVehiclePhysXConstraints*& constraints) = 0;

	virtual bool update(const PxReal dt, const PxVehicleSimulationContext& context)
	{
		PX_UNUSED(dt);
		PX_UNUSED(context);

		PX_PROFILE_ZONE("PxVehiclePhysXConstraintComponent::update", 0);

		const PxVehicleAxleDescription* axleDescription;
		const PxVehicleRigidBodyState* rigidBodyState;
		PxVehicleArrayData<const PxVehicleSuspensionParams> suspensionParams;
		PxVehicleArrayData<const PxVehiclePhysXSuspensionLimitConstraintParams> suspensionLimitParams;
		PxVehicleArrayData<const PxVehicleSuspensionState> suspensionStates;
		PxVehicleArrayData<const PxVehicleSuspensionComplianceState> suspensionComplianceStates;
		PxVehicleArrayData<const PxVehicleRoadGeometryState> wheelRoadGeomStates;
		PxVehicleArrayData<const PxVehicleTireDirectionState> tireDirectionStates;
		PxVehicleArrayData<const PxVehicleTireStickyState> tireStickyStates;
		PxVehiclePhysXConstraints* constraints;

		getDataForPhysXConstraintComponent(axleDescription, rigidBodyState, 
			suspensionParams, suspensionLimitParams, suspensionStates, suspensionComplianceStates,
			wheelRoadGeomStates, tireDirectionStates, tireStickyStates,
			constraints);

		PxVehicleConstraintsDirtyStateUpdate(*constraints);

		for (PxU32 i = 0; i < axleDescription->nbWheels; i++)
		{
			const PxU32 wheelId = axleDescription->wheelIdsInAxleOrder[i];

			PxVehiclePhysXConstraintStatesUpdate(
				suspensionParams[wheelId], suspensionLimitParams[wheelId],
				suspensionStates[wheelId], suspensionComplianceStates[wheelId], 
				wheelRoadGeomStates[wheelId].plane.n,
				context.tireStickyParams.stickyParams[PxVehicleTireDirectionModes::eLONGITUDINAL].damping,
				context.tireStickyParams.stickyParams[PxVehicleTireDirectionModes::eLATERAL].damping,
				tireDirectionStates[wheelId], tireStickyStates[wheelId], 
				*rigidBodyState,
				constraints->constraintStates[wheelId]);
		}

		return true;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

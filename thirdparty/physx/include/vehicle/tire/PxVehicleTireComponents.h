// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_TIRE_COMPONENTS_H
#define PX_VEHICLE_TIRE_COMPONENTS_H

#include "vehicle/PxVehicleParams.h"
#include "vehicle/PxVehicleComponent.h"

#include "vehicle/commands/PxVehicleCommandHelpers.h"
#include "vehicle/roadGeometry/PxVehicleRoadGeometryState.h"
#include "vehicle/suspension/PxVehicleSuspensionParams.h"
#include "vehicle/suspension/PxVehicleSuspensionStates.h"
#include "vehicle/wheel/PxVehicleWheelStates.h"
#include "vehicle/wheel/PxVehicleWheelParams.h"
#include "vehicle/wheel/PxVehicleWheelHelpers.h"

#include "PxVehicleTireFunctions.h"
#include "PxVehicleTireParams.h"

#include "common/PxProfileZone.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxVehicleTireComponent : public PxVehicleComponent
{
public:

	PxVehicleTireComponent() : PxVehicleComponent() {}
	virtual ~PxVehicleTireComponent() {}

	virtual void getDataForTireComponent(
		const PxVehicleAxleDescription*& axleDescription,
		PxVehicleArrayData<const PxReal>& steerResponseStates,
		const PxVehicleRigidBodyState*& rigidBodyState,
		PxVehicleArrayData<const PxVehicleWheelActuationState>& actuationStates,
		PxVehicleArrayData<const PxVehicleWheelParams>& wheelParams,
		PxVehicleArrayData<const PxVehicleSuspensionParams>& suspensionParams,
		PxVehicleArrayData<const PxVehicleTireForceParams>& tireForceParams,
		PxVehicleArrayData<const PxVehicleRoadGeometryState>& roadGeomStates,
		PxVehicleArrayData<const PxVehicleSuspensionState>& suspensionStates,
		PxVehicleArrayData<const PxVehicleSuspensionComplianceState>& suspensionComplianceStates,
		PxVehicleArrayData<const PxVehicleSuspensionForce>& suspensionForces,
		PxVehicleArrayData<const PxVehicleWheelRigidBody1dState>& wheelRigidBody1DStates,
		PxVehicleArrayData<PxVehicleTireGripState>& tireGripStates,
		PxVehicleArrayData<PxVehicleTireDirectionState>& tireDirectionStates,
		PxVehicleArrayData<PxVehicleTireSpeedState>& tireSpeedStates,
		PxVehicleArrayData<PxVehicleTireSlipState>& tireSlipStates,
		PxVehicleArrayData<PxVehicleTireCamberAngleState>& tireCamberAngleStates, 
		PxVehicleArrayData<PxVehicleTireStickyState>& tireStickyStates,
		PxVehicleArrayData<PxVehicleTireForce>& tireForces) = 0;

	virtual bool update(const PxReal dt, const PxVehicleSimulationContext& context)
	{
		PX_PROFILE_ZONE("PxVehicleTireComponent::update", 0);

		const PxVehicleAxleDescription* axleDescription;
		PxVehicleArrayData<const PxReal> steerResponseStates;
		const PxVehicleRigidBodyState* rigidBodyState;
		PxVehicleArrayData<const PxVehicleWheelActuationState> actuationStates;
		PxVehicleArrayData<const PxVehicleWheelParams> wheelParams;
		PxVehicleArrayData<const PxVehicleSuspensionParams> suspensionParams;
		PxVehicleArrayData<const PxVehicleTireForceParams> tireForceParams;
		PxVehicleArrayData<const PxVehicleRoadGeometryState> roadGeomStates;
		PxVehicleArrayData<const PxVehicleSuspensionState> suspensionStates;
		PxVehicleArrayData<const PxVehicleSuspensionComplianceState> suspensionComplianceStates;
		PxVehicleArrayData<const PxVehicleSuspensionForce> suspensionForces;
		PxVehicleArrayData<const PxVehicleWheelRigidBody1dState> wheelRigidBody1DStates;
		PxVehicleArrayData<PxVehicleTireGripState> tireGripStates;
		PxVehicleArrayData<PxVehicleTireDirectionState> tireDirectionStates;
		PxVehicleArrayData<PxVehicleTireSpeedState> tireSpeedStates;
		PxVehicleArrayData<PxVehicleTireSlipState> tireSlipStates;
		PxVehicleArrayData<PxVehicleTireCamberAngleState> tireCamberAngleStates;
		PxVehicleArrayData<PxVehicleTireStickyState> tireStickyStates;
		PxVehicleArrayData<PxVehicleTireForce> tireForces;

		getDataForTireComponent(axleDescription, steerResponseStates, 
			rigidBodyState, actuationStates, wheelParams, suspensionParams, tireForceParams,
			roadGeomStates, suspensionStates, suspensionComplianceStates, suspensionForces,
			wheelRigidBody1DStates, tireGripStates, tireDirectionStates, tireSpeedStates,
			tireSlipStates, tireCamberAngleStates, tireStickyStates, tireForces);

		for (PxU32 i = 0; i < axleDescription->nbWheels; i++)
		{
			const PxU32 wheelId = axleDescription->wheelIdsInAxleOrder[i];

			const bool isWheelOnGround = PxVehicleIsWheelOnGround(suspensionStates[wheelId]);

			//Compute the tire slip directions
			PxVehicleTireDirsUpdate(
				suspensionParams[wheelId],
				steerResponseStates[wheelId],
				roadGeomStates[wheelId].plane.n, isWheelOnGround,
				suspensionComplianceStates[wheelId],
				*rigidBodyState,
				context.frame,
				tireDirectionStates[wheelId]);

			//Compute the rigid body speeds along the tire slip directions.
			PxVehicleTireSlipSpeedsUpdate(
				wheelParams[wheelId], suspensionParams[wheelId],
				steerResponseStates[wheelId], suspensionStates[wheelId], tireDirectionStates[wheelId],
				*rigidBodyState, roadGeomStates[wheelId],
				context.frame,
				tireSpeedStates[wheelId]);

			//Compute the tire slip angles.
			PxVehicleTireSlipsUpdate(
				wheelParams[wheelId], context.tireSlipParams,
				actuationStates[wheelId], tireSpeedStates[wheelId],
				wheelRigidBody1DStates[wheelId],
				tireSlipStates[wheelId]);

			//Update the camber angle
			PxVehicleTireCamberAnglesUpdate(
				suspensionParams[wheelId], steerResponseStates[wheelId],
				roadGeomStates[wheelId].plane.n, isWheelOnGround,
				suspensionComplianceStates[wheelId], *rigidBodyState,
				context.frame,
				tireCamberAngleStates[wheelId]);

			//Compute the friction
			PxVehicleTireGripUpdate(
				tireForceParams[wheelId], roadGeomStates[wheelId].friction, 
				isWheelOnGround, suspensionForces[wheelId],
				tireSlipStates[wheelId], tireGripStates[wheelId]);

			//Update the tire sticky state
			//
			//Note: this should be skipped if tires do not use the sticky feature
			PxVehicleTireStickyStateUpdate(
				*axleDescription,
				wheelParams[wheelId],
				context.tireStickyParams,
				actuationStates, tireGripStates[wheelId],
				tireSpeedStates[wheelId], wheelRigidBody1DStates[wheelId],
				dt,
				tireStickyStates[wheelId]);

			//If sticky tire is active set the slip angle to zero.
			//
			//Note: this should be skipped if tires do not use the sticky feature
			PxVehicleTireSlipsAccountingForStickyStatesUpdate(
				tireStickyStates[wheelId],
				tireSlipStates[wheelId]);

			//Compute the tire forces
			PxVehicleTireForcesUpdate(
				wheelParams[wheelId], suspensionParams[wheelId],
				tireForceParams[wheelId],
				suspensionComplianceStates[wheelId],
				tireGripStates[wheelId], tireDirectionStates[wheelId],
				tireSlipStates[wheelId], tireCamberAngleStates[wheelId],
				*rigidBodyState,
				tireForces[wheelId]);
		}

		return true;
	}
};

/**
\deprecated This API was introduced with the new Vehicle API for transition purposes but will be removed in a future version.
*/
class PX_DEPRECATED PxVehicleLegacyTireComponent : public PxVehicleComponent
{
public:

	PxVehicleLegacyTireComponent() : PxVehicleComponent() {}
	virtual ~PxVehicleLegacyTireComponent() {}

	virtual void getDataForLegacyTireComponent(
		const PxVehicleAxleDescription*& axleDescription,
		PxVehicleArrayData<const PxReal>& steerResponseStates,
		const PxVehicleRigidBodyState*& rigidBodyState,
		PxVehicleArrayData<const PxVehicleWheelActuationState>& actuationStates,
		PxVehicleArrayData<const PxVehicleWheelParams>& wheelParams,
		PxVehicleArrayData<const PxVehicleSuspensionParams>& suspensionParams,
		PxVehicleArrayData<const PxVehicleTireForceParams>& tireForceParams,
		PxVehicleArrayData<const PxVehicleRoadGeometryState>& roadGeomStates,
		PxVehicleArrayData<const PxVehicleSuspensionState>& suspensionStates,
		PxVehicleArrayData<const PxVehicleSuspensionComplianceState>& suspensionComplianceStates,
		PxVehicleArrayData<const PxVehicleSuspensionForce>& suspensionForces,
		PxVehicleArrayData<const PxVehicleWheelRigidBody1dState>& wheelRigidBody1DStates,
		PxVehicleArrayData<PxVehicleTireGripState>& tireGripStates,
		PxVehicleArrayData<PxVehicleTireDirectionState>& tireDirectionStates,
		PxVehicleArrayData<PxVehicleTireSpeedState>& tireSpeedStates,
		PxVehicleArrayData<PxVehicleTireSlipState>& tireSlipStates,
		PxVehicleArrayData<PxVehicleTireCamberAngleState>& tireCamberAngleStates, 
		PxVehicleArrayData<PxVehicleTireStickyState>& tireStickyStates,
		PxVehicleArrayData<PxVehicleTireForce>& tireForces) = 0;

	virtual bool update(const PxReal dt, const PxVehicleSimulationContext& context)
	{
		PX_PROFILE_ZONE("PxVehicleLegacyTireComponent::update", 0);

		const PxVehicleAxleDescription* axleDescription;
		PxVehicleArrayData<const PxReal> steerResponseStates;
		const PxVehicleRigidBodyState* rigidBodyState;
		PxVehicleArrayData<const PxVehicleWheelActuationState> actuationStates;
		PxVehicleArrayData<const PxVehicleWheelParams> wheelParams;
		PxVehicleArrayData<const PxVehicleSuspensionParams> suspensionParams;
		PxVehicleArrayData<const PxVehicleTireForceParams> tireForceParams;
		PxVehicleArrayData<const PxVehicleRoadGeometryState> roadGeomStates;
		PxVehicleArrayData<const PxVehicleSuspensionState> suspensionStates;
		PxVehicleArrayData<const PxVehicleSuspensionComplianceState> suspensionComplianceStates;
		PxVehicleArrayData<const PxVehicleSuspensionForce> suspensionForces;
		PxVehicleArrayData<const PxVehicleWheelRigidBody1dState> wheelRigidBody1DStates;
		PxVehicleArrayData<PxVehicleTireGripState> tireGripStates;
		PxVehicleArrayData<PxVehicleTireDirectionState> tireDirectionStates;
		PxVehicleArrayData<PxVehicleTireSpeedState> tireSpeedStates;
		PxVehicleArrayData<PxVehicleTireSlipState> tireSlipStates;
		PxVehicleArrayData<PxVehicleTireCamberAngleState> tireCamberAngleStates;
		PxVehicleArrayData<PxVehicleTireStickyState> tireStickyStates;
		PxVehicleArrayData<PxVehicleTireForce> tireForces;

		getDataForLegacyTireComponent(axleDescription, steerResponseStates, 
			rigidBodyState, actuationStates,
			wheelParams, suspensionParams, tireForceParams,
			roadGeomStates, suspensionStates, suspensionComplianceStates,
			suspensionForces, wheelRigidBody1DStates,
			tireGripStates, tireDirectionStates, tireSpeedStates, tireSlipStates,
			tireCamberAngleStates, tireStickyStates, tireForces);

		for (PxU32 i = 0; i < axleDescription->nbWheels; i++)
		{
			const PxU32 wheelId = axleDescription->wheelIdsInAxleOrder[i];

			const bool isWheelOnGround = roadGeomStates[wheelId].hitState;
			// note: since this is the legacy component, PxVehicleIsWheelOnGround() is not used
			//       here

			//Compute the tire slip directions
			PxVehicleTireDirsLegacyUpdate(
				suspensionParams[wheelId],
				steerResponseStates[wheelId],
				roadGeomStates[wheelId], *rigidBodyState,
				context.frame,
				tireDirectionStates[wheelId]);

			//Compute the rigid body speeds along the tire slip directions.
			PxVehicleTireSlipSpeedsUpdate(
				wheelParams[wheelId], suspensionParams[wheelId],
				steerResponseStates[wheelId], suspensionStates[wheelId], tireDirectionStates[wheelId],
				*rigidBodyState, roadGeomStates[wheelId],
				context.frame,
				tireSpeedStates[wheelId]);

			//Compute the tire slip angles.
			PxVehicleTireSlipsLegacyUpdate(
				wheelParams[wheelId], context.tireSlipParams,
				actuationStates[wheelId], tireSpeedStates[wheelId],
				wheelRigidBody1DStates[wheelId],
				tireSlipStates[wheelId]);

			//Update the camber angle
			PxVehicleTireCamberAnglesUpdate(
				suspensionParams[wheelId], steerResponseStates[wheelId],
				roadGeomStates[wheelId].plane.n, isWheelOnGround,
				suspensionComplianceStates[wheelId], *rigidBodyState,
				context.frame,
				tireCamberAngleStates[wheelId]);

			//Compute the friction
			PxVehicleTireGripUpdate(
				tireForceParams[wheelId], roadGeomStates[wheelId].friction, 
				PxVehicleIsWheelOnGround(suspensionStates[wheelId]), suspensionForces[wheelId],
				tireSlipStates[wheelId], tireGripStates[wheelId]);
			// note: PxVehicleIsWheelOnGround() used here to reflect previous behavior since this is
			//       the legacy component after all

			//Update the tire sticky state
			//
			//Note: this should be skipped if tires do not use the sticky feature
			PxVehicleTireStickyStateUpdate(
				*axleDescription,
				wheelParams[wheelId],
				context.tireStickyParams,
				actuationStates, tireGripStates[wheelId],
				tireSpeedStates[wheelId], wheelRigidBody1DStates[wheelId],
				dt,
				tireStickyStates[wheelId]);

			//If sticky tire is active set the slip angle to zero.
			//
			//Note: this should be skipped if tires do not use the sticky feature
			PxVehicleTireSlipsAccountingForStickyStatesUpdate(
				tireStickyStates[wheelId],
				tireSlipStates[wheelId]);

			//Compute the tire forces
			PxVehicleTireForcesUpdate(
				wheelParams[wheelId], suspensionParams[wheelId],
				tireForceParams[wheelId],
				suspensionComplianceStates[wheelId],
				tireGripStates[wheelId], tireDirectionStates[wheelId],
				tireSlipStates[wheelId], tireCamberAngleStates[wheelId],
				*rigidBodyState,
				tireForces[wheelId]);
		}

		return true;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif



// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_PHYSX_ROAD_GEOMETRY_COMPONENTS_H
#define PX_VEHICLE_PHYSX_ROAD_GEOMETRY_COMPONENTS_H

#include "vehicle/PxVehicleParams.h"
#include "vehicle/PxVehicleComponent.h"

#include "vehicle/commands/PxVehicleCommandHelpers.h"
#include "vehicle/roadGeometry/PxVehicleRoadGeometryState.h"
#include "vehicle/suspension/PxVehicleSuspensionParams.h"
#include "vehicle/wheel/PxVehicleWheelParams.h"

#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryFunctions.h"
#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryParams.h"
#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryState.h"

#include "common/PxProfileZone.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxVehiclePhysXRoadGeometrySceneQueryComponent : public PxVehicleComponent
{
public:
	PxVehiclePhysXRoadGeometrySceneQueryComponent() : PxVehicleComponent() {}
	virtual ~PxVehiclePhysXRoadGeometrySceneQueryComponent() {}

	/**
	\brief Provide vehicle data items for this component.
	
	\param[out] axleDescription identifies the wheels on each axle.
	\param[out] roadGeomParams The road geometry parameters of the vehicle.
	\param[out] steerResponseStates The steer response state of the wheels.
	\param[out] rigidBodyState The pose, velocity etc. of the vehicle rigid body.
	\param[out] wheelParams The wheel parameters for the wheels.
	\param[out] suspensionParams The suspension parameters for the wheels.
	\param[out] materialFrictionParams The tire friction tables for the wheels.
	\param[out] roadGeometryStates The detected ground surface plane, friction value etc. for the wheels.
	\param[out] physxRoadGeometryStates Optional buffer to store additional information about the query (like actor/shape that got hit etc.).
	            Set to empty if not desired.
	*/
	virtual void getDataForPhysXRoadGeometrySceneQueryComponent(
		const PxVehicleAxleDescription*& axleDescription,
		const PxVehiclePhysXRoadGeometryQueryParams*& roadGeomParams,
		PxVehicleArrayData<const PxReal>& steerResponseStates,
		const PxVehicleRigidBodyState*& rigidBodyState,
		PxVehicleArrayData<const PxVehicleWheelParams>& wheelParams,
		PxVehicleArrayData<const PxVehicleSuspensionParams>& suspensionParams,
		PxVehicleArrayData<const PxVehiclePhysXMaterialFrictionParams>& materialFrictionParams,
		PxVehicleArrayData<PxVehicleRoadGeometryState>& roadGeometryStates,
		PxVehicleArrayData<PxVehiclePhysXRoadGeometryQueryState>& physxRoadGeometryStates) = 0;

	virtual bool update(const PxReal dt, const PxVehicleSimulationContext& context)
	{
		PX_UNUSED(dt);

		PX_PROFILE_ZONE("PxVehiclePhysXRoadGeometrySceneQueryComponent::update", 0);

		const PxVehicleAxleDescription* axleDescription;
		const PxVehiclePhysXRoadGeometryQueryParams* roadGeomParams;
		PxVehicleArrayData<const PxReal> steerResponseStates;
		const PxVehicleRigidBodyState* rigidBodyState;
		PxVehicleArrayData<const PxVehicleWheelParams> wheelParams;
		PxVehicleArrayData<const PxVehicleSuspensionParams> suspensionParams;
		PxVehicleArrayData<const PxVehiclePhysXMaterialFrictionParams> materialFrictionParams;
		PxVehicleArrayData<PxVehicleRoadGeometryState> roadGeometryStates;
		PxVehicleArrayData<PxVehiclePhysXRoadGeometryQueryState> physxRoadGeometryStates;

		getDataForPhysXRoadGeometrySceneQueryComponent(axleDescription, 
			roadGeomParams, steerResponseStates, rigidBodyState,
			wheelParams, suspensionParams, materialFrictionParams,
			roadGeometryStates, physxRoadGeometryStates);

		if (context.getType() == PxVehicleSimulationContextType::ePHYSX)
		{
			const PxVehiclePhysXSimulationContext& physxContext = static_cast<const PxVehiclePhysXSimulationContext&>(context);

			for(PxU32 i = 0; i < axleDescription->nbWheels; i++)
			{
				const PxU32 wheelId = axleDescription->wheelIdsInAxleOrder[i];

				const PxQueryFilterData* fdPtr = roadGeomParams->filterDataEntries ? (roadGeomParams->filterDataEntries + wheelId) : &roadGeomParams->defaultFilterData;

				PxVehiclePhysXRoadGeometryQueryUpdate(
					wheelParams[wheelId], suspensionParams[wheelId], 
					roadGeomParams->roadGeometryQueryType, roadGeomParams->filterCallback, *fdPtr,
					materialFrictionParams[wheelId],
					steerResponseStates[wheelId], *rigidBodyState,
					*physxContext.physxScene, physxContext.physxUnitCylinderSweepMesh, context.frame,
					roadGeometryStates[wheelId], 
					!physxRoadGeometryStates.isEmpty() ? &physxRoadGeometryStates[wheelId] : NULL);
			}
		}
		else
		{
			PX_ALWAYS_ASSERT();

			for(PxU32 i = 0; i < axleDescription->nbWheels; i++)
			{
				const PxU32 wheelId = axleDescription->wheelIdsInAxleOrder[i];

				roadGeometryStates[wheelId].setToDefault();
			}
		}

		return true;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

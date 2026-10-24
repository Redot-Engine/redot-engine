// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_API_H
#define PX_VEHICLE_API_H

#include "vehicle/PxVehicleLimits.h"
#include "vehicle/PxVehicleComponent.h"
#include "vehicle/PxVehicleComponentSequence.h"
#include "vehicle/PxVehicleParams.h"
#include "vehicle/PxVehicleFunctions.h"
#include "vehicle/PxVehicleMaths.h"

#include "vehicle/braking/PxVehicleBrakingParams.h"
#include "vehicle/braking/PxVehicleBrakingFunctions.h"

#include "vehicle/commands/PxVehicleCommandParams.h"
#include "vehicle/commands/PxVehicleCommandStates.h"
#include "vehicle/commands/PxVehicleCommandHelpers.h"

#include "vehicle/drivetrain/PxVehicleDrivetrainParams.h"
#include "vehicle/drivetrain/PxVehicleDrivetrainStates.h"
#include "vehicle/drivetrain/PxVehicleDrivetrainHelpers.h"
#include "vehicle/drivetrain/PxVehicleDrivetrainFunctions.h"
#include "vehicle/drivetrain/PxVehicleDrivetrainComponents.h"

#include "vehicle/physxActor/PxVehiclePhysXActorStates.h"
#include "vehicle/physxActor/PxVehiclePhysXActorHelpers.h"
#include "vehicle/physxActor/PxVehiclePhysXActorFunctions.h"
#include "vehicle/physxActor/PxVehiclePhysXActorComponents.h"

#include "vehicle/physxConstraints/PxVehiclePhysXConstraintParams.h"
#include "vehicle/physxConstraints/PxVehiclePhysXConstraintStates.h"
#include "vehicle/physxConstraints/PxVehiclePhysXConstraintHelpers.h"
#include "vehicle/physxConstraints/PxVehiclePhysXConstraintFunctions.h"
#include "vehicle/physxConstraints/PxVehiclePhysXConstraintComponents.h"

#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryState.h"
#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryParams.h"
#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryHelpers.h"
#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryFunctions.h"
#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryComponents.h"

#include "vehicle/pvd/PxVehiclePvdHelpers.h"
#include "vehicle/pvd/PxVehiclePvdFunctions.h"
#include "vehicle/pvd/PxVehiclePvdComponents.h"

#include "vehicle/rigidBody/PxVehicleRigidBodyParams.h"
#include "vehicle/rigidBody/PxVehicleRigidBodyStates.h"
#include "vehicle/rigidBody/PxVehicleRigidBodyFunctions.h"
#include "vehicle/rigidBody/PxVehicleRigidBodyComponents.h"

#include "vehicle/roadGeometry/PxVehicleRoadGeometryState.h"

#include "vehicle/steering/PxVehicleSteeringParams.h"
#include "vehicle/steering/PxVehicleSteeringFunctions.h"

#include "vehicle/suspension/PxVehicleSuspensionParams.h"
#include "vehicle/suspension/PxVehicleSuspensionStates.h"
#include "vehicle/suspension/PxVehicleSuspensionHelpers.h"
#include "vehicle/suspension/PxVehicleSuspensionFunctions.h"
#include "vehicle/suspension/PxVehicleSuspensionComponents.h"

#include "vehicle/tire/PxVehicleTireParams.h"
#include "vehicle/tire/PxVehicleTireStates.h"
#include "vehicle/tire/PxVehicleTireHelpers.h"
#include "vehicle/tire/PxVehicleTireFunctions.h"
#include "vehicle/tire/PxVehicleTireComponents.h"

#include "vehicle/wheel/PxVehicleWheelParams.h"
#include "vehicle/wheel/PxVehicleWheelStates.h"
#include "vehicle/wheel/PxVehicleWheelHelpers.h"
#include "vehicle/wheel/PxVehicleWheelFunctions.h"
#include "vehicle/wheel/PxVehicleWheelComponents.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/** \brief Initialize the PhysX Vehicle library.

	This should be called before calling any functions or methods in extensions which may require allocation.
	\note This function does not need to be called before creating a PxDefaultAllocator object.

	\param foundation a PxFoundation object

	\see PxCloseVehicleExtension PxFoundation 
	*/
	PX_FORCE_INLINE bool PxInitVehicleExtension(physx::PxFoundation& foundation)
	{
		PX_UNUSED(foundation);
		PX_CHECK_AND_RETURN_VAL(&PxGetFoundation() == &foundation, "Supplied foundation must match the one that will be used to perform allocations", false);
		PxIncFoundationRefCount();
		return true;
	}

	/** \brief Shut down the PhysX Vehicle library.

	This function should be called to cleanly shut down the PhysX Vehicle library before application exit.

	\note This function is required to be called to release foundation usage.

	\see PxInitVehicleExtension
	*/
	PX_FORCE_INLINE void PxCloseVehicleExtension()
	{
		PxDecFoundationRefCount();
	}


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

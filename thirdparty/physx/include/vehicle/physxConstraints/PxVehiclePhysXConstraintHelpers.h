// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_PHYSX_CONSTRAINT_HELPERS_H
#define PX_VEHICLE_PHYSX_CONSTRAINT_HELPERS_H

#include "foundation/PxPreprocessor.h"

#if !PX_DOXYGEN
namespace physx
{

class PxPhysics;
class PxRigidBody;

#endif

struct PxVehicleAxleDescription;
struct PxVehiclePhysXConstraints;

/**
\brief Instantiate the PhysX custom constraints.

Custom constraints will resolve excess suspension compression and velocity constraints that serve as
a replacement low speed tire model.

\param[in] axleDescription describes the axles of the vehicle and the wheels on each axle.
\param[in] physics is a PxPhysics instance.
\param[in] physxActor is the vehicle's PhysX representation as a PxRigidBody
\param[in] vehicleConstraints is a wrapper class that holds pointers to PhysX objects required to implement the custom constraint.
*/
void PxVehicleConstraintsCreate
(const PxVehicleAxleDescription& axleDescription,
 PxPhysics& physics, PxRigidBody& physxActor,
 PxVehiclePhysXConstraints& vehicleConstraints);

/**
\brief To ensure the constraints are processed by the PhysX scene they are marked as dirty prior to each simulate step.

\param[in] vehicleConstraints is a wrapper class that holds pointers to PhysX objects required to implement the custom constraint.

\see PxVehicleConstraintsCreate
*/
void PxVehicleConstraintsDirtyStateUpdate
(PxVehiclePhysXConstraints& vehicleConstraints);

/**
\brief Destroy the PhysX custom constraints.

\param[in,out] vehicleConstraints describes the PhysX custom constraints to be released.

\see PxVehicleConstraintsCreate
*/
void PxVehicleConstraintsDestroy
(PxVehiclePhysXConstraints& vehicleConstraints);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_PHYSX_ROAD_GEOMETRY_HELPERS_H
#define PX_VEHICLE_PHYSX_ROAD_GEOMETRY_HELPERS_H

#include "foundation/PxPreprocessor.h"

#if !PX_DOXYGEN
namespace physx
{

class PxConvexMesh;
class PxPhysics;
struct PxCookingParams;

#endif

struct PxVehicleFrame;

/**
\brief Create a cylindrical mesh with unit radius and half-width.
\param[in] vehicleFrame is a description of the lateral and longitudinal axes.
\param[in] physics is a PxPhysics instance.
\param[in] params is a PxCookingParams instance
\return Return a PxConvexMesh instance that represents a convex hull with unit radius and half-width.
\see PxVehicleUnitCylinderSweepMeshDestroy
*/
PxConvexMesh* PxVehicleUnitCylinderSweepMeshCreate(const PxVehicleFrame& vehicleFrame, PxPhysics& physics, const PxCookingParams& params);

/**
\brief Release the mesh created with PxVehicleUnitCylinderSweepMeshCreate.
\param[in] mesh is a PxConvexMesh instance.
\see PxVehicleUnitCylinderSweepMeshCreate
*/
void PxVehicleUnitCylinderSweepMeshDestroy(PxConvexMesh* mesh);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

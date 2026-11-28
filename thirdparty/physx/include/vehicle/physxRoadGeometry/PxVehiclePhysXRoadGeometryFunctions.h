// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_PHYSX_ROAD_GEOMETRY_FUNCTIONS_H
#define PX_VEHICLE_PHYSX_ROAD_GEOMETRY_FUNCTIONS_H

#include "foundation/PxSimpleTypes.h"

#include "vehicle/physxRoadGeometry/PxVehiclePhysXRoadGeometryParams.h"

#if !PX_DOXYGEN
namespace physx
{
class PxScene;
class PxConvexMesh;

#endif

struct PxVehicleWheelParams;
struct PxVehicleSuspensionParams;
struct PxVehiclePhysXRoadGeometryQueryState;
struct PxVehicleRigidBodyState;
struct PxVehicleFrame;
struct PxVehicleRoadGeometryState;

/**
\brief Compute the plane of the road geometry under a wheel and the tire friction of the contact.
\param[in] wheelParams describes the radius and halfwidth of the wheel.
\param[in] suspParams describes the frame of the suspension and wheel and the maximum suspension travel.
\param[in] queryType describes what type of PhysX scene query to use (see #PxVehiclePhysXRoadGeometryQueryType).
           If PxVehiclePhysXRoadGeometryQueryType::eNONE is used, no work will be done.
\param[in] filterCallback describes the filter callback to use for the PhysX scene query. NULL is a valid input.
\param[in] filterData describes the filter data to use for the PhysX scene query.
\param[in] materialFrictionParams describes a mapping between PxMaterial and friction in order to compute a tire friction value.
\param[in] wheelYawAngle is the yaw angle (in radians) of the wheel.
\param[in] rigidBodyState describes the pose of the rigid body.
\param[in] scene is the PhysX scene that will be queried by the scene query.
\param[in] unitCylinderSweepMesh is a convex cylindrical mesh of unit radius and half-width to be used in 
the event that a sweep query is to be used.
\param[in] frame describes the lateral, longitudinal and vertical axes and is used to scale unitCylinderSweepMesh 
by the wheel's radius and half-width.
\param[out] roadGeomState contains the plane and friction of the road geometry under the wheel.
\param[out] physxRoadGeometryState Optional buffer to store additional information about the query (like actor/shape that got hit etc.).
            Set to NULL if not needed.
\note PxVehicleRoadGeometryState::hitState will have value false in the event that the there is no reachable road geometry under the wheel and 
true if there is reachable road geometry under the wheel. Road geometry is considered reachable if the suspension can elongate from its
reference pose far enough to place wheel on the ground.
*/
void PxVehiclePhysXRoadGeometryQueryUpdate
(const PxVehicleWheelParams& wheelParams, const PxVehicleSuspensionParams& suspParams,
 const PxVehiclePhysXRoadGeometryQueryType::Enum queryType, 
 PxQueryFilterCallback* filterCallback, const PxQueryFilterData& filterData,
 const PxVehiclePhysXMaterialFrictionParams& materialFrictionParams,
 const PxReal wheelYawAngle, const PxVehicleRigidBodyState& rigidBodyState,
 const PxScene& scene, const PxConvexMesh* unitCylinderSweepMesh,
 const PxVehicleFrame& frame,
 PxVehicleRoadGeometryState& roadGeomState,
 PxVehiclePhysXRoadGeometryQueryState* physxRoadGeometryState);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

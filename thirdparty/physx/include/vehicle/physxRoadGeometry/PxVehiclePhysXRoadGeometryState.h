// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_PHYSX_ROAD_GEOMETRY_STATE_H
#define PX_VEHICLE_PHYSX_ROAD_GEOMETRY_STATE_H

#include "foundation/PxMemory.h"
#include "foundation/PxVec3.h"

#if !PX_DOXYGEN
namespace physx
{

class PxRigidActor;
class PxShape;
class PxMaterial;

#endif

struct PxVehiclePhysXRoadGeometryQueryState
{
	PxRigidActor* actor;				//!< The actor that got hit by the query.
	PxShape* shape;						//!< The shape that got hit by the query.
	PxMaterial* material;				//!< The material at the hit point.
	PxVec3 hitPosition;                 //!< The hit position in world space.

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehiclePhysXRoadGeometryQueryState));
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


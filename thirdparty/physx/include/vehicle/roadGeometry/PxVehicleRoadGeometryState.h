// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_ROAD_GEOMETRY_STATE_H
#define PX_VEHICLE_ROAD_GEOMETRY_STATE_H

#include "foundation/PxPlane.h"
#include "foundation/PxMemory.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxVehicleRoadGeometryState
{
	PxPlane plane;						//!< the plane under the wheel
	PxReal friction;					//!< the friction to be used by the tire model
	PxVec3 velocity;					//!< the velocity of the road geometry
	bool hitState;						//!< true if a plane is found, false if there is no plane.

	PX_FORCE_INLINE void setToDefault()
	{
		PxMemZero(this, sizeof(PxVehicleRoadGeometryState));
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

/**************************************************************************/
/*  physx_wheel_query_filter.h                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "../spaces/physx_filter_shader.h"

#include "PxPhysicsAPI.h"

// Filter for the wheels' road-geometry raycasts
// (PxVehiclePhysXRoadGeometryQueryParams::filterCallback). Without one, a
// wheel hit every scene-query shape: Area3D triggers and detection shapes,
// and bodies on layers outside the vehicle's collision_mask (characters,
// debris, the driver). Like a Godot raycast, a wheel now hits only a shape
// whose collision layer (query filter word0) is in the vehicle's mask. One
// per vehicle: the query params point at it, so it lives in the vehicle
// object itself.
class PhysXWheelQueryFilter : public physx::PxQueryFilterCallback {
public:
	physx::PxU32 collision_mask = 1;

	virtual physx::PxQueryHitType::Enum preFilter(
			const physx::PxFilterData &,
			const physx::PxShape *p_shape,
			const physx::PxRigidActor *,
			physx::PxHitFlags &) override {
		if (p_shape == nullptr || (p_shape->getFlags() & physx::PxShapeFlag::eTRIGGER_SHAPE)) {
			return physx::PxQueryHitType::eNONE;
		}
		// An area is never solid in this module — its detection shape must
		// not hold a wheel up either (see physx_filter_shader.h).
		if (p_shape->getQueryFilterData().word3 & PHYSX_FILTER_FLAG_IS_AREA) {
			return physx::PxQueryHitType::eNONE;
		}
		return (p_shape->getQueryFilterData().word0 & collision_mask) ? physx::PxQueryHitType::eBLOCK : physx::PxQueryHitType::eNONE;
	}

	virtual physx::PxQueryHitType::Enum postFilter(
			const physx::PxFilterData &,
			const physx::PxQueryHit &,
			const physx::PxShape *,
			const physx::PxRigidActor *) override {
		return physx::PxQueryHitType::eBLOCK;
	}
};

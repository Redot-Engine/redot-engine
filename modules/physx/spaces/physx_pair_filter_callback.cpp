/**************************************************************************/
/*  physx_pair_filter_callback.cpp                                        */
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

/**
 * @file physx_pair_filter_callback.cpp
 * @brief PxSimulationFilterCallback implementation — enforces collision exceptions.
 */

#include "physx_pair_filter_callback.h"
#include "../objects/physx_body_3d.h"
#include "../shapes/physx_user_data.h"

physx::PxFilterFlags PhysXPairFilterCallback::pairFound(
		physx::PxU64,
		physx::PxFilterObjectAttributes, physx::PxFilterData,
		const physx::PxActor *a0, const physx::PxShape *,
		physx::PxFilterObjectAttributes, physx::PxFilterData,
		const physx::PxActor *a1, const physx::PxShape *,
		physx::PxPairFlags &pairFlags) {
	// Collision exceptions only apply between bodies. If either side is not a
	// body (e.g. an area), there's nothing to check — let the pair through.
	if (!a0->userData || !a1->userData) {
		return physx::PxFilterFlags();
	}

	const PhysXActorUserData *ud0 = static_cast<const PhysXActorUserData *>(a0->userData);
	const PhysXActorUserData *ud1 = static_cast<const PhysXActorUserData *>(a1->userData);
	if (!ud0->object || !ud1->object) {
		return physx::PxFilterFlags();
	}

	const PhysXObject3D *obj0 = ud0->object;
	const PhysXObject3D *obj1 = ud1->object;

	// Both must be bodies — areas don't have collision exceptions.
	if (obj0->get_type() != PhysXObject3D::OBJECT_TYPE_BODY ||
			obj1->get_type() != PhysXObject3D::OBJECT_TYPE_BODY) {
		return physx::PxFilterFlags();
	}

	// Check: does body0 have an exception for body1, or vice versa?
	const PhysXBody3D *body0 = static_cast<const PhysXBody3D *>(obj0);
	const PhysXBody3D *body1 = static_cast<const PhysXBody3D *>(obj1);

	if (body0->get_collision_exception_set().has(body1->get_rid()) ||
			body1->get_collision_exception_set().has(body0->get_rid())) {
		// Suppress this pair — collision exception found.
		return physx::PxFilterFlag::eKILL;
	}

	return physx::PxFilterFlags();
}

/**************************************************************************/
/*  physx_query_filter_callback.h                                         */
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

/**
 * @file physx_query_filter_callback.h
 * @brief PxQueryFilterCallback for scene queries (raycast, overlap, sweep).
 *
 * Applies Godot's query semantics to PhysX scene queries:
 *   - collision_mask vs object's collision_layer (bitwise AND must be nonzero)
 *   - collide_with_bodies / collide_with_areas toggle
 *   - RID and ObjectID exclusion lists
 *   - eTOUCH vs eBLOCK for multi-hit vs single-hit queries
 *
 * All filtering reads from the PhysXActorUserData attached to each actor, so
 * every actor this module puts into a scene must carry one.
 *
 * Note: PxShape::userData points at the shared PhysXShape3D* blueprint.
 */

#ifndef PHYSX_QUERY_FILTER_CALLBACK_H
#define PHYSX_QUERY_FILTER_CALLBACK_H

#include "PxPhysicsAPI.h"
#include "core/object/object.h"
#include "core/templates/hash_set.h"
#include "core/templates/rid.h"

// PxQueryFilterCallback implementation that applies Godot's query semantics
// (collision mask vs layer, bodies-vs-areas toggle, RID/ObjectID exclusions)
// to PhysX scene queries (raycast / overlap / sweep).
//
// All filtering reads from the PhysXActorUserData attached to each actor, so
// every actor this module puts into a scene must carry one.
//
// multi_hit flag: when true, preFilter returns eTOUCH so PhysX collects all
// overlapping shapes up to the buffer size (intersect_point, intersect_shape,
// collide_shape, motion overlaps). When false, returns eBLOCK for the single
// closest hit (intersect_ray, cast_motion).
class PhysXBody3D;

class PhysXQueryFilterCallback : public physx::PxQueryFilterCallback {
public:
	uint32_t collision_mask = 0;
	bool collide_with_bodies = true;
	bool collide_with_areas = false;
	const HashSet<RID> *exclude_rids = nullptr;
	const HashSet<ObjectID> *exclude_objects = nullptr;
	/// When set (body_test_motion paths), pairs connected by a collision
	/// exception with this body are rejected — Godot's test_body_motion
	/// respects exceptions (see godot_space_3d.cpp _cull_aabb_for_body).
	const PhysXBody3D *motion_body = nullptr;
	bool multi_hit = false; ///< true = return eTOUCH (collect all); false = return eBLOCK (single hit)
	/// When true (intersect_ray only), objects whose is_ray_pickable() is false
	/// are rejected. Godot applies body/area ray-pickability to raycasts only —
	/// point/shape/sweep queries never consult it (reference: godot_physics_3d
	/// gathers pickable objects for _intersect_ray alone).
	bool pick_ray = false;
	/// Back-face rule for raycasts (intersect_ray only). Godot's contract: a
	/// mesh back face counts as a hit only when the SHAPE has
	/// backface_collision enabled AND the query asked for hit_back_faces.
	/// Redot's concave cook duplicates every triangle with reversed winding
	/// when backface_collision is on (front copies at even cooked indices,
	/// flipped copies at odd ones), so the postFilter can tell them apart by
	/// the cooked face index.
	bool ray_backface_filtering = false;
	bool ray_hit_back_faces = false;
	/// Normalized ray direction (intersect_ray only), for the geometric
	/// front/back test on one-sided meshes.
	float ray_dir_x = 0.0f;
	float ray_dir_y = 0.0f;
	float ray_dir_z = 0.0f;

	virtual physx::PxQueryHitType::Enum preFilter(
			const physx::PxFilterData &filterData,
			const physx::PxShape *shape,
			const physx::PxRigidActor *actor,
			physx::PxHitFlags &queryFlags) override;

	virtual physx::PxQueryHitType::Enum postFilter(
			const physx::PxFilterData &filterData,
			const physx::PxQueryHit &hit,
			const physx::PxShape *shape,
			const physx::PxRigidActor *actor) override;
};
#endif // PHYSX_QUERY_FILTER_CALLBACK_H

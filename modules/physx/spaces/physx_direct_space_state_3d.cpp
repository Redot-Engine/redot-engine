/**************************************************************************/
/*  physx_direct_space_state_3d.cpp                                       */
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
 * @file physx_direct_space_state_3d.cpp
 * @brief Scene query implementations (raycast, overlap, sweep, test_motion).
 *
 * Each query builds a PhysXQueryFilterCallback with Godot's mask/exclusion
 * parameters, invokes the corresponding PxScene method, and maps PhysX hit
 * results back to Godot's RayResult / ShapeResult / ShapeRestInfo / MotionResult
 * via the PhysXActorUserData attached to each actor (PxShape::userData points at
 * the shared PhysXShape3D* blueprint).
 */

#include "physx_direct_space_state_3d.h"
#include "../objects/physx_body_3d.h"
#include "../physx_server.h"
#include "../shapes/physx_separation_ray_shape_3d.h"
#include "../shapes/physx_shape_3d.h"
#include "../shapes/physx_user_data.h"
#include "physx_query_filter_callback.h"
#include "physx_space_3d.h"

#include "geometry/PxGeometryQuery.h"
#include "geometry/PxMeshQuery.h"

#include "core/templates/sort_array.h"

// Orders motion contacts deepest-first (godot_space_3d _rest_cbk_result keeps
// the largest contact length as the best result).
struct DeeperContactFirst {
	bool operator()(const PhysicsServer3D::MotionCollision &p_a, const PhysicsServer3D::MotionCollision &p_b) const {
		return p_a.depth > p_b.depth;
	}
};

// Async stepping: a scene query against an in-flight solve is a hard PhysX
// error, so every public query fetches the pending solve first. This collapses
// the async overlap for any frame that queries — the documented cost; frames
// whose scripts only read body state keep the overlap.
static void _ensure_space_synced(const PhysXSpace3D *p_space) {
	if (p_space) {
		const_cast<PhysXSpace3D *>(p_space)->sync();
	}
}

// Resolves a (actor, shape) query hit to the body-local shape index, or -1
// when the hit cannot be mapped (soft-body/articulation actors without
// userData, or a shape the object does not carry). -1 is the "invalid"
// sentinel: 0 is a legitimate shape index and must not double as the failure
// value.
// Both PhysXBody3D and PhysXArea3D are shaped objects; areas/bodies both
// support find_shape_index, so we check the type and cast accordingly.
// (Lives here rather than in physx_user_data.h so that header stays a leaf:
// the vehicle nodes attach a PhysXActorUserData from TUs that must not see
// physx_shape_3d.h's global forward declarations.)
static inline int physx_resolve_shape_index(const physx::PxRigidActor *p_actor,
		const physx::PxShape *p_shape) {
	if (!p_actor || !p_actor->userData || !p_shape) {
		return -1;
	}
	const auto *ad = static_cast<const PhysXActorUserData *>(p_actor->userData);
	if (!ad->object) {
		return -1;
	}
	// Bodies and areas are both shaped objects; areas/bodies both support this.
	if (ad->object->get_type() != PhysXObject3D::OBJECT_TYPE_BODY &&
			ad->object->get_type() != PhysXObject3D::OBJECT_TYPE_AREA) {
		return -1;
	}
	const PhysXShapedObject3D *shaped = static_cast<const PhysXShapedObject3D *>(ad->object);
	const int idx = shaped->find_shape_index(p_shape);
	return idx;
}

// Translates a raw PhysX ray face index into the author-facing index: a
// backface-cooked concave mesh doubles every triangle, and its blueprint
// (PxShape::userData) knows how to map back. Identity for every other shape.
static int _resolve_face_index(const physx::PxRaycastHit &p_block) {
	if (p_block.shape && p_block.shape->userData) {
		const PhysXShape3D *shape_bp = static_cast<const PhysXShape3D *>(p_block.shape->userData);
		return shape_bp->translate_face_index((int)p_block.faceIndex);
	}
	return (int)p_block.faceIndex;
}

// Upper bound on the number of results a single scene query can return.
// Caller-provided p_result_max is clamped to this so a hostile or buggy
// caller cannot exhaust the stack/heap. Godot caps its own result arrays
// at 64, so 256 leaves comfortable headroom.
static constexpr int PHYSX_QUERY_MAX_RESULTS = 256;

// Internal-edge handling for sweep hits (defined below, shared by cast_motion
// and the body_test_motion cast phase): re-derives the face normal on mesh
// hits and decides whether a forward hit is a genuine block or an edge
// artifact.
static bool _sweep_forward_hit_blocking(const physx::PxSweepHit &p_hit, physx::PxVec3 &r_normal);

// One-sided concave meshes (backface_collision off) never interact from
// their back side; defined below (used by rest_info, the motion recovery,
// cast and collide phases). Zero motion dir disables the moving-out
// shortcut and lets the behind-every-overlapped-triangle test decide.
static bool _skip_one_sided_overlap(const PhysXShape3D *p_blueprint, physx::PxShape *p_mesh_shape,
		const physx::PxTransform &p_mesh_pose, float p_depth, const physx::PxVec3 &p_push_dir,
		const physx::PxVec3 &p_motion_dir, const physx::PxGeometry &p_mover_geom, const physx::PxTransform &p_mover_pose);

PhysXDirectSpaceState3D::PhysXDirectSpaceState3D(PhysXSpace3D *p_space) {
	space = p_space;
}

// ---------------------------------------------------------------------------
// _build_query_shape — shared geometry/pose builder for shape-based queries.
//
// Resolves a Godot shape RID to its PhysXShape3D blueprint, generates the
// scaled PxGeometry via the shape's own get_physx_geometry() (so every shape
// type — box/sphere/capsule/convex/concave/heightmap/cylinder/cone — is
// supported, with scale and axis alignment baked in), and composes the query
// pose as world_transform * shape_local_pose. This mirrors how shapes are
// placed on a body in PhysXShapedObject3D::add_shape().
//
// Returns false (with an error) if the RID is invalid or geometry generation
// failed; callers return "no hit" in that case.
// ---------------------------------------------------------------------------
static bool _build_query_shape(const RID &p_shape_rid,
		const Transform3D &p_transform,
		physx::PxGeometryHolder &r_geometry,
		physx::PxTransform &r_pose,
		float p_margin = 0.0f) {
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	ERR_FAIL_NULL_V(server, false);
	const PhysXShape3D *shape = server->get_shape(p_shape_rid);
	if (!shape) {
		ERR_PRINT_ONCE("PhysX: query referenced an invalid shape RID.");
		return false;
	}

	// Bake the query transform's scale into the geometry (PhysX shapes carry
	// no scale of their own). The pose uses the unscaled rotation/translation.
	// Convex polygon meshes get the signed scale so a mirrored query shape
	// matches how the same mirrored attachment collides in simulation (the
	// convex shape bakes the mirror); every other geometry keeps the
	// absolute scale.
	const Vector3 scale = (shape->get_type() == PhysicsServer3D::SHAPE_CONVEX_POLYGON &&
								  !Math::is_zero_approx(p_transform.basis.determinant()))
			? p_transform.basis.get_scale()
			: p_transform.basis.get_scale_abs();
	const physx::PxVec3 px_scale(scale.x, scale.y, scale.z);
	if (!shape->get_physx_geometry(r_geometry, px_scale)) {
		ERR_PRINT_ONCE("PhysX: query shape failed to generate geometry.");
		return false;
	}

	// Query-margin inflation (godot_physics_3d contract: solve_static grows
	// both shapes by p_parameters.margin for intersect_shape / collide_shape /
	// cast_motion / rest_info). PhysX-native equivalent for the primitives we
	// can grow in place; cooked meshes (convex/concave/heightfield) and the
	// custom cylinder/cone are exact — Godot's GJK margin can't be reproduced
	// without re-cooking, so those stay uninflated (documented module gap,
	// audit §4 "query margin inflation contract").
	if (p_margin > 0.0f) {
		switch (r_geometry.getType()) {
			case physx::PxGeometryType::eSPHERE:
				r_geometry.sphere().radius += p_margin;
				break;
			case physx::PxGeometryType::eBOX:
				r_geometry.box().halfExtents += physx::PxVec3(p_margin);
				break;
			case physx::PxGeometryType::eCAPSULE:
				r_geometry.capsule().radius += p_margin;
				break;
			default:
				break;
		}
	}

	// Compose world pose * shape-local alignment pose (e.g. capsule Y->X).
	// Orthonormalize the basis so the PxTransform quaternion is valid even if
	// the input basis carried shear from the scale we just stripped out.
	// The alignment pose is scale-dependent for heightfields (quantization
	// lift) and separation rays (forward offset) — same scale as the geometry.
	Transform3D unscaled = p_transform;
	unscaled.basis.orthonormalize();
	r_pose = PhysXShapedObject3D::to_physx_transform(unscaled) * shape->get_local_pose(px_scale);
	return true;
}

// -----------------------------------------------------------------------
// INTERSECT RAY (Raycast)
// -----------------------------------------------------------------------
bool PhysXDirectSpaceState3D::intersect_ray(const RayParameters &p_parameters, RayResult &r_result) {
	_ensure_space_synced(space);
	if (!space || !space->get_px_scene()) {
		return false;
	}

	physx::PxScene *scene = space->get_px_scene();

	// Calculate origin, direction, and distance
	Vector3 godot_dir = p_parameters.to - p_parameters.from;
	real_t length = godot_dir.length();
	if (length == 0.0) {
		return false;
	}
	godot_dir.normalize();

	physx::PxVec3 origin(p_parameters.from.x, p_parameters.from.y, p_parameters.from.z);
	physx::PxVec3 dir(godot_dir.x, godot_dir.y, godot_dir.z);

	// Setup custom filter callback
	PhysXQueryFilterCallback filter_cb;
	filter_cb.collision_mask = p_parameters.collision_mask;
	filter_cb.collide_with_bodies = p_parameters.collide_with_bodies;
	filter_cb.collide_with_areas = p_parameters.collide_with_areas;
	filter_cb.exclude_rids = &p_parameters.exclude;
	// Raycasts honor body/area ray_pickable (Godot: raycasts only — the
	// hit_from_inside overlap below shares this callback, so the pickability
	// gate covers the synthesized origin hit too).
	filter_cb.pick_ray = true;
	// intersect_ray is single-hit — keep eBLOCK for the closest hit.

	// 2. Configure PhysX to USE the preFilter callback
	physx::PxQueryFilterData filter_data;
	filter_data.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER;

	// 3. Map Godot's backface/inside flags to PhysX Hit Flags.
	//   - hit_back_faces: let raycasts hit the back side of triangles
	//     (PhysX: eMESH_BOTH_SIDES).
	//   - hit_from_inside: Godot's contract is that when the ray origin is
	//     inside a shape, a hit at distance 0 (position = origin, zero normal)
	//     is returned. PhysX already returns a distance-0 hit when starting
	//     inside a convex shape, so hit_from_inside=true needs no extra flag.
	//     When hit_from_inside=false, Godot ignores shapes the origin is inside;
	//     we emulate that by dropping zero-distance hits below.
	//   (eMTD is for overlap/sweep penetration queries, NOT raycasts — using it
	//   here was a no-op.)
	physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eFACE_INDEX;

	if (p_parameters.hit_back_faces) {
		hit_flags |= physx::PxHitFlag::eMESH_BOTH_SIDES;
	}

	// Back-face rule for doubled-cook concave meshes: without the post-filter,
	// a backface_collision-enabled mesh's flipped copies hit rays that never
	// asked for back faces (the flipped triangle fronts the ray from behind).
	filter_cb.ray_backface_filtering = true;
	filter_cb.ray_hit_back_faces = p_parameters.hit_back_faces;
	{
		filter_cb.ray_dir_x = godot_dir.x;
		filter_cb.ray_dir_y = godot_dir.y;
		filter_cb.ray_dir_z = godot_dir.z;
	}
	filter_data.flags |= physx::PxQueryFlag::ePOSTFILTER;

	// When hit_from_inside is true, the origin may be inside a shape. PhysX
	// raycasts don't reliably report zero-distance inside hits (especially for
	// mesh shapes), so we do a tiny-sphere overlap to detect if the origin
	// is inside any shape and synthesize the Godot origin-hit if so.
	physx::PxTransform pose(origin);
	if (p_parameters.hit_from_inside) {
		constexpr float INSIDE_RADIUS = 1e-4f;
		physx::PxSphereGeometry inside_geo(INSIDE_RADIUS);
		// multi_hit returns eTOUCH from the pre-filter: overlap queries warn
		// when the filter returns eBLOCK.
		filter_cb.multi_hit = true;
		LocalVector<physx::PxOverlapHit> overlap_storage;
		overlap_storage.resize(1);
		physx::PxOverlapBuffer overlap_buf(overlap_storage.ptr(), 1);
		if (scene->overlap(inside_geo, pose, overlap_buf, filter_data, &filter_cb)) {
			const physx::PxOverlapHit &overlap = overlap_buf.getAnyHit(0);

			// Synthesize the origin hit (distance 0, zero normal) for the
			// first shape that contains the origin.
			r_result.position = p_parameters.from;
			r_result.normal = Vector3();

			if (overlap.actor && overlap.actor->userData) {
				auto *actor_data = static_cast<PhysXActorUserData *>(overlap.actor->userData);
				r_result.rid = actor_data->rid;
				r_result.collider_id = actor_data->object_id;
				r_result.collider = ObjectDB::get_instance(actor_data->object_id);
			} else {
				r_result.rid = RID();
				r_result.collider_id = ObjectID();
				r_result.collider = nullptr;
			}
			if (overlap.shape && overlap.shape->userData) {
				r_result.shape = physx_resolve_shape_index(overlap.actor, overlap.shape);
			} else {
				r_result.shape = 0;
			}
			// face_index is undefined for origin-inside hits; leave as default.
			return true;
		}
	}

	physx::PxRaycastBuffer hit;
	bool has_hit = scene->raycast(origin, dir, length, hit, hit_flags, filter_data, &filter_cb);

	if (has_hit && hit.hasBlock) {
		const physx::PxRaycastHit &block = hit.block;

		// Godot's contract (godot_space_3d intersect_ray): backface hits are
		// ordinary hits reported at their real position when hit_back_faces is
		// set — a ray inside a concave interior hits the far wall's inward
		// face and MUST report it. Only a hit at distance <= 0 means the ray
		// origin sits inside the shape.
		if (block.distance <= 0.0f) {
			if (!p_parameters.hit_from_inside) {
				// Origin inside the shape: Godot skips it (other shapes are
				// still searched there; the single closest-hit raycast cannot
				// skip and continue, so "no hit" is the faithful fallback).
				return false;
			}
			// hit_from_inside == true: report a synthetic from-inside hit.
			r_result.position = p_parameters.from;
			r_result.normal = Vector3();
			r_result.face_index = _resolve_face_index(block);

			// Map the PhysX hit back to Godot via the actor/shape userData.
			if (block.actor && block.actor->userData) {
				auto *actor_data = static_cast<PhysXActorUserData *>(block.actor->userData);
				r_result.rid = actor_data->rid;
				r_result.collider_id = actor_data->object_id;
				r_result.collider = ObjectDB::get_instance(actor_data->object_id);
			} else {
				r_result.rid = RID();
				r_result.collider_id = ObjectID();
				r_result.collider = nullptr;
			}
			if (block.shape && block.shape->userData) {
				r_result.shape = physx_resolve_shape_index(block.actor, block.shape);
			} else {
				r_result.shape = 0;
			}
			return true;
		}

		r_result.position = Vector3(block.position.x, block.position.y, block.position.z);
		r_result.normal = Vector3(block.normal.x, block.normal.y, block.normal.z);
		// Backface-cooked concave meshes double every triangle; translate the
		// raw PhysX face index into the author's index (identity otherwise).
		r_result.face_index = _resolve_face_index(block);

		// Map the PhysX hit back to Godot via the actor/shape userData.
		if (block.actor && block.actor->userData) {
			auto *actor_data = static_cast<PhysXActorUserData *>(block.actor->userData);
			r_result.rid = actor_data->rid;
			r_result.collider_id = actor_data->object_id;
			r_result.collider = ObjectDB::get_instance(actor_data->object_id);
		} else {
			r_result.rid = RID();
			r_result.collider_id = ObjectID();
			r_result.collider = nullptr;
		}
		if (block.shape && block.shape->userData) {
			r_result.shape = physx_resolve_shape_index(block.actor, block.shape);
		} else {
			r_result.shape = -1;
		}
		return true;
	}
	return false;
}
// -----------------------------------------------------------------------
// INTERSECT POINT (Overlap)
// -----------------------------------------------------------------------
int PhysXDirectSpaceState3D::intersect_point(const PointParameters &p_parameters, ShapeResult *r_results, int p_result_max) {
	_ensure_space_synced(space);
	if (!space || !space->get_px_scene() || p_result_max <= 0) {
		return 0;
	}
	physx::PxScene *scene = space->get_px_scene();
	constexpr float POINT_QUERY_RADIUS = 1e-4f;
	// Setup a tiny sphere geometry to simulate a "point"
	physx::PxSphereGeometry point_geo(POINT_QUERY_RADIUS);
	physx::PxTransform pose(physx::PxVec3(p_parameters.position.x, p_parameters.position.y, p_parameters.position.z));

	// Heap-allocate the hit buffer sized by the caller's request. alloca with
	// a caller-controlled count is a stack-exhaustion vector; the result_max
	// caps below also bound the engine-facing result arrays.
	const int result_max = MIN(p_result_max, PHYSX_QUERY_MAX_RESULTS);
	LocalVector<physx::PxOverlapHit> hit_storage;
	hit_storage.resize(result_max);
	physx::PxOverlapBuffer hit(hit_storage.ptr(), result_max);
	PhysXQueryFilterCallback filter_cb;
	filter_cb.collision_mask = p_parameters.collision_mask;
	filter_cb.collide_with_bodies = p_parameters.collide_with_bodies;
	filter_cb.collide_with_areas = p_parameters.collide_with_areas;
	filter_cb.exclude_rids = &p_parameters.exclude;
	filter_cb.multi_hit = true; ///< intersect_point: collect all overlaps

	physx::PxQueryFilterData filter_data;
	filter_data.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER;

	// Execute Overlap
	bool has_hit = scene->overlap(point_geo, pose, hit, filter_data, &filter_cb);

	if (!has_hit) {
		return 0;
	}

	// Map results back to Godot via userData.
	const int count = MIN((int)hit.getNbAnyHits(), result_max);
	for (int i = 0; i < count; ++i) {
		const physx::PxOverlapHit &overlap = hit.getAnyHit(i);

		if (overlap.actor && overlap.actor->userData) {
			auto *actor_data = static_cast<PhysXActorUserData *>(overlap.actor->userData);
			r_results[i].rid = actor_data->rid;
			r_results[i].collider_id = actor_data->object_id;
			r_results[i].collider = ObjectDB::get_instance(actor_data->object_id);
		} else {
			r_results[i].rid = RID();
			r_results[i].collider_id = ObjectID();
			r_results[i].collider = nullptr;
		}

		if (overlap.shape && overlap.shape->userData) {
			r_results[i].shape = physx_resolve_shape_index(overlap.actor, overlap.shape);
		} else {
			r_results[i].shape = 0;
		}
	}

	return count;
}

int PhysXDirectSpaceState3D::intersect_shape(const ShapeParameters &p_parameters, ShapeResult *r_results, int p_result_max) {
	_ensure_space_synced(space);
	if (!space || !space->get_px_scene() || p_result_max <= 0) {
		return 0;
	}

	// Build the query geometry + pose from the shape resource (supports every
	// shape type via the shape's own get_physx_geometry / get_local_pose).
	physx::PxGeometryHolder geometry;
	physx::PxTransform pose(physx::PxIdentity);
	if (!_build_query_shape(p_parameters.shape_rid, p_parameters.transform, geometry, pose, (float)p_parameters.margin)) {
		return 0;
	}

	// Setup our custom filter callback
	PhysXQueryFilterCallback filter_cb;
	filter_cb.collision_mask = p_parameters.collision_mask;
	filter_cb.collide_with_bodies = p_parameters.collide_with_bodies;
	filter_cb.collide_with_areas = p_parameters.collide_with_areas;
	filter_cb.exclude_rids = &p_parameters.exclude;
	filter_cb.multi_hit = true; ///< intersect_shape: collect all overlaps

	physx::PxQueryFilterData filter_data;
	filter_data.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER;

	// Heap-allocate the hit buffer; clamp to PHYSX_QUERY_MAX_RESULTS so a
	// caller-controlled count can't exhaust the stack.
	const int result_max = MIN(p_result_max, PHYSX_QUERY_MAX_RESULTS);
	LocalVector<physx::PxOverlapHit> hit_storage;
	hit_storage.resize(result_max);
	physx::PxOverlapBuffer hit(hit_storage.ptr(), result_max);

	// Fire the overlap query
	bool has_hit = space->get_px_scene()->overlap(geometry.any(), pose, hit, filter_data, &filter_cb);

	if (!has_hit) {
		return 0;
	}

	// Map the results using our flattened UserData
	const int count = MIN((int)hit.getNbAnyHits(), result_max);
	for (int i = 0; i < count; ++i) {
		const physx::PxOverlapHit &overlap = hit.getAnyHit(i);

		if (overlap.actor && overlap.actor->userData) {
			auto *actor_data = static_cast<PhysXActorUserData *>(overlap.actor->userData);
			r_results[i].rid = actor_data->rid;
			r_results[i].collider_id = actor_data->object_id;
			r_results[i].collider = ObjectDB::get_instance(actor_data->object_id);
		} else {
			r_results[i].rid = RID();
			r_results[i].collider_id = ObjectID();
			r_results[i].collider = nullptr;
		}

		if (overlap.shape && overlap.shape->userData) {
			r_results[i].shape = physx_resolve_shape_index(overlap.actor, overlap.shape);
		} else {
			r_results[i].shape = 0;
		}
	}

	return count;
}

bool PhysXDirectSpaceState3D::cast_motion(const ShapeParameters &p_parameters, real_t &r_closest_safe, real_t &r_closest_unsafe, ShapeRestInfo *r_info) {
	_ensure_space_synced(space);
	if (!space || !space->get_px_scene()) {
		return false;
	}

	// Build the query geometry + pose from the shape resource.
	physx::PxGeometryHolder geometry;
	physx::PxTransform pose(physx::PxIdentity);
	if (!_build_query_shape(p_parameters.shape_rid, p_parameters.transform, geometry, pose, (float)p_parameters.margin)) {
		r_closest_safe = 1.0;
		r_closest_unsafe = 1.0;
		return false;
	}

	Vector3 motion = p_parameters.motion;
	real_t length = motion.length();
	if (length == 0.0) {
		r_closest_safe = 1.0;
		r_closest_unsafe = 1.0;
		return false;
	}

	Vector3 dir = motion / length;
	physx::PxVec3 px_dir(dir.x, dir.y, dir.z);

	PhysXQueryFilterCallback filter_cb;
	filter_cb.collision_mask = p_parameters.collision_mask;
	filter_cb.collide_with_bodies = p_parameters.collide_with_bodies;
	filter_cb.collide_with_areas = p_parameters.collide_with_areas;
	filter_cb.exclude_rids = &p_parameters.exclude;
	filter_cb.multi_hit = true; ///< collect all touched shapes; overlapped ones are skipped per-object below

	physx::PxQueryFilterData filter_data;
	filter_data.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER;

	// Godot's cast_motion contract (godot_space_3d _cast_motion): objects the
	// query shape ALREADY overlaps are disregarded per-object, and the closest
	// FORWARD blocker among the rest bounds the motion. A single closest-block
	// sweep cannot express that — an initial overlap is always the nearest
	// hit (negative distance under eMTD) and masks every real blocker behind
	// it. Sweep with a touch buffer instead and scan the hits.
	const physx::PxU32 touch_max = PHYSX_QUERY_MAX_RESULTS;
	if (_sweep_touch_scratch.size() < touch_max) {
		_sweep_touch_scratch.resize(touch_max);
	}
	physx::PxHitBuffer<physx::PxSweepHit> hit(_sweep_touch_scratch.ptr(), touch_max);

	// eMTD makes initial-overlap sweeps report a well-defined (negative)
	// distance/normal/position instead of distance==0 with garbage fields.
	physx::PxHitFlags hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL |
			physx::PxHitFlag::eFACE_INDEX | physx::PxHitFlag::eMTD;

	space->get_px_scene()->sweep(geometry.any(), pose, px_dir, length, hit, hit_flags, filter_data, &filter_cb);

	real_t best_fraction = 1.0;
	const physx::PxSweepHit *best = nullptr;
	physx::PxVec3 best_normal(0.0f, 0.0f, 0.0f);
	for (physx::PxU32 i = 0; i < hit.nbTouches; ++i) {
		const physx::PxSweepHit &touch = hit.getTouch(i);
		// Disregard objects the query shape starts inside of (Godot contract).
		if (touch.hadInitialOverlap()) {
			continue;
		}
		// Internal-edge handling: a forward hit on a mesh whose triangle
		// winding contradicts the reported normal is a shared-edge contact,
		// not a surface the motion ran into.
		physx::PxVec3 face_normal;
		if (!_sweep_forward_hit_blocking(touch, face_normal)) {
			continue;
		}
		const real_t fraction = touch.distance / length;
		if (!best || fraction < best_fraction) {
			best_fraction = fraction;
			best = &touch;
			best_normal = face_normal;
		}
	}

	r_closest_safe = 1.0;
	r_closest_unsafe = 1.0;
	if (best) {
		// Godot reports the raw motion fraction — the margin only widens the
		// reference's broadphase query, it never shrinks the safe fraction.
		r_closest_safe = MAX(0.0, best_fraction);
		r_closest_unsafe = best_fraction;

		// Optional rest info for the blocking hit (C++-only output — the
		// script binding drops it). Same field mapping as rest_info():
		// point on the collider's surface, normal toward the query shape,
		// collider velocity at the point.
		if (r_info) {
			if (best->actor && best->actor->userData) {
				const auto *actor_data = static_cast<const PhysXActorUserData *>(best->actor->userData);
				r_info->rid = actor_data->rid;
				r_info->collider_id = actor_data->object_id;
			} else {
				r_info->rid = RID();
				r_info->collider_id = ObjectID();
			}
			r_info->shape = physx_resolve_shape_index(best->actor, best->shape);
			r_info->point = Vector3(best->position.x, best->position.y, best->position.z);
			r_info->normal = Vector3(best_normal.x, best_normal.y, best_normal.z);
			r_info->linear_velocity = Vector3();
			if (best->actor && best->actor->is<physx::PxRigidDynamic>()) {
				const physx::PxRigidDynamic *dyn = best->actor->is<physx::PxRigidDynamic>();
				const physx::PxVec3 v = physx::PxRigidBodyExt::getVelocityAtPos(*const_cast<physx::PxRigidDynamic *>(dyn), best->position);
				r_info->linear_velocity = Vector3(v.x, v.y, v.z);
			}
		}
	}

	// Godot returns true whenever the query ran (free motion reports [1,1]);
	// the script binding turns a false return into an empty array.
	return true;
}

bool PhysXDirectSpaceState3D::collide_shape(const ShapeParameters &p_parameters, Vector3 *r_results, int p_result_max, int &r_result_count) {
	r_result_count = 0;
	_ensure_space_synced(space);
	if (!space || !space->get_px_scene() || p_result_max <= 0) {
		return false;
	}

	// Build the query geometry + pose from the shape resource.
	physx::PxGeometryHolder query_geom;
	physx::PxTransform query_pose(physx::PxIdentity);
	if (!_build_query_shape(p_parameters.shape_rid, p_parameters.transform, query_geom, query_pose, (float)p_parameters.margin)) {
		return false;
	}

	// Filter fields
	PhysXQueryFilterCallback filter_cb;
	filter_cb.collision_mask = p_parameters.collision_mask;
	filter_cb.collide_with_bodies = p_parameters.collide_with_bodies;
	filter_cb.collide_with_areas = p_parameters.collide_with_areas;
	filter_cb.exclude_rids = &p_parameters.exclude;
	filter_cb.multi_hit = true; ///< collide_shape: collect all overlaps

	// Perform Overlap
	physx::PxQueryFilterData filter_data;
	filter_data.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER;

	const int result_max = MIN(p_result_max, PHYSX_QUERY_MAX_RESULTS);
	LocalVector<physx::PxOverlapHit> hit_storage;
	hit_storage.resize(result_max);
	physx::PxOverlapBuffer hit(hit_storage.ptr(), result_max);

	if (!space->get_px_scene()->overlap(query_geom.any(), query_pose, hit, filter_data, &filter_cb)) {
		return false;
	}

	// Process contacts using computePenetration. Godot's collide_shape contract
	// is that r_results holds PAIRS of contact points per collision: index
	// (i*2+0) is the contact point on the QUERY shape's surface, (i*2+1) is the
	// contact point on the COLLIDER's surface. r_result_count counts contacts
	// (pairs), not individual points. (Buffer is pre-sized p_result_max*2 by the
	// caller.) Collisions are processed nearest-first by depth.
	int written = 0;
	for (physx::PxU32 i = 0; i < hit.getNbAnyHits() && written < result_max; ++i) {
		const physx::PxOverlapHit &overlap = hit.getAnyHit(i);

		physx::PxGeometryHolder hit_geom = overlap.shape->getGeometry();
		physx::PxTransform hit_pose = physx::PxShapeExt::getGlobalPose(*overlap.shape, *overlap.actor);

		physx::PxVec3 mtd_dir;
		physx::PxReal penetration_depth = 0.0f;
		const bool is_pen = physx::PxGeometryQuery::computePenetration(
				mtd_dir, penetration_depth,
				query_geom.any(), query_pose,
				hit_geom.any(), hit_pose);

		if (!is_pen) {
			continue;
		}

		// Contact points must lie on the shapes' surfaces, not near the query
		// shape's centroid. The collider-side point is the closest point on the
		// collider's surface to the query shape's center; the query-side point
		// sits one penetration depth further along the MTD (push-out vector).
		// pointDistance is only valid for a strictly positive result (and is
		// unsupported for heightfields) — fall back to the MTD segment around
		// the query center instead of consuming an unwritten vector.
		physx::PxVec3 point_b;
		const physx::PxReal center_dist = physx::PxGeometryQuery::pointDistance(query_pose.p, hit_geom.any(), hit_pose, &point_b);
		if (center_dist <= 0.0f) {
			point_b = query_pose.p - mtd_dir * (penetration_depth * 0.5f);
		}
		const physx::PxVec3 point_a = point_b + mtd_dir * penetration_depth; // query side
		r_results[written * 2 + 0] = Vector3(point_a.x, point_a.y, point_a.z);
		r_results[written * 2 + 1] = Vector3(point_b.x, point_b.y, point_b.z);
		written++;
	}

	r_result_count = written;
	return r_result_count > 0;
}

bool PhysXDirectSpaceState3D::rest_info(const ShapeParameters &p_parameters, ShapeRestInfo *r_info) {
	// Returns the deepest-penetration contact info for the query shape:
	// overlaps the scene, computes penetration against every hit, and reports
	// the DEEPEST one (godot_space_3d _rest_cbk_result keeps the largest len).
	// Used by cast_motion when the sweep starts already overlapping, and by
	// direct rest_info queries (ground checks).
	_ensure_space_synced(space);
	if (!r_info || !space || !space->get_px_scene()) {
		return false;
	}

	physx::PxGeometryHolder query_geom;
	physx::PxTransform query_pose(physx::PxIdentity);
	if (!_build_query_shape(p_parameters.shape_rid, p_parameters.transform, query_geom, query_pose, (float)p_parameters.margin)) {
		return false;
	}

	PhysXQueryFilterCallback filter_cb;
	filter_cb.collision_mask = p_parameters.collision_mask;
	filter_cb.collide_with_bodies = p_parameters.collide_with_bodies;
	filter_cb.collide_with_areas = p_parameters.collide_with_areas;
	filter_cb.exclude_rids = &p_parameters.exclude;
	filter_cb.multi_hit = true; ///< rest_info: collect all overlaps

	physx::PxQueryFilterData filter_data;
	filter_data.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER;

	constexpr int MAX_REST_HITS = 32;
	physx::PxOverlapHit hit_buffer[MAX_REST_HITS];
	physx::PxOverlapBuffer hit(hit_buffer, MAX_REST_HITS);

	if (!space->get_px_scene()->overlap(query_geom.any(), query_pose, hit, filter_data, &filter_cb)) {
		return false;
	}

	// Find the overlapping shape with the deepest penetration (Godot selects
	// the largest contact length as the rest result).
	bool found = false;
	physx::PxReal best_depth = -1.0f;
	const physx::PxOverlapHit *best = nullptr;
	physx::PxVec3 best_mtd(0, 0, 0);
	physx::PxGeometryHolder best_geom;
	physx::PxTransform best_pose(physx::PxIdentity);

	for (physx::PxU32 i = 0; i < hit.getNbAnyHits(); ++i) {
		const physx::PxOverlapHit &overlap = hit.getAnyHit(i);

		physx::PxGeometryHolder hit_geom = overlap.shape->getGeometry();
		physx::PxTransform hit_pose = physx::PxShapeExt::getGlobalPose(*overlap.shape, *overlap.actor);

		physx::PxVec3 mtd_dir;
		physx::PxReal penetration_depth = 0.0f;
		if (!physx::PxGeometryQuery::computePenetration(
					mtd_dir, penetration_depth,
					query_geom.any(), query_pose,
					hit_geom.any(), hit_pose)) {
			continue;
		}

		// One-sided concave meshes (backface_collision off): a query
		// overlapping the mesh from its BACK gets no rest result — Godot's
		// one-sided trimeshes have no "resting on the back side", and
		// reporting the far face's plane lets the scene-side depenetration
		// fling a one-way platform's understider up onto it (the platform
		// jump-snap). Zero motion dir: the moving-out shortcut can't fire;
		// the behind-every-overlapped-triangle test decides.
		{
			const PhysXShape3D *shape_bp = overlap.shape && overlap.shape->userData
					? static_cast<const PhysXShape3D *>(overlap.shape->userData)
					: nullptr;
			if (_skip_one_sided_overlap(shape_bp, overlap.shape, hit_pose,
						penetration_depth, mtd_dir, physx::PxVec3(0.0f),
						query_geom.any(), query_pose)) {
				continue;
			}
		}

		if (penetration_depth > best_depth) {
			best_depth = penetration_depth;
			best = &overlap;
			best_mtd = mtd_dir;
			best_geom = hit_geom;
			best_pose = hit_pose;
			found = true;
		}
	}

	if (!found || !best) {
		return false;
	}

	// MTD points from the collider toward the query shape; Godot's rest normal
	// points toward the query shape (away from the surface it rests on). The
	// rest point is on the collider's surface (see collide_shape). When the
	// pointDistance fallback applies (query center inside the collider, or a
	// heightfield collider, which pointDistance does not support), approximate
	// it with the MTD segment around the query center.
	r_info->normal = Vector3(best_mtd.x, best_mtd.y, best_mtd.z);
	physx::PxVec3 rest_pt;
	const physx::PxReal center_dist = physx::PxGeometryQuery::pointDistance(query_pose.p, best_geom.any(), best_pose, &rest_pt);
	if (center_dist <= 0.0f) {
		rest_pt = query_pose.p - best_mtd * (best_depth * 0.5f);
	}
	r_info->point = Vector3(rest_pt.x, rest_pt.y, rest_pt.z);

	if (best->actor && best->actor->userData) {
		const auto *actor_data = static_cast<PhysXActorUserData *>(best->actor->userData);
		r_info->rid = actor_data->rid;
		r_info->collider_id = actor_data->object_id;
	} else {
		r_info->rid = RID();
		r_info->collider_id = ObjectID();
	}
	r_info->shape = physx_resolve_shape_index(best->actor, best->shape);

	// Velocity at the rest point (for dynamic colliders).
	r_info->linear_velocity = Vector3();
	if (best->actor && best->actor->is<physx::PxRigidDynamic>()) {
		const physx::PxRigidDynamic *dyn = best->actor->is<physx::PxRigidDynamic>();
		const physx::PxVec3 v = physx::PxRigidBodyExt::getVelocityAtPos(*const_cast<physx::PxRigidDynamic *>(dyn), rest_pt);
		r_info->linear_velocity = Vector3(v.x, v.y, v.z);
	}

	return true;
}

Vector3 PhysXDirectSpaceState3D::get_closest_point_to_object_volume(RID p_object, Vector3 p_point) const {
	_ensure_space_synced(space);
	// Resolve the RID to the internal PhysXBody3D wrapper.
	PhysXBody3D *body = PhysXServer3D::get_singleton()->get_body(p_object);

	if (!body) {
		return p_point;
	}

	physx::PxRigidActor *actor = body->get_px_actor();
	if (!actor) {
		return p_point;
	}

	physx::PxVec3 query_point(p_point.x, p_point.y, p_point.z);
	physx::PxVec3 closest_point(p_point.x, p_point.y, p_point.z); // Default to origin
	physx::PxReal min_distance = PX_MAX_F32;

	// Get all shapes from the actor.
	const physx::PxU32 nb_shapes = actor->getNbShapes();
	if (nb_shapes == 0) {
		return p_point;
	}

	LocalVector<physx::PxShape *> shapes;
	shapes.resize(nb_shapes);
	actor->getShapes(shapes.ptr(), nb_shapes);

	for (physx::PxU32 i = 0; i < nb_shapes; ++i) {
		physx::PxShape *shape = shapes[i];
		physx::PxGeometryHolder geom = shape->getGeometry();
		physx::PxTransform pose = physx::PxShapeExt::getGlobalPose(*shape, *actor);

		physx::PxVec3 local_closest;
		// 0.0f means we don't care about inflation
		physx::PxReal dist = physx::PxGeometryQuery::pointDistance(query_point, geom.any(), pose, &local_closest);
		// pointDistance only writes the closest point for a strictly positive
		// distance (heightfields return -1); at distance 0 the query point is
		// on the surface itself.
		if (dist < 0.0f) {
			continue;
		}

		if (dist < min_distance) {
			min_distance = dist;
			closest_point = dist > 0.0f ? local_closest : query_point;
		}
	}

	return Vector3(closest_point.x, closest_point.y, closest_point.z);
}

// Fills collisions[0] from a sweep hit when the overlap-based collide phase
// came back empty — at exact touch there is no penetration to report, and in
// the stuck case against trimesh/heightfield (computePenetration rejects
// those geometries) the sweep/eMTD hit is the only contact source.
static void _fill_collision_from_sweep(PhysicsServer3D::MotionResult *r_result,
		const Vector3 &p_position, const Vector3 &p_normal, real_t p_depth, int p_local_shape,
		const physx::PxRigidActor *p_actor, const physx::PxShape *p_shape) {
	if (!r_result || r_result->collision_count > 0 || !p_actor) {
		return;
	}
	PhysicsServer3D::MotionCollision &col = r_result->collisions[0];
	col.position = p_position;
	col.normal = p_normal;
	col.depth = p_depth;
	if (p_actor->userData) {
		const auto *actor_data = static_cast<const PhysXActorUserData *>(p_actor->userData);
		col.collider = actor_data->rid;
		col.collider_id = actor_data->object_id;
	} else {
		col.collider = RID();
		col.collider_id = ObjectID();
	}
	col.collider_shape = physx_resolve_shape_index(p_actor, p_shape);
	col.local_shape = p_local_shape >= 0 ? p_local_shape : 0;
	if (const physx::PxRigidDynamic *dyn = p_actor->is<physx::PxRigidDynamic>()) {
		const physx::PxVec3 v = physx::PxRigidBodyExt::getVelocityAtPos(
				*const_cast<physx::PxRigidDynamic *>(dyn),
				physx::PxVec3(p_position.x, p_position.y, p_position.z));
		col.collider_velocity = Vector3(v.x, v.y, v.z);
		const physx::PxVec3 av = dyn->getAngularVelocity();
		col.collider_angular_velocity = Vector3(av.x, av.y, av.z);
	} else {
		col.collider_velocity = Vector3();
		col.collider_angular_velocity = Vector3();
	}
	r_result->collision_count = 1;
}

// ---------------------------------------------------------------------------
// Internal-edge handling for sweep hits against triangle meshes/heightfields.
//
// Sweeping a convex over such a surface can catch on the shared edge between
// facets, and PhysX may then report an edge-derived normal (often
// axis-aligned, looking like a wall to a walking character). Re-derive the
// true world-space face normal from the hit triangle (PxMeshQuery::getTriangle;
// it flips the normal itself for negative-determinant mesh scales) and reject
// only the artifact class: a mesh hit whose triangle winding DISAGREES with
// the reported hit normal. A genuine forward hit's face normal agrees with
// what PhysX reported and is kept no matter how perpendicular it is to the
// motion — the old "normal must oppose the sweep" threshold discarded real
// near-perpendicular blocks (razor-thin steps, steep scrapes) and let the
// motion tunnel through them.
// ---------------------------------------------------------------------------
static bool _sweep_hit_face_normal(const physx::PxSweepHit &p_hit, physx::PxVec3 &r_normal) {
	const physx::PxGeometryType::Enum geom_type = p_hit.shape
			? p_hit.shape->getGeometry().getType()
			: physx::PxGeometryType::eINVALID;
	if (geom_type != physx::PxGeometryType::eTRIANGLEMESH && geom_type != physx::PxGeometryType::eHEIGHTFIELD) {
		return false;
	}
	if (!p_hit.actor || p_hit.faceIndex == 0xffffffffu) {
		return false;
	}
	const physx::PxTransform hit_pose = physx::PxShapeExt::getGlobalPose(*p_hit.shape, *p_hit.actor);
	physx::PxTriangle tri;
	if (geom_type == physx::PxGeometryType::eTRIANGLEMESH) {
		physx::PxMeshQuery::getTriangle(
				static_cast<const physx::PxTriangleMeshGeometry &>(p_hit.shape->getGeometry()),
				hit_pose, p_hit.faceIndex, tri);
	} else {
		physx::PxMeshQuery::getTriangle(
				static_cast<const physx::PxHeightFieldGeometry &>(p_hit.shape->getGeometry()),
				hit_pose, p_hit.faceIndex, tri);
	}
	tri.normal(r_normal);
	return !r_normal.isZero();
}

// Evaluates one FORWARD sweep hit (distance > 0): writes the normal to report
// (the re-derived face normal when the triangle winding agrees with the
// reported normal, otherwise the reported normal itself) and returns false
// only for the mesh edge-artifact case described above. Primitive hits are
// always blocking.
static bool _sweep_forward_hit_blocking(const physx::PxSweepHit &p_hit, physx::PxVec3 &r_normal) {
	r_normal = p_hit.normal;

	physx::PxVec3 face_n;
	if (_sweep_hit_face_normal(p_hit, face_n)) {
		if (face_n.dot(p_hit.normal) < 0.0f) {
			// The triangle's winding faces the opposite way of the reported
			// contact — the sweep caught the shared edge between facets, not
			// the surface the motion ran into. Not a forward block.
			return false;
		}
		r_normal = face_n;
	}
	return true;
}

bool PhysXDirectSpaceState3D::body_test_motion(const PhysXBody3D &p_body, const PhysicsServer3D::MotionParameters &p_parameters, PhysicsServer3D::MotionResult *r_result) const {
	_ensure_space_synced(space);
	if (!space) {
		return false;
	}

	Transform3D transform = p_parameters.from;
	Vector3 motion = p_parameters.motion;
	float margin = p_parameters.margin;

	// Build exclusion set with the body's own RID (prevents self-collision at PhysX level)
	HashSet<RID> self_and_excluded;
	if (!p_parameters.exclude_bodies.is_empty()) {
		self_and_excluded = p_parameters.exclude_bodies;
	}
	self_and_excluded.insert(p_body.get_rid());

	// RECOVER (Depenetrate initial overlaps)
	Vector3 recovery;
	_motion_shapes_fill(p_body);
	bool recovered = _body_motion_recover(p_body, transform, motion.normalized(), margin, _motion_shape_scratch, self_and_excluded, p_parameters.exclude_objects, recovery);
	// Godot always lifts the working pose by the recovery (godot_space_3d
	// test_body_motion: body_transform.origin += recover_motion) — the cast
	// and contact gather must run from the depenetrated position in both
	// recovery_as_collision modes, or the reported contacts disagree with
	// travel.
	transform.origin += recovery;

	const float effective_margin = MAX(margin, 0.0001f);
	const float min_contact_depth = effective_margin * 0.05f;
	const float motion_length = motion.length();
	// Contacts shallower than this are rest separation, not collisions
	// (godot_space_3d: min_allowed_depth = MIN(motion_length, min_contact_depth)).
	const float min_allowed_depth = MIN(motion_length, min_contact_depth);

	// CAST (Sweep the motion)
	real_t safe_fraction = 1.0;
	real_t unsafe_fraction = 1.0;
	Vector3 hit_position;
	Vector3 hit_normal;
	real_t hit_depth = 0.0;
	int hit_local_shape = -1;
	const physx::PxRigidActor *hit_actor = nullptr;
	const physx::PxShape *hit_shape = nullptr;

	bool hit = _body_motion_cast(p_body, transform, motion, p_parameters.collide_separation_ray, min_contact_depth, _motion_shape_scratch, self_and_excluded, p_parameters.exclude_objects, safe_fraction, unsafe_fraction, hit_position, hit_normal, hit_depth, hit_local_shape, hit_actor, hit_shape);

	if (r_result) {
		// The collision/contact gather runs at the unsafe pose (Godot: ugt =
		// body_transform + motion * unsafe) — with recovery_as_collision and
		// no cast hit that is the full-motion pose, mirroring the reference.
		if (p_parameters.recovery_as_collision && recovered) {
			if (hit) {
				// Cast hit: collide at the unsafe position and combine with recovery.
				Transform3D hit_transform = transform;
				hit_transform.origin += motion * unsafe_fraction;
				_body_motion_collide(p_body, hit_transform, motion, p_parameters.max_collisions, min_allowed_depth, _motion_shape_scratch, self_and_excluded, p_parameters.exclude_objects, r_result);
				_fill_collision_from_sweep(r_result, hit_position, hit_normal, hit_depth, hit_local_shape, hit_actor, hit_shape);

				r_result->travel = motion * safe_fraction + recovery;
				r_result->remainder = motion - motion * safe_fraction;
				r_result->collision_unsafe_fraction = unsafe_fraction;
				r_result->collision_safe_fraction = safe_fraction;
			} else {
				// No cast hit: recovery alone can be the collision, but only if
				// the contact pass at the unsafe pose actually found
				// touching/overlapping shapes — Godot never reports a hit with
				// an empty collision list (CharacterBody3D would hand out
				// slide collisions with no contacts).
				Transform3D hit_transform = transform;
				hit_transform.origin += motion * unsafe_fraction;
				_body_motion_collide(p_body, hit_transform, motion, p_parameters.max_collisions, min_allowed_depth, _motion_shape_scratch, self_and_excluded, p_parameters.exclude_objects, r_result);
				r_result->travel = motion + recovery;
				r_result->remainder = Vector3();
				r_result->collision_safe_fraction = 1.0;
				r_result->collision_unsafe_fraction = 1.0;
			}
		} else {
			if (!hit) {
				// travel = full motion + recovery offset (from + travel == final pos).
				r_result->travel = motion + recovery;
				r_result->remainder = Vector3();
				r_result->collision_depth = 0;
				r_result->collision_count = 0;
				r_result->collision_safe_fraction = 1.0;
				r_result->collision_unsafe_fraction = 1.0;
			} else {
				// COLLIDE (Generate detailed manifold at the hit location)
				Transform3D hit_transform = transform;
				hit_transform.origin += motion * unsafe_fraction;
				_body_motion_collide(p_body, hit_transform, motion, p_parameters.max_collisions, min_allowed_depth, _motion_shape_scratch, self_and_excluded, p_parameters.exclude_objects, r_result);

				// The collide phase overlaps the shape at the contact pose; at
				// exact touch there is no penetration, so it can come back
				// empty (same for computePenetration-rejected trimesh
				// contacts in the stuck case). Godot still reports the
				// blocking collision — synthesize it from the sweep hit.
				_fill_collision_from_sweep(r_result, hit_position, hit_normal, hit_depth, hit_local_shape, hit_actor, hit_shape);

				// travel = swept motion up to the safe fraction, plus the recovery offset
				// applied to the transform above (matches Godot: from + travel == final pos).
				r_result->travel = motion * safe_fraction + recovery;
				r_result->remainder = motion - motion * safe_fraction;
				r_result->collision_unsafe_fraction = unsafe_fraction;
				r_result->collision_safe_fraction = safe_fraction;
			}
		}

		// Godot reports collision_depth as the deepest contact length
		// (godot_space_3d: r_result->collision_depth = rcd.best_result.len).
		real_t deepest = 0.0;
		for (int i = 0; i < r_result->collision_count; ++i) {
			deepest = MAX(deepest, r_result->collisions[i].depth);
		}
		r_result->collision_depth = deepest;
	}

	// An initial penetration counts as a collision only when the caller asked
	// for it (recovery_as_collision) AND the contact pass actually recorded
	// contacts. Returning true with an empty collision list violates Godot's
	// contract — body_test_motion must never report a hit without collisions
	// (CharacterBody3D exposes those as slide collisions, and reading them
	// errors with "index out of bounds"). (REG-0012)
	return hit || (p_parameters.recovery_as_collision && recovered && r_result != nullptr && r_result->collision_count > 0);
}

// The collider body's collision_priority (Godot: weights test-motion recovery;
// godot_space_3d reads it per contact). Non-body actors weight 1.0.
static real_t _collision_priority_of(const physx::PxRigidActor *p_actor) {
	if (p_actor && p_actor->userData) {
		const auto *actor_data = static_cast<const PhysXActorUserData *>(p_actor->userData);
		const PhysXObject3D *obj = actor_data->object;
		if (obj && obj->get_type() == PhysXObject3D::OBJECT_TYPE_BODY) {
			return static_cast<const PhysXBody3D *>(obj)->get_collision_priority();
		}
	}
	return 1.0;
}

// Fills the shared per-call shape cache for the body_test_motion phases: one
// getShapes() fetch, one userData read, one getLocalPose() and one
// find_shape_index() per shape for the entire call (the phases previously
// repeated all of these per phase, and the rotation quaternion per shape per
// recovery iteration).
void PhysXDirectSpaceState3D::_motion_shapes_fill(const PhysXBody3D &p_body) const {
	_motion_shape_scratch.clear();
	physx::PxRigidActor *actor = p_body.get_px_actor();
	if (!actor) {
		return;
	}
	const physx::PxU32 nb_shapes = actor->getNbShapes();
	if (nb_shapes == 0) {
		return;
	}
	_motion_shape_ptr_scratch.clear();
	_motion_shape_ptr_scratch.resize(nb_shapes);
	actor->getShapes(_motion_shape_ptr_scratch.ptr(), nb_shapes);
	_motion_shape_scratch.resize(nb_shapes);
	for (physx::PxU32 s = 0; s < nb_shapes; ++s) {
		MotionShapeRef &ref = _motion_shape_scratch[s];
		ref.px_shape = _motion_shape_ptr_scratch[s];
		ref.blueprint = ref.px_shape->userData ? static_cast<PhysXShape3D *>(ref.px_shape->userData) : nullptr;
		ref.local_pose = ref.px_shape->getLocalPose();
		ref.body_index = p_body.find_shape_index(ref.px_shape);
	}
}

static bool _skip_one_sided_overlap(const PhysXShape3D *p_blueprint, physx::PxShape *p_mesh_shape,
		const physx::PxTransform &p_mesh_pose, float p_depth, const physx::PxVec3 &p_push_dir,
		const physx::PxVec3 &p_motion_dir, const physx::PxGeometry &p_mover_geom, const physx::PxTransform &p_mover_pose) {
	if (!p_blueprint || p_blueprint->get_type() != PhysicsServer3D::SHAPE_CONCAVE_POLYGON || p_blueprint->has_backface_collision()) {
		return false;
	}
	if (p_motion_dir.dot(p_push_dir) > 0.5f) {
		return true; // (a) leaving along the push
	}
	if (p_depth >= 0.0f && p_depth <= 0.05f) {
		return false; // shallow front-side overlap: a real block
	}
	const physx::PxTriangleMeshGeometry &mesh_geom = static_cast<const physx::PxTriangleMeshGeometry &>(p_mesh_shape->getGeometry());
	constexpr physx::PxU32 MAX_TRIS = 64;
	physx::PxU32 tris[MAX_TRIS];
	bool overflow = false;
	const physx::PxU32 count = physx::PxMeshQuery::findOverlapTriangleMesh(p_mover_geom, p_mover_pose, mesh_geom, p_mesh_pose, tris, MAX_TRIS, 0, overflow);
	if (count == 0 || overflow) {
		return false;
	}
	for (physx::PxU32 i = 0; i < count; i++) {
		physx::PxTriangle tri;
		physx::PxMeshQuery::getTriangle(mesh_geom, p_mesh_pose, tris[i], tri);
		physx::PxVec3 tri_n;
		tri.normal(tri_n);
		if (tri_n.dot(p_mover_pose.p - tri.verts[0]) >= 0.0f) {
			return false; // in front of at least one overlapped triangle: a real block
		}
	}
	return true; // behind every overlapped triangle: approached from the back
}

bool PhysXDirectSpaceState3D::_body_motion_recover(const PhysXBody3D &p_body, const Transform3D &p_transform, const Vector3 &p_motion_dir, float p_margin,
		const LocalVector<MotionShapeRef> &p_shapes, const HashSet<RID> &p_self_and_excluded, const HashSet<ObjectID> &p_excluded_objects, Vector3 &r_recovery) const {
	r_recovery = Vector3();

	if (!space || !space->get_px_scene() || p_shapes.is_empty()) {
		return false;
	}

	const physx::PxU32 nb_shapes = p_shapes.size();

	constexpr int MAX_RECOVER_ITERATIONS = 4;
	constexpr float MIN_PENETRATION_THRESHOLD = 1e-5f;
	// godot_physics_3d contract (godot_space_3d.cpp): the margin floors at
	// 1e-4, derives a contact-depth slack (margin * 0.05) below which
	// penetration counts as rest separation rather than a stuck body, and
	// each recovery pass applies only 40% of the remaining MTD.
	const float effective_margin = MAX(p_margin, 0.0001f);
	const float min_contact_depth = effective_margin * 0.05f;
	const float recovery_scale = 0.4f;

	Vector3 total_recovery;
	bool recovered = false;

	// Filter setup for environment overlap query
	PhysXQueryFilterCallback filter_cb;
	filter_cb.collision_mask = p_body.get_collision_mask();
	filter_cb.collide_with_bodies = true;
	filter_cb.collide_with_areas = false;
	filter_cb.exclude_rids = &p_self_and_excluded;
	filter_cb.exclude_objects = &p_excluded_objects;
	filter_cb.motion_body = &p_body;
	filter_cb.multi_hit = true; ///< _body_motion_recover: collect all overlaps

	physx::PxQueryFilterData filter_data;
	filter_data.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER;

	// Rotation is invariant across recovery iterations (only the origin
	// moves) — derive it once for the whole phase.
	const Quaternion body_q_gd = p_transform.basis.get_rotation_quaternion();
	const physx::PxQuat body_q(body_q_gd.x, body_q_gd.y, body_q_gd.z, body_q_gd.w);

	for (int iter = 0; iter < MAX_RECOVER_ITERATIONS; ++iter) {
		Transform3D current_transform = p_transform;
		current_transform.origin += total_recovery;

		_recover_scratch.clear();
		bool penetrating_in_this_iter = false;

		// The pose is shared by every shape in this iteration.
		const physx::PxTransform body_pose(
				physx::PxVec3(current_transform.origin.x, current_transform.origin.y, current_transform.origin.z),
				body_q);

		for (physx::PxU32 s = 0; s < nb_shapes; ++s) {
			const MotionShapeRef &ref = p_shapes[s];
			physx::PxShape *body_shape = ref.px_shape;

			// Handle separation-ray shapes as raycasts against the floor.
			if (ref.blueprint) {
				const PhysXShape3D *shape_bp = ref.blueprint;
				if (shape_bp && shape_bp->get_type() == PhysicsServer3D::SHAPE_SEPARATION_RAY) {
					const PhysXSeparationRayShape3D *sep_ray = static_cast<const PhysXSeparationRayShape3D *>(shape_bp);
					// The ray length scales with the body (the shape's baked
					// geometry does) — cast and measure in scaled units.
					const real_t ray_length = sep_ray->get_length() * p_body.get_body_scale().z;

					physx::PxTransform shape_pose = body_pose * ref.local_pose;

					// Ray direction = the SHAPE's composed +Z axis. The ray's
					// rotation lives in the CollisionShape3D placement (baked
					// into ref.local_pose), NOT in the body transform -- using
					// the body basis alone cast the ray sideways for any
					// rotated placement (e.g. the 90-degree X rotation that
					// points a character's ray straight down).
					const physx::PxVec3 px_shape_dir = shape_pose.q.rotate(physx::PxVec3(0, 0, 1));

					// Adjust the ray origin to the base of the separation-ray shape.
					// The attached PxShape's local pose carries the forward
					// half-length offset (length * body Z scale — see the
					// shape's get_local_pose), so walk back the same scaled
					// half-length along the shape's own +Z axis.
					shape_pose.p -= px_shape_dir * (ray_length * 0.5f);

					// The phase callback runs with multi_hit=true (the overlap
					// path collects every overlap), which makes the preFilter
					// return eTOUCH -- and this raycast uses a default
					// PxRaycastBuffer whose touch capacity is ZERO: PhysX
					// discards eTOUCH hits it cannot record and the query
					// always reports a miss. The ray wants the single closest
					// hit, so give it a single-hit callback.
					PhysXQueryFilterCallback ray_filter_cb = filter_cb;
					ray_filter_cb.multi_hit = false;
					physx::PxRaycastBuffer ray_hit;
					const bool raycast_hit = space->get_px_scene()->raycast(shape_pose.p, px_shape_dir, ray_length, ray_hit,
							physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL, filter_data, &ray_filter_cb);
					if (raycast_hit) {
						if (ray_hit.hasBlock) {
							// Depth is how far the ray tip has passed the surface.
							// block.distance is measured along the ray from its origin,
							// so depth = ray_length - distance is positive while penetrating.
							const float penetration_depth = ray_length - ray_hit.block.distance;

							if (penetration_depth - min_contact_depth > MIN_PENETRATION_THRESHOLD) {
								penetrating_in_this_iter = true;
								// Godot's separation-ray contact
								// (GodotCollisionSolver3D::solve_separation_ray): with
								// slide_on_slope the recovery pushes along the surface
								// normal; without it the contact normal is the negated
								// RAY direction, so a character resting on the ray does
								// not slide down slopes.
								const physx::PxVec3 push_dir_px = sep_ray->get_slide_on_slope()
										? ray_hit.block.normal
										: -px_shape_dir;
								const Vector3 push_dir(push_dir_px.x, push_dir_px.y, push_dir_px.z);
								_recover_scratch.push_back({ push_dir, penetration_depth - min_contact_depth,
										_collision_priority_of(ray_hit.block.actor) });
							}
						}
					}
					continue;
				}
			}

			// Regular shapes: overlap-based recovery
			physx::PxGeometryHolder body_geom = body_shape->getGeometry();
			physx::PxTransform global_shape_pose = body_pose * ref.local_pose;

			constexpr int MAX_OVERLAPS = 32;
			physx::PxOverlapHit hit_buffer[MAX_OVERLAPS];
			physx::PxOverlapBuffer hits(hit_buffer, MAX_OVERLAPS);

			if (space->get_px_scene()->overlap(body_geom.any(), global_shape_pose, hits, filter_data, &filter_cb)) {
				for (physx::PxU32 i = 0; i < hits.getNbAnyHits(); ++i) {
					const physx::PxOverlapHit &overlap = hits.getAnyHit(i);

					physx::PxGeometryHolder hit_geom = overlap.shape->getGeometry();
					physx::PxTransform hit_pose = physx::PxShapeExt::getGlobalPose(*overlap.shape, *overlap.actor);

					physx::PxVec3 mtd_dir;
					physx::PxReal penetration_depth = 0.0f;

					bool is_pen = physx::PxGeometryQuery::computePenetration(
							mtd_dir, penetration_depth,
							body_geom.any(), global_shape_pose,
							hit_geom.any(), hit_pose);

					// Penetration shallower than the margin-derived slack is
					// rest separation, not a stuck body -- leave it alone.
					const float effective_depth = penetration_depth - min_contact_depth;
					if (is_pen && effective_depth > MIN_PENETRATION_THRESHOLD) {
						// One-sided concave meshes (backface_collision off) are
						// exempt from back-side recovery (see
						// _skip_one_sided_overlap).
						const physx::PxVec3 px_motion_dir(p_motion_dir.x, p_motion_dir.y, p_motion_dir.z);
						if (overlap.shape && _skip_one_sided_overlap(ref.blueprint, overlap.shape, physx::PxShapeExt::getGlobalPose(*overlap.shape, *overlap.actor), penetration_depth, mtd_dir, px_motion_dir, body_geom.any(), global_shape_pose)) {
							continue;
						}
						penetrating_in_this_iter = true;
						Vector3 recovery_dir(mtd_dir.x, mtd_dir.y, mtd_dir.z);
						_recover_scratch.push_back({ recovery_dir, effective_depth,
								_collision_priority_of(overlap.actor) });
					}
				}
			}
		}

		if (!penetrating_in_this_iter || _recover_scratch.is_empty()) {
			break;
		}
		recovered = true;

		// Godot weights each contact by the collider body's collision_priority,
		// normalized so the average contact weights 1
		// (godot_space_3d: inv_total_weight = amount / total_weight, 1.0 when
		// the total is zero). Each contact then applies 40% of its remaining
		// MTD along the push-out direction.
		real_t total_weight = 0.0;
		for (physx::PxU32 c = 0; c < _recover_scratch.size(); ++c) {
			total_weight += _recover_scratch[c].weight;
		}
		const real_t inv_total_weight = Math::is_zero_approx(total_weight)
				? 1.0
				: (real_t)_recover_scratch.size() / total_weight;

		Vector3 step_recovery;
		for (physx::PxU32 c = 0; c < _recover_scratch.size(); ++c) {
			const RecoverContact &contact = _recover_scratch[c];
			// Reference projection (godot_space_3d): each contact contributes
			// against the recovery accumulated SO FAR — recover shrinks the
			// residual of contacts it already moved away from, so contact
			// order cannot double-count or cancel weighted contributions.
			const real_t residual = contact.depth - contact.normal.dot(step_recovery);
			if (residual > MIN_PENETRATION_THRESHOLD) {
				step_recovery += contact.normal * (residual * recovery_scale * contact.weight * inv_total_weight);
			}
		}

		if (step_recovery.length_squared() < MIN_PENETRATION_THRESHOLD) {
			// Weighted contacts cancelled out (e.g. all priorities zero, the
			// reference's recover_motion == Vector3() stop).
			break;
		}

		total_recovery += step_recovery;
	}

	r_recovery = total_recovery;
	return recovered;
}

// One-sided concave meshes (backface_collision off) never block from their
// back side: PhysX's overlap/MTD treats every mesh triangle two-sided, so a
// body jumping up through a one-way platform, or resting against a wall's
// back, reads as stuck/overlapping on a face whose front points away from
// the approach. Skipped when (a) the mover is leaving along the push, or
// (b) the overlap is deep and the mover's query-pose origin is behind EVERY
// overlapped triangle (it approached from the back).
bool PhysXDirectSpaceState3D::_body_motion_cast(const PhysXBody3D &p_body, const Transform3D &p_transform,
		const Vector3 &p_motion, bool p_collide_separation_ray, float p_rest_slack, const LocalVector<MotionShapeRef> &p_shapes,
		const HashSet<RID> &p_self_and_excluded, const HashSet<ObjectID> &p_excluded_objects,
		real_t &r_safe_fraction, real_t &r_unsafe_fraction, Vector3 &r_hit_position, Vector3 &r_hit_normal, real_t &r_hit_depth, int &r_hit_local_shape,
		const physx::PxRigidActor *&r_hit_actor, const physx::PxShape *&r_hit_shape) const {
	r_safe_fraction = 1.0;
	r_unsafe_fraction = 1.0;
	r_hit_depth = 0.0;
	r_hit_local_shape = -1;

	if (!space || !space->get_px_scene() || p_shapes.is_empty()) {
		return false;
	}

	const physx::PxU32 nb_shapes = p_shapes.size();

	Vector3 dir = p_motion;
	real_t motion_length = dir.length();
	if (motion_length == 0.0) {
		return false;
	}
	dir /= motion_length;
	physx::PxVec3 px_dir(dir.x, dir.y, dir.z);

	// Setup filter callback
	PhysXQueryFilterCallback filter_cb;
	filter_cb.collision_mask = p_body.get_collision_mask();
	filter_cb.collide_with_bodies = true;
	filter_cb.collide_with_areas = false;
	filter_cb.exclude_rids = &p_self_and_excluded;
	filter_cb.exclude_objects = &p_excluded_objects;
	filter_cb.motion_body = &p_body;
	filter_cb.multi_hit = true; ///< collect all touched shapes; overlapped objects are skipped per-object below

	physx::PxQueryFilterData filter_data;
	filter_data.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER;

	// eMTD makes initial-contact sweeps report a well-defined MTD distance
	// (negative while penetrating) together with the MTD normal/position,
	// instead of distance==0 with undefined fields. The stuck handling below
	// relies on both being well-defined.
	physx::PxHitFlags sweep_hit_flags = physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL | physx::PxHitFlag::eFACE_INDEX | physx::PxHitFlag::eMTD;

	bool hit_any = false;
	float min_hit_distance = motion_length;
	int min_hit_shape = -1;
	Vector3 best_position;
	Vector3 best_normal;
	const physx::PxRigidActor *best_actor = nullptr;
	const physx::PxShape *best_shape = nullptr;

	// Godot's stuck semantics (godot_space_3d test_body_motion): when a mover
	// shape still overlaps an object at the (recovered) start pose, the motion
	// is fully blocked — safe = unsafe = 0. Shapes the mover merely starts
	// inside of are otherwise DISREGARDED per object, so a forward blocker
	// behind the overlapped one still bounds the motion.
	bool stuck = false;
	real_t deepest_overlap = 0.0f;
	const physx::PxSweepHit *stuck_hit = nullptr;
	int stuck_shape = -1;

	// The pose is constant through the cast phase (transform + quaternion).
	const Quaternion cast_q = p_transform.basis.get_rotation_quaternion();
	const physx::PxTransform body_pose(
			physx::PxVec3(p_transform.origin.x, p_transform.origin.y, p_transform.origin.z),
			physx::PxQuat(cast_q.x, cast_q.y, cast_q.z, cast_q.w));

	for (physx::PxU32 s = 0; s < nb_shapes; ++s) {
		const MotionShapeRef &shape_ref = p_shapes[s];
		physx::PxShape *shape = shape_ref.px_shape;

		// Separation rays are not swept as a volume in the cast phase — they
		// participate only in recover/collide. Godot's contract
		// (godot_space_3d test_body_motion): they are skipped only when
		// collide_separation_ray is off AND slide_on_slope is off — with the
		// flag on, the shape acts like a regular shape so the body can snap to
		// the ground. When we fall through to the generic sweep we use the
		// ray's thin-box representation, because a downward raycast distance
		// is not comparable to a horizontal sweep distance and would
		// incorrectly clamp horizontal motion.
		if (shape_ref.blueprint) {
			const PhysXShape3D *shape_bp = shape_ref.blueprint;
			if (shape_bp && shape_bp->get_type() == PhysicsServer3D::SHAPE_SEPARATION_RAY) {
				const PhysXSeparationRayShape3D *sep_ray = static_cast<const PhysXSeparationRayShape3D *>(shape_bp);
				if (!p_collide_separation_ray && !sep_ray->get_slide_on_slope()) {
					continue;
				}
				// fall through to the generic sweep below
			}
		}

		// Regular shapes: touch-capable sweep (see cast_motion for why a
		// single closest-block sweep cannot express the Godot contract).
		physx::PxGeometryHolder geom = shape->getGeometry();
		physx::PxTransform shape_pose = body_pose * shape_ref.local_pose;

		const physx::PxU32 touch_max = PHYSX_QUERY_MAX_RESULTS;
		if (_sweep_touch_scratch.size() < touch_max) {
			_sweep_touch_scratch.resize(touch_max);
		}
		physx::PxHitBuffer<physx::PxSweepHit> sweep_hit(_sweep_touch_scratch.ptr(), touch_max);
		space->get_px_scene()->sweep(geom.any(), shape_pose, px_dir, motion_length, sweep_hit, sweep_hit_flags, filter_data, &filter_cb);

		bool shape_overlapped = false;
		for (physx::PxU32 i = 0; i < sweep_hit.nbTouches; ++i) {
			const physx::PxSweepHit &touch = sweep_hit.getTouch(i);

			if (touch.hadInitialOverlap()) {
				// Overlaps within the recovery slack are rest separation (the
				// recovery floor leaves bodies at this depth on purpose) —
				// disregard them exactly like godot_physics's post-ejection
				// state. Deeper survivors mean the ejection failed: the body
				// is stuck. (godot_physics never reaches this state for
				// slack-level overlaps because its margin-inflated recovery
				// ejects to margin separation; see the documented module gap
				// on query-margin inflation.)
				// One-sided concave meshes (backface_collision off) are exempt
				// from the stuck verdict when approached from their back (see
				// _skip_one_sided_overlap).
				if (-touch.distance > p_rest_slack && touch.actor && touch.shape &&
						_skip_one_sided_overlap(shape_ref.blueprint, touch.shape,
								physx::PxShapeExt::getGlobalPose(*touch.shape, *touch.actor),
								-touch.distance, touch.normal, px_dir, geom.any(), shape_pose)) {
					continue;
				}
				if (-touch.distance > p_rest_slack) {
					shape_overlapped = true;
					if (!stuck_hit || touch.distance < deepest_overlap) {
						deepest_overlap = touch.distance;
						stuck_hit = &touch;
						stuck_shape = (int)s;
					}
				}
				continue;
			}

			physx::PxVec3 face_normal;
			if (!_sweep_forward_hit_blocking(touch, face_normal)) {
				continue; // mesh edge artifact: not a forward block
			}
			// One-sided concave meshes (backface_collision off): a sweep hit
			// whose triangle front normal points ALONG the motion is a hit on
			// the face's BACK (the mover approached from behind) — Godot's
			// one-sided trimeshes don't block from the back, so disregard it.
			if (shape_ref.blueprint &&
					shape_ref.blueprint->get_type() == PhysicsServer3D::SHAPE_CONCAVE_POLYGON &&
					!shape_ref.blueprint->has_backface_collision() &&
					face_normal.dot(px_dir) > 0.0f) {
				continue; // back-face hit on a one-sided mesh: not a block
			}

			if (touch.distance <= min_hit_distance) {
				min_hit_distance = touch.distance;
				hit_any = true;
				min_hit_shape = (int)s;
				best_position = Vector3(touch.position.x, touch.position.y, touch.position.z);
				best_normal = Vector3(face_normal.x, face_normal.y, face_normal.z);
				best_actor = touch.actor;
				best_shape = touch.shape;
			}
		}
		stuck = stuck || shape_overlapped;
	}

	if (stuck) {
		// Godot: safe = unsafe = 0, the stuck mover shape is the reported
		// local shape. Report the deepest surviving overlap (its eMTD fields
		// are well-defined); the caller's collide phase adds the full contact
		// manifold, and _fill_collision_from_sweep covers computePenetration-
		// rejected trimesh/heightfield contacts from this hit.
		r_safe_fraction = 0.0;
		r_unsafe_fraction = 0.0;
		if (stuck_hit) {
			r_hit_position = Vector3(stuck_hit->position.x, stuck_hit->position.y, stuck_hit->position.z);
			r_hit_normal = Vector3(stuck_hit->normal.x, stuck_hit->normal.y, stuck_hit->normal.z);
			r_hit_depth = MAX(0.0, -(real_t)stuck_hit->distance);
			r_hit_local_shape = stuck_shape;
			r_hit_actor = stuck_hit->actor;
			r_hit_shape = stuck_hit->shape;
		}
		return true;
	}

	if (hit_any) {
		real_t hit_fraction = CLAMP(min_hit_distance / motion_length, 0.0, 1.0);

		r_unsafe_fraction = hit_fraction;
		// Back off slightly from the exact collision point to prevent starting inside geometry on the next frame
		r_safe_fraction = MAX(0.0, hit_fraction - (1e-4f / motion_length));
		r_hit_position = best_position;
		r_hit_normal = best_normal;
		r_hit_depth = 0.0;
		r_hit_local_shape = min_hit_shape;
		r_hit_actor = best_actor;
		r_hit_shape = best_shape;
		return true;
	}
	return false;
}

bool PhysXDirectSpaceState3D::_body_motion_collide(const PhysXBody3D &p_body, const Transform3D &p_transform, const Vector3 &p_motion,
		int p_max_collisions, float p_min_allowed_depth, const LocalVector<MotionShapeRef> &p_shapes,
		const HashSet<RID> &p_self_and_excluded, const HashSet<ObjectID> &p_excluded_objects,
		PhysicsServer3D::MotionResult *r_result) const {
	if (!r_result || !space || !space->get_px_scene() || p_max_collisions <= 0 || p_shapes.is_empty()) {
		return false;
	}

	const physx::PxU32 nb_shapes = p_shapes.size();

	// Filter setup
	PhysXQueryFilterCallback filter_cb;
	filter_cb.collision_mask = p_body.get_collision_mask();
	filter_cb.collide_with_bodies = true;
	filter_cb.collide_with_areas = false;
	filter_cb.exclude_rids = &p_self_and_excluded;
	filter_cb.exclude_objects = &p_excluded_objects;
	filter_cb.motion_body = &p_body;
	filter_cb.multi_hit = true; ///< _body_motion_collide: collect all overlaps

	physx::PxQueryFilterData filter_data;
	filter_data.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER;

	// Godot's contact gather keeps the DEEPEST contacts first
	// (godot_space_3d _rest_cbk_result) and discards contacts shallower than
	// min_allowed_depth (rest separation, not a collision). Stage every
	// contact, then sort deepest-first and report the top max_collisions.
	r_result->collision_count = 0;
	_collide_scratch.clear();

	const int max_cols = MIN(p_max_collisions, PhysicsServer3D::MotionResult::MAX_COLLISIONS);

	// The pose is constant through the collide phase (transform + quaternion).
	const Quaternion collide_q = p_transform.basis.get_rotation_quaternion();
	const physx::PxTransform body_pose(
			physx::PxVec3(p_transform.origin.x, p_transform.origin.y, p_transform.origin.z),
			physx::PxQuat(collide_q.x, collide_q.y, collide_q.z, collide_q.w));

	for (physx::PxU32 s = 0; s < nb_shapes; ++s) {
		const MotionShapeRef &shape_ref = p_shapes[s];
		physx::PxShape *body_shape = shape_ref.px_shape;

		// Handle separation-ray shapes in collide phase (when collide_separation_ray is true)
		if (shape_ref.blueprint) {
			const PhysXShape3D *shape_bp = shape_ref.blueprint;
			if (shape_bp && shape_bp->get_type() == PhysicsServer3D::SHAPE_SEPARATION_RAY) {
				const PhysXSeparationRayShape3D *sep_ray = static_cast<const PhysXSeparationRayShape3D *>(shape_bp);
				// Scaled ray length, as in the recover phase.
				const real_t ray_length = sep_ray->get_length() * p_body.get_body_scale().z;

				physx::PxTransform shape_pose = body_pose * shape_ref.local_pose;

				// Ray direction = the SHAPE's composed +Z axis (see the
				// recover phase: the placement rotation lives in
				// ref.local_pose, not in the body transform).
				const physx::PxVec3 px_shape_dir = shape_pose.q.rotate(physx::PxVec3(0, 0, 1));

				// Adjust the ray origin to the base of the separation-ray shape
				// (see the recover phase: the local pose carries the forward
				// scaled half-length offset).
				shape_pose.p -= px_shape_dir * (ray_length * 0.5f);

				// Single-hit callback (see the recover phase): the shared
				// multi_hit callback's eTOUCH hits fall out of a default
				// PxRaycastBuffer.
				PhysXQueryFilterCallback ray_filter_cb = filter_cb;
				ray_filter_cb.multi_hit = false;
				physx::PxRaycastBuffer ray_hit;
				if (space->get_px_scene()->raycast(shape_pose.p, px_shape_dir, ray_length, ray_hit, physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL, filter_data, &ray_filter_cb)) {
					if (ray_hit.hasBlock) {
						PhysicsServer3D::MotionCollision col;

						// Same normal contract as the recover phase (Godot's
						// solve_separation_ray): surface normal with
						// slide_on_slope, negated ray direction without it.
						const physx::PxVec3 normal_px = sep_ray->get_slide_on_slope()
								? ray_hit.block.normal
								: -px_shape_dir;
						col.position = Vector3(ray_hit.block.position.x, ray_hit.block.position.y, ray_hit.block.position.z);
						col.normal = Vector3(normal_px.x, normal_px.y, normal_px.z);
						// Depth = how far the ray tip has passed the surface (positive while penetrating).
						col.depth = ray_length - ray_hit.block.distance;

						if (ray_hit.block.actor && ray_hit.block.actor->userData) {
							auto *actor_data = static_cast<PhysXActorUserData *>(ray_hit.block.actor->userData);
							col.collider = actor_data->rid;
							col.collider_id = actor_data->object_id;
						} else {
							col.collider = RID();
							col.collider_id = ObjectID();
						}

						if (ray_hit.block.shape && ray_hit.block.shape->userData) {
							col.collider_shape = physx_resolve_shape_index(ray_hit.block.actor, ray_hit.block.shape);
						} else {
							col.collider_shape = 0;
						}

						col.local_shape = (int)s;

						if (ray_hit.block.actor && ray_hit.block.actor->is<physx::PxRigidDynamic>()) {
							physx::PxRigidDynamic *dyn = static_cast<physx::PxRigidDynamic *>(ray_hit.block.actor);
							const physx::PxVec3 v = physx::PxRigidBodyExt::getVelocityAtPos(*dyn, ray_hit.block.position);
							col.collider_velocity = Vector3(v.x, v.y, v.z);
							const physx::PxVec3 ang_vel = dyn->getAngularVelocity();
							col.collider_angular_velocity = Vector3(ang_vel.x, ang_vel.y, ang_vel.z);
						} else {
							col.collider_velocity = Vector3();
							col.collider_angular_velocity = Vector3();
						}
						_collide_scratch.push_back(col);
					}
				}
				continue;
			}
		}

		// Regular shapes: overlap-based collide
		physx::PxGeometryHolder body_geom = body_shape->getGeometry();

		physx::PxTransform global_shape_pose = body_pose * shape_ref.local_pose;

		constexpr int MAX_OVERLAPS = 16;
		physx::PxOverlapHit hit_buffer[MAX_OVERLAPS];
		physx::PxOverlapBuffer hits(hit_buffer, MAX_OVERLAPS);

		if (space->get_px_scene()->overlap(body_geom.any(), global_shape_pose, hits, filter_data, &filter_cb)) {
			for (physx::PxU32 i = 0; i < hits.getNbAnyHits(); ++i) {
				const physx::PxOverlapHit &overlap = hits.getAnyHit(i);

				physx::PxGeometryHolder hit_geom = overlap.shape->getGeometry();
				physx::PxTransform hit_pose = physx::PxShapeExt::getGlobalPose(*overlap.shape, *overlap.actor);

				physx::PxVec3 mtd_dir;
				physx::PxReal penetration_depth = 0.0f;

				bool is_pen = physx::PxGeometryQuery::computePenetration(
						mtd_dir, penetration_depth,
						body_geom.any(), global_shape_pose,
						hit_geom.any(), hit_pose);

				// Godot: contacts shallower than min_allowed_depth are rest
				// separation, not a collision (MIN(motion_length, margin*0.05)).
				// One-sided concave meshes (backface_collision off) are exempt
				// from back-side contacts (see _skip_one_sided_overlap) -- a
				// character jumping up through a one-way platform must not be
				// caught by the platform's top face while straddling it.
				const Vector3 motion_dir_n = p_motion == Vector3() ? Vector3() : (p_motion / p_motion.length());
				const physx::PxVec3 px_motion_dir(motion_dir_n.x, motion_dir_n.y, motion_dir_n.z);
				if (is_pen && (real_t)penetration_depth >= (real_t)p_min_allowed_depth &&
						_skip_one_sided_overlap(shape_ref.blueprint, overlap.shape,
								physx::PxShapeExt::getGlobalPose(*overlap.shape, *overlap.actor),
								penetration_depth, mtd_dir, px_motion_dir, body_geom.any(), global_shape_pose)) {
					continue;
				}
				if (is_pen && (real_t)penetration_depth >= (real_t)p_min_allowed_depth) {
					PhysicsServer3D::MotionCollision col;

					col.normal = Vector3(mtd_dir.x, mtd_dir.y, mtd_dir.z);

					physx::PxVec3 contact_pt = global_shape_pose.p - (mtd_dir * (penetration_depth * 0.5f));
					col.position = Vector3(contact_pt.x, contact_pt.y, contact_pt.z);

					col.depth = penetration_depth;

					if (overlap.actor && overlap.actor->userData) {
						auto *actor_data = static_cast<PhysXActorUserData *>(overlap.actor->userData);
						col.collider = actor_data->rid;
						col.collider_id = actor_data->object_id;
					} else {
						col.collider = RID();
						col.collider_id = ObjectID();
					}

					if (overlap.shape && overlap.shape->userData) {
						col.collider_shape = physx_resolve_shape_index(overlap.actor, overlap.shape);
					} else {
						col.collider_shape = 0;
					}

					// Cached per call in the shape refs (find_shape_index is a
					// linear scan; it ran per contact before).
					const int local_idx = shape_ref.body_index;
					col.local_shape = local_idx >= 0 ? local_idx : (int)s;

					if (overlap.actor->is<physx::PxRigidDynamic>()) {
						physx::PxRigidDynamic *dyn = static_cast<physx::PxRigidDynamic *>(overlap.actor);
						// Godot reports the collider's velocity AT the contact,
						// including the angular lever arm (linear + ω×r).
						const physx::PxVec3 v = physx::PxRigidBodyExt::getVelocityAtPos(*dyn, contact_pt);
						col.collider_velocity = Vector3(v.x, v.y, v.z);
						const physx::PxVec3 ang_vel = dyn->getAngularVelocity();
						col.collider_angular_velocity = Vector3(ang_vel.x, ang_vel.y, ang_vel.z);
					} else {
						col.collider_velocity = Vector3();
						col.collider_angular_velocity = Vector3();
					}
					_collide_scratch.push_back(col);
				}
			}
		}
	}

	// Deepest contacts first, then the top max_collisions are reported.
	if (!_collide_scratch.is_empty()) {
		SortArray<PhysicsServer3D::MotionCollision, DeeperContactFirst> sorter;
		sorter.sort(_collide_scratch.ptr(), _collide_scratch.size());
	}

	const int reported = MIN((int)_collide_scratch.size(), max_cols);
	for (int i = 0; i < reported; ++i) {
		r_result->collisions[i] = _collide_scratch[i];
	}
	r_result->collision_count = reported;
	return reported > 0;
}

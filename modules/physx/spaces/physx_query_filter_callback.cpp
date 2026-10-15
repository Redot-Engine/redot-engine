/**************************************************************************/
/*  physx_query_filter_callback.cpp                                       */
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

#include "physx_query_filter_callback.h"

#include "../objects/physx_body_3d.h"
#include "../objects/physx_object_3d.h"
#include "../shapes/physx_shape_3d.h"
#include "../shapes/physx_user_data.h"

physx::PxQueryHitType::Enum PhysXQueryFilterCallback::preFilter(
		const physx::PxFilterData &filterData,
		const physx::PxShape *shape,
		const physx::PxRigidActor *actor,
		physx::PxHitFlags &queryFlags) {
	// Every rigid actor this module owns carries a PhysXActorUserData. If it
	// doesn't, the actor is not one of ours — this includes GPU deformable
	// volumes (soft bodies), articulation links, and particle systems, which
	// are BY DESIGN invisible to scene queries (raycast/overlap/sweep).
	// Consequence: soft_body_set_ray_pickable is inert — documented module
	// limitation, not a bug. NOTE (2026-09): userData wiring for GPU soft
	// bodies was ATTEMPTED and REVERTED — with the bridge attached, a raycast
	// through the tet mesh HANGS the engine (overlap queries against the same
	// mesh return fine, so the hang is in the ray-vs-tet traversal of this
	// SDK build). Keep soft bodies query-invisible until that is resolved.
	if (!actor->userData) {
		return physx::PxQueryHitType::eNONE;
	}
	const PhysXActorUserData *actor_data = static_cast<const PhysXActorUserData *>(actor->userData);
	const PhysXObject3D *obj = actor_data->object;
	if (!obj) {
		return physx::PxQueryHitType::eNONE;
	}

	// Godot excludes separation-ray shapes from scene queries entirely: they
	// only participate in the owning body's test_body_motion (recover/collide,
	// which issues its own raycasts), never as a query target. Without this,
	// a raycast/overlap/sweep reports the thin box that emulates the ray.
	// (Mirrors the type check in the _body_motion_cast path.)
	if (shape->userData) {
		const PhysXShape3D *shape_blueprint = static_cast<const PhysXShape3D *>(shape->userData);
		if (shape_blueprint && shape_blueprint->get_type() == PhysicsServer3D::SHAPE_SEPARATION_RAY) {
			return physx::PxQueryHitType::eNONE;
		}
	}

	// Godot semantics: query mask AND object layer must be nonzero to collide.
	if ((collision_mask & obj->get_collision_layer()) == 0) {
		return physx::PxQueryHitType::eNONE;
	}

	// Bodies vs areas toggle. Soft bodies count as bodies for queries (they
	// are PhysicsBody3Ds in Godot's scene tree).
	const PhysXObject3D::ObjectType type = obj->get_type();
	if (type == PhysXObject3D::OBJECT_TYPE_AREA && !collide_with_areas) {
		return physx::PxQueryHitType::eNONE;
	}
	if (type == PhysXObject3D::OBJECT_TYPE_BODY && !collide_with_bodies) {
		return physx::PxQueryHitType::eNONE;
	}
	if (type == PhysXObject3D::OBJECT_TYPE_SOFT_BODY && !collide_with_bodies) {
		return physx::PxQueryHitType::eNONE;
	}

	// Raycasts honor ray_pickable (intersect_ray sets pick_ray; all other
	// queries leave it false and never consult the flag).
	if (pick_ray && !obj->is_ray_pickable()) {
		return physx::PxQueryHitType::eNONE;
	}

	// Collision exceptions (body_test_motion paths, mirroring godot's
	// _cull_aabb_for_body): skip bodies excepted against the moving body.
	if (motion_body && type == PhysXObject3D::OBJECT_TYPE_BODY) {
		const PhysXBody3D *other = static_cast<const PhysXBody3D *>(obj);
		if (other->get_collision_exception_set().has(motion_body->get_rid()) ||
				motion_body->get_collision_exception_set().has(other->get_rid())) {
			return physx::PxQueryHitType::eNONE;
		}
	}

	// Exclusion lists.
	if (exclude_rids && exclude_rids->has(obj->get_rid())) {
		return physx::PxQueryHitType::eNONE;
	}
	if (exclude_objects && exclude_objects->has(obj->get_instance_id())) {
		return physx::PxQueryHitType::eNONE;
	}

	// Multi-hit queries use eTOUCH to collect all overlaps up to buffer size;
	// single-hit queries use eBLOCK to stop at the first hit.
	return multi_hit ? physx::PxQueryHitType::eTOUCH : physx::PxQueryHitType::eBLOCK;
}

physx::PxQueryHitType::Enum PhysXQueryFilterCallback::postFilter(
		const physx::PxFilterData &filterData,
		const physx::PxQueryHit &hit,
		const physx::PxShape *shape,
		const physx::PxRigidActor *actor) {
	// Raycast back-face rule (only armed by intersect_ray). Everything else
	// pre-filters all Godot semantics; no post-filter needed.
	if (ray_backface_filtering && shape && shape->userData) {
		const PhysXShape3D *shape_blueprint = static_cast<const PhysXShape3D *>(shape->userData);
		if (!shape_blueprint->has_backface_collision() && shape_blueprint->get_type() == PhysicsServer3D::SHAPE_CONCAVE_POLYGON) {
			// One-sided mesh: a hit from the back NEVER counts, even when the
			// query asked for back faces (eMESH_BOTH_SIDES made PhysX report
			// it; Godot's one-sided trimeshes stay one-sided for rays).
			// Geometric test: the hit is a back hit when the triangle's front
			// normal points along the ray.
			const physx::PxTriangleMeshGeometry &mesh_geom = static_cast<const physx::PxTriangleMeshGeometry &>(shape->getGeometry());
			const physx::PxTransform mesh_pose = actor ? actor->getGlobalPose() * shape->getLocalPose() : physx::PxTransform(physx::PxIdentity);
			physx::PxTriangle tri;
			physx::PxMeshQuery::getTriangle(mesh_geom, mesh_pose, hit.faceIndex, tri);
			physx::PxVec3 tri_n;
			tri.normal(tri_n);
			// getTriangle's normal is Godot's face front (the cook's winding
			// reversal puts the CCW normal where Godot's clockwise front
			// points). A back hit has the face front running ALONG the ray.
			const physx::PxVec3 rd(ray_dir_x, ray_dir_y, ray_dir_z);
			if (tri_n.dot(rd) > 0.0f) {
				return physx::PxQueryHitType::eNONE;
			}
			return physx::PxQueryHitType::eBLOCK;
		}
		if (shape_blueprint->has_backface_collision()) {
			// The doubled cook fronts every triangle twice: the flipped copy
			// is a BACK-face hit and only counts when the query asked for it.
			// (Redot interleaves the copies -- original at even cooked
			// indices, flipped at odd -- so the parity of the cooked face
			// index tells them apart; translate_face_index() maps both to the
			// same source face for the reported face_index.)
			const bool flipped_copy = (hit.faceIndex & 1) != 0;
			if (flipped_copy && !ray_hit_back_faces) {
				return physx::PxQueryHitType::eNONE;
			}
		}
	}
	return physx::PxQueryHitType::eBLOCK;
}

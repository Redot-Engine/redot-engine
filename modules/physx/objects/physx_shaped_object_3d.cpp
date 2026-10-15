/**************************************************************************/
/*  physx_shaped_object_3d.cpp                                            */
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

#include "physx_shaped_object_3d.h"

// --- Includes ---
#include "../physx_conversions.h"
#include "../spaces/physx_filter_shader.h"
#include "../spaces/physx_space_3d.h"
#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "physx_server.h"

// Applies the space-mapped rest offset (SPACE_PARAM_CONTACT_MAX_ALLOWED_
// PENETRATION) to a freshly created PxShape: PhysX bodies rest at the SUM of
// a pair's rest offsets, so each shape carries half the allowed penetration.
// restOffset must stay below contactOffset (SDK validation), so the contact
// generation distance is raised to keep the margin as the gap above it. An
// unset value (0.0 — the default) leaves the PhysX defaults untouched, so
// default resting behavior is unchanged.
void physx_apply_space_rest_offset(physx::PxShape *p_px_shape, float p_margin, const PhysXSpace3D *p_space) {
	if (!p_px_shape || !p_space) {
		return;
	}
	const float rest = (float)p_space->get_shape_rest_offset();
	if (rest <= 0.0f) {
		return;
	}
	p_px_shape->setContactOffset(rest + p_margin);
	p_px_shape->setRestOffset(rest);
}

PhysXShapedObject3D::PhysXShapedObject3D(ObjectType p_type) :
		PhysXObject3D(p_type) {
}

PhysXShapedObject3D::~PhysXShapedObject3D() {
	// Clean up all shapes securely
	while (attached_shapes.size() > 0) {
		remove_shape(0);
	}
	// Release the per-object material if one was created (bodies only).
	if (px_material) {
		px_material->release();
		px_material = nullptr;
	}
}

physx::PxMaterial *PhysXShapedObject3D::_get_shape_material() {
	// Areas use the shared server default material.
	return PhysXServer3D::get_singleton()->get_default_material();
}

physx::PxTransform PhysXShapedObject3D::to_physx_transform(const Transform3D &p_transform) {
	// Shared module conversion (physx_conversions.h): clamps degenerate/NaN
	// transforms to identity rotation instead of passing them into PhysX.
	return godot_to_px(p_transform);
}

Vector3 PhysXShapedObject3D::_shape_geometry_scale(const Transform3D &p_shape_transform) const {
	// Per-shape transform scale (mirrored scales are absorbed with abs() -
	// PhysX geometries do not support negative scales) composed with the
	// object's body scale.
	const Vector3 shape_scale = p_shape_transform.basis.get_scale_abs();
	return Vector3(
			body_scale.x * shape_scale.x,
			body_scale.y * shape_scale.y,
			body_scale.z * shape_scale.z);
}

Vector3 PhysXShapedObject3D::_shape_geometry_scale_signed(const Transform3D &p_shape_transform) const {
	// Zero determinant is degenerate (SIGN(0) would zero the whole scale);
	// keep the absolute path there.
	if (Math::is_zero_approx(p_shape_transform.basis.determinant())) {
		return _shape_geometry_scale(p_shape_transform);
	}
	// Godot's signed scale (determinant sign applied uniformly). Paired with
	// get_rotation_quaternion() — which absorbs the same sign as (-1,-1,-1) —
	// pose * PxMeshScale(signed) reconstructs the original mirrored basis.
	const Vector3 shape_scale = p_shape_transform.basis.get_scale();
	return Vector3(
			body_scale.x * shape_scale.x,
			body_scale.y * shape_scale.y,
			body_scale.z * shape_scale.z);
}

Vector3 PhysXShapedObject3D::_shape_geometry_scale_for(const PhysXShape3D *p_shape, const Transform3D &p_shape_transform) const {
	// Convex polygon meshes are the only consumers of the signed scale: the
	// shape bakes a negative determinant by cooking a point-reflected hull
	// (PxMeshScale rejects negative components on convex geometries). Every
	// other geometry stays on the absolute path.
	if (p_shape && p_shape->get_type() == PhysicsServer3D::SHAPE_CONVEX_POLYGON) {
		return _shape_geometry_scale_signed(p_shape_transform);
	}
	return _shape_geometry_scale(p_shape_transform);
}

// Called by the PhysXShape3D destructor: nullify the attached pointer so
// a later remove_owner() does not dereference the freed shape.
void PhysXShapedObject3D::nullify_shape(PhysXShape3D *p_shape) {
	for (AttachedShape &record : attached_shapes) {
		if (record.shape_blueprint == p_shape) {
			record.shape_blueprint = nullptr;
		}
	}
}

// Re-bakes every attached shape against the CURRENT body_scale: geometry
// (body scale * per-shape transform scale) and local-pose origins (a shape
// offset scales with the body, like a child of a scaled Godot node). Called
// when the owner's transform scale changes -- PhysX actor poses carry no
// scale, so godot_physics' "the whole body transform scales its shapes"
// behavior is produced by baking the node scale into the geometry instead of
// rebuilding the actor.
void PhysXShapedObject3D::refresh_shape_scaling() {
	for (AttachedShape &record : attached_shapes) {
		if (!record.shape_blueprint || (!record.shape_instance && !record.detection_shape)) {
			continue;
		}

		physx::PxGeometryHolder holder;
		const Vector3 geom_scale = _shape_geometry_scale_for(record.shape_blueprint, record.relative_transform);
		physx::PxVec3 scale(geom_scale.x, geom_scale.y, geom_scale.z);
		if (record.shape_blueprint->get_physx_geometry(holder, scale)) {
			if (record.shape_instance) {
				record.shape_instance->setGeometry(holder.any());
			}
			if (record.detection_shape) {
				record.detection_shape->setGeometry(holder.any());
			}
		}

		// Local pose: scaled offset, unchanged (orthonormalized) rotation,
		// composed with the shape's intrinsic alignment pose -- the same
		// composition set_shape_transform() uses.
		Transform3D final_tr = record.relative_transform;
		final_tr.origin *= body_scale;
		final_tr.basis.orthonormalize();
		const physx::PxTransform composed = to_physx_transform(final_tr) * record.shape_blueprint->get_local_pose(scale);
		if (record.shape_instance) {
			record.shape_instance->setLocalPose(composed);
		}
		if (record.detection_shape) {
			record.detection_shape->setLocalPose(composed);
		}

		if (record.shape_instance) {
			record.shape_instance->setContactOffset(record.shape_blueprint->get_margin());
			physx_apply_space_rest_offset(record.shape_instance, record.shape_blueprint->get_margin(), space);
		}
		if (record.detection_shape) {
			record.detection_shape->setContactOffset(record.shape_blueprint->get_margin());
			physx_apply_space_rest_offset(record.detection_shape, record.shape_blueprint->get_margin(), space);
		}
	}

	// Derived-class hook: bodies recompute the inertia tensor (it scales with
	// the geometry); areas have nothing to do.
	_on_shape_geometry_changed();

	// Wake dynamic bodies -- the mass distribution changed. Kinematic dynamics
	// never sleep and reject wakeUp() ("Body must be non-kinematic!").
	if (px_actor) {
		if (physx::PxRigidDynamic *dyn = px_actor->is<physx::PxRigidDynamic>()) {
			if (!(dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC)) {
				dyn->wakeUp();
			}
		}
	}
}

void PhysXShapedObject3D::detach_shape(PhysXShape3D *p_shape) {
	// Detaches PxShape instances from the actor (broadphase mutation) — fetch
	// an in-flight solve first (async stepping).
	if (space) {
		space->ensure_synced();
	}
	for (int i = attached_shapes.size() - 1; i >= 0; i--) {
		AttachedShape &record = attached_shapes[i];
		if (record.shape_blueprint == p_shape && (record.shape_instance || record.detection_shape)) {
			// Detach from the actor (destroying the PxShape instance(s)).
			if (px_actor) {
				physx::PxRigidActor *rigid_actor = px_actor->is<physx::PxRigidActor>();
				if (rigid_actor) {
					if (record.shape_instance) {
						rigid_actor->detachShape(*record.shape_instance);
					}
					if (record.detection_shape) {
						rigid_actor->detachShape(*record.detection_shape);
					}
				}
			}
			record.shape_instance = nullptr;
			record.detection_shape = nullptr;
			record.shape_blueprint = nullptr;
		}
	}
}

// Called by PhysXShape3D when its geometry changes (e.g. editor resize):
// updates every attached instance's PxGeometry in place (PxShape::
// setGeometry() is mutable -- no detach/re-attach), respecting the current
// body scale. Dynamic bodies are woken since the inertia tensor may have
// changed.
void PhysXShapedObject3D::shape_changed(PhysXShape3D *p_shape) {
	for (AttachedShape &record : attached_shapes) {
		if (record.shape_blueprint == p_shape && (record.shape_instance || record.detection_shape)) {
			// Rebuild the geometry with the current body scale AND the
			// per-shape transform scale, and refresh the contact offset (the
			// margin is stored per PxShape, not per shape_blueprint).
			physx::PxGeometryHolder holder;
			const Vector3 geom_scale = _shape_geometry_scale_for(p_shape, record.relative_transform);
			physx::PxVec3 scale(geom_scale.x, geom_scale.y, geom_scale.z);
			if (p_shape->get_physx_geometry(holder, scale)) {
				if (record.shape_instance) {
					record.shape_instance->setGeometry(holder.any());
				}
				if (record.detection_shape) {
					record.detection_shape->setGeometry(holder.any());
				}
			}
			if (record.shape_instance) {
				record.shape_instance->setContactOffset(p_shape->get_margin());
				// Re-apply the space rest offset: setContactOffset above resets
				// the generation distance to the margin, which would leave
				// restOffset >= contactOffset (invalid) when the space mapped
				// CONTACT_MAX_ALLOWED_PENETRATION onto this shape.
				physx_apply_space_rest_offset(record.shape_instance, p_shape->get_margin(), space);
			}
			if (record.detection_shape) {
				record.detection_shape->setContactOffset(p_shape->get_margin());
				physx_apply_space_rest_offset(record.detection_shape, p_shape->get_margin(), space);
			}
		}
	}

	// Notify derived classes (e.g. PhysXBody3D) that a shape's geometry changed,
	// so they can update their inertia tensors if needed.
	_on_shape_geometry_changed();

	// Wake dynamic bodies — the inertia tensor may need recomputing.
	// Kinematic dynamics never sleep and reject wakeUp() ("Body must be
	// non-kinematic!"), so they are skipped.
	if (px_actor) {
		if (physx::PxRigidDynamic *dyn = px_actor->is<physx::PxRigidDynamic>()) {
			if (!(dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC)) {
				dyn->wakeUp();
			}
		}
	}
}

// Attaches a shape blueprint to this object: creates the PxShape instance
// with the body's current scale, positions it via its local pose, attaches
// it, then releases our reference -- the actor owns the instance from that
// point (refcount drops from 2 to 1).
void PhysXShapedObject3D::add_shape(PhysXShape3D *p_shape, const Transform3D &p_transform, bool p_disabled) {
	// attachShape() below mutates the scene's broadphase — forbidden while a
	// solve is in flight (async stepping). Fetch first; mutating frames lose
	// the async overlap by design.
	if (space) {
		space->ensure_synced();
	}
	AttachedShape record;
	record.shape_blueprint = p_shape;
	record.relative_transform = p_transform;
	record.disabled = p_disabled;

	if (!p_shape) {
		attached_shapes.push_back(record);
		return;
	}

	// Register as an observer so shape_changed() fires when the resource is edited.
	p_shape->add_owner(this);

	if (px_actor) {
		// Body scale AND per-shape transform scale are baked into the geometry
		// (e.g. a 1x1 trimesh quad whose shape offset scales it (50,1,50) into a
		// floor -- the standard Godot idiom). Convex polygon meshes additionally
		// receive the signed (mirror-capable) scale.
		const Vector3 geom_scale = _shape_geometry_scale_for(p_shape, p_transform);
		physx::PxVec3 total_scale(geom_scale.x, geom_scale.y, geom_scale.z);
		physx::PxPhysics &physics = PhysXServer3D::get_singleton()->get_physics();

		// Determine if this is a concave shape being attached to a non-kinematic
		// dynamic body. PhysX forbids attaching triangle meshes, heightfields,
		// and planes to dynamic actors — attachShape() will fail and leave the
		// actor in an inconsistent state (subsequent server calls that dereference
		// the half-attached shape segfault). For such combinations, we create the
		// shape with eSCENE_QUERY_SHAPE only (no simulation) so the shape still
		// participates in raycasts/overlaps but never collides.
		physx::PxShapeFlags shape_flags = physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE | physx::PxShapeFlag::eSIMULATION_SHAPE;

		// Separation-ray shapes are QUERY-ONLY on every body: Godot's ray
		// shapes never generate simulation contacts — the ray participates
		// only in its owner's motion queries (which raycast/sweep the
		// geometry directly, not through this attached PxShape). As a
		// simulation shape the emulated thin box produced real contacts
		// Godot rays do not.
		if (p_shape->get_type() == PhysicsServer3D::SHAPE_SEPARATION_RAY) {
			shape_flags = physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE;
		}
		// Check: is the shape concave? (triangle mesh / heightfield / plane)
		// Concave geometries on a non-kinematic dynamic body are created
		// query-only: PhysX forbids those simulation shapes there.
		else if (!p_shape->is_convex()) {
			// Check: is the actor a non-kinematic dynamic body?
			physx::PxRigidDynamic *dyn = px_actor->is<physx::PxRigidDynamic>();
			if (dyn && !(dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC)) {
				shape_flags = physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE;
			}
		}

		// Godot passes the shape owner's disabled state at add time
		// (CollisionObject3D::shape_owner_add_shape forwards it). A disabled
		// shape must attach inert — no simulation, no queries — exactly as a
		// later set_shape_disabled(idx, true) would leave it; without this the
		// toggle can never fire because the wrapper already records the flag.
		if (p_disabled) {
			shape_flags = physx::PxShapeFlag::eVISUALIZATION;
		}

		record.shape_instance = p_shape->create_shape(physics, total_scale, _get_shape_material(), shape_flags);

		if (record.shape_instance) {
			// Set the shape's local pose (placement * shape alignment). The placement
			// is body-relative with its origin scaled by the body scale (a child
			// offset sits at offset*scale, matching a scaled Godot node); the
			// alignment is the shape's intrinsic axis rotation (e.g. capsule Z-90 to
			// map PhysX's X-axis to Godot's Y-axis). create_shape() applied the
			// alignment once already; compose both so neither is lost.
			Transform3D placed_tr = p_transform;
			placed_tr.origin *= body_scale;
			const physx::PxTransform composed = to_physx_transform(placed_tr) * p_shape->get_local_pose(total_scale);
			record.shape_instance->setLocalPose(composed);

			// Apply the owner's collision filter BEFORE attaching: PxShape creation
			// leaves the filter data zeroed (layer=0, mask=0), which the simulation
			// filter shader treats as "collides with nothing" -- without this, bodies
			// fall through everything.
			physx::PxFilterData filter_data;
			filter_data.word0 = collision_layer;
			filter_data.word1 = collision_mask;
			filter_data.word3 = shape_filter_flags();
			record.shape_instance->setSimulationFilterData(filter_data);
			record.shape_instance->setQueryFilterData(filter_data);
			physx_apply_space_rest_offset(record.shape_instance, p_shape->get_margin(), space);

			// Attach to the actor. attachShape() returns void, not a status: for the
			// concave-on-dynamic case it succeeds but the simulation silently ignores
			// the shape. Attach anyway so the actor's shape list stays consistent.
			if (physx::PxRigidActor *rigid_actor = px_actor->is<physx::PxRigidActor>()) {
				rigid_actor->attachShape(*record.shape_instance);
			}

			// Drop our local reference — the actor now owns the shape.
			record.shape_instance->release();
		}
	}
	attached_shapes.push_back(record);

	// Notify derived classes (e.g. PhysXBody3D) that a shape was added,
	// so they can update their inertia tensors if needed.
	_on_shape_added();
}

void PhysXShapedObject3D::remove_shape(int p_index) {
	ERR_FAIL_INDEX(p_index, (int)attached_shapes.size());

	// detachShape() below removes the shape from the scene's broadphase —
	// forbidden while a solve is in flight (async stepping). Fetch first.
	if (space) {
		space->ensure_synced();
	}

	AttachedShape &record = attached_shapes[p_index];

	// A null shape_blueprint means the shape was already freed and its
	// destructor nullified this pointer to prevent use-after-free.
	if (record.shape_blueprint) {
		record.shape_blueprint->remove_owner(this);
	}

	// Detaching decrements the reference count; since release() was called
	// after attaching, this destroys the PxShape.
	if (px_actor && (record.shape_instance || record.detection_shape)) {
		physx::PxRigidActor *rigid_actor = px_actor->is<physx::PxRigidActor>();
		if (rigid_actor) {
			if (record.shape_instance) {
				rigid_actor->detachShape(*record.shape_instance);
			}
			if (record.detection_shape) {
				rigid_actor->detachShape(*record.detection_shape);
			}
		}
	}

	// Remove from vector
	attached_shapes.remove_at(p_index);

	// Notify derived classes (e.g. PhysXBody3D) that a shape was removed,
	// so they can update their inertia tensors if needed.
	_on_shape_removed();
}

void PhysXShapedObject3D::remove_shape(PhysXShape3D *p_shape) {
	// Find indices backwards to avoid invalidating iterators while removing
	for (int i = attached_shapes.size() - 1; i >= 0; i--) {
		if (attached_shapes[i].shape_blueprint == p_shape) {
			remove_shape(i);
		}
	}
}

void PhysXShapedObject3D::set_shape_transform(int p_index, const Transform3D &p_transform) {
	ERR_FAIL_INDEX(p_index, (int)attached_shapes.size());

	// The paths below issue PxShape::setGeometry/setLocalPose — forbidden
	// while a solve is in flight (async stepping; setGeometry also mutates
	// broadphase bounds). Fetch first, like the attach/detach paths above.
	if (space) {
		space->ensure_synced();
	}

	AttachedShape &record = attached_shapes[p_index];

	// A scale change must be re-baked into the geometry (PxShape::setGeometry);
	// a pure pose change only needs setLocalPose. Convex polygon shapes also
	// detect a MIRROR change (sign flip) as a geometry change.
	const Vector3 old_geom_scale = _shape_geometry_scale_for(record.shape_blueprint, record.relative_transform);
	const Vector3 new_geom_scale = _shape_geometry_scale_for(record.shape_blueprint, p_transform);
	const bool scale_changed = old_geom_scale != new_geom_scale;

	record.relative_transform = p_transform;

	if ((record.shape_instance || record.detection_shape) && record.shape_blueprint) {
		// Apply body scale to the new offset
		Transform3D final_tr = p_transform;
		final_tr.origin *= body_scale;

		// Compose placement with the shape's intrinsic alignment pose
		// (e.g. capsule Z-90°) so it isn't clobbered.
		const physx::PxTransform composed = to_physx_transform(final_tr) * record.shape_blueprint->get_local_pose(physx::PxVec3(new_geom_scale.x, new_geom_scale.y, new_geom_scale.z));

		if (scale_changed) {
			physx::PxGeometryHolder holder;
			physx::PxVec3 total_scale(new_geom_scale.x, new_geom_scale.y, new_geom_scale.z);
			if (record.shape_blueprint->get_physx_geometry(holder, total_scale)) {
				if (record.shape_instance) {
					record.shape_instance->setGeometry(holder.any());
				}
				if (record.detection_shape) {
					record.detection_shape->setGeometry(holder.any());
				}
			}
		}

		if (record.shape_instance) {
			record.shape_instance->setLocalPose(composed);
		}
		if (record.detection_shape) {
			record.detection_shape->setLocalPose(composed);
		}
	}

	// Notify derived classes (e.g. PhysXBody3D) that a shape's transform changed,
	// so they can update their inertia tensors if needed.
	_on_shape_transform_changed();
}

RID PhysXShapedObject3D::get_shape_rid(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)attached_shapes.size(), RID());
	return attached_shapes[p_index].shape_blueprint ? attached_shapes[p_index].shape_blueprint->get_rid() : RID();
}

Transform3D PhysXShapedObject3D::get_shape_transform(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, (int)attached_shapes.size(), Transform3D());
	return attached_shapes[p_index].relative_transform;
}

int PhysXShapedObject3D::find_shape_index(const physx::PxShape *p_px_shape) const {
	// Linear scan; shape counts per body are small. PxShape::userData points at
	// the shared PhysXShape3D* shape_blueprint, not a per-instance struct.
	// Areas may report their non-trigger detection shape in trigger events —
	// it maps to the same logical shape index as the trigger shape.
	for (int i = 0; i < (int)attached_shapes.size(); i++) {
		if (attached_shapes[i].shape_instance == p_px_shape || attached_shapes[i].detection_shape == p_px_shape) {
			return i;
		}
	}
	return -1;
}

void PhysXShapedObject3D::rebuild_shapes() {
	if (!px_actor) {
		return;
	}

	physx::PxRigidActor *rigid_actor = px_actor->is<physx::PxRigidActor>();
	if (!rigid_actor) {
		return;
	}

	physx::PxPhysics &physics = PhysXServer3D::get_singleton()->get_physics();
	physx::PxMaterial *material = _get_shape_material();

	// Is this actor a non-kinematic dynamic body? The per-shape is_convex()
	// check happens inside the loop (exactly like add_shape()): concave shapes
	// on such actors must be created query-only, but convex shapes must keep
	// eSIMULATION_SHAPE -- otherwise a body that went through an actor-recreating
	// mode switch (static <-> dynamic) would lose simulation on ALL of its
	// shapes and fall through the world.
	physx::PxRigidDynamic *dyn = px_actor->is<physx::PxRigidDynamic>();
	const bool dynamic_non_kinematic = dyn && !(dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC);

	for (AttachedShape &record : attached_shapes) {
		// The old shape_instance was released when the previous PxActor was released.
		record.shape_instance = nullptr;
		if (!record.shape_blueprint) {
			continue;
		}

		// Per-shape flags: only concave geometries on a non-kinematic
		// dynamic actor degrade to query-only. Separation-ray shapes are
		// query-only everywhere (see add_shape).
		physx::PxShapeFlags shape_flags = physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE | physx::PxShapeFlag::eSIMULATION_SHAPE;
		if (record.shape_blueprint->get_type() == PhysicsServer3D::SHAPE_SEPARATION_RAY) {
			shape_flags = physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE;
		} else if (dynamic_non_kinematic && !record.shape_blueprint->is_convex()) {
			shape_flags = physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE;
		}
		// Preserve the slot's disabled state across the actor recreation — a
		// mode switch must not re-enable shapes that were added disabled (or
		// disabled via set_shape_disabled before the switch).
		if (record.disabled) {
			shape_flags = physx::PxShapeFlag::eVISUALIZATION;
		}

		// Body scale + per-shape transform scale, per record (each attached
		// shape can carry its own offset scale). Convex polygon meshes get the
		// signed scale so mirrors survive actor-recreating mode switches too.
		const Vector3 geom_scale = _shape_geometry_scale_for(record.shape_blueprint, record.relative_transform);
		physx::PxVec3 total_scale(geom_scale.x, geom_scale.y, geom_scale.z);
		record.shape_instance = record.shape_blueprint->create_shape(physics, total_scale, material, shape_flags);
		if (!record.shape_instance) {
			continue;
		}
		physx_apply_space_rest_offset(record.shape_instance, record.shape_blueprint->get_margin(), space);

		// Restore local pose (placement * intrinsic alignment).
		Transform3D final_tr = record.relative_transform;
		final_tr.origin *= body_scale;
		record.shape_instance->setLocalPose(to_physx_transform(final_tr) * record.shape_blueprint->get_local_pose(total_scale));

		// Attach to the new actor (refcount -> 2, then release drops to 1).
		rigid_actor->attachShape(*record.shape_instance);
		record.shape_instance->release();
	}

	// Re-apply collision filter data on the fresh shapes.
	update_shapes_collision_filter();

	// Notify derived classes (e.g. PhysXBody3D) that shapes were rebuilt,
	// so they can update their inertia tensors if needed.
	_on_shape_added();
}

uint32_t PhysXShapedObject3D::shape_filter_flags() const {
	uint32_t flags = 0;
	if (contact_notify) {
		flags |= PHYSX_FILTER_FLAG_CONTACT_NOTIFY;
	}
	// Areas must never generate solver contacts: the simulation filter shader
	// kills any non-trigger pair involving an area shape.
	if (type == OBJECT_TYPE_AREA) {
		flags |= PHYSX_FILTER_FLAG_IS_AREA;
	}
	return flags;
}

void PhysXShapedObject3D::update_shapes_collision_filter() {
	physx::PxFilterData filter_data;
	filter_data.word0 = collision_layer; // "I belong to this layer"
	filter_data.word1 = collision_mask; // "I collide with these layers"
	// Soft-body exception slot (0 unless this object participates in at least
	// one soft-body collision exception — see spaces/physx_filter_shader.h).
	filter_data.word2 = exception_slot;
	filter_data.word3 = shape_filter_flags();

	for (const AttachedShape &record : attached_shapes) {
		if (record.shape_instance) {
			record.shape_instance->setSimulationFilterData(filter_data);
			record.shape_instance->setQueryFilterData(filter_data);
		}
		if (record.detection_shape) {
			record.detection_shape->setSimulationFilterData(filter_data);
		}
	}
}

void PhysXShapedObject3D::_on_shape_added() {}
void PhysXShapedObject3D::_on_shape_removed() {}
void PhysXShapedObject3D::_on_shape_geometry_changed() {}
void PhysXShapedObject3D::_on_shape_transform_changed() {}

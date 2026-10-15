/**************************************************************************/
/*  physx_area_3d.cpp                                                     */
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

#include "physx_area_3d.h"
#include "../physx_server.h"
#include "../shapes/physx_shape_3d.h"
#include "../spaces/physx_space_3d.h"
#include "physx_body_3d.h"

#include "PxPhysicsAPI.h"

#include "core/math/math_defs.h"
#include "core/math/math_funcs.h"

	// Lifecycle

PhysXArea3D::PhysXArea3D() :
		PhysXShapedObject3D(OBJECT_TYPE_AREA) {
	// Areas are KINEMATIC rigid bodies, not static ones:
	// - PhysX never runs the simulation filter shader for pairs of two static
	//   rigid actors, so static-vs-static area pairs could never generate the
	//   trigger events that drive area-vs-area monitoring.
	// - Kinematic-vs-kinematic pairs are filtered normally when one side is a
	//   trigger shape, which is what areas are (PxFiltering.h).
	// Kinematic bodies are unaffected by gravity, never sleep, and are moved
	// manually via set_transform (setKinematicTarget() once in a scene) -- the semantics a Godot area needs. The detection shapes
	// (see _sync_detection_shape) make them detectable by other areas.
	physx::PxPhysics &physics = PhysXServer3D::get_singleton()->get_physics();
	physx::PxRigidDynamic *dynamic = physics.createRigidDynamic(physx::PxTransform(physx::PxIdentity));
	if (dynamic) {
		dynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
	}
	px_actor = dynamic;
	if (px_actor) {
		actor_user_data.object = this;
		actor_user_data.rid = get_rid();
		actor_user_data.object_id = get_instance_id();
		px_actor->userData = &actor_user_data;
	}
}

PhysXArea3D::~PhysXArea3D() {
	// Order matters: leave the scene, detach all shapes, THEN release the actor.
	// clear_shapes() MUST run before px_actor->release() — releasing the actor
	// destroys its attached PxShapes, so the base-class destructor's
	// remove_shape() loop would otherwise detachShape() against a freed actor.
	PhysXSpace3D *previous_space = space;
	if (previous_space) {
		set_space(nullptr);
	}

	clear_shapes();

	if (px_actor) {
		// Route the release through the space: with async stepping a
		// mid-flight area destruction queues the release until after the
		// fetch — releasing into a live solve would free an actor the solver
		// still references (see PhysXSpace3D::release_actor).
		if (previous_space) {
			previous_space->release_actor(px_actor);
		} else {
			px_actor->release();
		}
		px_actor = nullptr;
	}
}

	// Space Management

void PhysXArea3D::set_space(PhysXSpace3D *p_space) {
	if (space == p_space) {
		return;
	}

	if (space) {
	// Queue exit events for all overlapping bodies/areas before leaving the
	// space -- trigger events may not fire (e.g., when a body is freed while
	// overlapping this area), and the overlap lists must not retain dangling
	// pointers.
		for (unsigned int i = 0; i < overlapping_bodies.size(); i++) {
			PhysXBody3D *body = overlapping_bodies[i];
			// Remove the body from this area's overlap list.
			body->remove_overlapping_area(this);
			// Queue the body_monitor dispatch for flush_queries().
			if (space) {
				PhysXSpace3D::TriggerEvent ev;
				ev.trigger_area = this;
				ev.body = body;
				ev.body_rid = body->get_rid();
				ev.body_id = body->get_instance_id();
				ev.status = PhysicsServer3D::AREA_BODY_REMOVED;
				ev.other_shape_idx = 0;
				ev.area_shape_idx = 0;
				ev.is_area_vs_area = false;
				space->queue_trigger(ev);
			}
		}
		overlapping_bodies.clear();

		for (unsigned int i = 0; i < overlapping_areas.size(); i++) {
			PhysXArea3D *other_area = overlapping_areas[i];
			// Remove this area from the other area's overlap list.
			other_area->remove_overlapping_area(this);
			// Queue the area_monitor dispatch for flush_queries().
			if (space) {
				PhysXSpace3D::TriggerEvent ev;
				ev.trigger_area = this;
				ev.other_area = other_area;
				ev.other_area_rid = other_area->get_rid();
				ev.other_area_id = other_area->get_instance_id();
				ev.status = PhysicsServer3D::AREA_BODY_REMOVED;
				ev.other_shape_idx = 0;
				ev.area_shape_idx = 0;
				ev.is_area_vs_area = true;
				space->queue_trigger(ev);
			}
		}
		overlapping_areas.clear();

		space->unregister_area(this);

		if (px_actor) {
			space->remove_actor(px_actor);
		}
	}

	space = p_space;

	if (space) {
		space->register_area(this);

		if (px_actor) {
			space->add_actor(px_actor);
		}
	}
}

	// Transform

void PhysXArea3D::set_transform(const Transform3D &p_transform) {
	transform = p_transform;

	// Same node-scale bake as bodies (see PhysXBody3D::set_state): the
	// trigger/override geometry is re-baked against the captured scale (PhysX
	// actor poses carry no scale). Runs before the pose push so the world
	// bounds queried by the wake pass below already reflect the new size.
	const Vector3 new_scale = p_transform.basis.get_scale();
	if (!new_scale.is_equal_approx(body_scale)) {
		body_scale = new_scale;
		refresh_shape_scaling();
	}

	if (!px_actor) {
		return;
	}

	const physx::PxTransform pose = to_physx_transform(p_transform);

	// Areas are kinematic actors and must move through
	// the kinematic pipeline -- setGlobalPose() on a kinematic actor only
	// rewrites the stored pose; the broadphase/pair-cache update driven by
	// kinematic motion never runs, so relocating an area never re-evaluated
	// its trigger pairs and enter/exit events stopped firing (the SDK's own
	// docs point to setKinematicTarget() for such actors). setKinematicTarget()
	// requires the actor to be in a scene; an area's transform may be set
	// before area_set_space(), so fall back to setGlobalPose() until then
	// (same pattern as PhysXBody3D::set_state).
	if (physx::PxRigidDynamic *dyn = px_actor->is<physx::PxRigidDynamic>()) {
		if (space && space->get_px_scene()) {
			dyn->setKinematicTarget(pose);
		} else {
			dyn->setGlobalPose(pose);
		}
	} else {
		px_actor->setGlobalPose(pose);
	}

	// The kinematic target only takes effect during the next simulation step,
	// and moving a trigger actor does not by itself wake dynamic bodies at the
	// destination in a timely way -- overrides (gravity/damp) and contact-
	// driven state would be delayed until those bodies wake for another
	// reason. Wake the bodies overlapping the area's NEW world AABB via one
	// batched overlap query. The pass reads actor bounds and runs a scene
	// query -- both forbidden mid-solve (async stepping), so it is skipped in
	// the in-flight window; the target still applies and bodies wake through
	// normal solver interaction.
	// Gated on override presence: a pure monitoring area imposes
	// nothing on the bodies it sweeps over, so waking them (and paying the
	// overlap query per move) is pure overhead that keeps sleeping stacks
	// from ever settling. Wind is unaffected: sleeping bodies receive it
	// from their own pre-step, and the kinematic move re-evaluates the
	// trigger pairs driving the overlap lists.
	if (!space || !space->is_stepping()) {
		if (has_gravity_override() || has_linear_damp_override() || has_angular_damp_override()) {
			_wake_overlapping_dynamic_bodies(pose);
		}
	}
}

void PhysXArea3D::_wake_overlapping_dynamic_bodies(const physx::PxTransform &p_target_pose) const {
	if (!space || !space->get_px_scene() || px_actor->getNbShapes() == 0) {
		return;
	}
	physx::PxScene *scene = space->get_px_scene();

	// Use the area's world-space bounds (PxRigidActor aggregates its shapes).
	// The kinematic target applies only during the next simulation step, so
	// before then the actor reports its PREVIOUS pose -- transform the bounds
	// by the rigid delta so the query covers the destination.
	physx::PxBounds3 bounds = px_actor->getWorldBounds(1.0f);
	if (bounds.isEmpty()) {
		return;
	}
	const physx::PxTransform old_pose = px_actor->getGlobalPose();
	if (!(old_pose == p_target_pose)) {
		bounds = physx::PxBounds3::transformSafe(p_target_pose * old_pose.getInverse(), bounds);
	}

	// Overlap a box covering the bounds and wake every dynamic body it touches.
	const physx::PxVec3 center = bounds.getCenter();
	const physx::PxVec3 half = bounds.getDimensions() * 0.5f;
	const physx::PxBoxGeometry box_geo(half);
	const physx::PxTransform pose(center);

	physx::PxOverlapHit hits[64];
	physx::PxOverlapBuffer buf(hits, 64);
	physx::PxQueryFilterData fd;
	// We want every dynamic body in the volume regardless of layer/mask; disable
	// the hardcoded shader so no pair is rejected before we see it.
	fd.flags = physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eDISABLE_HARDCODED_FILTER;
	if (scene->overlap(box_geo, pose, buf, fd)) {
		for (physx::PxU32 i = 0; i < buf.getNbAnyHits(); i++) {
			const physx::PxOverlapHit &h = buf.getAnyHit(i);
			if (h.actor != px_actor) {
				if (physx::PxRigidDynamic *dyn = h.actor->is<physx::PxRigidDynamic>()) {
					// Actor found by overlap query is guaranteed to be in the
					// scene, so wakeUp() is safe here. Kinematic dynamics never
					// sleep and reject wakeUp() ("Body must be non-kinematic!"),
					// so they are skipped.
					if (!(dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC)) {
						dyn->wakeUp();
					}
				}
			}
		}
	}
}

Transform3D PhysXArea3D::get_transform() const {
	return transform;
}

	// Shape Management

void PhysXArea3D::_configure_shape_as_trigger(physx::PxShape *p_shape) const {
	// Areas never simulate; their shapes are either triggers (when the area
	// detects, can be detected, or exerts space overrides -- a plain gravity
	// area must still track the bodies inside it so on_pre_step can resolve
	// the override) or purely scene-query shapes. Godot decouples monitoring
	// from monitorable -- a shape must be a trigger for EITHER. Keeping
	// eSCENE_QUERY_SHAPE lets raycasts/overlaps still hit the area.
	const bool detects = monitor_callback.is_valid() || area_monitor_callback.is_valid();
	const bool overrides = has_gravity_override() || has_linear_damp_override() || has_angular_damp_override();
	p_shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
	p_shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, detects || monitorable || overrides);
	p_shape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, true);
}

void PhysXArea3D::add_shape(PhysXShape3D *p_shape, const Transform3D &p_transform, bool p_disabled) {
	PhysXShapedObject3D::add_shape(p_shape, p_transform, p_disabled);

	// Re-apply the area's trigger/filter state to every shape (the base
	// appended the new record without area semantics) and (re)create the
	// detection shape of the new record when this area is monitorable.
	_update_shapes();
}

void PhysXArea3D::set_shape(int p_shape_idx, PhysXShape3D *p_shape) {
	ERR_FAIL_INDEX(p_shape_idx, get_shape_count());

	// Preserve the original local transform + disabled state across the swap;
	// Godot expects body/area_set_shape to keep them. The base remove + append
	// would discard the transform and reorder the slot, so we capture first.
	const Transform3D saved_transform = attached_shapes[p_shape_idx].relative_transform;
	const bool saved_disabled = attached_shapes[p_shape_idx].disabled;

	// Detach old shape and remove owner tracking.
	remove_shape(p_shape_idx);

	// Insert the new shape at the end, then move its AttachedShape record into
	// the original slot so the index is preserved. The PxShape itself is
	// position-independent (its placement is dictated by its local pose), so
	// reordering the bookkeeping vector is safe.
	add_shape(p_shape, saved_transform, saved_disabled);

	const int last_idx = get_shape_count() - 1;
	if (last_idx != p_shape_idx) {
		SWAP(attached_shapes[p_shape_idx], attached_shapes[last_idx]);
	}
}

void PhysXArea3D::set_shape_transform(int p_shape_idx, const Transform3D &p_transform) {
	ERR_FAIL_INDEX(p_shape_idx, get_shape_count());
	PhysXShapedObject3D::set_shape_transform(p_shape_idx, p_transform);
}

void PhysXArea3D::set_shape_disabled(int p_shape_idx, bool p_disabled) {
	ERR_FAIL_INDEX(p_shape_idx, get_shape_count());

	// The detection-shape sync below can attach/detach PxShapes (broadphase
	// mutation) — fetch an in-flight solve first (async stepping).
	if (space) {
		space->ensure_synced();
	}

	if (p_disabled == attached_shapes[p_shape_idx].disabled) {
		return;
	}

	attached_shapes[p_shape_idx].disabled = p_disabled;

	if (attached_shapes[p_shape_idx].shape_instance) {
		const bool detects = monitor_callback.is_valid() || area_monitor_callback.is_valid();
		const bool overrides = has_gravity_override() || has_linear_damp_override() || has_angular_damp_override();
		attached_shapes[p_shape_idx].shape_instance->setFlag(
				physx::PxShapeFlag::eTRIGGER_SHAPE, !p_disabled && (detects || monitorable || overrides));
		attached_shapes[p_shape_idx].shape_instance->setFlag(
				physx::PxShapeFlag::eSCENE_QUERY_SHAPE, !p_disabled);
	}
	// A disabled shape cannot be detected by other areas — drop its detection
	// shape (and recreate it when re-enabled).
	_sync_detection_shape(attached_shapes[p_shape_idx]);
}

void PhysXArea3D::remove_shape(int p_shape_idx) {
	ERR_FAIL_INDEX(p_shape_idx, get_shape_count());
	PhysXShapedObject3D::remove_shape(p_shape_idx);
}

void PhysXArea3D::clear_shapes() {
	while (get_shape_count() > 0) {
		PhysXShapedObject3D::remove_shape(0);
	}
}

// ============================================================================
// Area Parameters
// ============================================================================

void PhysXArea3D::set_param(PhysicsServer3D::AreaParameter p_param, const Variant &p_value) {
	switch (p_param) {
		case PhysicsServer3D::AREA_PARAM_GRAVITY_OVERRIDE_MODE:
			gravity_override_mode = (PhysicsServer3D::AreaSpaceOverrideMode)(int)p_value;
			_update_shapes(); // Trigger state depends on whether overrides are active.
			break;
		case PhysicsServer3D::AREA_PARAM_GRAVITY:
			gravity = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_VECTOR:
			gravity_vector = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_IS_POINT:
			gravity_is_point = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_POINT_UNIT_DISTANCE:
			gravity_point_unit_distance = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_LINEAR_DAMP_OVERRIDE_MODE:
			linear_damp_override_mode = (PhysicsServer3D::AreaSpaceOverrideMode)(int)p_value;
			_update_shapes(); // Trigger state depends on whether overrides are active.
			break;
		case PhysicsServer3D::AREA_PARAM_LINEAR_DAMP:
			linear_damp = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_ANGULAR_DAMP_OVERRIDE_MODE:
			angular_damp_override_mode = (PhysicsServer3D::AreaSpaceOverrideMode)(int)p_value;
			_update_shapes(); // Trigger state depends on whether overrides are active.
			break;
		case PhysicsServer3D::AREA_PARAM_ANGULAR_DAMP:
			angular_damp = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_PRIORITY:
			priority = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_WIND_FORCE_MAGNITUDE:
			wind_force_magnitude = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_WIND_SOURCE:
			wind_source = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_WIND_DIRECTION:
			wind_direction = p_value;
			break;
		case PhysicsServer3D::AREA_PARAM_WIND_ATTENUATION_FACTOR:
			wind_attenuation_factor = p_value;
			break;
		default:
			break;
	}
}

Variant PhysXArea3D::get_param(PhysicsServer3D::AreaParameter p_param) const {
	switch (p_param) {
		case PhysicsServer3D::AREA_PARAM_GRAVITY_OVERRIDE_MODE:
			return (int)gravity_override_mode;
		case PhysicsServer3D::AREA_PARAM_GRAVITY:
			return gravity;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_VECTOR:
			return gravity_vector;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_IS_POINT:
			return gravity_is_point;
		case PhysicsServer3D::AREA_PARAM_GRAVITY_POINT_UNIT_DISTANCE:
			return gravity_point_unit_distance;
		case PhysicsServer3D::AREA_PARAM_LINEAR_DAMP_OVERRIDE_MODE:
			return (int)linear_damp_override_mode;
		case PhysicsServer3D::AREA_PARAM_LINEAR_DAMP:
			return linear_damp;
		case PhysicsServer3D::AREA_PARAM_ANGULAR_DAMP_OVERRIDE_MODE:
			return (int)angular_damp_override_mode;
		case PhysicsServer3D::AREA_PARAM_ANGULAR_DAMP:
			return angular_damp;
		case PhysicsServer3D::AREA_PARAM_PRIORITY:
			return priority;
		case PhysicsServer3D::AREA_PARAM_WIND_FORCE_MAGNITUDE:
			return wind_force_magnitude;
		case PhysicsServer3D::AREA_PARAM_WIND_SOURCE:
			return wind_source;
		case PhysicsServer3D::AREA_PARAM_WIND_DIRECTION:
			return wind_direction;
		case PhysicsServer3D::AREA_PARAM_WIND_ATTENUATION_FACTOR:
			return wind_attenuation_factor;
		default:
			return Variant();
	}
}

// ============================================================================
// Monitoring & Callbacks
// ============================================================================

void PhysXArea3D::set_monitorable(bool p_monitorable) {
	if (monitorable == p_monitorable) {
		return;
	}

	monitorable = p_monitorable;

	if (!p_monitorable) {
		// _update_shapes() below destroys this area's detection shapes, which
		// terminates the other areas' trigger pairs against them. PhysX exit
		// events for shape-removed pairs (eREMOVED_SHAPE_*) are skipped in
		// onTrigger, so emit the area-vs-area exits manually — otherwise the
		// overlap lists leak and keep dangling pointers.
		_emit_area_exit_events();
	}

	// Re-apply trigger flags (monitorable participates in the trigger test)
	// and create/destroy the detection shapes.
	_update_shapes();
}

bool PhysXArea3D::get_monitorable() const {
	return monitorable;
}

void PhysXArea3D::set_monitoring(bool p_enable) {
	// Monitoring is controlled purely by callback validity (see dispatch_body_monitor).
}

bool PhysXArea3D::get_monitoring() const {
	return monitor_callback.is_valid();
}

void PhysXArea3D::set_ray_pickable(bool p_enable) {
	ray_pickable = p_enable;
}

bool PhysXArea3D::is_ray_pickable() const {
	return ray_pickable;
}

// ============================================================================
// Monitor dispatch (called by PhysXSimulationEventCallback::onTrigger)
// ============================================================================

void PhysXArea3D::dispatch_body_monitor(int p_status, const RID &p_body_rid, ObjectID p_body_id, int p_body_shape, int p_area_shape) {
	// Godot gates purely on callback validity (Area3D::set_monitoring installs
	// the callback and never tells the server about a separate flag); a local
	// `monitoring` member would stay false and drop every event.
	if (!monitor_callback.is_valid()) {
		return;
	}
	// Matches Godot Area3D::_body_inout(int status, RID body, ObjectID instance,
	// int body_shape, int area_shape).
	Variant args[5] = { p_status, p_body_rid, p_body_id, p_body_shape, p_area_shape };
	const Variant *arg_ptrs[5] = { &args[0], &args[1], &args[2], &args[3], &args[4] };
	Callable::CallError ce;
	Variant rv;
	monitor_callback.callp(arg_ptrs, 5, rv, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		ERR_PRINT_ONCE("PhysX: area body-monitor callback signature mismatch; expected (int, RID, ObjectID, int, int).");
	}
}

void PhysXArea3D::dispatch_area_monitor(int p_status, const RID &p_area_rid, ObjectID p_area_id, int p_area_shape, int p_self_shape) {
	// Same fix: gate purely on callback validity, not the unwired monitoring flag.
	if (!area_monitor_callback.is_valid()) {
		return;
	}
	// Matches Godot Area3D::_area_inout(int status, RID area, ObjectID instance,
	// int area_shape, int self_shape).
	Variant args[5] = { p_status, p_area_rid, p_area_id, p_area_shape, p_self_shape };
	const Variant *arg_ptrs[5] = { &args[0], &args[1], &args[2], &args[3], &args[4] };
	Callable::CallError ce;
	Variant rv;
	area_monitor_callback.callp(arg_ptrs, 5, rv, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		ERR_PRINT_ONCE("PhysX: area area-monitor callback signature mismatch; expected (int, RID, ObjectID, int, int).");
	}
}

// ============================================================================
// PhysX Integration
// ============================================================================

void PhysXArea3D::refresh_user_data() {
	actor_user_data.rid = get_rid();
	actor_user_data.object_id = get_instance_id();
}

// ============================================================================
// Overlap tracking (maintained by PhysXSimulationEventCallback)
// ============================================================================

void PhysXArea3D::add_overlapping_body(PhysXBody3D *p_body) {
	if (p_body == nullptr) {
		return;
	}
	if (overlapping_bodies.find(p_body) != -1) {
		return;
	}
	overlapping_bodies.push_back(p_body);
}

void PhysXArea3D::remove_overlapping_body(PhysXBody3D *p_body) {
	if (p_body == nullptr) {
		return;
	}
	for (unsigned int i = 0; i < overlapping_bodies.size(); i++) {
		if (overlapping_bodies[i] == p_body) {
			overlapping_bodies.remove_at(i);
			return;
		}
	}
}

void PhysXArea3D::add_overlapping_area(PhysXArea3D *p_area) {
	if (p_area == nullptr) {
		return;
	}
	if (overlapping_areas.find(p_area) != -1) {
		return;
	}
	overlapping_areas.push_back(p_area);
}

void PhysXArea3D::remove_overlapping_area(PhysXArea3D *p_area) {
	if (p_area == nullptr) {
		return;
	}
	for (unsigned int i = 0; i < overlapping_areas.size(); i++) {
		if (overlapping_areas[i] == p_area) {
			overlapping_areas.remove_at(i);
			return;
		}
	}
}

void PhysXArea3D::_update_shapes() {
	// Detection-shape create/destroy below attach/detach PxShapes (broadphase
	// mutation) — fetch an in-flight solve first (async stepping).
	if (space) {
		space->ensure_synced();
	}

	// Re-apply collision filter data to all shapes (layer/mask/IS_AREA).
	update_shapes_collision_filter();

	// Ensure all shapes have the correct trigger flags and detection shapes.
	for (AttachedShape &record : attached_shapes) {
		if (record.shape_instance) {
			_configure_shape_as_trigger(record.shape_instance);
			if (record.disabled) {
				// Mirror set_shape_disabled(): a disabled area shape neither
				// triggers nor answers scene queries.
				record.shape_instance->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
				record.shape_instance->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, false);
			}
		}
		_sync_detection_shape(record);
	}
}

void PhysXArea3D::_sync_detection_shape(AttachedShape &p_record) {
	// A monitorable area is detectable by other areas only through a second,
	// non-trigger simulation shape. It is never queryable (the
	// trigger shape already answers queries) and the filter shader kills any
	// non-trigger pair involving it, so it can never collide.
	if (p_record.detection_shape) {
		// Once created, the detection shape lives until the record is removed
		// (mirroring shape_instance's lifetime). It is only destroyed on explicit
		// state changes — not when the shape_blueprint pointer was nullified by
		// shape-resource destruction (the PxShape keeps its geometry copy,
		// exactly like the trigger shape does).
		if (!monitorable || p_record.disabled) {
			if (px_actor) {
				if (physx::PxRigidActor *rigid_actor = px_actor->is<physx::PxRigidActor>()) {
					rigid_actor->detachShape(*p_record.detection_shape);
				}
			}
			p_record.detection_shape = nullptr;
		}
		return;
	}

	const bool needed = monitorable && !p_record.disabled && px_actor != nullptr && p_record.shape_blueprint != nullptr;
	if (!needed) {
		return;
	}

	physx::PxPhysics &physics = PhysXServer3D::get_singleton()->get_physics();
	// Body scale + per-shape transform scale, matching PhysXShapedObject3D::add_shape()
	// (convex polygon shapes receive the signed/mirror-capable scale).
	const Vector3 geom_scale = _shape_geometry_scale_for(p_record.shape_blueprint, p_record.relative_transform);
	physx::PxVec3 total_scale(geom_scale.x, geom_scale.y, geom_scale.z);
	// Plain simulation shape: no trigger (that would be the trigger shape's
	// job), no scene query (avoids double query hits), no visualization
	// (avoids double debug drawing).
	const physx::PxShapeFlags flags = physx::PxShapeFlag::eSIMULATION_SHAPE;
	physx::PxShape *detection = p_record.shape_blueprint->create_shape(physics, total_scale, _get_shape_material(), flags);
	if (!detection) {
		return;
	}

	// Same placement as the trigger shape (body-relative transform * the
	// shape's intrinsic alignment pose).
	Transform3D final_tr = p_record.relative_transform;
	final_tr.origin *= body_scale;
	detection->setLocalPose(to_physx_transform(final_tr) * p_record.shape_blueprint->get_local_pose(total_scale));

	// Areas never collide (see physx_simulation_filter_shader).
	physx::PxFilterData filter_data;
	filter_data.word0 = collision_layer;
	filter_data.word1 = collision_mask;
	filter_data.word3 = shape_filter_flags();
	detection->setSimulationFilterData(filter_data);
	detection->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
	detection->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, false);
	physx_apply_space_rest_offset(detection, p_record.shape_blueprint->get_margin(), space);

	if (physx::PxRigidActor *rigid_actor = px_actor->is<physx::PxRigidActor>()) {
		rigid_actor->attachShape(*detection);
	}
	// Drop our local reference — the actor now owns the shape.
	detection->release();
	p_record.detection_shape = detection;
}

void PhysXArea3D::_emit_area_exit_events() {
	// Mirror of the area section in set_space(nullptr): emit AREA_BODY_REMOVED
	// for every recorded overlap, from BOTH areas' perspectives, and clear the
	// lists so no dangling pointers survive.
	for (unsigned int i = 0; i < overlapping_areas.size(); i++) {
		PhysXArea3D *other_area = overlapping_areas[i];
		// Remove this area from the other area's overlap list.
		other_area->remove_overlapping_area(this);
		if (!space) {
			continue;
		}
		// "This area detects the other": this area's callback hears about the exit.
		PhysXSpace3D::TriggerEvent ev;
		ev.trigger_area = this;
		ev.other_area = other_area;
		ev.other_area_rid = other_area->get_rid();
		ev.other_area_id = other_area->get_instance_id();
		ev.status = PhysicsServer3D::AREA_BODY_REMOVED;
		ev.other_shape_idx = 0;
		ev.area_shape_idx = 0;
		ev.is_area_vs_area = true;
		space->queue_trigger(ev);
		// "The other area detects this": the other area's callback hears about the exit.
		PhysXSpace3D::TriggerEvent ev2;
		ev2.trigger_area = other_area;
		ev2.other_area = this;
		ev2.other_area_rid = get_rid();
		ev2.other_area_id = get_instance_id();
		ev2.status = PhysicsServer3D::AREA_BODY_REMOVED;
		ev2.other_shape_idx = 0;
		ev2.area_shape_idx = 0;
		ev2.is_area_vs_area = true;
		space->queue_trigger(ev2);
	}
	overlapping_areas.clear();
}

	// Wind force at a world position (same convention as the Area3D wind
	// module): constant magnitude along wind_direction, attenuated over
	// downwind distance from wind_source when wind_attenuation_factor is
	// nonzero. Zero when wind is unused, so the common case adds no work.
physx::PxBounds3 PhysXArea3D::get_world_bounds() const {
	if (!px_actor || px_actor->getNbShapes() == 0) {
		return physx::PxBounds3::empty();
	}
	return px_actor->getWorldBounds(1.0f);
}

// The area's gravity contribution at p_position, applying the area's WORLD
// transform to the gravity vector (point-gravity reference point). Moved here
// from physx_body_3d.cpp so the soft-body gravity resolver shares it.
Vector3 physx_area_gravity_at(const PhysXArea3D &p_area, const Vector3 &p_position) {
	const real_t mag = p_area.get_gravity();
	if (p_area.get_gravity_is_point()) {
		// Gravity point in world space = area world transform applied to the
		// stored gravity_vector (which for a point area is the local-space point).
		const Vector3 point_ws = p_area.get_transform().xform(p_area.get_gravity_vector());
		const Vector3 to_point = point_ws - p_position;
		const real_t d_sq = to_point.length_squared();
		if (d_sq <= 0.0f) {
			return Vector3(); // body exactly at the singularity
		}
		const real_t unit = p_area.get_gravity_point_unit_distance();
		if (unit > 0.0f) {
			return to_point.normalized() * (mag * unit * unit / d_sq);
		}
		return to_point.normalized() * mag;
	}
	return p_area.get_gravity_vector() * mag;
}

Vector3 PhysXArea3D::wind_at(const Vector3 &p_position) const {
	if (wind_force_magnitude == 0.0 || wind_direction.length_squared() < CMP_EPSILON) {
		return Vector3();
	}
	const Vector3 dir = wind_direction.normalized();
	real_t attenuation = 1.0;
	if (wind_attenuation_factor != 0.0) {
		const real_t along = MAX((p_position - wind_source).dot(dir), (real_t)1.0);
		attenuation = Math::pow(along, -wind_attenuation_factor);
	}
	return dir * (wind_force_magnitude * attenuation);
}

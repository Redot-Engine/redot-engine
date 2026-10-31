/**************************************************************************/
/*  physx_body_3d.cpp                                                     */
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
 * @file physx_body_3d.cpp
 * @brief Implementation of PhysXBody3D — the PhysX rigid body wrapper.
 *
 * This file implements the full body API: actor creation/destruction, mode
 * switching (static/dynamic/kinematic), state (transform/velocity/sleep),
 * parameters (mass/friction/damping), forces/impulses, CCD, axis locks,
 * collision exceptions, contact reporting, and per-step hooks.
 */

#include "physx_body_3d.h"
#include "../joints/physx_joint_3d.h"
#include "../physx_conversions.h"
#include "../physx_project_settings.h"
#include "../physx_server.h"
#include "../spaces/physx_space_3d.h"
#include "../vehicles/physx_vehicle_server.h"
#include "PxPhysicsAPI.h"
#include "physx_area_3d.h"
#include "physx_direct_body_state_3d.h"

// Overlap tracking (maintained by PhysXSimulationEventCallback)

void PhysXBody3D::add_overlapping_area(PhysXArea3D *p_area) {
	if (p_area == nullptr) {
		return;
	}
	if (overlapping_areas.find(p_area) != -1) {
		return;
	}
	overlapping_areas.push_back(p_area);
}

void PhysXBody3D::remove_overlapping_area(PhysXArea3D *p_area) {
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

PhysXBody3D::PhysXBody3D() :
		PhysXShapedObject3D(OBJECT_TYPE_BODY) {
	_create_actor();
}

PhysXBody3D::~PhysXBody3D() {
	if (direct_state) {
		memdelete(direct_state);
		direct_state = nullptr;
	}
	// Connected joints drop the PxJoint (which references this body's actor)
	// and unlink themselves; the joint wrappers survive with their RIDs and
	// cached parameters, matching what the Joint3D nodes hold.
	{
		const LocalVector<PhysXJoint3D *> js = joints; // copy: body_removed unlinks
		for (PhysXJoint3D *joint : js) {
			joint->body_removed(this);
		}
		joints.clear();
	}
	// Cache the space before set_space(nullptr) clears it: the actor release
	// below must route through the (former) space so an async mid-flight
	// destroy queues remove+release until after the fetch instead of releasing
	// into a live solve (mirrors ~PhysXArea3D).
	PhysXSpace3D *previous_space = space;
	// Emit exit events for all overlapping areas before the body is destroyed —
	// trigger events may not fire when the body is freed while overlapping an
	// area, and Godot must receive these events to clean up its state.
	// free(body) normally detaches via set_space(nullptr) first, so this only
	// runs as a defensive fallback for destruction without prior detachment.
	if (space) {
		_emit_all_area_exits();
		space->unregister_body(this);
	}
	_destroy_actor(previous_space);
}

physx::PxRigidDynamic *PhysXBody3D::get_px_dynamic() const {
	if (px_actor && px_actor->is<physx::PxRigidDynamic>()) {
		return px_actor->is<physx::PxRigidDynamic>();
	}
	return nullptr;
}

void PhysXBody3D::refresh_user_data() {
	actor_user_data.rid = get_rid();
	actor_user_data.object_id = get_instance_id();
}

void PhysXBody3D::_create_actor() {
	physx::PxPhysics &physics = PhysXServer3D::get_singleton()->get_physics();
	physx::PxTransform default_transform(physx::PxIdentity);

	// 1. Create the appropriate PhysX actor based on BodyMode
	if (mode == PhysicsServer3D::BODY_MODE_STATIC) {
		px_actor = physics.createRigidStatic(default_transform);
	} else {
		physx::PxRigidDynamic *dynamic_actor = physics.createRigidDynamic(default_transform);

		// If kinematic, set the kinematic flag
		if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC) {
			dynamic_actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
		}
		px_actor = dynamic_actor;
	}

	// The actor carries a PhysXActorUserData so raycasts/overlaps/sweeps and
	// simulation callbacks can recover the Godot RID/ObjectID — the ONLY
	// type that may be stored in PxActor::userData.
	if (px_actor) {
		actor_user_data.object = this;
		actor_user_data.rid = get_rid();
		actor_user_data.object_id = get_instance_id();
		px_actor->userData = &actor_user_data;

		// Native scene gravity (see PhysXSpace3D::_initialize_scene) is enabled
		// on the actor; on_pre_step() applies only the correction delta on top
		// of it (zero in the default case). A vehicle2 chassis re-enables
		// eDISABLE_GRAVITY in PhysXVehicleServer::adopt() because vehicle2 owns
		// the chassis gravity.

		// Apply cached parameters to the freshly created actor.
		_apply_params_to_actor();
	}
}

void PhysXBody3D::_destroy_actor(PhysXSpace3D *p_space) {
	if (px_actor) {
		// Route the release through a space (the current one, or the one the
		// body just left): in async stepping a mid-flight destroy queues both
		// the scene removal and the release until after the fetch — releasing
		// into a live solve would free an actor the solver still references.
		// The remove is skipped when the actor was never added to the scene
		// (no shapes yet), since PhysX errors on removing an absent actor.
		PhysXSpace3D *routing = p_space ? p_space : space;
		if (routing && body_added_to_scene) {
			routing->remove_actor(px_actor);
		}
		if (routing) {
			routing->release_actor(px_actor);
		} else {
			px_actor->release();
		}
		px_actor = nullptr;
	}
	// Reset so that a new actor (e.g., after mode switch) can be added
	// to the scene by _add_to_scene() after shapes are rebuilt.
	body_added_to_scene = false;
	// A freshly created actor has gravity enabled; the cached flag must match
	// or on_pre_step would skip the first eDISABLE_GRAVITY write.
	gravity_disabled_cached = false;
}

void PhysXBody3D::set_space(PhysXSpace3D *p_space) {
	if (space == p_space) {
		return;
	}
	// Unregister from the current space, if any.
	if (space) {
		// Queue exit events for all overlapping areas before leaving the space —
		// trigger events may not fire (e.g., when the body is freed while
		// overlapping an area). We must emit these events so Godot does not
		// leak state and so the area's overlap lists do not retain dangling
		// pointers to this body.
		_emit_all_area_exits();
		space->unregister_body(this);

		if (px_actor && body_added_to_scene) {
			space->remove_actor(px_actor);
			body_added_to_scene = false;
		}
	}

	space = p_space;
	// Register with the new space, if any.
	if (space) {
		space->register_body(this);

		// Vehicles borrowing this body as chassis follow it into the new
		// space: a vehicle simulating in a different scene than its chassis
		// would raycast road geometry and read gravity from the wrong PxScene.
		for (PhysXVehicleServer *vehicle : chassis_vehicles) {
			vehicle->set_space(space);
		}

		// Only add the actor to the scene if shapes are already attached.
		// When the body is created, shapes aren't attached yet — we must
		// defer scene addition until the first shape is attached so the
		// inertia tensor is computed from the shape geometry before the
		// body enters the simulation.
		if (px_actor && !body_added_to_scene) {
			_add_to_scene();
		}
	}
}

/**
 * Changes the body's simulation mode.
 *
 * Static <-> Dynamic requires full actor recreation (PxRigidStatic and
 * PxRigidDynamic are distinct types): the old actor is released (its shapes
 * die with it), a new one is created, the cached pose/velocity restored,
 * attached_shapes rebuilt via rebuild_shapes(), and the body rejoins its
 * space. Dynamic <-> Kinematic / RigidLinear is the fast path: toggling
 * eKINEMATIC on the existing actor (re-waking when leaving kinematic, since
 * idle kinematics may be asleep).
 */
void PhysXBody3D::set_mode(PhysicsServer3D::BodyMode p_mode) {
	if (mode == p_mode) {
		return;
	}

	// A vehicle2 chassis must stay BODY_MODE_RIGID: the vehicle borrows the
	// chassis PxRigidDynamic*, and a static<->dynamic mode change recreates the
	// actor (see below), which would dangle that pointer. Kinematic /
	// rigid-linear / static are rejected for the same reason (plan risk R4).
	if (is_vehicle_chassis && p_mode != PhysicsServer3D::BODY_MODE_RIGID) {
		ERR_FAIL_MSG("PhysX: a vehicle chassis body must remain dynamic (BODY_MODE_RIGID).");
	}

	bool was_static = (mode == PhysicsServer3D::BODY_MODE_STATIC);
	bool is_static = (p_mode == PhysicsServer3D::BODY_MODE_STATIC);
	mode = p_mode;

	// Switching between Static and Dynamic requires recreating the actor,
	// because PxRigidStatic and PxRigidDynamic are distinct types.
	if (was_static != is_static) {
		// Recreation mutates the scene (actor remove+release, shape re-attach)
		// and reads back the current pose/velocity — all forbidden while a
		// solve is in flight (async stepping). Fetch first; mutating frames
		// lose the async overlap by design.
		if (space) {
			space->ensure_synced();
		}
		physx::PxTransform cached_pose(physx::PxIdentity);
		// Velocity is only meaningful on the old dynamic actor; statistics have
		// none. Cache it now (before _destroy_actor releases the old actor) so
		// a dynamic→static→dynamic round-trip preserves momentum.
		physx::PxVec3 cached_lin_vel(0, 0, 0);
		physx::PxVec3 cached_ang_vel(0, 0, 0);
		if (px_actor) {
			cached_pose = px_actor->getGlobalPose();
			if (physx::PxRigidDynamic *old_dyn = get_px_dynamic()) {
				cached_lin_vel = old_dyn->getLinearVelocity();
				cached_ang_vel = old_dyn->getAngularVelocity();
			}
		}

		// Preserve space membership across recreation.
		PhysXSpace3D *cached_space = space;
		// The old actor dies here: an area the body overlaps would never see
		// it leave (its trigger pairs come back as REMOVED, skipped by the
		// event callback). Queue explicit exits — the rebuilt actor re-enters
		// on the next touch, canceling out in Godot's refcounted monitors.
		if (space) {
			_emit_all_area_exits();
		}
		// Joints attached to this body hold a PxJoint bound to the old actor;
		// PhysX deletes a constraint when either of its actors dies. Release
		// them here, before the actor dies, so they rebuild against the fresh
		// one from _add_to_scene below instead of keeping dangling pointers.
		for (PhysXJoint3D *joint : joints) {
			joint->actor_invalidated(this);
		}
		_destroy_actor();

		// Re-create with the new mode. The new actor starts with no shapes;
		// releasing the old actor released them, so we must rebuild.
		_create_actor();
		if (px_actor) {
			px_actor->setGlobalPose(cached_pose);
			// Restore velocity on the new dynamic actor (no-op for static).
			// PhysX forbids setLinearVelocity/setAngularVelocity on kinematic
			// bodies — cache into the kinematic tracking members instead, so a
			// dynamic->static->kinematic round-trip preserves the requested
			// velocity (setting velocity on a kinematic body is legal and must
			// not error). See set_state().
			if (physx::PxRigidDynamic *new_dyn = get_px_dynamic()) {
				if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC) {
					kinematic_linear_velocity = Vector3(cached_lin_vel.x, cached_lin_vel.y, cached_lin_vel.z);
					kinematic_angular_velocity = Vector3(cached_ang_vel.x, cached_ang_vel.y, cached_ang_vel.z);
				} else {
					new_dyn->setLinearVelocity(cached_lin_vel);
					new_dyn->setAngularVelocity(cached_ang_vel);
				}
			}
		}

		// Re-create all PxShapes against the new actor.
		rebuild_shapes();

		// Rejoin the space. The actor is added to the scene by
		// _on_shape_added() → _add_to_scene() after shapes are rebuilt.
		space = cached_space;
	} else {
		// Fast path: toggling Kinematic / Rigid / RigidLinear on a dynamic actor.
		if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
			const bool is_kinematic = (mode == PhysicsServer3D::BODY_MODE_KINEMATIC);
			if (is_kinematic) {
				// Snapshot the dynamic velocity into the kinematic tracking
				// members before flipping the flag: once kinematic, getLinearVelocity()
				// no longer reflects simulated motion and setLinearVelocity() becomes
				// illegal. Preserves momentum for rigid->kinematic.
				const physx::PxVec3 lv = dyn->getLinearVelocity();
				const physx::PxVec3 av = dyn->getAngularVelocity();
				kinematic_linear_velocity = Vector3(lv.x, lv.y, lv.z);
				kinematic_angular_velocity = Vector3(av.x, av.y, av.z);
			}
			dyn->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, is_kinematic);
			if (!is_kinematic && body_added_to_scene) {
				// Leaving kinematic: an idle kinematic body may be asleep, and
				// clearing eKINEMATIC does not wake it — without this the body
				// would stay inert instead of resuming simulation.
				// The static->rigid path gets a fresh (awake) actor on
				// recreation, so mirror that here.
				dyn->wakeUp();
			}
		}
	}

	_apply_params_to_actor();
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

void PhysXBody3D::set_state(PhysicsServer3D::BodyState p_state, const Variant &p_value) {
	if (!px_actor) {
		return;
	}

	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM: {
			// setKinematicTarget/setGlobalPose below, and a scale change's
			// refresh_shape_scaling() (PxShape::setGeometry), are forbidden
			// while a solve is in flight (async stepping). Fetch first.
			if (space) {
				space->ensure_synced();
			}
			// Capture the node scale BEFORE the pose conversion strips it:
			// PhysX actor poses carry no scale, so godot_physics' "the whole
			// body transform scales its shapes" behavior is produced by baking
			// the captured scale into every attached shape's geometry instead.
			const Transform3D xform = p_value.operator Transform3D();
			const Vector3 new_scale = xform.basis.get_scale();
			if (!new_scale.is_equal_approx(body_scale)) {
				body_scale = new_scale;
				refresh_shape_scaling();
			}
			physx::PxTransform pose(physx::PxIdentity);
			// godot_to_px clamps NaN/degenerate quaternions before they can
			// reach PhysX (physx_conversions.h); scale was captured above.
			pose = godot_to_px(p_value.operator Transform3D());
			if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC) {
				// Kinematic bodies must use setKinematicTarget() for the solver
				// to interpolate — but that requires the body to be in a scene.
				// CharacterBody3D sets its transform on creation, before it has
				// a space, so fall back to setGlobalPose() until it's added.
				if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
					if (space && space->get_px_scene()) {
						dyn->setKinematicTarget(pose);
					} else {
						dyn->setGlobalPose(pose);
					}
				}
			} else {
				px_actor->setGlobalPose(pose);
			}
			break;
		}
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY: {
			Vector3 v = p_value;
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				// PhysX forbids setLinearVelocity on kinematic bodies. For
				// kinematics, the velocity is read back from the pose delta in
				// _update_kinematic_velocity(), so just cache the requested value.
				if (mode != PhysicsServer3D::BODY_MODE_KINEMATIC) {
					dyn->setLinearVelocity(physx::PxVec3(v.x, v.y, v.z));
				}
				kinematic_linear_velocity = v;
			}
			break;
		}
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY: {
			Vector3 v = p_value;
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				// PhysX forbids setAngularVelocity on kinematic bodies (see above).
				if (mode != PhysicsServer3D::BODY_MODE_KINEMATIC) {
					dyn->setAngularVelocity(physx::PxVec3(v.x, v.y, v.z));
				}
				kinematic_angular_velocity = v;
			}
			break;
		}
		case PhysicsServer3D::BODY_STATE_SLEEPING: {
			bool sleep = p_value;
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC) {
					// PhysX kinematics never sleep, and PxRigidDynamic::
					// wakeUp/putToSleep reject them ("Body must be
					// non-kinematic!"). Godot treats the request as a legal
					// no-op for kinematic bodies (the Jolt backend behaves
					// the same), so swallow it here — lab code like
					// "wake all bodies" iterates kinematics too.
					break;
				}
				if (sleep) {
					dyn->putToSleep();
				} else if (body_added_to_scene) {
					dyn->wakeUp();
				}
			}
			break;
		}
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP: {
			// Godot's CAN_SLEEP gates whether the body may ever go to sleep.
			// PhysX equivalent: a sleep threshold of 0 is never undercut.
			can_sleep = (bool)p_value;
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				_apply_sleep_policy(dyn);
			}
			break;
		}
	}
}

Variant PhysXBody3D::get_state(PhysicsServer3D::BodyState p_state) const {
	if (!px_actor) {
		return Variant();
	}

	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM: {
			physx::PxTransform pose = px_actor->getGlobalPose();
			Transform3D t;
			t.origin = Vector3(pose.p.x, pose.p.y, pose.p.z);
			Quaternion q(pose.q.x, pose.q.y, pose.q.z, pose.q.w);
			// The set path decomposes the node basis via get_scale() (per-axis
			// lengths, stored as body_scale and baked into shape geometry) and
			// get_rotation_quaternion() (the actor pose). Rebuild with a LOCAL
			// scale composition — Basis(q) * diag(body_scale) — so the axis
			// lengths and rotation both round-trip: this is the fixed point of
			// that decomposition pair; native scaled() is a GLOBAL-axis scale and
			// does not reproduce get_scale().
			t.basis = Basis(q).scaled_local(body_scale);
			return t;
		}
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY: {
			// PhysX reports no meaningful velocity for kinematic actors outside
			// of target-driven steps; return the tracked pose-delta value.
			if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC) {
				return kinematic_linear_velocity;
			}
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				physx::PxVec3 v = dyn->getLinearVelocity();
				return Vector3(v.x, v.y, v.z);
			}
			return Vector3();
		}
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY: {
			if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC) {
				return kinematic_angular_velocity;
			}
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				physx::PxVec3 v = dyn->getAngularVelocity();
				return Vector3(v.x, v.y, v.z);
			}
			return Vector3();
		}
		case PhysicsServer3D::BODY_STATE_SLEEPING: {
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				return dyn->isSleeping();
			}
			return false;
		}
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP: {
			return can_sleep;
		}
	}
	return Variant();
}

// ---------------------------------------------------------------------------
// Parameters
// ---------------------------------------------------------------------------

void PhysXBody3D::set_param(PhysicsServer3D::BodyParameter p_param, const Variant &p_value) {
	// No-op writes are skipped entirely: the apply path below runs
	// _apply_sleep_policy() and rewrites the actor's material/damping/flags —
	// re-assigning the same value (RigidBody3D setter churn, inspector, scene
	// reload) must not dirty the actor for nothing.
	switch (p_param) {
		case PhysicsServer3D::BODY_PARAM_BOUNCE:
			if ((real_t)p_value == bounce) {
				return;
			}
			break;
		case PhysicsServer3D::BODY_PARAM_FRICTION:
			if ((real_t)p_value == friction) {
				return;
			}
			break;
		case PhysicsServer3D::BODY_PARAM_MASS:
			if ((real_t)p_value == mass) {
				return;
			}
			break;
		case PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE:
			if ((real_t)p_value == gravity_scale) {
				return;
			}
			break;
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP:
			if ((real_t)p_value == linear_damp) {
				return;
			}
			break;
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP:
			if ((real_t)p_value == angular_damp) {
				return;
			}
			break;
		case PhysicsServer3D::BODY_PARAM_INERTIA:
			if (inertia_set && Vector3(p_value) == inertia_override) {
				return;
			}
			break;
		case PhysicsServer3D::BODY_PARAM_CENTER_OF_MASS:
			if (center_of_mass_set && Vector3(p_value) == center_of_mass_override) {
				return;
			}
			break;
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP_MODE:
			if ((PhysicsServer3D::BodyDampMode)(int)p_value == linear_damp_mode) {
				return;
			}
			break;
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP_MODE:
			if ((PhysicsServer3D::BodyDampMode)(int)p_value == angular_damp_mode) {
				return;
			}
			break;
		default:
			break;
	}
	switch (p_param) {
		case PhysicsServer3D::BODY_PARAM_BOUNCE:
			bounce = p_value;
			break;
		case PhysicsServer3D::BODY_PARAM_FRICTION:
			friction = p_value;
			break;
		case PhysicsServer3D::BODY_PARAM_MASS:
			mass = p_value;
			break;
		case PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE:
			gravity_scale = p_value;
			break;
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP:
			linear_damp = p_value;
			break;
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP:
			angular_damp = p_value;
			break;
		case PhysicsServer3D::BODY_PARAM_INERTIA: {
			inertia_set = true;
			inertia_override = p_value;
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				dyn->setMassSpaceInertiaTensor(physx::PxVec3(inertia_override.x, inertia_override.y, inertia_override.z));
			}
			break;
		}
		case PhysicsServer3D::BODY_PARAM_CENTER_OF_MASS: {
			center_of_mass_set = true;
			center_of_mass_override = p_value;
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				dyn->setCMassLocalPose(physx::PxTransform(physx::PxVec3(center_of_mass_override.x, center_of_mass_override.y, center_of_mass_override.z)));
			}
			break;
		}
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP_MODE:
			linear_damp_mode = (PhysicsServer3D::BodyDampMode)(int)p_value;
			break;
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP_MODE:
			angular_damp_mode = (PhysicsServer3D::BodyDampMode)(int)p_value;
			break;
		default:
			break;
	}
	// Mass properties (setMassAndUpdateInertia re-derives the inertia tensor
	// and center of mass from the shapes) are only recomputed for the
	// parameters that can affect them; surface parameters (bounce/friction/
	// damping/CCD/locks/sleep) apply without that cost.
	switch (p_param) {
		case PhysicsServer3D::BODY_PARAM_MASS:
		case PhysicsServer3D::BODY_PARAM_INERTIA:
		case PhysicsServer3D::BODY_PARAM_CENTER_OF_MASS:
			_apply_params_to_actor();
			break;
		default:
			_apply_surface_params_to_actor();
			break;
	}
}

Variant PhysXBody3D::get_param(PhysicsServer3D::BodyParameter p_param) const {
	switch (p_param) {
		case PhysicsServer3D::BODY_PARAM_BOUNCE:
			return bounce;
		case PhysicsServer3D::BODY_PARAM_FRICTION:
			return friction;
		case PhysicsServer3D::BODY_PARAM_MASS: {
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				return (real_t)dyn->getMass();
			}
			return mass;
		}
		case PhysicsServer3D::BODY_PARAM_GRAVITY_SCALE:
			return gravity_scale;
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP:
			return linear_damp;
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP:
			return angular_damp;
		case PhysicsServer3D::BODY_PARAM_INERTIA: {
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				const physx::PxVec3 i = dyn->getMassSpaceInertiaTensor();
				return Vector3(i.x, i.y, i.z);
			}
			return inertia_set ? Variant(inertia_override) : Variant();
		}
		case PhysicsServer3D::BODY_PARAM_CENTER_OF_MASS: {
			if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
				const physx::PxVec3 c = dyn->getCMassLocalPose().p;
				return Vector3(c.x, c.y, c.z);
			}
			return center_of_mass_set ? Variant(center_of_mass_override) : Variant();
		}
		case PhysicsServer3D::BODY_PARAM_LINEAR_DAMP_MODE:
			return linear_damp_mode;
		case PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP_MODE:
			return angular_damp_mode;
		default:
			return Variant();
	}
}

void PhysXBody3D::reset_mass_properties() {
	// Re-derive mass and the inertia tensor from the attached shapes' geometries
	// at the user-supplied mass (mirrors godot_physics_3d, where mass is explicit
	// and inertia is derived). Godot's contract: BODY_PARAM_CENTER_OF_MASS /
	// BODY_PARAM_INERTIA overrides live until THIS call — RigidBody3D invokes it
	// when the center-of-mass mode returns to AUTO — so clear both overrides
	// here or a custom COM would silently survive the switch back to AUTO.
	center_of_mass_set = false;
	inertia_set = false;
	update_inertia();
}

bool PhysXBody3D::update_inertia() {
	physx::PxRigidDynamic *dyn = get_px_dynamic();
	if (!dyn) {
		return false;
	}

	// PxRigidBodyExt::setMassAndUpdateInertia is the PhysX-sanctioned helper for
	// Godot's "mass is explicit, inertia is derived from shapes" model: it sets
	// the mass to the user value AND computes the inertia tensor + center of mass
	// from the shapes' geometries scaled to that mass.
	//
	// Do NOT use updateMassAndInertia(density): the SDK requires density > 0 and
	// it overwrites the user mass. The previous density=0 call was a no-op that
	// left the inertia tensor at its (1,1,1) default — ~10x too high for a
	// typical sphere, so ramp-friction torque under-spun it and it slid instead
	// of rolling.
	//
	// includeNonSimShapes=false so disabled/query-only shapes don't contribute.
	const physx::PxU32 nb_shapes = px_actor->getNbShapes();
	const float safe_mass = (float)mass > 0.0f ? (float)mass : 1.0f;
	if (nb_shapes == 0) {
		// No shapes to derive geometry from: keep mass, leave inertia at (1,1,1).
		dyn->setMass(safe_mass);
		return false;
	}
	const bool ok = physx::PxRigidBodyExt::setMassAndUpdateInertia(*dyn, safe_mass, nullptr, false);
	if (ok) {
		// setMassAndUpdateInertia derives the inertia tensor and center of mass
		// from the shapes — re-apply the user overrides, if any.
		if (inertia_set) {
			dyn->setMassSpaceInertiaTensor(physx::PxVec3(inertia_override.x, inertia_override.y, inertia_override.z));
		}
		if (center_of_mass_set) {
			dyn->setCMassLocalPose(physx::PxTransform(physx::PxVec3(center_of_mass_override.x, center_of_mass_override.y, center_of_mass_override.z)));
		}
	}
	return ok;
}

void PhysXBody3D::_apply_sleep_policy(physx::PxRigidDynamic *p_dyn) {
	// Godot sleep policy: CAN_SLEEP (per body) AND the module-wide
	// physics/physx_3d/simulation/allow_sleep project setting gate whether a
	// body may ever sleep. PhysX has no "disable sleeping" flag; a threshold
	// of 0 is never undercut.
	if (!can_sleep || !PhysXProjectSettings::allow_sleep) {
		if (cached_sleep_threshold < 0.0f) {
			cached_sleep_threshold = p_dyn->getSleepThreshold();
		}
		p_dyn->setSleepThreshold(0.0f);
		// Kinematic dynamics never sleep and reject wakeUp() ("Body must be
		// non-kinematic!"), so skip the wake — they are always awake anyway.
		if (body_added_to_scene && mode != PhysicsServer3D::BODY_MODE_KINEMATIC) {
			p_dyn->wakeUp();
		}
		return;
	}

	// Sleeping (re-)enabled: restore the value captured while disabled, if any.
	if (cached_sleep_threshold >= 0.0f) {
		p_dyn->setSleepThreshold((float)cached_sleep_threshold);
		cached_sleep_threshold = -1.0f;
	}
	// Kinematic dynamics never sleep and PhysX rejects setWakeCounter() on
	// them ("Body must be non-kinematic!") — sleep policy does not apply.
	if (space && mode != PhysicsServer3D::BODY_MODE_KINEMATIC) {
		// Godot's velocity thresholds (m/s, rad/s) mapped onto PhysX's
		// mass-normalized kinetic-energy threshold, and time-before-sleep
		// onto the wake counter. The counter is only (re-)armed while the body
		// can actually be sleeping: a positive counter on a sleeping body wakes
		// it, and a policy re-apply must never wake anything.
		// isSleeping() is only meaningful for scene actors, so bodies outside a
		// scene always arm the counter.
		p_dyn->setSleepThreshold((float)space->get_sleep_energy_threshold());
		if (!body_added_to_scene || !p_dyn->isSleeping()) {
			p_dyn->setWakeCounter((float)space->get_time_before_sleep());
		}
	}
}

void PhysXBody3D::refresh_sleep_policy() {
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		_apply_sleep_policy(dyn);
	}
}

physx::PxMaterial *PhysXBody3D::_get_shape_material() {
	// Lazily create a body-private material. This is essential: without it,
	// every shape references the server's single shared default PxMaterial, and
	// _apply_params_to_actor() would mutate that shared object — so one body's
	// bounce/friction would overwrite every other body's. A private material
	// keeps each body's surface properties isolated.
	if (!px_material) {
		physx::PxPhysics &physics = PhysXServer3D::get_singleton()->get_physics();
		// Clamp restitution to [0,1] for the PxMaterial. PhysX 5 interprets a
		// NEGATIVE restitution as a compliant (spring-damper) contact, which is
		// not Godot's "absorbent" intent — that is handled by the contact-modify
		// callback reading the signed value from userData. The material's own
		// restitution is just a sane fallback / non-compliant baseline.
		const float mat_bounce = CLAMP((float)bounce, 0.0f, 1.0f);
		const float mat_friction = Math::abs((float)friction);
		px_material = physics.createMaterial(mat_friction, mat_friction, mat_bounce);
	}
	return px_material;
}

//
// Lock flags actually enforced: the user-set axis locks plus the mode-derived
// ones — BODY_MODE_RIGID_LINEAR is Godot's "moves but never rotates" mode,
// so all three angular axes stay locked regardless of axis_lock_flags.
//
physx::PxRigidDynamicLockFlags PhysXBody3D::_effective_lock_flags() const {
	physx::PxRigidDynamicLockFlags effective = axis_lock_flags;
	if (mode == PhysicsServer3D::BODY_MODE_RIGID_LINEAR) {
		effective |= physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
	}
	return effective;
}

void PhysXBody3D::_apply_surface_params_to_actor() {
	if (!px_actor) {
		return;
	}

	// The userData bounce/friction floats are read by the worker-thread
	// contact-modify callback DURING the solve — a write here while a solve is
	// in flight (async stepping) is a data race. Fetch first; also covers the
	// PxMaterial restitution/friction writes below. No-op when idle.
	if (space) {
		space->ensure_synced();
	}

	// Cache the SIGNED bounce/friction into the actor userData so the worker-
	// thread contact-modify callback can apply Godot's combiner without touching
	// Godot objects. Done before any material writes so it stays in sync.
	actor_user_data.bounce = (float)bounce;
	actor_user_data.friction = (float)friction;

	// Apply bounce/friction to the body's PRIVATE material (all of this body's
	// shapes share it), not the per-shape lists — which would also touch the
	// shared default material if any shape still references it. The signed
	// values for the combiner live in userData.
	if (px_material) {
		const float mat_bounce = CLAMP((float)bounce, 0.0f, 1.0f);
		const float mat_friction = Math::abs((float)friction);
		px_material->setRestitution(mat_bounce);
		px_material->setStaticFriction(mat_friction);
		px_material->setDynamicFriction(mat_friction);
	}

	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		dyn->setLinearDamping((float)linear_damp);
		dyn->setAngularDamping((float)angular_damp);
		dyn->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, ccd_enabled);
		// Cap how fast a depenetrating body may pop out of an overlap
		// (physics/physx_3d/simulation/max_depenetration_velocity; 0 = off).
		// Applied here so creation, param changes and mode flips all pick it
		// up — they all funnel through this function.
		if (PhysXProjectSettings::max_depenetration_velocity > 0.0f) {
			dyn->setMaxDepenetrationVelocity((physx::PxReal)PhysXProjectSettings::max_depenetration_velocity);
		}
		dyn->setRigidDynamicLockFlags(_effective_lock_flags());
		_apply_sleep_policy(dyn);
	}
}

void PhysXBody3D::_apply_params_to_actor() {
	if (!px_actor) {
		return;
	}
	_apply_surface_params_to_actor();
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		dyn->setMass((float)mass);

		// Derive the inertia tensor from the attached shapes' geometries, scaled
		// to the user mass. setMassAndUpdateInertia overwrites the setMass() above
		// with the same value when shapes exist; the explicit setMass() covers the
		// no-shapes early path inside update_inertia().
		update_inertia();
	}
}

/**
 * Derives linear/angular velocity for a kinematic body from its pose delta:
 * PhysX kinematics do not simulate velocity (getLinearVelocity() returns the
 * last manually-set value), but Godot's AnimatableBody3D reads it from
 * PhysicsDirectBodyState3D every step. Must run AFTER fetchResults().
 */
void PhysXBody3D::_update_kinematic_velocity(float p_step) {
	if (mode != PhysicsServer3D::BODY_MODE_KINEMATIC || !px_actor || p_step <= 0.0f) {
		return;
	}
	const physx::PxTransform pose = px_actor->getGlobalPose();

	if (has_previous_kinematic_pose) {
		const physx::PxVec3 dp = pose.p - previous_kinematic_pose.p;
		kinematic_linear_velocity = Vector3(dp.x / p_step, dp.y / p_step, dp.z / p_step);

		// Angular velocity from the quaternion delta: omega = 2 * (q_delta - conj(q_delta)).xyz,
		// which for small steps is the rotation axis scaled by the angle. Use PhysX's helper.
		const physx::PxQuat dq = pose.q * previous_kinematic_pose.q.getConjugate();
		// Shortest-rotation sign correction.
		const float w = dq.w < 0.0f ? -1.0f : 1.0f;
		// angular velocity (axis * angle / dt); angle ~= 2*acos(|w|) for the small-delta regime.
		const float angle = 2.0f * physx::PxAcos(physx::PxAbs(dq.w));
		const float inv_step = 1.0f / p_step;
		kinematic_angular_velocity = Vector3(dq.x * w * inv_step, dq.y * w * inv_step, dq.z * w * inv_step).normalized() * (angle * inv_step);
	} else {
		kinematic_linear_velocity = Vector3();
		kinematic_angular_velocity = Vector3();
	}

	previous_kinematic_pose = pose;
	has_previous_kinematic_pose = true;
}

// ---------------------------------------------------------------------------
// Forces / impulses
// ---------------------------------------------------------------------------

// PhysX's addForce/addTorque dereference the body's Sc::BodySim, which only
// exists once the actor is in a PxScene — a force applied before that (e.g.
// from SceneTree._initialize(), where godot_physics_3d happily accepts it and
// applies it once simulation starts) crashes. Buffer such calls and apply
// them from _add_to_scene(), when the actor joins the scene.
bool PhysXBody3D::_apply_force_op(const PendingForceOp &p_op) {
	physx::PxRigidDynamic *dyn = get_px_dynamic();
	if (!dyn) {
		return false;
	}
	// PhysX forbids addForce/addTorque on kinematic/static bodies.
	if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC || mode == PhysicsServer3D::BODY_MODE_STATIC) {
		return false;
	}
	if (!px_actor->getScene()) {
		// Still sceneless (e.g. queued while static and since flipped) — keep
		// buffering; the next scene entry flushes again.
		return false;
	}

	const physx::PxVec3 v((physx::PxReal)p_op.force_vector.x, (physx::PxReal)p_op.force_vector.y, (physx::PxReal)p_op.force_vector.z);
	switch (p_op.kind) {
		case PendingForceOp::CENTRAL_IMPULSE: {
			dyn->addForce(v, physx::PxForceMode::eIMPULSE);
		} break;
		case PendingForceOp::TORQUE_IMPULSE: {
			dyn->addTorque(v, physx::PxForceMode::eIMPULSE);
		} break;
		case PendingForceOp::CENTRAL_FORCE: {
			dyn->addForce(v, physx::PxForceMode::eFORCE);
		} break;
		case PendingForceOp::TORQUE: {
			dyn->addTorque(v, physx::PxForceMode::eFORCE);
		} break;
		case PendingForceOp::IMPULSE_AT:
		case PendingForceOp::FORCE_AT: {
			// Godot's p_position is an OFFSET from the body origin in GLOBAL
			// coordinates (RigidBody3D.apply_impulse contract; godot_physics_3d
			// computes the torque arm as (position - center_of_mass), where its
			// center_of_mass is the origin-relative COM in global axes).
			// PxRigidBodyExt::addForceAtPos takes an absolute world position, so
			// translate the offset by the actor origin; PhysX then applies the
			// identical (world - COM) torque arm.
			const physx::PxTransform pose = dyn->getGlobalPose();
			const physx::PxVec3 world(pose.p.x + (physx::PxReal)p_op.offset.x,
					pose.p.y + (physx::PxReal)p_op.offset.y,
					pose.p.z + (physx::PxReal)p_op.offset.z);
			physx::PxRigidBodyExt::addForceAtPos(*dyn, v, world,
					p_op.kind == PendingForceOp::IMPULSE_AT ? physx::PxForceMode::eIMPULSE : physx::PxForceMode::eFORCE);
		} break;
	}
	return true;
}

void PhysXBody3D::_flush_pending_force_ops() {
	// Apply in order, then compact: an op only leaves the buffer once its
	// actor can actually take it (a flush while still sceneless keeps it).
	for (uint32_t i = 0; i < pending_force_ops.size(); i++) {
		if (_apply_force_op(pending_force_ops[i])) {
			pending_force_ops.remove_at_unordered(i);
			i--;
		}
	}
}

void PhysXBody3D::apply_central_impulse(const Vector3 &p_impulse) {
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		// PhysX forbids addForce on kinematic/static bodies.
		if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC || mode == PhysicsServer3D::BODY_MODE_STATIC) {
			return;
		}
		if (!px_actor->getScene()) {
			PendingForceOp op;
			op.kind = PendingForceOp::CENTRAL_IMPULSE;
			op.force_vector = p_impulse;
			pending_force_ops.push_back(op);
			return;
		}
		dyn->addForce(physx::PxVec3(p_impulse.x, p_impulse.y, p_impulse.z), physx::PxForceMode::eIMPULSE);
	}
}

void PhysXBody3D::apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position) {
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC || mode == PhysicsServer3D::BODY_MODE_STATIC) {
			return;
		}
		if (!px_actor->getScene()) {
			PendingForceOp op;
			op.kind = PendingForceOp::IMPULSE_AT;
			op.force_vector = p_impulse;
			op.offset = p_position;
			pending_force_ops.push_back(op);
			return;
		}
		// Same position contract as apply_impulse (offset from the body origin in
		// global coordinates, translated to a world point for addForceAtPos).
		const physx::PxTransform pose = dyn->getGlobalPose();
		const physx::PxVec3 world(pose.p.x + (physx::PxReal)p_position.x,
				pose.p.y + (physx::PxReal)p_position.y,
				pose.p.z + (physx::PxReal)p_position.z);
		physx::PxRigidBodyExt::addForceAtPos(*dyn,
				physx::PxVec3((physx::PxReal)p_impulse.x, (physx::PxReal)p_impulse.y, (physx::PxReal)p_impulse.z),
				world, physx::PxForceMode::eIMPULSE);
	}
}

void PhysXBody3D::apply_torque_impulse(const Vector3 &p_impulse) {
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC || mode == PhysicsServer3D::BODY_MODE_STATIC) {
			return;
		}
		if (!px_actor->getScene()) {
			PendingForceOp op;
			op.kind = PendingForceOp::TORQUE_IMPULSE;
			op.force_vector = p_impulse;
			pending_force_ops.push_back(op);
			return;
		}
		dyn->addTorque(physx::PxVec3(p_impulse.x, p_impulse.y, p_impulse.z), physx::PxForceMode::eIMPULSE);
	}
}

void PhysXBody3D::apply_central_force(const Vector3 &p_force) {
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC || mode == PhysicsServer3D::BODY_MODE_STATIC) {
			return;
		}
		if (!px_actor->getScene()) {
			PendingForceOp op;
			op.kind = PendingForceOp::CENTRAL_FORCE;
			op.force_vector = p_force;
			pending_force_ops.push_back(op);
			return;
		}
		dyn->addForce(physx::PxVec3(p_force.x, p_force.y, p_force.z), physx::PxForceMode::eFORCE);
	}
}

void PhysXBody3D::apply_force(const Vector3 &p_force, const Vector3 &p_position) {
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC || mode == PhysicsServer3D::BODY_MODE_STATIC) {
			return;
		}
		if (!px_actor->getScene()) {
			PendingForceOp op;
			op.kind = PendingForceOp::FORCE_AT;
			op.force_vector = p_force;
			op.offset = p_position;
			pending_force_ops.push_back(op);
			return;
		}
		// Same position contract as apply_impulse: offset from the body origin
		// in global coordinates, translated to a world point for addForceAtPos.
		const physx::PxTransform pose = dyn->getGlobalPose();
		const physx::PxVec3 world(pose.p.x + (physx::PxReal)p_position.x,
				pose.p.y + (physx::PxReal)p_position.y,
				pose.p.z + (physx::PxReal)p_position.z);
		physx::PxRigidBodyExt::addForceAtPos(*dyn,
				physx::PxVec3((physx::PxReal)p_force.x, (physx::PxReal)p_force.y, (physx::PxReal)p_force.z),
				world, physx::PxForceMode::eFORCE);
	}
}

void PhysXBody3D::apply_torque(const Vector3 &p_torque) {
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC || mode == PhysicsServer3D::BODY_MODE_STATIC) {
			return;
		}
		if (!px_actor->getScene()) {
			PendingForceOp op;
			op.kind = PendingForceOp::TORQUE;
			op.force_vector = p_torque;
			pending_force_ops.push_back(op);
			return;
		}
		dyn->addTorque(physx::PxVec3(p_torque.x, p_torque.y, p_torque.z), physx::PxForceMode::eFORCE);
	}
}

// ---------------------------------------------------------------------------
// Constant forces
// ---------------------------------------------------------------------------

void PhysXBody3D::add_constant_central_force(const Vector3 &p_force) {
	constant_force += p_force;
}

void PhysXBody3D::add_constant_force(const Vector3 &p_force, const Vector3 &p_position) {
	// Godot's contract (godot_physics_3d GodotBody3D::add_constant_force): the
	// force accumulates directly and contributes the torque
	//     (position - center_of_mass) x force
	// where position is an offset from the body origin in GLOBAL coordinates
	// and center_of_mass is the origin-relative COM in global axes. Subtract
	// the rotated COM offset to produce the same arm.
	constant_force += p_force;
	Vector3 arm = p_position;
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		const physx::PxVec3 com_offset = dyn->getGlobalPose().rotate(dyn->getCMassLocalPose().p);
		arm -= Vector3(com_offset.x, com_offset.y, com_offset.z);
	}
	constant_torque += arm.cross(p_force);
}

void PhysXBody3D::add_constant_torque(const Vector3 &p_torque) {
	constant_torque += p_torque;
}

void PhysXBody3D::set_constant_force(const Vector3 &p_force) {
	constant_force = p_force;
}

void PhysXBody3D::set_constant_torque(const Vector3 &p_torque) {
	constant_torque = p_torque;
}

void PhysXBody3D::set_axis_velocity(const Vector3 &p_axis_velocity) {
	// Godot semantics: project out the existing velocity along the axis,
	// then add the supplied vector. The vector is a direction with magnitude = speed.
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		// PhysX forbids setLinearVelocity on kinematic bodies.
		if (mode == PhysicsServer3D::BODY_MODE_KINEMATIC) {
			return;
		}
		physx::PxVec3 current = dyn->getLinearVelocity();
		Vector3 v(current.x, current.y, current.z);
		Vector3 axis = p_axis_velocity.normalized();
		if (axis.length_squared() > 0.0) {
			v -= axis * axis.dot(v);
			v += p_axis_velocity;
		}
		dyn->setLinearVelocity(physx::PxVec3(v.x, v.y, v.z));
		if (body_added_to_scene) {
			dyn->wakeUp();
		}
	}
}

// ---------------------------------------------------------------------------
// Axis lock
// ---------------------------------------------------------------------------

void PhysXBody3D::set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_lock) {
	physx::PxRigidDynamicLockFlag::Enum flag;
	switch (p_axis) {
		case PhysicsServer3D::BODY_AXIS_LINEAR_X:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_X;
			break;
		case PhysicsServer3D::BODY_AXIS_LINEAR_Y:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Y;
			break;
		case PhysicsServer3D::BODY_AXIS_LINEAR_Z:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;
			break;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_X:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X;
			break;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_Y:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y;
			break;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_Z:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
			break;
		default:
			return;
	}
	if (p_lock) {
		axis_lock_flags |= flag;
	} else {
		axis_lock_flags &= ~flag;
	}
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		// Re-apply the full effective set (not just the touched flag) so a
		// mode-derived lock — RIGID_LINEAR locks all angular axes — cannot be
		// broken by an individual flag update.
		dyn->setRigidDynamicLockFlags(_effective_lock_flags());
	}
}

bool PhysXBody3D::is_axis_locked(PhysicsServer3D::BodyAxis p_axis) const {
	physx::PxRigidDynamicLockFlag::Enum flag;
	switch (p_axis) {
		case PhysicsServer3D::BODY_AXIS_LINEAR_X:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_X;
			break;
		case PhysicsServer3D::BODY_AXIS_LINEAR_Y:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Y;
			break;
		case PhysicsServer3D::BODY_AXIS_LINEAR_Z:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;
			break;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_X:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X;
			break;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_Y:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y;
			break;
		case PhysicsServer3D::BODY_AXIS_ANGULAR_Z:
			flag = physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
			break;
		default:
			return false;
	}
	// Report the effective state so mode-derived locks (RIGID_LINEAR locks all
	// angular axes) are visible to the getter.
	return _effective_lock_flags().isSet(flag);
}

// ---------------------------------------------------------------------------
// CCD
// ---------------------------------------------------------------------------

void PhysXBody3D::set_continuous_collision_detection(bool p_enable) {
	ccd_enabled = p_enable;
	if (physx::PxRigidDynamic *dyn = get_px_dynamic()) {
		dyn->setRigidBodyFlag(physx::PxRigidBodyFlag::eENABLE_CCD, p_enable);
	}
}

bool PhysXBody3D::is_continuous_collision_detection_enabled() const {
	return ccd_enabled;
}

// ---------------------------------------------------------------------------
// Collision exceptions
// ---------------------------------------------------------------------------

void PhysXBody3D::add_collision_exception(const RID &p_excepted_body) {
	collision_exceptions.insert(p_excepted_body);
}

void PhysXBody3D::remove_collision_exception(const RID &p_excepted_body) {
	collision_exceptions.erase(p_excepted_body);
}

void PhysXBody3D::get_collision_exceptions(List<RID> *p_exceptions) const {
	for (const RID &rid : collision_exceptions) {
		p_exceptions->push_back(rid);
	}
}

const HashSet<RID> &PhysXBody3D::get_collision_exception_set() const {
	return collision_exceptions;
}

// ---------------------------------------------------------------------------
// Contact reporting / omit-force
// ---------------------------------------------------------------------------

void PhysXBody3D::set_max_contacts_reported(int p_amount) {
	max_contacts_reported = p_amount;
	// Toggle the per-shape contact-notify marker so the filter shader requests
	// contact-point notifications for this body's shape pairs.
	set_contact_notify(p_amount > 0);
	update_shapes_collision_filter();
}

void PhysXBody3D::set_omit_force_integration(bool p_enable) {
	omit_force_integration = p_enable;
	// PhysX has no direct "custom integrator" flag; the force-integration
	// callback is consulted in on_pre_step / on_post_step to emulate it.
}

// ---------------------------------------------------------------------------
// Area overlap detection and effective parameter computation
// ---------------------------------------------------------------------------

void PhysXBody3D::set_shape_disabled(int p_shape_idx, bool p_disabled) {
	ERR_FAIL_INDEX(p_shape_idx, get_shape_count());
	if (p_disabled == attached_shapes[p_shape_idx].disabled) {
		return;
	}

	// The eSIMULATION_SHAPE/eSCENE_QUERY_SHAPE flag writes below mutate the
	// scene's broadphase pair state — forbidden while a solve is in flight
	// (async stepping). Fetch first, like the attach/detach paths.
	if (space) {
		space->ensure_synced();
	}

	attached_shapes[p_shape_idx].disabled = p_disabled;

	if (attached_shapes[p_shape_idx].shape_instance) {
		// A body shape is a simulation shape (not a trigger). Disabling it
		// means removing it from both simulation and queries until re-enabled.
		// Re-enabling must NOT restore eSIMULATION_SHAPE on a concave shape
		// attached to a non-kinematic dynamic body: those shapes were created
		// query-only because PhysX forbids mesh/heightfield/plane simulation
		// shapes on dynamic actors.
		physx::PxRigidDynamic *dyn = get_px_dynamic();
		const bool sim_allowed = dyn == nullptr ||
				(dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC) ||
				!attached_shapes[p_shape_idx].shape_blueprint ||
				(attached_shapes[p_shape_idx].shape_blueprint->is_convex() &&
						attached_shapes[p_shape_idx].shape_blueprint->get_type() != PhysicsServer3D::SHAPE_SEPARATION_RAY);
		attached_shapes[p_shape_idx].shape_instance->setFlag(
				physx::PxShapeFlag::eSIMULATION_SHAPE, !p_disabled && sim_allowed);
		attached_shapes[p_shape_idx].shape_instance->setFlag(
				physx::PxShapeFlag::eSCENE_QUERY_SHAPE, !p_disabled);
	}
}

void PhysXBody3D::set_shape(int p_shape_idx, PhysXShape3D *p_shape) {
	ERR_FAIL_INDEX(p_shape_idx, get_shape_count());
	ERR_FAIL_NULL(p_shape);

	// Godot's body_set_shape keeps the slot's transform and disabled state
	// AND the other slots' order: remove_shape shifts everything after the
	// slot down one, so SWAP would leave them reversed. Move the
	// freshly-appended record to the original index instead.
	const Transform3D saved_transform = attached_shapes[p_shape_idx].relative_transform;
	const bool saved_disabled = attached_shapes[p_shape_idx].disabled;

	remove_shape(p_shape_idx);
	add_shape(p_shape, saved_transform, saved_disabled);

	const int last_idx = get_shape_count() - 1;
	if (last_idx != p_shape_idx) {
		AttachedShape rec = attached_shapes[last_idx];
		attached_shapes.remove_at(last_idx);
		attached_shapes.insert(p_shape_idx, rec);
	}
}

// ---------------------------------------------------------------------------
// Per-step hooks
// ---------------------------------------------------------------------------

/**
 * Pre-step hook, called by PhysXSpace3D::step() before simulate(): clears the
 * contact buffer, resolves overlapping-area gravity/damping overrides (plus
 * wind), applies the gravity-scale correction delta and the constant force/
 * torque accumulators, and invokes the custom integrator instead of all of
 * that when omit_force_integration is set. See the sleep-cadence contract
 * at the integrator call below.
 */
void PhysXBody3D::on_pre_step(float p_step) {
	if (!px_actor) {
		return;
	}

	physx::PxRigidDynamic *dyn = get_px_dynamic();
	if (!dyn) {
		contacts.clear();
		return;
	}

	// A fully-sleeping pair no longer runs narrowphase in PhysX, so no
	// onContact events arrive while asleep. Keep the previous step's contacts
	// as a pre-sleep snapshot (served via get_report_contacts) and only clear
	// the live buffer while awake. Note the sleep transition happens DURING
	// the step, so the pre-step still sees the body awake one last time —
	// that final clear must preserve the snapshot, hence copy-then-clear.
	if (!dyn->isSleeping()) {
		last_step_contacts = contacts;
		contacts.clear();
	}

	// Kinematic and static bodies don't take forces/damping from the solver —
	// PhysX forbids addForce/addTorque/setLinearDamping on them. Kinematics are
	// moved exclusively via setKinematicTarget(). We still clear contacts and
	// run the custom integrator below so CharacterBody3D gets its state sync.
	const bool is_simulated = (mode != PhysicsServer3D::BODY_MODE_KINEMATIC &&
			mode != PhysicsServer3D::BODY_MODE_STATIC);

	// A running joint motor keeps this body awake: PhysX drives do not wake a
	// sleeping body, so a slow motor (a low force limit on a heavy body) let it
	// rest before it got going. Waking every pre-step resets the wake counter;
	// once no connected motor reports running, the body sleeps normally.
	if (is_simulated) {
		for (PhysXJoint3D *joint : joints) {
			if (joint->has_running_motor()) {
				dyn->wakeUp();
				break;
			}
		}
	}

	// --- Resolve area overrides in priority order (highest first) ---
	// Sort a lightweight index list ascending by priority; iterate descending.
	// The index buffer is a member (cleared per call) so a steady-state step
	// does not allocate per body.
	const int n = (int)overlapping_areas.size();
	_area_order_scratch.clear();
	LocalVector<int> &order = _area_order_scratch;
	if (n > 0) {
		order.resize(n);
		for (int i = 0; i < n; i++) {
			order[i] = i;
		}
		// Simple insertion sort — n is small (overlapping areas per body).
		for (int i = 1; i < n; i++) {
			int key = order[i];
			int kp = overlapping_areas[key]->get_priority();
			int j = i - 1;
			while (j >= 0 && overlapping_areas[order[j]]->get_priority() > kp) {
				order[j + 1] = order[j];
				j--;
			}
			order[j + 1] = key;
		}
	}

	const Vector3 body_pos = [&] {
		const physx::PxVec3 p = dyn->getGlobalPose().p;
		return Vector3(p.x, p.y, p.z);
	}();

	Vector3 total_gravity(0, 0, 0);
	real_t total_linear_damp = 0.0f;
	real_t total_angular_damp = 0.0f;
	bool gravity_done = false, linear_done = false, angular_done = false;

	for (int idx = n - 1; idx >= 0 && !(gravity_done && linear_done && angular_done); idx--) {
		const PhysXArea3D *area = overlapping_areas[order[idx]];
		if (!gravity_done) {
			// Computed inside the getter so DISABLED-mode areas (and the
			// point-gravity math) are skipped entirely.
			gravity_done = physx_apply_area_override(total_gravity, area->get_gravity_override_mode(),
					[&] { return physx_area_gravity_at(*area, body_pos); });
		}
		if (!linear_done) {
			linear_done = physx_apply_area_override(total_linear_damp, area->get_linear_damp_override_mode(),
					[&] { return area->get_linear_damp(); });
		}
		if (!angular_done) {
			angular_done = physx_apply_area_override(total_angular_damp, area->get_angular_damp_override_mode(),
					[&] { return area->get_angular_damp(); });
		}
	}

	// Area wind (World3D wind force/attenuation params). Stock Godot applies
	// area wind only to SoftBody3D; this backend also pushes rigid bodies,
	// matching the Area3D wind model. Wind is additive across areas and zero
	// unless an area sets wind_force_magnitude, so scenes that don't use it
	// behave identically.
	Vector3 total_wind(0, 0, 0);
	for (int idx = 0; idx < n; idx++) {
		const PhysXArea3D *area = overlapping_areas[order[idx]];
		if (area->has_wind()) {
			total_wind += area->wind_at(body_pos);
		}
	}

	// Additive fallback from the space's default area (priority -1) for any
	// channel not resolved above. Mirrors godot_physics_3d::integrate_forces.
	if (space && space->get_default_area()) {
		const PhysXArea3D *def = space->get_default_area();
		if (!gravity_done) {
			total_gravity += physx_area_gravity_at(*def, body_pos);
		}
		if (!linear_done) {
			total_linear_damp += def->get_linear_damp();
		}
		if (!angular_done) {
			total_angular_damp += def->get_angular_damp();
		}
	}

	// Apply the body's own damp mode (after areas + default).
	switch (linear_damp_mode) {
		case PhysicsServer3D::BODY_DAMP_MODE_COMBINE:
			total_linear_damp += linear_damp;
			break;
		case PhysicsServer3D::BODY_DAMP_MODE_REPLACE:
			total_linear_damp = linear_damp;
			break;
	}
	switch (angular_damp_mode) {
		case PhysicsServer3D::BODY_DAMP_MODE_COMBINE:
			total_angular_damp += angular_damp;
			break;
		case PhysicsServer3D::BODY_DAMP_MODE_REPLACE:
			total_angular_damp = angular_damp;
			break;
	}

	// --- Apply gravity ---
	// Native scene gravity is the base gravity for dynamic bodies, so a resting
	// body can reach PhysX's sleep threshold. (A per-step addForce of the full
	// gravity wakes the body every step and resets the sleep accumulator, which
	// prevented auto-sleep.) This PhysX version has no per-actor gravity scale,
	// so native gravity stays at scene strength and on_pre_step applies only the
	// correction delta:
	//     delta = total_gravity * gravity_scale - scene_gravity
	// which is zero in the default (gravity_scale == 1, no area override) case.
	// A vehicle2 chassis and a custom force integrator own their gravity, so
	// native gravity is disabled for them (eDISABLE_GRAVITY). PhysX forbids
	// addForce/damping on kinematic/static bodies; the cached totals below keep
	// get_total_gravity/get_total_*_damp correct regardless.
	if (is_simulated) {
		const bool use_native = !is_vehicle_chassis && !omit_force_integration;
		// Flag writes are not free in PhysX (they dirty the actor); only push
		// when the resolved state actually flips.
		if (gravity_disabled_cached != !use_native) {
			gravity_disabled_cached = !use_native;
			dyn->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !use_native);
		}

		const bool apply_forces = !omit_force_integration && !dyn->isSleeping();
		if (apply_forces) {
			// Apply the correction delta on top of native gravity (skipped for
			// the vehicle chassis / integrator, which apply their own gravity.
			// Zero in the default case, so no addForce and bodies can
			// sleep.
			//
			// autowake=false: addForce(autowake=true) raises the wake counter
			// to the reset value on every call, which would keep any body with
			// a non-zero delta (gravity_scale != 1, area overrides, constant
			// forces) awake forever. These forces are applied only while the
			// body is awake and must not reset its sleep accumulation.
			if (use_native) {
				// Cached on the space at scene creation (gravity is never
				// mutated afterward); was a PxScene::getGravity() virtual per
				// awake body per step.
				const Vector3 scene_gravity = space ? space->get_scene_gravity() : Vector3();
				const Vector3 delta = (total_gravity * (float)gravity_scale) - scene_gravity;
				if (!delta.is_zero_approx()) {
					dyn->addForce(physx::PxVec3((float)delta.x, (float)delta.y, (float)delta.z),
							physx::PxForceMode::eACCELERATION, false);
				}
			}

			if (!constant_force.is_zero_approx()) {
				dyn->addForce(physx::PxVec3((float)constant_force.x, (float)constant_force.y, (float)constant_force.z),
						physx::PxForceMode::eFORCE, false);
			}
			if (!constant_torque.is_zero_approx()) {
				dyn->addTorque(physx::PxVec3((float)constant_torque.x, (float)constant_torque.y, (float)constant_torque.z),
						physx::PxForceMode::eFORCE, false);
			}
		}

		if (!dyn->isSleeping()) {
			// Godot skips damping entirely when a custom force integrator is
			// set (godot_body_3d integrate_forces guards the whole velocity
			// update on omit_force_integration) — zero the solver damping so
			// PhysX does not double-damp on top of the custom integrator.
			dyn->setLinearDamping((float)(omit_force_integration ? 0.0f : total_linear_damp));
			dyn->setAngularDamping((float)(omit_force_integration ? 0.0f : total_angular_damp));
		}

		// Wind is applied OUTSIDE the sleep gate: a constant wind must keep
		// acting on (and therefore wake) a resting body, unlike the gravity
		// delta which is deliberately force-free in the default case so
		// bodies can sleep.
		if (!total_wind.is_zero_approx()) {
			dyn->addForce(physx::PxVec3((float)total_wind.x, (float)total_wind.y, (float)total_wind.z),
					physx::PxForceMode::eFORCE);
		}
	}

	// Cache the resolved totals so get_total_gravity/get_total_*_damp are correct.
	cached_total_gravity = total_gravity * (float)gravity_scale;
	cached_total_linear_damp = total_linear_damp;
	cached_total_angular_damp = total_angular_damp;

	// Godot runs _integrate_forces only for bodies that integrate —
	// a sleeping body gets no callback. The cadence matches godot_physics
	// without a synthesized extra call: the last invocation fires on the last
	// awake step (isSleeping() still reads false at that pre-step; PhysX puts
	// the body to sleep during the solve that follows). Kinematic bodies
	// never sleep in PhysX, so the gate leaves their callbacks untouched.
	if (force_integration_callback.is_valid() && !dyn->isSleeping()) {
		Variant state_variant = get_direct_state();
		const Variant *vp[2] = { &state_variant, &force_integration_userdata };
		Callable::CallError ce;
		int argc = (force_integration_userdata.get_type() == Variant::NIL) ? 1 : 2;
		Variant rv;
		force_integration_callback.callp(vp, argc, rv, ce);
	}
}

void PhysXBody3D::on_post_step(float p_step) {
	if (!px_actor) {
		return;
	}

	// Derive velocity for kinematic bodies (AnimatableBody3D) BEFORE firing the
	// callback so _body_state_changed reads correct values.
	_update_kinematic_velocity(p_step);

	// Fire the state sync callback so Godot nodes can read the new transform.
	// RigidBody3D._body_state_changed(PhysicsDirectBodyState3D*) is the
	// canonical signature, so the DirectBodyState pointer is passed as a Variant.
	if (state_sync_callback.is_valid()) {
		PhysXDirectBodyState3D *state = get_direct_state();
		state->set_step(p_step);
		Variant state_variant = state;
		state_sync_callback.call(state_variant);
	}
}

void PhysXBody3D::_update_shapes() {
	// Re-apply collision filter data to all shapes in case layer/mask changed.
	update_shapes_collision_filter();
}

PhysXDirectBodyState3D *PhysXBody3D::get_direct_state() {
	if (!direct_state) {
		direct_state = memnew(PhysXDirectBodyState3D(this));
	}
	return direct_state;
}

void PhysXBody3D::_on_shape_added() {
	update_inertia();

	// Add the body to the PhysX scene now that shapes are attached.
	// The inertia tensor is computed from the shape geometry before the
	// body enters the simulation. Only do this once.
	if (space && px_actor && !body_added_to_scene) {
		_add_to_scene();
	}
}

void PhysXBody3D::_add_to_scene() {
	if (!px_actor || !space || body_added_to_scene) {
		return;
	}
	space->add_actor(px_actor);
	body_added_to_scene = true;
	// Forces/impulses buffered while the actor was sceneless (before entering
	// a world) are now legal to apply — PhysX's addForce needs the BodySim
	// that scene membership creates.
	_flush_pending_force_ops();
	// Dormant joints configured against this body can now come alive: the
	// rebuild is idempotent and quietly stays dormant for the ones that still
	// lack a dynamic actor. This also covers the fresh actor after a
	// static<->dynamic mode switch (set_mode -> rebuild_shapes -> here).
	for (PhysXJoint3D *joint : joints) {
		joint->rebuild();
	}
}

void PhysXBody3D::_emit_all_area_exits() {
	// The caller is about to tear the actor down (free, space change, or the
	// static<->dynamic mode flip): the trigger pairs return as REMOVED, which
	// the simulation event callback skips, so overlaps would leak. Queue the
	// body_monitor dispatch for flush_queries(). Identity is cached because
	// the dispatch must not dereference this body.
	for (unsigned int i = 0; i < overlapping_areas.size(); i++) {
		PhysXArea3D *area = overlapping_areas[i];
		area->remove_overlapping_body(this);
		PhysXSpace3D::TriggerEvent ev;
		ev.trigger_area = area;
		ev.body = this;
		ev.body_rid = get_rid();
		ev.body_id = get_instance_id();
		ev.status = PhysicsServer3D::AREA_BODY_REMOVED;
		ev.other_shape_idx = 0;
		ev.area_shape_idx = 0;
		ev.is_area_vs_area = false;
		space->queue_trigger(ev);
	}
	overlapping_areas.clear();
}

void PhysXBody3D::_on_shape_removed() {
	update_inertia();
}

void PhysXBody3D::_on_shape_geometry_changed() {
	update_inertia();
}

void PhysXBody3D::_on_shape_transform_changed() {
	update_inertia();
}

void PhysXBody3D::shape_changed(PhysXShape3D *p_shape) {
	PhysXShapedObject3D::shape_changed(p_shape);
	// If the body is not yet in a scene, the wakeUp() call above would fail.
	// Re-wake only if the body is now in a scene (shape may have been added
	// while the body was still being initialized). Kinematic dynamics reject
	// wakeUp() ("Body must be non-kinematic!") and never sleep, so skip them.
	if (px_actor && px_actor->is<physx::PxRigidDynamic>() && body_added_to_scene &&
			mode != PhysicsServer3D::BODY_MODE_KINEMATIC) {
		static_cast<physx::PxRigidDynamic *>(px_actor)->wakeUp();
	}
}

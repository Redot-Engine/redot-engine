/**************************************************************************/
/*  physx_vehicle_3d.cpp                                                  */
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

#include "physx_vehicle_3d.h"

#include "../physx_conversions.h"
#include "../physx_project_settings.h"
#include "../physx_server.h"
#include "../shapes/physx_user_data.h"
#include "../spaces/physx_space_3d.h"
#include "physx_vehicle4w.h"
#include "physx_vehicle_wheel_3d.h"

#include "core/config/engine.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/world_3d.h"

struct PhysXVehicle3D::Impl {
	Vehicle4W vehicle;
	PxVehiclePhysXSimulationContext simulationContext;
	PxScene *scene = nullptr;
	PhysXSpace3D *space = nullptr; // for ensure_synced in _destroy
	PxReal sleep_threshold = 0.0f;
	PxReal time_before_sleep = 0.0f;
	bool built = false;
	// Attached to the chassis actor's userData (the contact-modify combiner
	// reads its signed friction/bounce during the solve). object stays
	// nullptr — the chassis is not a PhysicsServer3D object.
	PhysXActorUserData chassis_user_data;
	// wheel_order[Vehicle4W::WHEEL_FL/FR/RL/RR] = index into the parent's own
	// `wheels` vector (child-registration order) for that canonical slot --
	// see configure_vehicle4w()'s own doc comment.
	PxU32 wheel_order[4] = {};
};

PhysXVehicle3D::PhysXVehicle3D() {
	impl = memnew(Impl);
}

PhysXVehicle3D::~PhysXVehicle3D() {
	_destroy();
	chassis_exceptions.dispose();
	memdelete(impl);
}

bool PhysXVehicle3D::_build() {
	if (impl->built) {
		return true;
	}
	// PxVehicleRigidBodyComponent integrates gravity/velocity into the
	// chassis pose itself, inside v.step() -- independent of whether
	// PxScene::simulate() is ever called. In the editor (not Play), nothing
	// steps the scene, but NOTIFICATION_INTERNAL_PHYSICS_PROCESS still fires
	// on this node, so without this guard the car just falls forever right
	// in the viewport the moment it's placed. Same precedent as
	// PhysXDestructible3D: no physics runs in the editor at all, only real
	// gameplay (Engine::is_editor_hint() == false).
	if (Engine::get_singleton()->is_editor_hint()) {
		return false;
	}
	if (!is_inside_world() || get_world_3d().is_null()) {
		return false;
	}
	if (wheels.size() != 4) {
		// Not a real error -- this fires transiently while wheel children are
		// still entering the tree one at a time (see PhysXVehicleWheel3D's
		// own NOTIFICATION_ENTER_TREE), and permanently if the scene author
		// hasn't added exactly 4 yet. get_configuration_warnings() surfaces
		// the permanent case in the Inspector.
		return false;
	}

	CollisionShape3D *chassis_shape_node = nullptr;
	for (int i = 0; i < get_child_count(); i++) {
		chassis_shape_node = Object::cast_to<CollisionShape3D>(get_child(i));
		if (chassis_shape_node) {
			break;
		}
	}
	if (!chassis_shape_node || chassis_shape_node->get_shape().is_null()) {
		return false;
	}
	Ref<BoxShape3D> box_shape = chassis_shape_node->get_shape();
	if (box_shape.is_null()) {
		ERR_PRINT("PhysXVehicle3D: chassis CollisionShape3D must use a BoxShape3D.");
		return false;
	}

	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server) {
		return false;
	}
	PhysXSpace3D *space = server->get_space(get_world_3d()->get_space());
	if (!space) {
		// Not running on the PhysX backend -- this node offers PxVehicle2-
		// specific capability and has no fallback for another backend.
		return false;
	}
	// A rebuild or a deferred build can land in the async in-flight window:
	// the scene add/remove below must not run during a solve.
	space->ensure_synced();
	PxPhysics *physics = space->get_px_physics();
	PxScene *scene = space->get_px_scene();
	if (!physics || !scene) {
		return false;
	}

	Vehicle4WConfig config;
	config.mass = mass;
	config.moment_of_inertia = moment_of_inertia;
	config.chassis_half_extents = box_shape->get_size() * 0.5;
	config.chassis_box_center_local = chassis_shape_node->get_position();
	config.chassis_com_local = center_of_mass_mode == CENTER_OF_MASS_MODE_CUSTOM ? center_of_mass : chassis_shape_node->get_position();
	config.max_engine_torque = max_engine_torque;
	config.max_brake_torque = max_brake_torque;
	config.handbrake_torque = handbrake_torque;
	config.max_steer_angle = max_steer_angle;
	config.ackermann_strength = ackermann_strength;
	config.front_anti_roll_stiffness = front_anti_roll_stiffness;
	config.rear_anti_roll_stiffness = rear_anti_roll_stiffness;
	config.collision_layer = collision_layer;
	config.collision_mask = collision_mask;
	{
		real_t friction, bounce;
		_chassis_material(friction, bounce);
		config.chassis_friction = (float)friction;
		config.chassis_bounce = (float)bounce;
	}
	config.engineDrive = use_gearbox;
	if (use_gearbox) {
		config.engine_peak_torque = engine_peak_torque;
		config.engine_idle_omega = engine_idle_omega;
		config.engine_max_omega = engine_max_omega;
		config.clutch_strength = clutch_strength;
		config.gear_final_ratio = gear_final_ratio;
		config.gear_switch_time = gear_switch_time;
		config.autobox_latency = autobox_latency;
		config.autobox_up_ratio = autobox_up_ratio;
		config.autobox_down_ratio = autobox_down_ratio;
		config.use_autobox = use_autobox;
		config.gear_neutral = 1;
		// Empty PackedFloat32Array = keep the composition's default ratio
		// table (Vehicle4WConfig::gear_ratios). Copying unconditionally left
		// nbRatios at 0, which fails PxVehicleGearboxParams::isValid and
		// silently no-ops the whole gearbox/engine-drivetrain update (an
		// idle-locked engine that never shifts or drives).
		if (gear_ratios.size() > 0) {
			config.gear_ratios.clear();
			for (int g = 0; g < gear_ratios.size(); g++) {
				config.gear_ratios.push_back(gear_ratios[g]);
			}
		}
	}

	for (int i = 0; i < 4; i++) {
		PhysXVehicleWheel3D *w = wheels[i];
		Vehicle4WWheelConfig &wc = config.wheels[i];
		wc.position = w->get_authored_position();
		wc.basis = w->get_authored_basis();
		wc.radius = w->get_radius();
		wc.half_width = w->get_half_width();
		wc.wheel_mass = w->get_wheel_mass();
		wc.wheel_moment_of_inertia = w->get_wheel_moment_of_inertia();
		wc.damping_rate = w->get_damping_rate();
		wc.suspension_travel = w->get_suspension_travel();
		wc.suspension_stiffness = w->get_suspension_stiffness();
		wc.suspension_damping = w->get_suspension_damping();
		wc.tire_lateral_stiffness = w->get_tire_lateral_stiffness();
		wc.tire_longitudinal_stiffness = w->get_tire_longitudinal_stiffness();
		wc.tire_camber_stiffness = w->get_tire_camber_stiffness();
		wc.tire_friction = w->get_tire_friction();
		wc.tire_rest_grip = w->get_tire_rest_grip();
		wc.tire_slide_grip = w->get_tire_slide_grip();
		wc.use_as_steering = w->is_used_as_steering();
		wc.use_as_traction = w->is_used_as_traction();
	}

	if (!configure_vehicle4w(impl->vehicle, config, *physics, *scene, impl->simulationContext, impl->wheel_order)) {
		return false;
	}

	Vehicle4W &v = impl->vehicle;
	v.physxActor.rigidBody->setGlobalPose(godot_to_px(get_global_transform()));
	impl->sleep_threshold = (PxReal)space->get_sleep_energy_threshold();
	impl->time_before_sleep = (PxReal)space->get_time_before_sleep();
	PxRigidDynamic *dynamic_body = v.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL_V(dynamic_body, false);
	dynamic_body->setSleepThreshold(can_sleep && PhysXProjectSettings::allow_sleep ? impl->sleep_threshold : 0.0f);
	dynamic_body->setWakeCounter(impl->time_before_sleep);
	scene->addActor(*v.physxActor.rigidBody);
	v.physxActor.rigidBody->setName("PhysXVehicle3D");
	// Chassis contact material: the contact-modify combiner reads the signed
	// friction/bounce from this userData during the solve (the chassis
	// PxMaterial authored in configure_vehicle4w is the GPU-path fallback).
	impl->chassis_user_data = PhysXActorUserData();
	impl->chassis_user_data.friction = config.chassis_friction;
	impl->chassis_user_data.bounce = config.chassis_bounce;
	v.physxActor.rigidBody->userData = &impl->chassis_user_data;

	impl->scene = scene;
	impl->space = space;
	impl->built = true;
	// Exceptions added before this build (or kept across a rebuild/world exit)
	// re-stamp the fresh chassis shapes and re-wire the wheel filter.
	if (chassis_exceptions.get_slot() != 0) {
		chassis_exceptions.get_or_alloc_slot(impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>());
		impl->vehicle.wheel_query_filter.self_exception_slot = chassis_exceptions.get_slot_ptr();
	}
	return true;
}

void PhysXVehicle3D::_destroy() {
	// chassis_exceptions deliberately survives: exceptions are node config,
	// and _destroy also runs on rebuilds/world exits (the fresh chassis is
	// re-stamped by _build when a slot exists).
	if (impl->built) {
		// A queue_free or a deferred rebuild can land in the async in-flight
		// window: the remove/release below must not run during a solve.
		if (impl->space) {
			impl->space->ensure_synced();
		}
		if (impl->scene) {
			impl->scene->removeActor(*impl->vehicle.physxActor.rigidBody);
		}
		impl->vehicle.destroy();
		impl->built = false;
		impl->scene = nullptr;
		impl->space = nullptr;
	}
}

void PhysXVehicle3D::_rebuild_if_live() {
	if (!is_inside_world()) {
		// Not in the tree/world yet -- NOTIFICATION_ENTER_WORLD will do the
		// real first build once it is.
		return;
	}
	// Unconditional, not just "if already built" -- a wheel child can finish
	// registering (e.g. the 4th of 4, added after this node's own
	// NOTIFICATION_ENTER_WORLD already ran and failed with too few wheels)
	// and that later success needs a real attempt here, not just a rebuild
	// of something that was never built. Also toggles physics processing
	// itself -- NOTIFICATION_ENTER_WORLD isn't the only path that can
	// transition built false->true (a wheel registering after is exactly
	// that), so it can't be the only place this gets turned on.
	_destroy();
	set_physics_process_internal(_build());
}

void PhysXVehicle3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			set_physics_process_internal(_build());
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			set_physics_process_internal(false);
			_destroy();
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (!impl->built) {
				break;
			}
			// Two-way transform contract: the node's transform normally
			// follows the chassis (synced below), so a mismatch here means a
			// script wrote global_transform since our last sync -- push it
			// INTO the chassis, hard-setting the pose and zeroing all
			// velocities (reset/respawn semantics, RigidBody3D-style).
			{
				const Transform3D node_xform = get_global_transform();
				if (!_last_synced_valid || !node_xform.is_equal_approx(_last_synced_xform)) {
					if (PxRigidDynamic *dynamic_body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>()) {
						dynamic_body->setGlobalPose(godot_to_px(node_xform), true);
						dynamic_body->setLinearVelocity(PxVec3(0, 0, 0));
						dynamic_body->setAngularVelocity(PxVec3(0, 0, 0));
					}
					_last_synced_xform = node_xform;
					_last_synced_valid = true;
				}
			}
			Vehicle4W &v = impl->vehicle;
			v.commandState.brakes[0] = (PxReal)brake;
			v.commandState.brakes[1] = (PxReal)handbrake;
			v.commandState.nbBrakes = 2;
			v.commandState.steer = (PxReal)steer;
			if (v.engineDrive) {
				// Engine drive: the throttle is a 0..1 magnitude (never
				// negated -- the gear ratios' signs pick the roll direction,
				// see Vehicle4WConfig's ratio note) and the transmission
				// command carries clutch + target gear. `reverse` selects
				// the reverse gear; `target_gear` (255 = DRIVE) otherwise.
				v.commandState.throttle = (PxReal)throttle;
				v.engineTransmissionCommand.clutch = 0.0f;
				v.engineTransmissionCommand.targetGear = reverse ? 0u : (PxU32)target_gear;
			} else {
				// Direct drive: the throttle is a 0..1 magnitude; the
				// transmission command's gear selects the direction. The
				// composition's right-handed frame (lngAxis = eNegZ,
				// latAxis = eNegX) rolls the vehicle nose-first (-Z) on
				// positive command throttle (measured on 5.11) -- no flip.
				v.commandState.throttle = (PxReal)throttle;
				v.transmissionCommandState.gear = reverse ? PxVehicleDirectDriveTransmissionCommandState::eREVERSE : PxVehicleDirectDriveTransmissionCommandState::eFORWARD;
			}
			v.step((PxReal)get_physics_process_delta_time(), impl->simulationContext);

			// v.rigidBodyState.pose is CoM-relative, not the actor's real
			// origin (confirmed directly by comparing it against a raw
			// getGlobalPose() read while root-causing the chassis-collision
			// regression) -- the CollisionShape3D/wheel/mesh children are all
			// positioned relative to the actor's own origin (that's the frame
			// PxVehiclePhysXActorCreate cooked the real PxShape geometry
			// into), so this node's own transform has to track that same
			// origin, not the CoM. Using rigidBodyState.pose here left the
			// visual body a constant offset away from where the real
			// collision geometry actually sits at rest, and made that offset
			// visibly swing whenever the body rotated (braking, cornering, a
			// bump) since it rotates with the body.
			_last_synced_xform = px_to_godot(v.physxActor.rigidBody->getGlobalPose());
			_last_synced_valid = true;
			set_global_transform(_last_synced_xform);
			// Push each wheel's live pose (suspension jounce + steer angle +
			// roll spin, all baked into wheelLocalPoses by PxVehicleWheelComponent)
			// onto that wheel's own node -- same idea as VehicleBody3D's own
			// _update_wheel_transform(): the wheel NODE moves, and any
			// MeshInstance3D the scene author parented under it inherits that
			// motion automatically, no separate mesh-transform wiring needed.
			//
			// wheelLocalPoses[i].localPose is in the CoM frame, NOT the actor's
			// own origin frame (confirmed directly in PhysX's own write-back
			// code, VhPhysXActorFunctions.cpp: "Local pose in actor frame" is
			// computed as cmassLocalPose * wheelLocalPose, not wheelLocalPose
			// alone) -- this node's own transform above is the actor's real
			// origin, so composing wheelLocalPose directly onto it left every
			// wheel a constant offset away (equal to the CoM offset, ~0.35m by
			// default) from where it should be, visible as wheels sunk deep
			// into the ground regardless of wheel radius (a real regression
			// caught by an A/B on radius that showed the SAME absolute
			// penetration depth for two different radii -- ruling out anything
			// radius-proportional and pointing straight at a fixed frame
			// offset instead).
			const PxTransform cmass_local_pose = v.physxActor.rigidBody->getCMassLocalPose();
			for (uint32_t i = 0; i < 4; i++) {
				wheels[impl->wheel_order[i]]->set_transform(px_to_godot(cmass_local_pose * v.wheelLocalPoses[i].localPose));
			}
		} break;
	}
}

Vector3 PhysXVehicle3D::get_linear_velocity() const {
	if (!impl->built) {
		return Vector3();
	}
	return px_to_godot(impl->vehicle.rigidBodyState.linearVelocity);
}

real_t PhysXVehicle3D::get_forward_speed() const {
	if (!impl->built) {
		return 0.0;
	}
	// frame.getLngAxis() is a fixed LOCAL-frame constant (e.g. (0,0,-1)),
	// not a world-space direction -- rotate it into world space by the
	// actor's current orientation before dotting against the world-space
	// velocity, or this is only correct when yaw matches spawn orientation
	// (found via a real bug report: correct at first, sign-flips as the
	// vehicle yaws further away from its start heading).
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	// frame.getLngAxis() is a fixed LOCAL-frame constant (e.g. (0,0,-1)), not
	// a world-space direction -- rotate it into world space by the actor's
	// current orientation before dotting against the world-space velocity
	// (upstream fix: otherwise this is only correct when yaw matches spawn
	// orientation). Positive while driving forward, negative rolling backward.
	const PxVec3 fwd = actor_pose.q.rotate(impl->vehicle.frame.getLngAxis());
	return (real_t)impl->vehicle.rigidBodyState.linearVelocity.dot(fwd);
}

real_t PhysXVehicle3D::get_wheel_jounce(int p_wheel) const {
	if (!impl->built) {
		return 0.0;
	}
	ERR_FAIL_INDEX_V(p_wheel, (int)wheels.size(), 0.0);
	for (uint32_t i = 0; i < 4; i++) {
		if (impl->wheel_order[i] == (uint32_t)p_wheel) {
			return (real_t)impl->vehicle.suspensionStates[i].jounce;
		}
	}
	return 0.0;
}

real_t PhysXVehicle3D::get_wheel_separation(int p_wheel) const {
	if (!impl->built) {
		return 0.0;
	}
	ERR_FAIL_INDEX_V(p_wheel, (int)wheels.size(), 0.0);
	for (uint32_t i = 0; i < 4; i++) {
		if (impl->wheel_order[i] == (uint32_t)p_wheel) {
			return (real_t)impl->vehicle.suspensionStates[i].separation;
		}
	}
	return 0.0;
}

float PhysXVehicle3D::get_engine_rpm() const {
	if (!impl->built || !impl->vehicle.engineDrive) {
		return 0.0;
	}
	return (real_t)impl->vehicle.engineState.rotationSpeed * (60.0f / 6.28318530717958647692f);
}

int PhysXVehicle3D::get_engine_gear() const {
	if (!impl->built || !impl->vehicle.engineDrive) {
		return -1;
	}
	return (int)impl->vehicle.gearboxState.currentGear;
}

void PhysXVehicle3D::_update_live_wheel_tire_friction(PhysXVehicleWheel3D *p_wheel) {
	if (!impl->built) {
		// Not live -- the authored value is picked up at the next build.
		return;
	}
	int cfg_index = -1;
	for (uint32_t i = 0; i < wheels.size(); i++) {
		if (wheels[i] == p_wheel) {
			cfg_index = (int)i;
			break;
		}
	}
	if (cfg_index < 0) {
		return; // still registering
	}
	for (uint32_t i = 0; i < 4; i++) {
		if (impl->wheel_order[i] == (uint32_t)cfg_index) {
			Vehicle4W &v = impl->vehicle;
			const PxReal friction = (PxReal)p_wheel->get_tire_friction();
			// Same curve shape configure_vehicle4w() authors, rescaled to the
			// new friction: peak grip at ~10% slip, rest/slide shoulders
			// scaled by their authored grip factors.
			v.physxMaterialFrictionParams[i].defaultFriction = friction;
			v.tireForceParams[i].frictionVsSlip[0][1] = (PxReal)p_wheel->get_tire_rest_grip() * friction;
			v.tireForceParams[i].frictionVsSlip[1][1] = friction;
			v.tireForceParams[i].frictionVsSlip[2][1] = (PxReal)p_wheel->get_tire_slide_grip() * friction;
			return;
		}
	}
}

float PhysXVehicle3D::get_clutch() const {
	if (!impl->built || !impl->vehicle.engineDrive) {
		return 0.0;
	}
	// normalisedCommandResponse is the [0,1] engagement the property doc
	// promises; commandResponse is strength-scaled (up to ~40 at the default
	// clutch strength).
	return (real_t)impl->vehicle.clutchResponseState.normalisedCommandResponse;
}

float PhysXVehicle3D::get_wheel_rpm(int p_wheel) const {
	if (!impl->built) {
		return 0.0;
	}
	ERR_FAIL_INDEX_V(p_wheel, (int)wheels.size(), 0.0);
	for (uint32_t i = 0; i < 4; i++) {
		if (impl->wheel_order[i] == (uint32_t)p_wheel) {
			return (real_t)impl->vehicle.wheelRigidBody1dStates[i].rotationSpeed * (60.0f / 6.28318530717958647692f);
		}
	}
	return 0.0;
}

float PhysXVehicle3D::get_wheel_skid(int p_wheel) const {
	if (!impl->built) {
		return 0.0;
	}
	ERR_FAIL_INDEX_V(p_wheel, (int)wheels.size(), 0.0);
	for (uint32_t i = 0; i < 4; i++) {
		if (impl->wheel_order[i] == (uint32_t)p_wheel) {
			return (real_t)impl->vehicle.tireSlipStates[i].slips[physx::PxVehicleTireDirectionModes::eLONGITUDINAL];
		}
	}
	return 0.0;
}

Dictionary PhysXVehicle3D::get_wheel_contact(int p_wheel) const {
	Dictionary out;
	out["contact"] = false;
	out["normal"] = Vector3(0, 1, 0);
	if (!impl->built) {
		return out;
	}
	ERR_FAIL_INDEX_V(p_wheel, (int)wheels.size(), out);
	for (uint32_t i = 0; i < 4; i++) {
		if (impl->wheel_order[i] == (uint32_t)p_wheel) {
			const PxVehicleRoadGeometryState &rg = impl->vehicle.roadGeomStates[i];
			out["contact"] = rg.hitState ? true : false;
			if (rg.hitState) {
				out["normal"] = Vector3((real_t)rg.plane.n.x, (real_t)rg.plane.n.y, (real_t)rg.plane.n.z);
			}
			return out;
		}
	}
	return out;
}

float PhysXVehicle3D::get_wheel_steer_angle(int p_wheel) const {
	if (!impl->built) {
		return 0.0;
	}
	ERR_FAIL_INDEX_V(p_wheel, (int)wheels.size(), 0.0);
	for (uint32_t i = 0; i < 4; i++) {
		if (impl->wheel_order[i] == (uint32_t)p_wheel) {
			return (real_t)impl->vehicle.steerCommandResponseStates[i];
		}
	}
	return 0.0;
}

Vector3 PhysXVehicle3D::get_actor_position() const {
	if (!impl->built) {
		return Vector3();
	}
	return px_to_godot(impl->vehicle.physxActor.rigidBody->getGlobalPose().p);
}

PackedStringArray PhysXVehicle3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	bool has_box_shape = false;
	for (int i = 0; i < get_child_count(); i++) {
		CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(get_child(i));
		if (cs && cs->get_shape().is_valid() && Object::cast_to<BoxShape3D>(cs->get_shape().ptr())) {
			has_box_shape = true;
			break;
		}
	}
	if (!has_box_shape) {
		warnings.push_back(RTR("PhysXVehicle3D needs a CollisionShape3D child with a BoxShape3D for its chassis."));
	}
	int nb_steering = 0;
	for (uint32_t i = 0; i < wheels.size(); i++) {
		if (wheels[i]->is_used_as_steering()) {
			nb_steering++;
		}
	}
	if (wheels.size() != 4) {
		warnings.push_back(vformat(RTR("PhysXVehicle3D needs exactly 4 PhysXVehicleWheel3D children (has %d)."), (int)wheels.size()));
	} else if (nb_steering != 2) {
		warnings.push_back(vformat(RTR("PhysXVehicle3D needs exactly 2 wheels with use_as_steering enabled (has %d)."), nb_steering));
	}
	// Custom gear table validation (VEHN-8): an invalid table fails
	// PxVehicleGearboxParams::isValid, which silently disables the whole
	// engine drivetrain -- surface it here instead.
	if (use_gearbox && gear_ratios.size() >= 3) {
		if (gear_ratios[1] != 0.0f) {
			warnings.push_back(RTR("Gear table index 1 (neutral) must be exactly 0.0, or the gearbox cannot engage neutral."));
		}
		if (gear_ratios[0] >= 0.0f) {
			warnings.push_back(RTR("Gear table index 0 (reverse) must be negative."));
		}
		bool fwd_ok = true;
		for (int i = 2; i < gear_ratios.size(); i++) {
			if (gear_ratios[i] <= 0.0f) {
				fwd_ok = false;
				break;
			}
		}
		if (!fwd_ok) {
			warnings.push_back(RTR("Forward gear ratios (indices 2+) must all be positive."));
		}
	}
	return warnings;
}

#define PHYSX_VEHICLE_SETTER(m_name, m_field)       \
	void PhysXVehicle3D::set_##m_name(real_t p_v) { \
		m_field = p_v;                              \
		_rebuild_if_live();                         \
	}

void PhysXVehicle3D::set_mass(real_t p_mass) {
	mass = p_mass;
	// Wheel gizmos estimate rest ride height from this vehicle's own mass
	// (sprung_mass = mass * 0.25, matching configure_vehicle4w()) -- refresh
	// them so the editor preview stays accurate as mass is tuned.
	for (PhysXVehicleWheel3D *w : wheels) {
		w->update_gizmos();
	}
	_rebuild_if_live();
}
void PhysXVehicle3D::set_moment_of_inertia(const Vector3 &p_moi) {
	moment_of_inertia = p_moi;
	_rebuild_if_live();
}
void PhysXVehicle3D::set_center_of_mass_mode(CenterOfMassMode p_mode) {
	if (center_of_mass_mode == p_mode) {
		return;
	}
	center_of_mass_mode = p_mode;
	notify_property_list_changed();
	_rebuild_if_live();
}
void PhysXVehicle3D::set_center_of_mass(const Vector3 &p_center_of_mass) {
	if (center_of_mass == p_center_of_mass) {
		return;
	}
	ERR_FAIL_COND(center_of_mass_mode != CENTER_OF_MASS_MODE_CUSTOM);
	center_of_mass = p_center_of_mass;
	_rebuild_if_live();
}
void PhysXVehicle3D::set_can_sleep(bool p_can_sleep) {
	if (can_sleep == p_can_sleep) {
		return;
	}
	can_sleep = p_can_sleep;
	if (impl->built) {
		PxRigidDynamic *body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>();
		ERR_FAIL_NULL(body);
		body->setSleepThreshold(can_sleep && PhysXProjectSettings::allow_sleep ? impl->sleep_threshold : 0.0f);
		body->setWakeCounter(impl->time_before_sleep);
		if (!can_sleep) {
			body->wakeUp();
		}
	}
}

bool PhysXVehicle3D::is_sleeping() const {
	PxRigidDynamic *body = impl->built ? impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>() : nullptr;
	return body && body->isSleeping();
}
PHYSX_VEHICLE_SETTER(max_engine_torque, max_engine_torque)
PHYSX_VEHICLE_SETTER(max_brake_torque, max_brake_torque)

void PhysXVehicle3D::set_use_gearbox(bool p_enabled) {
	use_gearbox = p_enabled;
	_rebuild_if_live();
}

void PhysXVehicle3D::set_engine_peak_torque(real_t p_v) {
	engine_peak_torque = MAX(p_v, 1.0f);
	_rebuild_if_live();
}

void PhysXVehicle3D::set_engine_idle_omega(real_t p_v) {
	engine_idle_omega = MAX(p_v, 1.0f);
	_rebuild_if_live();
}

void PhysXVehicle3D::set_engine_max_omega(real_t p_v) {
	engine_max_omega = MAX(engine_idle_omega + 1.0f, p_v);
	_rebuild_if_live();
}

void PhysXVehicle3D::set_clutch_strength(real_t p_v) {
	clutch_strength = MAX(p_v, 0.1f);
	_rebuild_if_live();
}

void PhysXVehicle3D::set_gear_ratios(const PackedFloat32Array &p_ratios) {
	// Gear 0 = reverse, gear 1 = neutral (0.0), gears 2.. = forward. Sign
	// convention (PxVehicleDrivetrainParams + Vehicle4WConfig's own note):
	// REVERSE ratios are NEGATIVE, forward ratios POSITIVE -- an inverted
	// table fails PxVehicleGearboxParams::isValid, which silently disables
	// the whole engine drivetrain.
	gear_ratios = p_ratios;
	_rebuild_if_live();
}

PackedFloat32Array PhysXVehicle3D::get_gear_ratios() const {
	if (gear_ratios.size() > 0) {
		return gear_ratios;
	}
	// The composition's defaults (authored there; mirrored here so the
	// inspector shows what a rebuild will actually use).
	PackedFloat32Array defaults;
	defaults.push_back(-4.0f);
	defaults.push_back(0.0f);
	defaults.push_back(4.0f);
	defaults.push_back(2.0f);
	defaults.push_back(1.5f);
	defaults.push_back(1.1f);
	defaults.push_back(0.9f);
	return defaults;
}

void PhysXVehicle3D::set_gear_final_ratio(real_t p_v) {
	// Math::abs, not unqualified abs: the C overload set can pick abs(int),
	// truncating 3.5 to 3 (clamped 0.1) and silently changing the ratio.
	gear_final_ratio = MAX((float)Math::abs(p_v), 0.1f);
	_rebuild_if_live();
}

void PhysXVehicle3D::set_gear_switch_time(real_t p_v) {
	gear_switch_time = MAX(p_v, 0.05f);
	_rebuild_if_live();
}

void PhysXVehicle3D::set_autobox_latency(real_t p_v) {
	autobox_latency = MAX(p_v, 0.0f);
	_rebuild_if_live();
}

void PhysXVehicle3D::set_target_gear(int p_gear) {
	target_gear = CLAMP(p_gear, 0, 255);
}

void PhysXVehicle3D::set_use_autobox(bool p_enabled) {
	use_autobox = p_enabled;
	_rebuild_if_live();
}
PHYSX_VEHICLE_SETTER(handbrake_torque, handbrake_torque)
PHYSX_VEHICLE_SETTER(max_steer_angle, max_steer_angle)
PHYSX_VEHICLE_SETTER(ackermann_strength, ackermann_strength)
PHYSX_VEHICLE_SETTER(front_anti_roll_stiffness, front_anti_roll_stiffness)
PHYSX_VEHICLE_SETTER(rear_anti_roll_stiffness, rear_anti_roll_stiffness)

#undef PHYSX_VEHICLE_SETTER

void PhysXVehicle3D::set_collision_layer(uint32_t p_layer) {
	collision_layer = p_layer;
	_rebuild_if_live();
}
void PhysXVehicle3D::set_collision_mask(uint32_t p_mask) {
	collision_mask = p_mask;
	_rebuild_if_live();
}

void PhysXVehicle3D::set_physics_material_override(const Ref<PhysicsMaterial> &p_material) {
	if (physics_material_override == p_material) {
		return;
	}
	if (physics_material_override.is_valid()) {
		physics_material_override->disconnect_changed(callable_mp(this, &PhysXVehicle3D::_chassis_material_changed));
	}
	physics_material_override = p_material;
	if (physics_material_override.is_valid()) {
		physics_material_override->connect_changed(callable_mp(this, &PhysXVehicle3D::_chassis_material_changed));
	}
	_chassis_material_changed();
}

void PhysXVehicle3D::_chassis_material(real_t &r_friction, real_t &r_bounce) const {
	if (physics_material_override.is_valid()) {
		r_friction = physics_material_override->computed_friction();
		r_bounce = physics_material_override->computed_bounce();
	} else {
		// No override: the contact-modify combiner's defaults for an actor
		// without material userData — the behavior the chassis had before
		// this property existed (NOT the PxMaterial's 0.0/0.1, which the
		// combiner always overrode on the CPU path).
		r_friction = 1.0;
		r_bounce = 0.0;
	}
}

// The chassis material is the vehicle's own, so a change — or an edit to the
// assigned material — is applied in place, no rebuild.
void PhysXVehicle3D::_chassis_material_changed() {
	if (!impl->built) {
		return; // the next build reads it
	}
	real_t friction, bounce;
	_chassis_material(friction, bounce);
	// The userData floats are read by the worker-thread contact-modify
	// callback DURING the solve — fetch an in-flight solve first (same
	// pattern as PhysXBody3D::_apply_surface_params_to_actor).
	if (PhysXServer3D *server = PhysXServer3D::get_singleton()) {
		if (PhysXSpace3D *space = server->get_space(get_world_3d()->get_space())) {
			space->ensure_synced();
		}
	}
	impl->chassis_user_data.friction = (float)friction;
	impl->chassis_user_data.bounce = (float)bounce;
	// A material change doesn't wake the actor: one resting on its roof would
	// keep sleeping on the old grip.
	if (PxRigidDynamic *body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>()) {
		if (body->getScene()) {
			body->wakeUp();
		}
	}
}

void PhysXVehicle3D::_validate_property(PropertyInfo &p_property) const {
	if (center_of_mass_mode != CENTER_OF_MASS_MODE_CUSTOM && p_property.name == "center_of_mass") {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void PhysXVehicle3D::apply_force_at_local_position(const Vector3 &p_force, const Vector3 &p_local_position) {
	if (!impl->built) {
		return;
	}
	PxRigidDynamic *dynamic_body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL(dynamic_body);
	// eFORCE (not eIMPULSE): meant to be applied continuously across ticks
	// (autopilot steering, docking assists), like RigidBody3D.apply_force.
	PxRigidBodyExt::addForceAtLocalPos(*dynamic_body, godot_to_px(p_force),
			godot_to_px(p_local_position), PxForceMode::eFORCE);
}

void PhysXVehicle3D::apply_torque(const Vector3 &p_torque) {
	if (!impl->built) {
		return;
	}
	PxRigidDynamic *dynamic_body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL(dynamic_body);
	dynamic_body->addTorque(godot_to_px(p_torque), PxForceMode::eFORCE);
}

void PhysXVehicle3D::_ensure_exception_synced() {
	if (is_inside_world() && get_world_3d().is_valid()) {
		PhysXServer3D *server = PhysXServer3D::get_singleton();
		// Bound entry points reach this whether or not the vehicle is built;
		// with PhysX not the active physics backend the singleton is null.
		if (!server) {
			return;
		}
		if (PhysXSpace3D *space = server->get_space(get_world_3d()->get_space())) {
			space->ensure_synced();
		}
	}
}

void PhysXVehicle3D::add_collision_exception_with(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	_ensure_exception_synced();
	chassis_exceptions.add(impl->built ? impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>() : nullptr, p_node);
	if (impl->built) {
		impl->vehicle.wheel_query_filter.self_exception_slot = chassis_exceptions.get_slot_ptr();
	}
}

void PhysXVehicle3D::remove_collision_exception_with(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	_ensure_exception_synced();
	chassis_exceptions.remove(impl->built ? impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>() : nullptr, p_node);
}

Array PhysXVehicle3D::get_collision_exceptions() const {
	return chassis_exceptions.get_nodes();
}

uint32_t PhysXVehicle3D::vehicle_exception_slot_or_alloc() {
	_ensure_exception_synced();
	return chassis_exceptions.get_or_alloc_slot(impl->built ? impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>() : nullptr);
}

void PhysXVehicle3D::_bind_methods() {
	BIND_ENUM_CONSTANT(CENTER_OF_MASS_MODE_AUTO);
	BIND_ENUM_CONSTANT(CENTER_OF_MASS_MODE_CUSTOM);

	ClassDB::bind_method(D_METHOD("apply_force_at_local_position", "force", "local_position"), &PhysXVehicle3D::apply_force_at_local_position);
	ClassDB::bind_method(D_METHOD("apply_torque", "torque"), &PhysXVehicle3D::apply_torque);
	ClassDB::bind_method(D_METHOD("add_collision_exception_with", "node"), &PhysXVehicle3D::add_collision_exception_with);
	ClassDB::bind_method(D_METHOD("remove_collision_exception_with", "node"), &PhysXVehicle3D::remove_collision_exception_with);
	ClassDB::bind_method(D_METHOD("get_collision_exceptions"), &PhysXVehicle3D::get_collision_exceptions);
	ClassDB::bind_method(D_METHOD("set_mass", "mass"), &PhysXVehicle3D::set_mass);
	ClassDB::bind_method(D_METHOD("get_mass"), &PhysXVehicle3D::get_mass);
	ClassDB::bind_method(D_METHOD("set_moment_of_inertia", "moi"), &PhysXVehicle3D::set_moment_of_inertia);
	ClassDB::bind_method(D_METHOD("get_moment_of_inertia"), &PhysXVehicle3D::get_moment_of_inertia);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mass", PROPERTY_HINT_RANGE, "1,10000,1,or_greater"), "set_mass", "get_mass");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "moment_of_inertia"), "set_moment_of_inertia", "get_moment_of_inertia");
	ClassDB::bind_method(D_METHOD("set_center_of_mass_mode", "mode"), &PhysXVehicle3D::set_center_of_mass_mode);
	ClassDB::bind_method(D_METHOD("get_center_of_mass_mode"), &PhysXVehicle3D::get_center_of_mass_mode);
	ClassDB::bind_method(D_METHOD("set_center_of_mass", "center_of_mass"), &PhysXVehicle3D::set_center_of_mass);
	ClassDB::bind_method(D_METHOD("get_center_of_mass"), &PhysXVehicle3D::get_center_of_mass);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "center_of_mass_mode", PROPERTY_HINT_ENUM, "Auto,Custom"), "set_center_of_mass_mode", "get_center_of_mass_mode");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center_of_mass", PROPERTY_HINT_RANGE, "-10,10,0.01,or_less,or_greater,suffix:m"), "set_center_of_mass", "get_center_of_mass");
	ClassDB::bind_method(D_METHOD("set_can_sleep", "able_to_sleep"), &PhysXVehicle3D::set_can_sleep);
	ClassDB::bind_method(D_METHOD("is_able_to_sleep"), &PhysXVehicle3D::is_able_to_sleep);
	ClassDB::bind_method(D_METHOD("is_sleeping"), &PhysXVehicle3D::is_sleeping);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "can_sleep"), "set_can_sleep", "is_able_to_sleep");

	ClassDB::bind_method(D_METHOD("set_max_engine_torque", "value"), &PhysXVehicle3D::set_max_engine_torque);
	ClassDB::bind_method(D_METHOD("get_max_engine_torque"), &PhysXVehicle3D::get_max_engine_torque);
	ClassDB::bind_method(D_METHOD("set_max_brake_torque", "value"), &PhysXVehicle3D::set_max_brake_torque);
	ClassDB::bind_method(D_METHOD("get_max_brake_torque"), &PhysXVehicle3D::get_max_brake_torque);
	ClassDB::bind_method(D_METHOD("set_handbrake", "value"), &PhysXVehicle3D::set_handbrake);
	ClassDB::bind_method(D_METHOD("get_handbrake"), &PhysXVehicle3D::get_handbrake);
	ClassDB::bind_method(D_METHOD("set_handbrake_torque", "value"), &PhysXVehicle3D::set_handbrake_torque);
	ClassDB::bind_method(D_METHOD("get_handbrake_torque"), &PhysXVehicle3D::get_handbrake_torque);
	ClassDB::bind_method(D_METHOD("set_max_steer_angle", "value"), &PhysXVehicle3D::set_max_steer_angle);
	ClassDB::bind_method(D_METHOD("get_max_steer_angle"), &PhysXVehicle3D::get_max_steer_angle);
	ClassDB::bind_method(D_METHOD("set_use_gearbox", "enabled"), &PhysXVehicle3D::set_use_gearbox);
	ClassDB::bind_method(D_METHOD("get_use_gearbox"), &PhysXVehicle3D::get_use_gearbox);
	ClassDB::bind_method(D_METHOD("set_engine_peak_torque", "value"), &PhysXVehicle3D::set_engine_peak_torque);
	ClassDB::bind_method(D_METHOD("get_engine_peak_torque"), &PhysXVehicle3D::get_engine_peak_torque);
	ClassDB::bind_method(D_METHOD("set_engine_idle_omega", "value"), &PhysXVehicle3D::set_engine_idle_omega);
	ClassDB::bind_method(D_METHOD("get_engine_idle_omega"), &PhysXVehicle3D::get_engine_idle_omega);
	ClassDB::bind_method(D_METHOD("set_engine_max_omega", "value"), &PhysXVehicle3D::set_engine_max_omega);
	ClassDB::bind_method(D_METHOD("get_engine_max_omega"), &PhysXVehicle3D::get_engine_max_omega);
	ClassDB::bind_method(D_METHOD("set_clutch_strength", "value"), &PhysXVehicle3D::set_clutch_strength);
	ClassDB::bind_method(D_METHOD("get_clutch_strength"), &PhysXVehicle3D::get_clutch_strength);
	ClassDB::bind_method(D_METHOD("set_gear_ratios", "ratios"), &PhysXVehicle3D::set_gear_ratios);
	ClassDB::bind_method(D_METHOD("get_gear_ratios"), &PhysXVehicle3D::get_gear_ratios);
	ClassDB::bind_method(D_METHOD("set_gear_final_ratio", "value"), &PhysXVehicle3D::set_gear_final_ratio);
	ClassDB::bind_method(D_METHOD("get_gear_final_ratio"), &PhysXVehicle3D::get_gear_final_ratio);
	ClassDB::bind_method(D_METHOD("set_gear_switch_time", "value"), &PhysXVehicle3D::set_gear_switch_time);
	ClassDB::bind_method(D_METHOD("get_gear_switch_time"), &PhysXVehicle3D::get_gear_switch_time);
	ClassDB::bind_method(D_METHOD("set_autobox_latency", "value"), &PhysXVehicle3D::set_autobox_latency);
	ClassDB::bind_method(D_METHOD("get_autobox_latency"), &PhysXVehicle3D::get_autobox_latency);
	ClassDB::bind_method(D_METHOD("set_autobox_up_ratio", "value"), &PhysXVehicle3D::set_autobox_up_ratio);
	ClassDB::bind_method(D_METHOD("get_autobox_up_ratio"), &PhysXVehicle3D::get_autobox_up_ratio);
	ClassDB::bind_method(D_METHOD("set_autobox_down_ratio", "value"), &PhysXVehicle3D::set_autobox_down_ratio);
	ClassDB::bind_method(D_METHOD("get_autobox_down_ratio"), &PhysXVehicle3D::get_autobox_down_ratio);
	ClassDB::bind_method(D_METHOD("set_target_gear", "gear"), &PhysXVehicle3D::set_target_gear);
	ClassDB::bind_method(D_METHOD("get_target_gear"), &PhysXVehicle3D::get_target_gear);
	ClassDB::bind_method(D_METHOD("set_use_autobox", "enabled"), &PhysXVehicle3D::set_use_autobox);
	ClassDB::bind_method(D_METHOD("get_use_autobox"), &PhysXVehicle3D::get_use_autobox);
	ClassDB::bind_integer_constant(get_class_static(), "", "GEAR_REVERSE", 0);
	ClassDB::bind_integer_constant(get_class_static(), "", "GEAR_NEUTRAL", 1);
	ClassDB::bind_integer_constant(get_class_static(), "", "GEAR_AUTOMATIC", 255);
	ClassDB::bind_method(D_METHOD("set_ackermann_strength", "value"), &PhysXVehicle3D::set_ackermann_strength);
	ClassDB::bind_method(D_METHOD("get_ackermann_strength"), &PhysXVehicle3D::get_ackermann_strength);
	ClassDB::bind_method(D_METHOD("set_front_anti_roll_stiffness", "value"), &PhysXVehicle3D::set_front_anti_roll_stiffness);
	ClassDB::bind_method(D_METHOD("get_front_anti_roll_stiffness"), &PhysXVehicle3D::get_front_anti_roll_stiffness);
	ClassDB::bind_method(D_METHOD("set_rear_anti_roll_stiffness", "value"), &PhysXVehicle3D::set_rear_anti_roll_stiffness);
	ClassDB::bind_method(D_METHOD("get_rear_anti_roll_stiffness"), &PhysXVehicle3D::get_rear_anti_roll_stiffness);
	ADD_GROUP("Drivetrain", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_engine_torque", PROPERTY_HINT_RANGE, "0,5000,10,or_greater"), "set_max_engine_torque", "get_max_engine_torque");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_brake_torque", PROPERTY_HINT_RANGE, "0,20000,10,or_greater"), "set_max_brake_torque", "get_max_brake_torque");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "handbrake_torque", PROPERTY_HINT_RANGE, "0,20000,10,or_greater"), "set_handbrake_torque", "get_handbrake_torque");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "handbrake", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_handbrake", "get_handbrake");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_steer_angle", PROPERTY_HINT_RANGE, "0,1.5708,0.01"), "set_max_steer_angle", "get_max_steer_angle");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_gearbox"), "set_use_gearbox", "get_use_gearbox");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "engine_peak_torque", PROPERTY_HINT_RANGE, "1,5000,1,or_greater"), "set_engine_peak_torque", "get_engine_peak_torque");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "engine_idle_omega", PROPERTY_HINT_RANGE, "1,500,1"), "set_engine_idle_omega", "get_engine_idle_omega");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "engine_max_omega", PROPERTY_HINT_RANGE, "2,2000,1"), "set_engine_max_omega", "get_engine_max_omega");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "clutch_strength", PROPERTY_HINT_RANGE, "0.1,500,0.1,or_greater"), "set_clutch_strength", "get_clutch_strength");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "gear_ratios"), "set_gear_ratios", "get_gear_ratios");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gear_final_ratio", PROPERTY_HINT_RANGE, "0.1,20,0.1,or_greater"), "set_gear_final_ratio", "get_gear_final_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gear_switch_time", PROPERTY_HINT_RANGE, "0.05,3,0.05,or_greater"), "set_gear_switch_time", "get_gear_switch_time");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "autobox_latency", PROPERTY_HINT_RANGE, "0,5,0.05,or_greater"), "set_autobox_latency", "get_autobox_latency");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "autobox_up_ratio", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_autobox_up_ratio", "get_autobox_up_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "autobox_down_ratio", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_autobox_down_ratio", "get_autobox_down_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "target_gear", PROPERTY_HINT_RANGE, "0,255,1"), "set_target_gear", "get_target_gear");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_autobox"), "set_use_autobox", "get_use_autobox");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ackermann_strength", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_ackermann_strength", "get_ackermann_strength");
	ADD_GROUP("Anti-Roll", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "front_anti_roll_stiffness", PROPERTY_HINT_RANGE, "-50000,50000,100,or_less,or_greater"), "set_front_anti_roll_stiffness", "get_front_anti_roll_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rear_anti_roll_stiffness", PROPERTY_HINT_RANGE, "-50000,50000,100,or_less,or_greater"), "set_rear_anti_roll_stiffness", "get_rear_anti_roll_stiffness");

	ClassDB::bind_method(D_METHOD("set_throttle", "value"), &PhysXVehicle3D::set_throttle);
	ClassDB::bind_method(D_METHOD("get_throttle"), &PhysXVehicle3D::get_throttle);
	ClassDB::bind_method(D_METHOD("set_brake", "value"), &PhysXVehicle3D::set_brake);
	ClassDB::bind_method(D_METHOD("get_brake"), &PhysXVehicle3D::get_brake);
	ClassDB::bind_method(D_METHOD("set_steer", "value"), &PhysXVehicle3D::set_steer);
	ClassDB::bind_method(D_METHOD("get_steer"), &PhysXVehicle3D::get_steer);
	ADD_GROUP("Controls", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "throttle", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_throttle", "get_throttle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "brake", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_brake", "get_brake");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "steer", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_steer", "get_steer");
	ClassDB::bind_method(D_METHOD("set_reverse", "value"), &PhysXVehicle3D::set_reverse);
	ClassDB::bind_method(D_METHOD("is_reverse"), &PhysXVehicle3D::is_reverse);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reverse"), "set_reverse", "is_reverse");

	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &PhysXVehicle3D::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &PhysXVehicle3D::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &PhysXVehicle3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysXVehicle3D::get_collision_mask);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ClassDB::bind_method(D_METHOD("set_physics_material_override", "physics_material_override"), &PhysXVehicle3D::set_physics_material_override);
	ClassDB::bind_method(D_METHOD("get_physics_material_override"), &PhysXVehicle3D::get_physics_material_override);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "physics_material_override", PROPERTY_HINT_RESOURCE_TYPE, PhysicsMaterial::get_class_static()), "set_physics_material_override", "get_physics_material_override");

	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &PhysXVehicle3D::get_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_forward_speed"), &PhysXVehicle3D::get_forward_speed);
	ClassDB::bind_method(D_METHOD("get_wheel_jounce", "wheel"), &PhysXVehicle3D::get_wheel_jounce);
	ClassDB::bind_method(D_METHOD("get_wheel_separation", "wheel"), &PhysXVehicle3D::get_wheel_separation);
	ClassDB::bind_method(D_METHOD("get_actor_position"), &PhysXVehicle3D::get_actor_position);
	ClassDB::bind_method(D_METHOD("get_engine_rpm"), &PhysXVehicle3D::get_engine_rpm);
	ClassDB::bind_method(D_METHOD("get_engine_gear"), &PhysXVehicle3D::get_engine_gear);
	ClassDB::bind_method(D_METHOD("get_clutch"), &PhysXVehicle3D::get_clutch);
	ClassDB::bind_method(D_METHOD("get_wheel_rpm", "wheel"), &PhysXVehicle3D::get_wheel_rpm);
	ClassDB::bind_method(D_METHOD("get_wheel_skid", "wheel"), &PhysXVehicle3D::get_wheel_skid);
	ClassDB::bind_method(D_METHOD("get_wheel_contact", "wheel"), &PhysXVehicle3D::get_wheel_contact);
	ClassDB::bind_method(D_METHOD("get_wheel_steer_angle", "wheel"), &PhysXVehicle3D::get_wheel_steer_angle);
}

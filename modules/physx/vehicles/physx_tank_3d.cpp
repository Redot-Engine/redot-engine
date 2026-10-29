/**************************************************************************/
/*  physx_tank_3d.cpp                                                     */
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

#include "physx_tank_3d.h"

#include "physx_vehicle_shared.h"

#include "../physx_conversions.h"
#include "../physx_project_settings.h"
#include "../physx_server.h"
#include "../shapes/physx_user_data.h"
#include "../spaces/physx_space_3d.h"
#include "physx_vehicle_track.h"
#include "physx_vehicle_wheel_3d.h"

#include "core/config/engine.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/world_3d.h"

struct PhysXTank3D::Impl {
	VehicleTrack vehicle;
	PxVehiclePhysXSimulationContext simulationContext;
	PxScene *scene = nullptr;
	PhysXSpace3D *space = nullptr; // for ensure_synced in _destroy
	PxReal sleep_threshold = 0.0f;
	PxReal time_before_sleep = 0.0f;
	bool built = false;
	// Attached to the hull actor's userData (the contact-modify combiner
	// reads its signed friction/bounce during the solve). object stays
	// nullptr — the hull is not a PhysicsServer3D object.
	PhysXActorUserData chassis_user_data;
};

PhysXTank3D::PhysXTank3D() {
	impl = memnew(Impl);
}

PhysXTank3D::~PhysXTank3D() {
	chassis_exceptions.dispose();
	_destroy();
	memdelete(impl);
}

bool PhysXTank3D::_build() {
	if (impl->built) {
		return true;
	}
	// Same editor-vs-play guard as PhysXVehicle3D/PhysXMotorcycle3D -- see
	// either class's own comment for why.
	if (Engine::get_singleton()->is_editor_hint()) {
		return false;
	}
	if (!is_inside_world() || get_world_3d().is_null()) {
		return false;
	}
	if (wheels.size() < 2 || wheels.size() > VehicleTrack::MAX_WHEELS) {
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
		ERR_PRINT("PhysXTank3D: chassis CollisionShape3D must use a BoxShape3D.");
		return false;
	}

	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server) {
		return false;
	}
	PhysXSpace3D *space = server->get_space(get_world_3d()->get_space());
	if (!space) {
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

	VehicleTrackConfig config;
	config.mass = mass;
	config.moment_of_inertia = moment_of_inertia;
	config.chassis_half_extents = box_shape->get_size() * 0.5;
	config.chassis_box_center_local = chassis_shape_node->get_position();
	config.chassis_com_local = center_of_mass_mode == CENTER_OF_MASS_MODE_CUSTOM ? center_of_mass : chassis_shape_node->get_position();
	config.max_engine_torque = max_engine_torque;
	config.max_brake_torque = max_brake_torque;
	config.collision_layer = collision_layer;
	config.collision_mask = collision_mask;
	{
		real_t friction, bounce;
		_chassis_material(friction, bounce);
		config.chassis_friction = (float)friction;
		config.chassis_bounce = (float)bounce;
	}

	// Unlike PhysXMotorcycle3D's front/rear reordering, wheels are used in
	// their natural child-registration order directly -- configure_vehicle_track()
	// infers each wheel's track side from its own local X position, so there's
	// no canonical slot order to reorder into (see this class's own doc
	// comment for why).
	config.wheels.resize(wheels.size());
	for (uint32_t i = 0; i < wheels.size(); i++) {
		PhysXVehicleWheel3D *w = wheels[i];
		VehicleTrackWheelConfig &wc = config.wheels[i];
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
	}

	if (!configure_vehicle_track(impl->vehicle, config, *physics, *scene, impl->simulationContext)) {
		return false;
	}

	VehicleTrack &v = impl->vehicle;
	v.physxActor.rigidBody->setGlobalPose(godot_to_px(get_global_transform()));
	impl->sleep_threshold = (PxReal)space->get_sleep_energy_threshold();
	impl->time_before_sleep = (PxReal)space->get_time_before_sleep();
	PxRigidDynamic *dynamic_body = v.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL_V(dynamic_body, false);
	dynamic_body->setSleepThreshold(can_sleep && PhysXProjectSettings::allow_sleep ? impl->sleep_threshold : 0.0f);
	dynamic_body->setWakeCounter(impl->time_before_sleep);
	scene->addActor(*v.physxActor.rigidBody);
	v.physxActor.rigidBody->setName("PhysXTank3D");
	// Hull contact material: the contact-modify combiner reads the signed
	// friction/bounce from this userData during the solve (the hull PxMaterial
	// authored in configure_vehicle_track is the GPU-path fallback).
	impl->chassis_user_data = PhysXActorUserData();
	impl->chassis_user_data.friction = config.chassis_friction;
	impl->chassis_user_data.bounce = config.chassis_bounce;
	v.physxActor.rigidBody->userData = &impl->chassis_user_data;

	impl->scene = scene;
	impl->space = space;
	impl->built = true;
	// Exceptions kept across rebuilds/world exits re-stamp the fresh hull
	// shapes and re-wire the wheel filter.
	if (chassis_exceptions.get_slot() != 0) {
		chassis_exceptions.get_or_alloc_slot(impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>());
		impl->vehicle.wheel_query_filter.self_exception_slot = chassis_exceptions.get_slot_ptr();
	}
	return true;
}

void PhysXTank3D::_destroy() {
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

void PhysXTank3D::_rebuild_if_live() {
	if (!is_inside_world() || rebuild_scheduled) {
		return;
	}
	rebuild_scheduled = true;
	callable_mp(this, &PhysXTank3D::_do_deferred_rebuild).call_deferred();
}

void PhysXTank3D::_do_deferred_rebuild() {
	rebuild_scheduled = false;
	if (!is_inside_world()) {
		return;
	}
	_destroy();
	set_physics_process_internal(_build());
}

void PhysXTank3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			// Same coalescing as _rebuild_if_live() -- see rebuild_scheduled's
			// own doc comment. The tank's own ENTER_WORLD fires before its
			// wheel children's ENTER_TREE (Godot propagates tree/world
			// notifications top-down), so building synchronously here would
			// always run with wheels.size() == 0 and immediately fail anyway;
			// deferring lets it run once, after every already-present wheel
			// child has finished registering itself.
			_rebuild_if_live();
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			set_physics_process_internal(false);
			_destroy();
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (!impl->built) {
				break;
			}
			VehicleTrack &v = impl->vehicle;
			// Ratios clamp to [-1, 1] (per-track signed drive command), brake
			// to [0, 1] -- out-of-range script values otherwise scale torque
			// linearly beyond the tuned budget (VEHN-7).
			v.setDriverInput(physx_vehicle_clamp_sym(left_ratio), physx_vehicle_clamp_sym(right_ratio), physx_vehicle_clamp01(brake));
			v.step((PxReal)get_physics_process_delta_time(), impl->simulationContext);
			// Same CoM-vs-actor-origin frame notes as PhysXVehicle3D/
			// PhysXMotorcycle3D's own identical code -- see PhysXVehicle3D's
			// comment for the SDK source citations that confirmed this.
			set_global_transform(px_to_godot(v.physxActor.rigidBody->getGlobalPose()));
			const PxTransform cmass_local_pose = v.physxActor.rigidBody->getCMassLocalPose();
			for (uint32_t i = 0; i < wheels.size(); i++) {
				wheels[i]->set_transform(px_to_godot(cmass_local_pose * v.wheelLocalPoses[i].localPose));
			}
		} break;
	}
}

Vector3 PhysXTank3D::get_linear_velocity() const {
	if (!impl->built) {
		return Vector3();
	}
	return px_to_godot(impl->vehicle.rigidBodyState.linearVelocity);
}

Vector3 PhysXTank3D::get_angular_velocity() const {
	if (!impl->built) {
		return Vector3();
	}
	PxRigidDynamic *dynamic_body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL_V(dynamic_body, Vector3());
	return px_to_godot(dynamic_body->getAngularVelocity());
}

real_t PhysXTank3D::get_forward_speed() const {
	if (!impl->built) {
		return 0.0;
	}
	// frame.getLngAxis() is a fixed LOCAL-frame constant, not a world-space
	// direction -- rotate it into world space by the actor's current
	// orientation first (see PhysXVehicle3D::get_forward_speed()'s own
	// comment for the real bug this was found from).
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	const PxVec3 fwd = actor_pose.q.rotate(impl->vehicle.frame.getLngAxis());
	return (real_t)impl->vehicle.rigidBodyState.linearVelocity.dot(fwd);
}

Vector3 PhysXTank3D::get_up() const {
	if (!impl->built) {
		return Vector3(0, 1, 0);
	}
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	return px_to_godot(actor_pose.q.getBasisVector1());
}

Vector3 PhysXTank3D::get_forward() const {
	if (!impl->built) {
		return -get_global_transform().basis.get_column(2);
	}
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	return -px_to_godot(actor_pose.q.getBasisVector2());
}

real_t PhysXTank3D::get_wheel_jounce(int p_wheel) const {
	if (!impl->built) {
		return 0.0;
	}
	ERR_FAIL_INDEX_V(p_wheel, (int)wheels.size(), 0.0);
	return (real_t)impl->vehicle.suspensionStates[p_wheel].jounce;
}

real_t PhysXTank3D::get_wheel_separation(int p_wheel) const {
	if (!impl->built) {
		return 0.0;
	}
	ERR_FAIL_INDEX_V(p_wheel, (int)wheels.size(), 0.0);
	return (real_t)impl->vehicle.suspensionStates[p_wheel].separation;
}

Vector3 PhysXTank3D::get_actor_position() const {
	if (!impl->built) {
		return Vector3();
	}
	return px_to_godot(impl->vehicle.physxActor.rigidBody->getGlobalPose().p);
}

PackedStringArray PhysXTank3D::get_configuration_warnings() const {
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
		warnings.push_back(RTR("PhysXTank3D needs a CollisionShape3D child with a BoxShape3D for its hull."));
	}
	if (wheels.size() < 2) {
		warnings.push_back(RTR("PhysXTank3D needs at least 2 PhysXVehicleWheel3D children."));
	} else if (wheels.size() > VehicleTrack::MAX_WHEELS) {
		warnings.push_back(vformat(RTR("PhysXTank3D supports at most %d PhysXVehicleWheel3D children (has %d)."), (int)VehicleTrack::MAX_WHEELS, (int)wheels.size()));
	} else {
		int nb_left = 0, nb_right = 0, nb_centerline = 0;
		for (uint32_t i = 0; i < wheels.size(); i++) {
			real_t wheel_x = wheels[i]->get_position().x;
			if (Math::is_zero_approx(wheel_x)) {
				nb_centerline++;
			} else if (wheel_x < 0.0) {
				nb_left++;
			} else {
				nb_right++;
			}
		}
		if (nb_centerline > 0) {
			warnings.push_back(vformat(RTR("%d wheel(s) sit exactly on the centerline (local X = 0) -- PhysXTank3D can't tell which track they belong to."), nb_centerline));
		}
		if (nb_left == 0 || nb_right == 0) {
			warnings.push_back(RTR("PhysXTank3D needs at least 1 wheel on each side (negative and positive local X)."));
		}
	}
	return warnings;
}

void PhysXTank3D::_validate_property(PropertyInfo &p_property) const {
	if (center_of_mass_mode != CENTER_OF_MASS_MODE_CUSTOM && p_property.name == "center_of_mass") {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

#define PHYSX_TANK_SETTER(m_name, m_field)       \
	void PhysXTank3D::set_##m_name(real_t p_v) { \
		m_field = p_v;                           \
		_rebuild_if_live();                      \
	}

void PhysXTank3D::set_mass(real_t p_mass) {
	mass = p_mass;
	_rebuild_if_live();
}
void PhysXTank3D::set_moment_of_inertia(const Vector3 &p_moi) {
	moment_of_inertia = p_moi;
	_rebuild_if_live();
}
void PhysXTank3D::set_center_of_mass_mode(CenterOfMassMode p_mode) {
	if (center_of_mass_mode == p_mode) {
		return;
	}
	center_of_mass_mode = p_mode;
	notify_property_list_changed();
	_rebuild_if_live();
}
void PhysXTank3D::set_center_of_mass(const Vector3 &p_center_of_mass) {
	if (center_of_mass == p_center_of_mass) {
		return;
	}
	ERR_FAIL_COND(center_of_mass_mode != CENTER_OF_MASS_MODE_CUSTOM);
	center_of_mass = p_center_of_mass;
	_rebuild_if_live();
}
void PhysXTank3D::set_can_sleep(bool p_can_sleep) {
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

bool PhysXTank3D::is_sleeping() const {
	PxRigidDynamic *body = impl->built ? impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>() : nullptr;
	return body && body->isSleeping();
}
PHYSX_TANK_SETTER(max_engine_torque, max_engine_torque)
PHYSX_TANK_SETTER(max_brake_torque, max_brake_torque)

#undef PHYSX_TANK_SETTER

void PhysXTank3D::set_collision_layer(uint32_t p_layer) {
	collision_layer = p_layer;
	_rebuild_if_live();
}
void PhysXTank3D::set_collision_mask(uint32_t p_mask) {
	collision_mask = p_mask;
	_rebuild_if_live();
}

void PhysXTank3D::set_physics_material_override(const Ref<PhysicsMaterial> &p_material) {
	if (physics_material_override == p_material) {
		return;
	}
	if (physics_material_override.is_valid()) {
		physics_material_override->disconnect_changed(callable_mp(this, &PhysXTank3D::_chassis_material_changed));
	}
	physics_material_override = p_material;
	if (physics_material_override.is_valid()) {
		physics_material_override->connect_changed(callable_mp(this, &PhysXTank3D::_chassis_material_changed));
	}
	_chassis_material_changed();
}

void PhysXTank3D::_chassis_material(real_t &r_friction, real_t &r_bounce) const {
	if (physics_material_override.is_valid()) {
		r_friction = physics_material_override->computed_friction();
		r_bounce = physics_material_override->computed_bounce();
	} else {
		// No override: the contact-modify combiner's defaults for an actor
		// without material userData — the behavior the hull had before this
		// property existed (NOT the PxMaterial's 0.0/0.1, which the combiner
		// always overrode on the CPU path).
		r_friction = 1.0;
		r_bounce = 0.0;
	}
}

// The hull material is the tank's own, so a change — or an edit to the
// assigned material — is applied in place, no rebuild.
void PhysXTank3D::_chassis_material_changed() {
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

void PhysXTank3D::_ensure_exception_synced() {
	if (is_inside_world() && get_world_3d().is_valid()) {
		PhysXServer3D *server = PhysXServer3D::get_singleton();
		if (PhysXSpace3D *space = server->get_space(get_world_3d()->get_space())) {
			space->ensure_synced();
		}
	}
}

void PhysXTank3D::apply_force_at_local_position(const Vector3 &p_force, const Vector3 &p_local_position) {
	if (!impl->built) {
		return;
	}
	PxRigidDynamic *dynamic_body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL(dynamic_body);
	PxRigidBodyExt::addForceAtLocalPos(*dynamic_body, godot_to_px(p_force),
			godot_to_px(p_local_position), PxForceMode::eFORCE);
}

void PhysXTank3D::apply_torque(const Vector3 &p_torque) {
	if (!impl->built) {
		return;
	}
	PxRigidDynamic *dynamic_body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL(dynamic_body);
	dynamic_body->addTorque(godot_to_px(p_torque), PxForceMode::eFORCE);
}

void PhysXTank3D::add_collision_exception_with(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	_ensure_exception_synced();
	chassis_exceptions.add(impl->built ? impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>() : nullptr, p_node);
	if (impl->built) {
		impl->vehicle.wheel_query_filter.self_exception_slot = chassis_exceptions.get_slot_ptr();
	}
}

void PhysXTank3D::remove_collision_exception_with(Node *p_node) {
	ERR_FAIL_NULL(p_node);
	_ensure_exception_synced();
	chassis_exceptions.remove(impl->built ? impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>() : nullptr, p_node);
}

Array PhysXTank3D::get_collision_exceptions() const {
	return chassis_exceptions.get_nodes();
}

uint32_t PhysXTank3D::vehicle_exception_slot_or_alloc() {
	_ensure_exception_synced();
	return chassis_exceptions.get_or_alloc_slot(impl->built ? impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>() : nullptr);
}

void PhysXTank3D::_bind_methods() {
	BIND_ENUM_CONSTANT(CENTER_OF_MASS_MODE_AUTO);
	BIND_ENUM_CONSTANT(CENTER_OF_MASS_MODE_CUSTOM);

	ClassDB::bind_method(D_METHOD("apply_force_at_local_position", "force", "local_position"), &PhysXTank3D::apply_force_at_local_position);
	ClassDB::bind_method(D_METHOD("apply_torque", "torque"), &PhysXTank3D::apply_torque);
	ClassDB::bind_method(D_METHOD("add_collision_exception_with", "node"), &PhysXTank3D::add_collision_exception_with);
	ClassDB::bind_method(D_METHOD("remove_collision_exception_with", "node"), &PhysXTank3D::remove_collision_exception_with);
	ClassDB::bind_method(D_METHOD("get_collision_exceptions"), &PhysXTank3D::get_collision_exceptions);
	ClassDB::bind_method(D_METHOD("set_mass", "mass"), &PhysXTank3D::set_mass);
	ClassDB::bind_method(D_METHOD("get_mass"), &PhysXTank3D::get_mass);
	ClassDB::bind_method(D_METHOD("set_moment_of_inertia", "moi"), &PhysXTank3D::set_moment_of_inertia);
	ClassDB::bind_method(D_METHOD("get_moment_of_inertia"), &PhysXTank3D::get_moment_of_inertia);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mass", PROPERTY_HINT_RANGE, "1,20000,1,or_greater"), "set_mass", "get_mass");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "moment_of_inertia"), "set_moment_of_inertia", "get_moment_of_inertia");
	ClassDB::bind_method(D_METHOD("set_center_of_mass_mode", "mode"), &PhysXTank3D::set_center_of_mass_mode);
	ClassDB::bind_method(D_METHOD("get_center_of_mass_mode"), &PhysXTank3D::get_center_of_mass_mode);
	ClassDB::bind_method(D_METHOD("set_center_of_mass", "center_of_mass"), &PhysXTank3D::set_center_of_mass);
	ClassDB::bind_method(D_METHOD("get_center_of_mass"), &PhysXTank3D::get_center_of_mass);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "center_of_mass_mode", PROPERTY_HINT_ENUM, "Auto,Custom"), "set_center_of_mass_mode", "get_center_of_mass_mode");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "center_of_mass", PROPERTY_HINT_RANGE, "-10,10,0.01,or_less,or_greater,suffix:m"), "set_center_of_mass", "get_center_of_mass");
	ClassDB::bind_method(D_METHOD("set_can_sleep", "able_to_sleep"), &PhysXTank3D::set_can_sleep);
	ClassDB::bind_method(D_METHOD("is_able_to_sleep"), &PhysXTank3D::is_able_to_sleep);
	ClassDB::bind_method(D_METHOD("is_sleeping"), &PhysXTank3D::is_sleeping);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "can_sleep"), "set_can_sleep", "is_able_to_sleep");

	ClassDB::bind_method(D_METHOD("set_max_engine_torque", "value"), &PhysXTank3D::set_max_engine_torque);
	ClassDB::bind_method(D_METHOD("get_max_engine_torque"), &PhysXTank3D::get_max_engine_torque);
	ClassDB::bind_method(D_METHOD("set_max_brake_torque", "value"), &PhysXTank3D::set_max_brake_torque);
	ClassDB::bind_method(D_METHOD("get_max_brake_torque"), &PhysXTank3D::get_max_brake_torque);
	ADD_GROUP("Drivetrain", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_engine_torque", PROPERTY_HINT_RANGE, "0,20000,10,or_greater"), "set_max_engine_torque", "get_max_engine_torque");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_brake_torque", PROPERTY_HINT_RANGE, "0,40000,10,or_greater"), "set_max_brake_torque", "get_max_brake_torque");

	ClassDB::bind_method(D_METHOD("set_left_ratio", "value"), &PhysXTank3D::set_left_ratio);
	ClassDB::bind_method(D_METHOD("get_left_ratio"), &PhysXTank3D::get_left_ratio);
	ClassDB::bind_method(D_METHOD("set_right_ratio", "value"), &PhysXTank3D::set_right_ratio);
	ClassDB::bind_method(D_METHOD("get_right_ratio"), &PhysXTank3D::get_right_ratio);
	ClassDB::bind_method(D_METHOD("set_brake", "value"), &PhysXTank3D::set_brake);
	ClassDB::bind_method(D_METHOD("get_brake"), &PhysXTank3D::get_brake);
	ADD_GROUP("Controls", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "left_ratio", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_left_ratio", "get_left_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "right_ratio", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_right_ratio", "get_right_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "brake", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_brake", "get_brake");

	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &PhysXTank3D::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &PhysXTank3D::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &PhysXTank3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysXTank3D::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_physics_material_override", "physics_material_override"), &PhysXTank3D::set_physics_material_override);
	ClassDB::bind_method(D_METHOD("get_physics_material_override"), &PhysXTank3D::get_physics_material_override);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "physics_material_override", PROPERTY_HINT_RESOURCE_TYPE, PhysicsMaterial::get_class_static()), "set_physics_material_override", "get_physics_material_override");

	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &PhysXTank3D::get_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_angular_velocity"), &PhysXTank3D::get_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_forward_speed"), &PhysXTank3D::get_forward_speed);
	ClassDB::bind_method(D_METHOD("get_up"), &PhysXTank3D::get_up);
	ClassDB::bind_method(D_METHOD("get_forward"), &PhysXTank3D::get_forward);
	ClassDB::bind_method(D_METHOD("get_wheel_jounce", "wheel"), &PhysXTank3D::get_wheel_jounce);
	ClassDB::bind_method(D_METHOD("get_wheel_separation", "wheel"), &PhysXTank3D::get_wheel_separation);
	ClassDB::bind_method(D_METHOD("get_actor_position"), &PhysXTank3D::get_actor_position);
}

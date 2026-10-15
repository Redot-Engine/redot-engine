/**************************************************************************/
/*  physx_vehicle_probe.cpp                                               */
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

#include "physx_vehicle_probe.h"

#include "../physx_conversions.h"
#include "../physx_server.h"
#include "../spaces/physx_space_3d.h"
#include "physx_vehicle4w.h"

#include "core/object/class_db.h"

struct PhysXVehicleProbe::Impl {
	Vehicle4W vehicle;
	PxVehiclePhysXSimulationContext simulationContext;
	PxScene *scene = nullptr;
	bool initialized = false;
};

PhysXVehicleProbe::PhysXVehicleProbe() {
	impl = memnew(Impl);
}

PhysXVehicleProbe::~PhysXVehicleProbe() {
	if (impl) {
		if (impl->initialized && impl->scene) {
			impl->scene->removeActor(*impl->vehicle.physxActor.rigidBody);
			impl->vehicle.destroy();
		}
		memdelete(impl);
	}
}

bool PhysXVehicleProbe::initialize(RID p_space, const Vector3 &p_position, real_t p_wheel_radius) {
	ERR_FAIL_COND_V(impl->initialized, false);

	PhysXServer3D *server = PhysXServer3D::get_singleton();
	ERR_FAIL_NULL_V(server, false);
	PhysXSpace3D *space = server->get_space(p_space);
	ERR_FAIL_NULL_V(space, false);
	PxPhysics *physics = space->get_px_physics();
	PxScene *scene = space->get_px_scene();
	ERR_FAIL_NULL_V(physics, false);
	ERR_FAIL_NULL_V(scene, false);

	// A ~1500kg sedan: half-track 0.75m, wheelbase 2.7m (front axle +1.35,
	// rear -1.35) -- explicit here (not just Vehicle4WWheelConfig's own
	// defaults) since wheel *position* has no sensible default of its own,
	// unlike every other per-wheel field.
	Vehicle4WConfig config;
	config.wheels[0].position = Vector3(-0.75f, 0.05f, 1.35f); // FL
	config.wheels[0].use_as_steering = true;
	config.wheels[1].position = Vector3(0.75f, 0.05f, 1.35f); // FR
	config.wheels[1].use_as_steering = true;
	config.wheels[2].position = Vector3(-0.75f, 0.05f, -1.35f); // RL
	config.wheels[2].use_as_steering = false;
	config.wheels[3].position = Vector3(0.75f, 0.05f, -1.35f); // RR
	config.wheels[3].use_as_steering = false;

	if (p_wheel_radius > 0.0) {
		for (int i = 0; i < 4; i++) {
			config.wheels[i].radius = p_wheel_radius;
		}
	}

	PxU32 wheel_order[4];
	if (!configure_vehicle4w(impl->vehicle, config, *physics, *scene, impl->simulationContext, wheel_order)) {
		return false;
	}

	Vehicle4W &v = impl->vehicle;
	const PxTransform startPose(godot_to_px(p_position), PxQuat(PxIdentity));
	v.physxActor.rigidBody->setGlobalPose(startPose);
	scene->addActor(*v.physxActor.rigidBody);
	v.physxActor.rigidBody->setName("PhysXVehicleProbe");

	impl->scene = scene;
	impl->initialized = true;
	return true;
}

void PhysXVehicleProbe::step(real_t p_dt, real_t p_throttle, real_t p_brake, real_t p_steer) {
	ERR_FAIL_COND(!impl->initialized);
	Vehicle4W &v = impl->vehicle;
	// Positive throttle rolls the vehicle -Z (nose-first) with the
	// composition's right-handed frame (see configure_vehicle4w's axis note).
	v.commandState.throttle = (PxReal)p_throttle;
	v.commandState.brakes[0] = (PxReal)p_brake;
	v.commandState.nbBrakes = 1;
	v.commandState.steer = (PxReal)p_steer;
	v.step((PxReal)p_dt, impl->simulationContext);
}

Vector3 PhysXVehicleProbe::get_position() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	return px_to_godot(impl->vehicle.rigidBodyState.pose.p);
}

Vector3 PhysXVehicleProbe::get_linear_velocity() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	return px_to_godot(impl->vehicle.rigidBodyState.linearVelocity);
}

real_t PhysXVehicleProbe::get_forward_speed() const {
	ERR_FAIL_COND_V(!impl->initialized, 0.0);
	// frame.getLngAxis() is a fixed LOCAL-frame constant, not a world-space
	// direction -- rotate it into world space by the actor's current
	// orientation first (see PhysXVehicle3D::get_forward_speed()'s own
	// comment for the real bug this was found from).
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	const PxVec3 fwd = actor_pose.q.rotate(impl->vehicle.frame.getLngAxis());
	return (real_t)impl->vehicle.rigidBodyState.linearVelocity.dot(fwd);
}

Vector3 PhysXVehicleProbe::get_actor_position() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	return px_to_godot(impl->vehicle.physxActor.rigidBody->getGlobalPose().p);
}

Vector3 PhysXVehicleProbe::get_wheel_position(int p_wheel) const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	ERR_FAIL_INDEX_V(p_wheel, 4, Vector3());
	// wheelLocalPoses[i].localPose is in the CoM frame, not the actor's own
	// origin frame -- see PhysXVehicle3D's identical note for the SDK source
	// citation that confirmed this.
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	const PxTransform cmass_local_pose = impl->vehicle.physxActor.rigidBody->getCMassLocalPose();
	const PxTransform wheel_world = actor_pose * cmass_local_pose * impl->vehicle.wheelLocalPoses[p_wheel].localPose;
	return px_to_godot(wheel_world.p);
}

real_t PhysXVehicleProbe::get_wheel_jounce(int p_wheel) const {
	ERR_FAIL_COND_V(!impl->initialized, 0.0);
	ERR_FAIL_INDEX_V(p_wheel, 4, 0.0);
	return (real_t)impl->vehicle.suspensionStates[p_wheel].jounce;
}

real_t PhysXVehicleProbe::get_wheel_separation(int p_wheel) const {
	ERR_FAIL_COND_V(!impl->initialized, 0.0);
	ERR_FAIL_INDEX_V(p_wheel, 4, 0.0);
	return (real_t)impl->vehicle.suspensionStates[p_wheel].separation;
}

void PhysXVehicleProbe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("initialize", "space", "position", "wheel_radius"), &PhysXVehicleProbe::initialize, DEFVAL(-1.0));
	ClassDB::bind_method(D_METHOD("step", "dt", "throttle", "brake", "steer"), &PhysXVehicleProbe::step);
	ClassDB::bind_method(D_METHOD("get_position"), &PhysXVehicleProbe::get_position);
	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &PhysXVehicleProbe::get_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_forward_speed"), &PhysXVehicleProbe::get_forward_speed);
	ClassDB::bind_method(D_METHOD("get_wheel_jounce", "wheel"), &PhysXVehicleProbe::get_wheel_jounce);
	ClassDB::bind_method(D_METHOD("get_wheel_separation", "wheel"), &PhysXVehicleProbe::get_wheel_separation);
	ClassDB::bind_method(D_METHOD("get_actor_position"), &PhysXVehicleProbe::get_actor_position);
	ClassDB::bind_method(D_METHOD("get_wheel_position", "wheel"), &PhysXVehicleProbe::get_wheel_position);
}

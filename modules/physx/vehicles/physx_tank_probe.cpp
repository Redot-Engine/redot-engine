/**************************************************************************/
/*  physx_tank_probe.cpp                                                  */
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

#include "physx_tank_probe.h"

#include "../physx_conversions.h"
#include "../physx_server.h"
#include "../spaces/physx_space_3d.h"
#include "physx_vehicle_track.h"

#include "core/object/class_db.h"

struct PhysXTankProbe::Impl {
	VehicleTrack vehicle;
	PxVehiclePhysXSimulationContext simulationContext;
	PxScene *scene = nullptr;
	bool initialized = false;
};

PhysXTankProbe::PhysXTankProbe() {
	impl = memnew(Impl);
}

PhysXTankProbe::~PhysXTankProbe() {
	if (impl) {
		if (impl->initialized && impl->scene) {
			impl->scene->removeActor(*impl->vehicle.physxActor.rigidBody);
			impl->vehicle.destroy();
		}
		memdelete(impl);
	}
}

bool PhysXTankProbe::initialize(RID p_space, const Vector3 &p_position, const PackedVector3Array &p_wheel_positions_local) {
	ERR_FAIL_COND_V(impl->initialized, false);

	PhysXServer3D *server = PhysXServer3D::get_singleton();
	ERR_FAIL_NULL_V(server, false);
	PhysXSpace3D *space = server->get_space(p_space);
	ERR_FAIL_NULL_V(space, false);
	PxPhysics *physics = space->get_px_physics();
	PxScene *scene = space->get_px_scene();
	ERR_FAIL_NULL_V(physics, false);
	ERR_FAIL_NULL_V(scene, false);

	VehicleTrackConfig config;
	config.wheels.resize(p_wheel_positions_local.size());
	for (int i = 0; i < p_wheel_positions_local.size(); i++) {
		VehicleTrackWheelConfig wc;
		wc.position = p_wheel_positions_local[i];
		config.wheels[i] = wc;
	}

	PxVehiclePhysXSimulationContext &out_context = impl->simulationContext;
	if (!configure_vehicle_track(impl->vehicle, config, *physics, *scene, out_context)) {
		return false;
	}

	VehicleTrack &v = impl->vehicle;
	const PxTransform startPose(godot_to_px(p_position), PxQuat(PxIdentity));
	v.physxActor.rigidBody->setGlobalPose(startPose);
	scene->addActor(*v.physxActor.rigidBody);
	v.physxActor.rigidBody->setName("PhysXTankProbe");

	impl->scene = scene;
	impl->initialized = true;
	return true;
}

void PhysXTankProbe::step(real_t p_dt, real_t p_left_ratio, real_t p_right_ratio, real_t p_brake) {
	ERR_FAIL_COND(!impl->initialized);
	VehicleTrack &v = impl->vehicle;
	v.setDriverInput((PxReal)p_left_ratio, (PxReal)p_right_ratio, (PxReal)p_brake);
	v.step((PxReal)p_dt, impl->simulationContext);
}

Vector3 PhysXTankProbe::get_position() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	return px_to_godot(impl->vehicle.rigidBodyState.pose.p);
}

Vector3 PhysXTankProbe::get_linear_velocity() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	return px_to_godot(impl->vehicle.rigidBodyState.linearVelocity);
}

Vector3 PhysXTankProbe::get_angular_velocity() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	PxRigidDynamic *dynamic_body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL_V(dynamic_body, Vector3());
	return px_to_godot(dynamic_body->getAngularVelocity());
}

real_t PhysXTankProbe::get_forward_speed() const {
	ERR_FAIL_COND_V(!impl->initialized, 0.0);
	// frame.getLngAxis() is a fixed LOCAL-frame constant, not a world-space
	// direction -- rotate it into world space by the actor's current
	// orientation first (see PhysXVehicle3D::get_forward_speed()'s own
	// comment for the real bug this was found from).
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	const PxVec3 fwd = actor_pose.q.rotate(impl->vehicle.frame.getLngAxis());
	return (real_t)impl->vehicle.rigidBodyState.linearVelocity.dot(fwd);
}

Vector3 PhysXTankProbe::get_up() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3(0, 1, 0));
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	return px_to_godot(actor_pose.q.getBasisVector1());
}

Vector3 PhysXTankProbe::get_forward() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3(0, 0, -1));
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	// Godot forward is -Z, PxVehicleFrame::eNegZ (see configure_vehicle_track()) --
	// basis vector 2 is the local Z axis, negate it.
	return -px_to_godot(actor_pose.q.getBasisVector2());
}

real_t PhysXTankProbe::get_wheel_jounce(int p_wheel) const {
	ERR_FAIL_COND_V(!impl->initialized, 0.0);
	ERR_FAIL_INDEX_V((uint32_t)p_wheel, impl->vehicle.numWheels, 0.0);
	return (real_t)impl->vehicle.suspensionStates[p_wheel].jounce;
}

real_t PhysXTankProbe::get_wheel_separation(int p_wheel) const {
	ERR_FAIL_COND_V(!impl->initialized, 0.0);
	ERR_FAIL_INDEX_V((uint32_t)p_wheel, impl->vehicle.numWheels, 0.0);
	return (real_t)impl->vehicle.suspensionStates[p_wheel].separation;
}

Vector3 PhysXTankProbe::get_actor_position() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	return px_to_godot(impl->vehicle.physxActor.rigidBody->getGlobalPose().p);
}

void PhysXTankProbe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_forward_speed"), &PhysXTankProbe::get_forward_speed);
	ClassDB::bind_method(D_METHOD("initialize", "space", "position", "wheel_positions_local"), &PhysXTankProbe::initialize);
	ClassDB::bind_method(D_METHOD("step", "dt", "left_ratio", "right_ratio", "brake"), &PhysXTankProbe::step);
	ClassDB::bind_method(D_METHOD("get_position"), &PhysXTankProbe::get_position);
	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &PhysXTankProbe::get_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_angular_velocity"), &PhysXTankProbe::get_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_up"), &PhysXTankProbe::get_up);
	ClassDB::bind_method(D_METHOD("get_forward"), &PhysXTankProbe::get_forward);
	ClassDB::bind_method(D_METHOD("get_wheel_jounce", "wheel"), &PhysXTankProbe::get_wheel_jounce);
	ClassDB::bind_method(D_METHOD("get_wheel_separation", "wheel"), &PhysXTankProbe::get_wheel_separation);
	ClassDB::bind_method(D_METHOD("get_actor_position"), &PhysXTankProbe::get_actor_position);
}

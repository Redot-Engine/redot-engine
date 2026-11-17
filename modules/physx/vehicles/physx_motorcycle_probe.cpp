/**************************************************************************/
/*  physx_motorcycle_probe.cpp                                            */
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

#include "physx_motorcycle_probe.h"

#include "physx_vehicle_shared.h"

#include "../physx_conversions.h"
#include "../physx_server.h"
#include "../spaces/physx_space_3d.h"
#include "physx_vehicle2w.h"

#include "core/object/class_db.h"

struct PhysXMotorcycleProbe::Impl {
	Vehicle2W vehicle;
	PxVehiclePhysXSimulationContext simulationContext;
	PxScene *scene = nullptr;
	bool initialized = false;
};

PhysXMotorcycleProbe::PhysXMotorcycleProbe() {
	impl = memnew(Impl);
}

PhysXMotorcycleProbe::~PhysXMotorcycleProbe() {
	if (impl) {
		if (impl->initialized && impl->scene) {
			impl->scene->removeActor(*impl->vehicle.physxActor.rigidBody);
			impl->vehicle.destroy();
		}
		memdelete(impl);
	}
}

bool PhysXMotorcycleProbe::initialize(RID p_space, const Vector3 &p_position) {
	ERR_FAIL_COND_V(impl->initialized, false);

	PhysXServer3D *server = PhysXServer3D::get_singleton();
	ERR_FAIL_NULL_V(server, false);
	PhysXSpace3D *space = server->get_space(p_space);
	ERR_FAIL_NULL_V(space, false);
	PxPhysics *physics = space->get_px_physics();
	PxScene *scene = space->get_px_scene();
	ERR_FAIL_NULL_V(physics, false);
	ERR_FAIL_NULL_V(scene, false);

	// A ~220kg motorcycle: wheelbase 1.4m (front +0.7, rear -0.7), wheels on
	// the ground plane at rest (y matches Vehicle4W's own probe convention of
	// a small positive offset the suspension settles into).
	Vehicle2WConfig config;
	config.front_wheel.position = Vector3(0.0f, 0.05f, 0.7f);
	config.rear_wheel.position = Vector3(0.0f, 0.05f, -0.7f);

	PxVehiclePhysXSimulationContext &out_context = impl->simulationContext;
	if (!configure_vehicle2w(impl->vehicle, config, *physics, *scene, out_context)) {
		return false;
	}

	Vehicle2W &v = impl->vehicle;
	const PxTransform startPose(godot_to_px(p_position), PxQuat(PxIdentity));
	v.physxActor.rigidBody->setGlobalPose(startPose);
	scene->addActor(*v.physxActor.rigidBody);
	v.physxActor.rigidBody->setName("PhysXMotorcycleProbe");

	impl->scene = scene;
	impl->initialized = true;
	return true;
}

void PhysXMotorcycleProbe::step(real_t p_dt, real_t p_throttle, real_t p_brake, real_t p_steer) {
	ERR_FAIL_COND(!impl->initialized);
	Vehicle2W &v = impl->vehicle;
	// Positive throttle rolls -Z natively (right-handed frame; the negation
	// compensated the old left-handed frame).
	v.commandState.throttle = physx_vehicle_clamp01(p_throttle);
	v.commandState.brakes[0] = (PxReal)p_brake;
	v.commandState.nbBrakes = 1;
	v.commandState.steer = (PxReal)p_steer;
	v.step((PxReal)p_dt, impl->simulationContext);
}

void PhysXMotorcycleProbe::apply_torque_impulse(const Vector3 &p_impulse) {
	ERR_FAIL_COND(!impl->initialized);
	PxRigidDynamic *dynamic_body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL(dynamic_body);
	dynamic_body->addTorque(godot_to_px(p_impulse), PxForceMode::eIMPULSE);
}

void PhysXMotorcycleProbe::set_angular_velocity(const Vector3 &p_angular_velocity) {
	ERR_FAIL_COND(!impl->initialized);
	PxRigidDynamic *dynamic_body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL(dynamic_body);
	dynamic_body->setAngularVelocity(godot_to_px(p_angular_velocity));
}

Vector3 PhysXMotorcycleProbe::get_position() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	return px_to_godot(impl->vehicle.rigidBodyState.pose.p);
}

Vector3 PhysXMotorcycleProbe::get_linear_velocity() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	return px_to_godot(impl->vehicle.rigidBodyState.linearVelocity);
}

real_t PhysXMotorcycleProbe::get_forward_speed() const {
	ERR_FAIL_COND_V(!impl->initialized, 0.0);
	// frame.getLngAxis() is a fixed LOCAL-frame constant, not a world-space
	// direction -- rotate it into world space by the actor's current
	// orientation first (see PhysXVehicle3D::get_forward_speed()'s own
	// comment for the real bug this was found from).
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	const PxVec3 fwd = actor_pose.q.rotate(impl->vehicle.frame.getLngAxis());
	return (real_t)impl->vehicle.rigidBodyState.linearVelocity.dot(fwd);
}

Vector3 PhysXMotorcycleProbe::get_up() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3(0, 1, 0));
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	return px_to_godot(actor_pose.q.getBasisVector1());
}

Vector3 PhysXMotorcycleProbe::get_forward() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3(0, 0, -1));
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	// Godot forward is -Z, PxVehicleFrame::eNegZ (see configure_vehicle2w()) --
	// basis vector 2 is the local Z axis, negate it.
	return -px_to_godot(actor_pose.q.getBasisVector2());
}

Vector3 PhysXMotorcycleProbe::get_angular_velocity() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	PxRigidDynamic *dynamic_body = impl->vehicle.physxActor.rigidBody->is<PxRigidDynamic>();
	ERR_FAIL_NULL_V(dynamic_body, Vector3());
	return px_to_godot(dynamic_body->getAngularVelocity());
}

real_t PhysXMotorcycleProbe::get_roll_angle() const {
	ERR_FAIL_COND_V(!impl->initialized, 0.0);
	const PxTransform actor_pose = impl->vehicle.physxActor.rigidBody->getGlobalPose();
	// right.y/up.y ratio isolates rotation about the forward axis regardless
	// of yaw -- exact when pitch is small (always true here, the bike never
	// pitches far from level), which is the practical case a lean controller
	// cares about. Positive = leaning right.
	const PxVec3 right = actor_pose.q.getBasisVector0();
	const PxVec3 up = actor_pose.q.getBasisVector1();
	return (real_t)Math::atan2((double)right.y, (double)up.y);
}

Vector3 PhysXMotorcycleProbe::get_actor_position() const {
	ERR_FAIL_COND_V(!impl->initialized, Vector3());
	return px_to_godot(impl->vehicle.physxActor.rigidBody->getGlobalPose().p);
}

real_t PhysXMotorcycleProbe::get_wheel_jounce(int p_wheel) const {
	ERR_FAIL_COND_V(!impl->initialized, 0.0);
	ERR_FAIL_INDEX_V(p_wheel, 2, 0.0);
	return (real_t)impl->vehicle.suspensionStates[p_wheel].jounce;
}

real_t PhysXMotorcycleProbe::get_wheel_separation(int p_wheel) const {
	ERR_FAIL_COND_V(!impl->initialized, 0.0);
	ERR_FAIL_INDEX_V(p_wheel, 2, 0.0);
	return (real_t)impl->vehicle.suspensionStates[p_wheel].separation;
}

void PhysXMotorcycleProbe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_forward_speed"), &PhysXMotorcycleProbe::get_forward_speed);
	ClassDB::bind_method(D_METHOD("get_roll_angle"), &PhysXMotorcycleProbe::get_roll_angle);
	ClassDB::bind_method(D_METHOD("initialize", "space", "position"), &PhysXMotorcycleProbe::initialize);
	ClassDB::bind_method(D_METHOD("step", "dt", "throttle", "brake", "steer"), &PhysXMotorcycleProbe::step);
	ClassDB::bind_method(D_METHOD("apply_torque_impulse", "impulse"), &PhysXMotorcycleProbe::apply_torque_impulse);
	ClassDB::bind_method(D_METHOD("set_angular_velocity", "angular_velocity"), &PhysXMotorcycleProbe::set_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_position"), &PhysXMotorcycleProbe::get_position);
	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &PhysXMotorcycleProbe::get_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_up"), &PhysXMotorcycleProbe::get_up);
	ClassDB::bind_method(D_METHOD("get_forward"), &PhysXMotorcycleProbe::get_forward);
	ClassDB::bind_method(D_METHOD("get_angular_velocity"), &PhysXMotorcycleProbe::get_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_wheel_jounce", "wheel"), &PhysXMotorcycleProbe::get_wheel_jounce);
	ClassDB::bind_method(D_METHOD("get_wheel_separation", "wheel"), &PhysXMotorcycleProbe::get_wheel_separation);
	ClassDB::bind_method(D_METHOD("get_actor_position"), &PhysXMotorcycleProbe::get_actor_position);
}

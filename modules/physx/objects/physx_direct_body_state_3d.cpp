/**************************************************************************/
/*  physx_direct_body_state_3d.cpp                                        */
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

#include "physx_direct_body_state_3d.h"
#include "../physx_server.h"
#include "../spaces/physx_direct_space_state_3d.h"
#include "../spaces/physx_space_3d.h"
#include "physx_body_3d.h"

#include "PxPhysicsAPI.h"
#include "extensions/PxRigidBodyExt.h"

PhysXDirectBodyState3D::PhysXDirectBodyState3D(PhysXBody3D *p_body) :
		body(p_body) {
}

static inline physx::PxRigidDynamic *_dyn(const PhysXBody3D *p_body) {
	return p_body ? p_body->get_px_dynamic() : nullptr;
}

// ---------------------------------------------------------------------------
// Gravity / damp
// ---------------------------------------------------------------------------

Vector3 PhysXDirectBodyState3D::get_total_gravity() const {
	return body ? body->get_cached_total_gravity() : Vector3();
}
real_t PhysXDirectBodyState3D::get_total_linear_damp() const {
	return body ? body->get_cached_total_linear_damp() : 0.0;
}
real_t PhysXDirectBodyState3D::get_total_angular_damp() const {
	return body ? body->get_cached_total_angular_damp() : 0.0;
}

// ---------------------------------------------------------------------------
// Mass properties
// ---------------------------------------------------------------------------

Vector3 PhysXDirectBodyState3D::get_center_of_mass() const {
	if (const physx::PxRigidDynamic *dyn = _dyn(body)) {
		// Godot's contract (godot_physics_3d's direct-state center_of_mass):
		// a WORLD-frame OFFSET from the body's transform origin — the local
		// COM rotated into world axes, NOT translated by the pose (the
		// built-in stores and returns it exactly that way; its own
		// get_velocity_in_local_point(rel_pos) subtracts it from a
		// world-origin-relative point). Returning the global COM position
		// here leaks the body's world position into every caller.
		physx::PxTransform gp = dyn->getGlobalPose();
		physx::PxTransform com_local = dyn->getCMassLocalPose();
		physx::PxVec3 com_world = gp.transform(com_local.p);
		physx::PxVec3 offset = com_world - gp.p;
		return Vector3(offset.x, offset.y, offset.z);
	}
	return Vector3();
}

Vector3 PhysXDirectBodyState3D::get_center_of_mass_local() const {
	if (const physx::PxRigidDynamic *dyn = _dyn(body)) {
		physx::PxVec3 p = dyn->getCMassLocalPose().p;
		return Vector3(p.x, p.y, p.z);
	}
	return Vector3();
}

Basis PhysXDirectBodyState3D::get_principal_inertia_axes() const {
	if (const physx::PxRigidDynamic *dyn = _dyn(body)) {
		// The orientation of the center-of-mass frame is the principal inertia
		// axes rotation, relative to the actor's local frame.
		const physx::PxQuat q = dyn->getCMassLocalPose().q;
		return Basis(Quaternion(q.x, q.y, q.z, q.w));
	}
	return Basis();
}

real_t PhysXDirectBodyState3D::get_inverse_mass() const {
	if (const physx::PxRigidDynamic *dyn = _dyn(body)) {
		physx::PxReal m = dyn->getMass();
		return m > 0.0f ? (real_t)(1.0 / m) : 0.0;
	}
	return 0.0;
}

Vector3 PhysXDirectBodyState3D::get_inverse_inertia() const {
	if (const physx::PxRigidDynamic *dyn = _dyn(body)) {
		physx::PxVec3 i = dyn->getMassSpaceInvInertiaTensor();
		return Vector3(i.x, i.y, i.z);
	}
	return Vector3();
}

Basis PhysXDirectBodyState3D::get_inverse_inertia_tensor() const {
	if (const physx::PxRigidDynamic *dyn = _dyn(body)) {
		physx::PxVec3 inv = dyn->getMassSpaceInvInertiaTensor();
		// Transform the mass-space diagonal into world space through the COM
		// frame: the mass-space tensor lives in the principal-axes frame whose
		// rotation relative to the actor is getCMassLocalPose().q (the same
		// value get_principal_inertia_axes() reports). World rotation of that
		// frame = actor rotation ∘ COM-frame rotation — using the actor
		// rotation alone is wrong for every asymmetric body (OBJ-2).
		// World-space tensor = R * diag(inv) * Rᵀ.
		const physx::PxQuat px_rot = dyn->getGlobalPose().q * dyn->getCMassLocalPose().q;
		Basis R(Quaternion(px_rot.x, px_rot.y, px_rot.z, px_rot.w));
		Basis diag(Vector3(inv.x, 0, 0), Vector3(0, inv.y, 0), Vector3(0, 0, inv.z));
		return R * diag * R.transposed();
	}
	return Basis();
}

// ---------------------------------------------------------------------------
// Velocities
// ---------------------------------------------------------------------------

void PhysXDirectBodyState3D::set_linear_velocity(const Vector3 &p_velocity) {
	if (body) {
		body->set_state(PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY, p_velocity);
	}
}

Vector3 PhysXDirectBodyState3D::get_linear_velocity() const {
	if (!body) {
		return Vector3();
	}
	// Kinematic bodies have no simulated velocity; the body derives it from
	// the per-step pose delta (see PhysXBody3D::_update_kinematic_velocity).
	if (body->get_mode() == PhysicsServer3D::BODY_MODE_KINEMATIC) {
		return body->get_kinematic_linear_velocity();
	}

	if (const physx::PxRigidDynamic *dyn = _dyn(body)) {
		physx::PxVec3 v = dyn->getLinearVelocity();
		return Vector3(v.x, v.y, v.z);
	}
	return Vector3();
}

void PhysXDirectBodyState3D::set_angular_velocity(const Vector3 &p_velocity) {
	if (body) {
		body->set_state(PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY, p_velocity);
	}
}

Vector3 PhysXDirectBodyState3D::get_angular_velocity() const {
	if (!body) {
		return Vector3();
	}
	// Kinematic bodies have no simulated velocity; the body derives it from
	// the per-step pose delta (see PhysXBody3D::_update_kinematic_velocity).
	if (body->get_mode() == PhysicsServer3D::BODY_MODE_KINEMATIC) {
		return body->get_kinematic_angular_velocity();
	}
	if (const physx::PxRigidDynamic *dyn = _dyn(body)) {
		physx::PxVec3 v = dyn->getAngularVelocity();
		return Vector3(v.x, v.y, v.z);
	}
	return Vector3();
}

// ---------------------------------------------------------------------------
// Transform
// ---------------------------------------------------------------------------

void PhysXDirectBodyState3D::set_transform(const Transform3D &p_transform) {
	if (body) {
		body->set_state(PhysicsServer3D::BODY_STATE_TRANSFORM, p_transform);
	}
}

Transform3D PhysXDirectBodyState3D::get_transform() const {
	if (body) {
		return body->get_state(PhysicsServer3D::BODY_STATE_TRANSFORM);
	}
	return Transform3D();
}

Vector3 PhysXDirectBodyState3D::get_velocity_at_local_position(const Vector3 &p_position) const {
	const physx::PxRigidDynamic *dyn = _dyn(body);
	if (!dyn) {
		return Vector3();
	}
	// Godot's contract (godot_physics_3d's get_velocity_in_local_point, which
	// the scene's CharacterBody3D platform-velocity path relies on): despite
	// the name, p_position is a WORLD-frame OFFSET from the body's transform
	// origin, used as-is -- NO rotation applied. (CharacterBody3D passes
	// gt.origin - platform.origin; rotating that by the platform's pose
	// computes the velocity at a different point, and a rider standing on a
	// spinning platform under-measures the ride velocity.) The spin
	// contribution is measured about the center of mass, at origin + offset.
	const physx::PxTransform gp = dyn->getGlobalPose();
	const physx::PxVec3 world_point(gp.p.x + p_position.x, gp.p.y + p_position.y, gp.p.z + p_position.z);
	physx::PxVec3 vel = physx::PxRigidBodyExt::getVelocityAtPos(*const_cast<physx::PxRigidDynamic *>(dyn), world_point);
	return Vector3(vel.x, vel.y, vel.z);
}

// ---------------------------------------------------------------------------
// Forces / impulses
// ---------------------------------------------------------------------------

void PhysXDirectBodyState3D::apply_central_impulse(const Vector3 &p_impulse) {
	if (body) {
		body->apply_central_impulse(p_impulse);
	}
}
void PhysXDirectBodyState3D::apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position) {
	if (body) {
		body->apply_impulse(p_impulse, p_position);
	}
}
void PhysXDirectBodyState3D::apply_torque_impulse(const Vector3 &p_impulse) {
	if (body) {
		body->apply_torque_impulse(p_impulse);
	}
}
void PhysXDirectBodyState3D::apply_central_force(const Vector3 &p_force) {
	if (body) {
		body->apply_central_force(p_force);
	}
}
void PhysXDirectBodyState3D::apply_force(const Vector3 &p_force, const Vector3 &p_position) {
	if (body) {
		body->apply_force(p_force, p_position);
	}
}
void PhysXDirectBodyState3D::apply_torque(const Vector3 &p_torque) {
	if (body) {
		body->apply_torque(p_torque);
	}
}
void PhysXDirectBodyState3D::add_constant_central_force(const Vector3 &p_force) {
	if (body) {
		body->add_constant_central_force(p_force);
	}
}
void PhysXDirectBodyState3D::add_constant_force(const Vector3 &p_force, const Vector3 &p_position) {
	if (body) {
		body->add_constant_force(p_force, p_position);
	}
}
void PhysXDirectBodyState3D::add_constant_torque(const Vector3 &p_torque) {
	if (body) {
		body->add_constant_torque(p_torque);
	}
}
void PhysXDirectBodyState3D::set_constant_force(const Vector3 &p_force) {
	if (body) {
		body->set_constant_force(p_force);
	}
}
Vector3 PhysXDirectBodyState3D::get_constant_force() const {
	return body ? body->get_constant_force() : Vector3();
}
void PhysXDirectBodyState3D::set_constant_torque(const Vector3 &p_torque) {
	if (body) {
		body->set_constant_torque(p_torque);
	}
}
Vector3 PhysXDirectBodyState3D::get_constant_torque() const {
	return body ? body->get_constant_torque() : Vector3();
}

// ---------------------------------------------------------------------------
// Sleep
// ---------------------------------------------------------------------------

void PhysXDirectBodyState3D::set_sleep_state(bool p_sleep) {
	if (body) {
		body->set_state(PhysicsServer3D::BODY_STATE_SLEEPING, p_sleep);
	}
}

bool PhysXDirectBodyState3D::is_sleeping() const {
	if (body) {
		return body->get_state(PhysicsServer3D::BODY_STATE_SLEEPING);
	}
	return false;
}

// ---------------------------------------------------------------------------
// Collision
// ---------------------------------------------------------------------------

void PhysXDirectBodyState3D::set_collision_layer(uint32_t p_layer) {
	if (body) {
		body->set_collision_layer(p_layer);
	}
}
uint32_t PhysXDirectBodyState3D::get_collision_layer() const {
	return body ? body->get_collision_layer() : 0;
}
void PhysXDirectBodyState3D::set_collision_mask(uint32_t p_mask) {
	if (body) {
		body->set_collision_mask(p_mask);
	}
}
uint32_t PhysXDirectBodyState3D::get_collision_mask() const {
	return body ? body->get_collision_mask() : 0;
}

// ---------------------------------------------------------------------------
// Contacts (read from the body's per-step buffer)
// ---------------------------------------------------------------------------

int PhysXDirectBodyState3D::get_contact_count() const {
	return body ? body->get_report_contacts().size() : 0;
}

Vector3 PhysXDirectBodyState3D::get_contact_local_position(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, get_contact_count(), Vector3());
	return body->get_report_contacts()[p_contact_idx].local_position;
}

Vector3 PhysXDirectBodyState3D::get_contact_local_normal(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, get_contact_count(), Vector3());
	return body->get_report_contacts()[p_contact_idx].local_normal;
}

Vector3 PhysXDirectBodyState3D::get_contact_impulse(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, get_contact_count(), Vector3());
	return body->get_report_contacts()[p_contact_idx].impulse;
}

int PhysXDirectBodyState3D::get_contact_local_shape(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, get_contact_count(), -1);
	return body->get_report_contacts()[p_contact_idx].local_shape;
}

Vector3 PhysXDirectBodyState3D::get_contact_local_velocity_at_position(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, get_contact_count(), Vector3());
	return body->get_report_contacts()[p_contact_idx].local_velocity;
}

RID PhysXDirectBodyState3D::get_contact_collider(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, get_contact_count(), RID());
	return body->get_report_contacts()[p_contact_idx].collider;
}

Vector3 PhysXDirectBodyState3D::get_contact_collider_position(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, get_contact_count(), Vector3());
	return body->get_report_contacts()[p_contact_idx].collider_position;
}

ObjectID PhysXDirectBodyState3D::get_contact_collider_id(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, get_contact_count(), ObjectID());
	return body->get_report_contacts()[p_contact_idx].collider_id;
}

int PhysXDirectBodyState3D::get_contact_collider_shape(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, get_contact_count(), -1);
	return body->get_report_contacts()[p_contact_idx].collider_shape;
}

Vector3 PhysXDirectBodyState3D::get_contact_collider_velocity_at_position(int p_contact_idx) const {
	ERR_FAIL_INDEX_V(p_contact_idx, get_contact_count(), Vector3());
	return body->get_report_contacts()[p_contact_idx].collider_velocity;
}

real_t PhysXDirectBodyState3D::get_step() const {
	return step;
}

PhysicsDirectSpaceState3D *PhysXDirectBodyState3D::get_space_state() {
	return body && body->get_space() ? body->get_space()->get_direct_state() : nullptr;
}

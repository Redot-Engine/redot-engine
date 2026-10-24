/**************************************************************************/
/*  physx_joint_3d.cpp                                                    */
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
 * @file physx_joint_3d.cpp
 * @brief Implementation of PhysXJoint3D — type-dispatched wrapper for PhysX joints.
 */

#include "physx_joint_3d.h"
#include "../objects/physx_body_3d.h"
#include "../physx_conversions.h"
#include "../physx_server.h"
#include "../spaces/physx_space_3d.h"

#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"
#include "core/string/print_string.h"

#include "PxPhysics.h"
#include "common/PxTolerancesScale.h"
#include "extensions/PxD6Joint.h"
#include "extensions/PxJointLimit.h"
#include "extensions/PxPrismaticJoint.h"
#include "extensions/PxRevoluteJoint.h"
#include "foundation/PxMat33.h"
#include "foundation/PxTransform.h"

// Godot's G6DOF motor is a pure velocity servo: accelerate toward the target
// velocity with up to the force limit and no position term. A PxD6JointDrive
// is a force-limited damped spring — force = stiffness * (targetPos - pos) +
// damping * (targetVel - vel) — so the servo is emulated with zero stiffness
// and a large finite damping: the drive force is damping * velocity-error,
// saturated at the force limit, i.e. the Godot motor. Any value large enough
// to keep the drive saturated for realistic force limits works (same
// technique as the reference PhysX backend's 1.0e6 motor drives).
static constexpr float G6DOF_MOTOR_DAMPING = 1.0e6f;

// ---------------------------------------------------------------------------
// Static helpers — convert between Godot and PhysX types
// (delegating to the module-wide conversions header, which clamps degenerate
// transforms before they can assert inside PhysX)
// ---------------------------------------------------------------------------

physx::PxTransform PhysXJoint3D::to_physx_transform(const Transform3D &p_transform) {
	return godot_to_px(p_transform);
}

physx::PxVec3 PhysXJoint3D::to_physx_vec3(const Vector3 &p_vec) {
	return godot_to_px(p_vec);
}

Vector3 PhysXJoint3D::to_godot_vec3(const physx::PxVec3 &p_vec) {
	return px_to_godot(p_vec);
}

/**
 * @brief Builds a PxTransform from a pivot point and axis direction.
 *
 * The axis becomes the X-axis of the joint frame (PhysX convention).
 * The Y and Z axes are orthogonalized via Gram-Schmidt.
 */
physx::PxTransform PhysXJoint3D::compute_joint_frame(const physx::PxVec3 &p_pivot, const physx::PxVec3 &p_axis) {
	// A zero-length axis (caller bug, reachable from scripts) would normalize
	// to NaN and poison the joint quaternion; fall back to an axis-less frame
	// at the pivot so the joint stays well-defined.
	if (p_axis.magnitudeSquared() < 1.0e-12f) {
		return physx::PxTransform(p_pivot, physx::PxQuat(physx::PxIdentity));
	}

	// X = axis direction
	physx::PxVec3 x = p_axis;
	x.normalize();

	// Y = arbitrary orthogonal vector (cross with world-up, fallback to world-right)
	physx::PxVec3 world_up(0.0f, 1.0f, 0.0f);
	physx::PxVec3 y;
	if (fabsf(x.dot(world_up)) > 0.99f) {
		// Axis is nearly parallel to world-up, use world-right instead.
		physx::PxVec3 world_right(1.0f, 0.0f, 0.0f);
		y = x.cross(world_right);
	} else {
		y = x.cross(world_up);
	}
	y.normalize();

	// Z = x × y
	physx::PxVec3 z = x.cross(y);

	// Build a 3x3 rotation matrix from the orthogonal basis and convert to quaternion.
	physx::PxMat33 rotmat(x, y, z);
	physx::PxQuat px_quat(rotmat);
	return physx::PxTransform(p_pivot, px_quat);
}

// ---------------------------------------------------------------------------
// Construction / destruction / lifecycle
// ---------------------------------------------------------------------------

PhysXJoint3D::~PhysXJoint3D() {
	release();
}

void PhysXJoint3D::release() {
	_destroy_px_joint();
	// Unlink from the connected bodies so they drop their back-pointer to this
	// (about-to-die) wrapper.
	if (body_a) {
		body_a->remove_joint(this);
	}
	if (body_b) {
		body_b->remove_joint(this);
	}
	kind = JOINT_KIND_NONE;
	body_a = nullptr;
	body_b = nullptr;
}

void PhysXJoint3D::body_removed(PhysXBody3D *p_body) {
	if (p_body != body_a && p_body != body_b) {
		return;
	}
	// The PxJoint is bound to both actors — losing either body invalidates the
	// whole constraint. The wrapper, its kind and the full parameter cache
	// survive so the joint can be reconfigured (make) later, and so the
	// owning Joint3D node keeps a valid RID with readable state.
	_destroy_px_joint();
	if (body_a == p_body) {
		body_a->remove_joint(this);
		body_a = nullptr;
	}
	if (body_b == p_body) {
		body_b->remove_joint(this);
		body_b = nullptr;
	}
}

void PhysXJoint3D::actor_invalidated(PhysXBody3D *p_body) {
	if (p_body != body_a && p_body != body_b) {
		return;
	}
	// The attached actor is about to be released and re-created (static<->
	// dynamic flip): PhysX deletes a constraint when either of its actors
	// dies, so drop the PxJoint here, while the old actor is still alive and
	// the release is a plain constraint release. Unlike body_removed the
	// wrapper keeps both body links; the rebuild triggered from
	// PhysXBody3D::_add_to_scene re-creates the constraint against the fresh
	// actor and _apply_params restores the cached configuration.
	_destroy_px_joint();
}

void PhysXJoint3D::_reset_param_caches() {
	pin_params = PinJointParams();
	hinge_params = HingeJointParams();
	slider_params = SliderJointParams();
	slider_limit_enabled = false;
	cone_twist_params = ConeTwistJointParams();
	cone_limits_set = false;
	for (int a = 0; a < 6; a++) {
		g6dof_params[a] = G6DOFJointAxisParams();
		g6dof_flags[a] = G6DOFJointAxisFlags();
		g6dof_lin_drives[a] = G6DOFDriveState();
		g6dof_ang_drives[a] = G6DOFDriveState();
	}
	cached_g6dof_lin_drive_vel = physx::PxVec3(0.0f);
	cached_g6dof_ang_drive_vel = physx::PxVec3(0.0f);
}

void PhysXJoint3D::make(PhysicsServer3D::JointType p_type, JointKind p_kind,
		PhysXBody3D *p_body_a, const physx::PxTransform &p_local_a,
		PhysXBody3D *p_body_b, const physx::PxTransform &p_local_b) {
	// A fresh configuration must not inherit state from the joint that
	// previously occupied this RID (godot_physics's joint_make_* replaces the
	// joint object with fresh defaults; re-making the same kind would
	// otherwise resurrect stale limits/motors onto the new PxJoint). Setters
	// must run AFTER make(); params set before make are dropped, like on the
	// reference backend.
	_reset_param_caches();

	// Reconfiguring an existing RID: drop the old PxJoint and re-link to the
	// given bodies.
	_destroy_px_joint();
	if (body_a) {
		body_a->remove_joint(this);
	}
	if (body_b) {
		body_b->remove_joint(this);
	}
	kind = p_kind;
	body_a = p_body_a;
	body_b = p_body_b;
	frame_a = p_local_a;
	frame_b = p_local_b;
	if (body_a) {
		body_a->add_joint(this);
	}
	if (body_b) {
		body_b->add_joint(this);
	}
	rebuild();
}

void PhysXJoint3D::rebuild() {
	if (px_joint || kind == JOINT_KIND_NONE) {
		return;
	}
	px_joint = _create();
	if (!px_joint) {
		return; // stays dormant — configuration can't drive a constraint yet
	}
	// Restore the cached configuration onto the fresh PxJoint. A fresh
	// PxD6Joint defaults to all-locked motions, so for 6DOF this is what
	// turns every axis FREE unless a limit flag says otherwise.
	_apply_params();
}

void PhysXJoint3D::_destroy_px_joint() {
	if (!px_joint) {
		return;
	}
	// Releasing the constraint mutates the owning scene — fetch an in-flight
	// solve first (async stepping), resolved through the connected bodies.
	for (PhysXBody3D *body : { body_a, body_b }) {
		if (body && body->get_space()) {
			body->get_space()->ensure_synced();
		}
	}
	// Wake the connected dynamics before destroying the constraint: a body
	// that fell asleep while constrained has gravity integration skipped
	// (see PhysXBody3D::on_pre_step), so it would keep floating in place
	// after the joint disappears. Kinematic dynamics never sleep and reject
	// wakeUp() ("Body must be non-kinematic!"), so they are skipped as well.
	for (PhysXBody3D *body : { body_a, body_b }) {
		if (!body) {
			continue;
		}
		physx::PxRigidActor *actor = body->get_rigid_actor();
		if (actor && actor->getScene() && actor->is<physx::PxRigidDynamic>()) {
			physx::PxRigidDynamic *dyn = static_cast<physx::PxRigidDynamic *>(actor);
			if (!(dyn->getRigidBodyFlags() & physx::PxRigidBodyFlag::eKINEMATIC)) {
				dyn->wakeUp();
			}
		}
	}
	px_joint->release();
	px_joint = nullptr;
}

physx::PxJoint *PhysXJoint3D::_create() {
	physx::PxRigidActor *actor_a = body_a ? body_a->get_rigid_actor() : nullptr;
	physx::PxRigidActor *actor_b = body_b ? body_b->get_rigid_actor() : nullptr;

	// PhysX joints require at least one dynamic rigid actor; a Joint3D node
	// transiently configures the joint as soon as the first node path is set
	// (the other side still unset, i.e. the world frame). Stay dormant in that
	// state without an error -- the joint is recreated once both node paths
	// are assigned (or a body turns dynamic and notifies its joints).
	if (!(actor_a && actor_a->is<physx::PxRigidDynamic>()) && !(actor_b && actor_b->is<physx::PxRigidDynamic>())) {
		print_verbose("PhysX: joint dormant -- no dynamic actor yet (waiting for Joint3D to assign both nodes).");
		return nullptr;
	}

	// PhysX has no self-constraints: a joint whose endpoints resolve to the
	// same actor puts a degenerate constraint in the solver -- checked builds
	// abort on a worker thread mid-solve (silent process death, no handler
	// output). Godot's own solver no-ops A==B joints, so stay dormant and
	// re-resolve on the next make()/rebuild() instead of creating one.
	if (actor_a && actor_b && actor_a == actor_b) {
		WARN_PRINT_ONCE("PhysX: joint endpoints resolve to the same actor; the constraint is skipped and the joint stays dormant until reconfigured.");
		return nullptr;
	}

	physx::PxJoint *created = create_px_joint(PhysXServer3D::get_singleton()->get_physics(),
			static_cast<PhysicsServer3D::JointType>(kind), actor_a, frame_a, actor_b, frame_b);
	return created;
}

// ---------------------------------------------------------------------------
// 6DOF helpers — axis mapping and drive velocity caching
// ---------------------------------------------------------------------------
physx::PxD6Axis::Enum PhysXJoint3D::_px_linear_axis(Vector3::Axis p_axis) {
	switch (p_axis) {
		case Vector3::AXIS_X:
			return physx::PxD6Axis::eX;
		case Vector3::AXIS_Y:
			return physx::PxD6Axis::eY;
		case Vector3::AXIS_Z:
			return physx::PxD6Axis::eZ;
		default:
			return physx::PxD6Axis::eX;
	}
}

physx::PxD6Axis::Enum PhysXJoint3D::_px_angular_axis(Vector3::Axis p_axis) {
	switch (p_axis) {
		case Vector3::AXIS_X:
			return physx::PxD6Axis::eTWIST;
		case Vector3::AXIS_Y:
			return physx::PxD6Axis::eSWING1;
		case Vector3::AXIS_Z:
			return physx::PxD6Axis::eSWING2;
		default:
			return physx::PxD6Axis::eTWIST;
	}
}

// Cache store for the 6DOF parameter switch — always runs, even while the
// joint is dormant (no PxJoint yet), so nothing a node configures is lost.
static void _store_g6dof_param(G6DOFJointAxisParams &p, PhysicsServer3D::G6DOFJointAxisParam p_param, real_t p_value) {
	switch (p_param) {
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_LOWER_LIMIT:
			p.linear_lower_limit = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_UPPER_LIMIT:
			p.linear_upper_limit = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS:
			p.linear_limit_softness = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_RESTITUTION:
			p.linear_restitution = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_DAMPING:
			p.linear_damping = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_MOTOR_TARGET_VELOCITY:
			p.linear_motor_target_velocity = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_MOTOR_FORCE_LIMIT:
			p.linear_motor_force_limit = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_STIFFNESS:
			p.linear_spring_stiffness = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_DAMPING:
			p.linear_spring_damping = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT:
			p.linear_spring_equilibrium_point = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_LOWER_LIMIT:
			p.angular_lower_limit = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_UPPER_LIMIT:
			p.angular_upper_limit = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS:
			p.angular_limit_softness = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_RESTITUTION:
			p.angular_restitution = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_DAMPING:
			p.angular_damping = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_FORCE_LIMIT:
			p.angular_force_limit = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_ERP:
			p.angular_erp = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_MOTOR_TARGET_VELOCITY:
			p.angular_motor_target_velocity = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_MOTOR_FORCE_LIMIT:
			p.angular_motor_force_limit = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS:
			p.angular_spring_stiffness = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_DAMPING:
			p.angular_spring_damping = (float)p_value;
			break;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT:
			p.angular_spring_equilibrium_point = (float)p_value;
			break;
		default:
			break;
	}
}

void PhysXJoint3D::cache_g6dof_drive_velocity(const physx::PxVec3 &p_lin, const physx::PxVec3 &p_ang) {
	cached_g6dof_lin_drive_vel = p_lin;
	cached_g6dof_ang_drive_vel = p_ang;
}

void PhysXJoint3D::apply_cached_g6dof_drive_velocity(physx::PxD6Joint *p_d6) {
	p_d6->setDriveVelocity(cached_g6dof_lin_drive_vel, cached_g6dof_ang_drive_vel, true);
}

bool PhysXJoint3D::has_running_motor() const {
	if (kind == JOINT_KIND_HINGE) {
		return hinge_params.enable_motor && hinge_params.motor_target_velocity != 0.0f;
	}
	if (kind == JOINT_KIND_6DOF) {
		for (int a = 0; a < 3; a++) {
			if ((g6dof_flags[a].linear_motor && g6dof_params[a].linear_motor_target_velocity != 0.0f) ||
					(g6dof_flags[a].angular_motor && g6dof_params[a].angular_motor_target_velocity != 0.0f)) {
				return true;
			}
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Factory — creates the correct PhysX joint type based on Godot joint type
// ---------------------------------------------------------------------------
physx::PxJoint *PhysXJoint3D::create_px_joint(physx::PxPhysics &p_physics,
		PhysicsServer3D::JointType p_type,
		physx::PxRigidActor *p_body_a,
		const physx::PxTransform &p_local_a,
		physx::PxRigidActor *p_body_b,
		const physx::PxTransform &p_local_b) {
	physx::PxJoint *px_joint = nullptr;

	switch (p_type) {
		case PhysicsServer3D::JOINT_TYPE_PIN: {
			// PinJoint = pure ball joint: all 3 linear DOFs locked, all 3
			// angular DOFs free. PxD6Joint is the cleanest way to express this.
			px_joint = physx::PxD6JointCreate(p_physics, p_body_a, p_local_a, p_body_b, p_local_b);
			if (px_joint) {
				physx::PxD6Joint *d6 = static_cast<physx::PxD6Joint *>(px_joint);
				// Lock all linear DOFs (X, Y, Z)
				d6->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLOCKED);
				d6->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLOCKED);
				d6->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLOCKED);
				// Free all angular DOFs (twist, swing1, swing2)
				d6->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eFREE);
				d6->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eFREE);
				d6->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eFREE);
			}
			break;
		}
		case PhysicsServer3D::JOINT_TYPE_HINGE: {
			px_joint = physx::PxRevoluteJointCreate(p_physics, p_body_a, p_local_a, p_body_b, p_local_b);
			break;
		}
		case PhysicsServer3D::JOINT_TYPE_SLIDER: {
			px_joint = physx::PxPrismaticJointCreate(p_physics, p_body_a, p_local_a, p_body_b, p_local_b);
			break;
		}
		case PhysicsServer3D::JOINT_TYPE_CONE_TWIST: {
			px_joint = physx::PxD6JointCreate(p_physics, p_body_a, p_local_a, p_body_b, p_local_b);
			if (px_joint) {
				physx::PxD6Joint *d6 = static_cast<physx::PxD6Joint *>(px_joint);
				// ConeTwist: linear DOFs locked, twist free, swing limited
				d6->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLOCKED);
				d6->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLOCKED);
				d6->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLOCKED);
				d6->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eLIMITED);
				d6->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eLIMITED);
				d6->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eLIMITED);
			}
			break;
		}
		case PhysicsServer3D::JOINT_TYPE_6DOF: {
			px_joint = physx::PxD6JointCreate(p_physics, p_body_a, p_local_a, p_body_b, p_local_b);
			break;
		}
		default:
			ERR_FAIL_V_MSG(nullptr, "PhysX: unsupported joint type.");
	}

	if (!px_joint) {
		ERR_FAIL_V_MSG(nullptr, "PhysX: failed to create joint.");
	}

	return px_joint;
}

// ---------------------------------------------------------------------------
// Cached configuration -> live PxJoint
// ---------------------------------------------------------------------------

void PhysXJoint3D::_apply_params() {
	if (!px_joint) {
		return;
	}
	px_joint->setConstraintFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, !collisions_disabled);

	switch (kind) {
		case JOINT_KIND_PIN: {
			px_joint->setLocalPose(physx::PxJointActorIndex::eACTOR0, frame_a);
			px_joint->setLocalPose(physx::PxJointActorIndex::eACTOR1, frame_b);
		} break;

		case JOINT_KIND_HINGE: {
			physx::PxRevoluteJoint *revolute = static_cast<physx::PxRevoluteJoint *>(px_joint);
			revolute->setRevoluteJointFlag(physx::PxRevoluteJointFlag::eLIMIT_ENABLED, hinge_params.use_limit);
			if (hinge_params.use_limit) {
				_apply_hinge_limit();
			}
			revolute->setRevoluteJointFlag(physx::PxRevoluteJointFlag::eDRIVE_ENABLED, hinge_params.enable_motor);
			if (hinge_params.enable_motor) {
				revolute->setDriveVelocity(-hinge_params.motor_target_velocity, true); // runs the other way round too (see _apply_hinge_limit)
				revolute->setDriveForceLimit(hinge_params.motor_max_impulse);
			}
		} break;

		case JOINT_KIND_SLIDER: {
			physx::PxPrismaticJoint *prismatic = static_cast<physx::PxPrismaticJoint *>(px_joint);
			// The limit constraint only exists once a linear limit bound was
			// actually configured (see set_slider_param) — a fresh slider must
			// stay unlimited.
			if (slider_limit_enabled) {
				physx::PxJointLinearLimitPair limit = _slider_limit();
				prismatic->setLimit(limit);
				prismatic->setPrismaticJointFlag(physx::PxPrismaticJointFlag::eLIMIT_ENABLED, true);
			}
		} break;

		case JOINT_KIND_CONE_TWIST: {
			physx::PxD6Joint *d6 = static_cast<physx::PxD6Joint *>(px_joint);
			d6->setMotion(physx::PxD6Axis::eX, physx::PxD6Motion::eLOCKED);
			d6->setMotion(physx::PxD6Axis::eY, physx::PxD6Motion::eLOCKED);
			d6->setMotion(physx::PxD6Axis::eZ, physx::PxD6Motion::eLOCKED);
			d6->setMotion(physx::PxD6Axis::eTWIST, physx::PxD6Motion::eLIMITED);
			d6->setMotion(physx::PxD6Axis::eSWING1, physx::PxD6Motion::eLIMITED);
			d6->setMotion(physx::PxD6Axis::eSWING2, physx::PxD6Motion::eLIMITED);
			// Wide SDK-default cones until a span/bias parameter was set —
			// pushing the clamped cache values on a never-configured joint
			// would read as hair-thin (locked), which is not what a fresh
			// cone-twist does.
			if (cone_limits_set) {
				_apply_cone_twist_limits();
			}
		} break;

		case JOINT_KIND_6DOF: {
			physx::PxD6Joint *d6 = static_cast<physx::PxD6Joint *>(px_joint);
			// Godot's G6DOF motor force limits are FORCES (newton/torque); a
			// PhysX drive forceLimit is an impulse per step unless this flag
			// says otherwise. Without it a "5 N" motor applies ~5 N*s per step
			// — ~300 N at 60 Hz. The hinge's separate constraint keeps the
			// default impulse interpretation (Godot's hinge motor param is an
			// impulse).
			px_joint->setConstraintFlag(physx::PxConstraintFlag::eDRIVE_LIMITS_ARE_FORCES, true);
			// Godot semantics: a fresh 6DOF joint has every axis FREE — limits
			// apply only where the enable flags say so (PxD6Joint's own default
			// is all-LOCKED, which would silently weld the bodies together).
			// _apply_g6dof_linear_limit marks the axis LIMITED only while the
			// limit flag is on AND the cached range is valid.
			for (int a = 0; a < 3; a++) {
				const Vector3::Axis axis = (Vector3::Axis)a;
				const physx::PxD6Axis::Enum ang_axis = _px_angular_axis(axis);

				_apply_g6dof_linear_limit(d6, axis);

				d6->setMotion(ang_axis, g6dof_flags[a].angular_limit ? physx::PxD6Motion::eLIMITED : physx::PxD6Motion::eFREE);
				if (g6dof_flags[a].angular_limit) {
					_apply_g6dof_angular_limit(d6, axis);
				}

				// One resolver call per axis: it picks the drive winner
				// (spring > motor > none) from the cached flags and frees the
				// axis motion when a spring sits on a degenerate limit range.
				_apply_g6dof_axis_drive(d6, axis, false);
				_apply_g6dof_axis_drive(d6, axis, true);
			}
				_apply_g6dof_drive_position();
				// Velocity targets only matter for axes whose resolved drive is a
				// motor (a spring ignores them); harmless when none.
				d6->setDriveVelocity(cached_g6dof_lin_drive_vel, cached_g6dof_ang_drive_vel, true);
			} break;

		default:
			break;
	}
}

// ---------------------------------------------------------------------------
// Joint configuration — set actors and local poses
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Solver priority — stored for round-trip access only.
// ---------------------------------------------------------------------------
// Note: PhysX 5.x doesn't expose setPriority/getPriority on PxConstraint.
void PhysXJoint3D::set_solver_priority(int p_priority) {
	solver_priority = p_priority;
}

int PhysXJoint3D::get_solver_priority() const {
	return solver_priority;
}

// ---------------------------------------------------------------------------
// Collision between bodies — PxConstraintFlag::eCOLLISION_ENABLED
// ---------------------------------------------------------------------------
void PhysXJoint3D::set_disable_collisions(bool p_disable) {
	// Defaults to true: PhysX constraints start with eCOLLISION_ENABLED off,
	// which is also Godot's own Joint3D default (exclude_nodes_from_collision).
	// _apply_params() re-asserts the cached value on every (re)creation.
	collisions_disabled = p_disable;
	if (!px_joint) {
		return;
	}
	physx::PxConstraint *constraint = px_joint->getConstraint();
	if (constraint) {
		constraint->setFlag(physx::PxConstraintFlag::eCOLLISION_ENABLED, !p_disable);
	}
}

bool PhysXJoint3D::is_disabled_collisions() const {
	return collisions_disabled;
}

// ---------------------------------------------------------------------------
// Pin joint — local frames, params
// ---------------------------------------------------------------------------
void PhysXJoint3D::set_local_a(const Vector3 &p_local_a) {
	frame_a.p = physx::PxVec3(p_local_a.x, p_local_a.y, p_local_a.z);
	if (!px_joint) {
		return;
	}
	px_joint->setLocalPose(physx::PxJointActorIndex::eACTOR0, frame_a);
}

Vector3 PhysXJoint3D::get_local_a() const {
	return to_godot_vec3(frame_a.p);
}

void PhysXJoint3D::set_local_b(const Vector3 &p_local_b) {
	frame_b.p = physx::PxVec3(p_local_b.x, p_local_b.y, p_local_b.z);
	if (!px_joint) {
		return;
	}
	px_joint->setLocalPose(physx::PxJointActorIndex::eACTOR1, frame_b);
}

Vector3 PhysXJoint3D::get_local_b() const {
	return to_godot_vec3(frame_b.p);
}

void PhysXJoint3D::set_pin_param(PhysicsServer3D::PinJointParam p_param, real_t p_value) {
	// PinJoint parameters have no PhysX equivalent — stored for round-trip.
	switch (p_param) {
		case PhysicsServer3D::PIN_JOINT_BIAS:
			pin_params.bias = (float)p_value;
			break;
		case PhysicsServer3D::PIN_JOINT_DAMPING:
			pin_params.damping = (float)p_value;
			break;
		case PhysicsServer3D::PIN_JOINT_IMPULSE_CLAMP:
			pin_params.impulse_clamp = (float)p_value;
			break;
		default:
			break;
	}
}

real_t PhysXJoint3D::get_pin_param(PhysicsServer3D::PinJointParam p_param) const {
	switch (p_param) {
		case PhysicsServer3D::PIN_JOINT_BIAS:
			return (real_t)pin_params.bias;
		case PhysicsServer3D::PIN_JOINT_DAMPING:
			return (real_t)pin_params.damping;
		case PhysicsServer3D::PIN_JOINT_IMPULSE_CLAMP:
			return (real_t)pin_params.impulse_clamp;
		default:
			return 0.0f;
	}
}

// ---------------------------------------------------------------------------
// Hinge joint — params and flags
// ---------------------------------------------------------------------------

physx::PxJointLinearLimitPair PhysXJoint3D::_slider_limit() const {
	physx::PxJointLinearLimitPair limit(physx::PxTolerancesScale(), slider_params.linear_limit_lower, slider_params.linear_limit_upper);
	limit.stiffness = slider_params.linear_limit_softness;
	limit.damping = slider_params.linear_limit_damping;
	limit.restitution = slider_params.linear_limit_restitution;
	return limit;
}

// Rebuilds and applies the hinge angular limit from hinge_params. No-op when
// the limit is disabled. Centralizes the limit construction that was previously
// duplicated across 6 param cases + the USE_LIMIT flag case.
void PhysXJoint3D::_apply_hinge_limit() {
	if (!hinge_params.use_limit || !px_joint) {
		return;
	}
	physx::PxRevoluteJoint *revolute = static_cast<physx::PxRevoluteJoint *>(px_joint);
	// Godot's hinge angle runs the other way round to PhysX's (the built-in's
	// get_hinge_angle() measures B's clockwise turn about the axis relative to
	// A, and Jolt negates the same way): [lower, upper] is [-upper, -lower]
	// here. Passed straight through, a ragdoll's knees bent backwards.
	//
	// Godot's limit_softness/relaxation/bias are deliberately NOT mapped:
	// HingeJoint3D's node defaults push softness=1.0, and PhysX treats any
	// stiffness > 0 as a limit SPRING -- mapping softness straight through
	// turned every node-created hinge limit into a weak spring the joint
	// drags straight through. A hard limit (stiffness = 0) is what the
	// built-in and Jolt do (Jolt warns "softness is not supported"), so the
	// SDK defaults stay.
	physx::PxJointAngularLimitPair limit(-hinge_params.limit_upper, -hinge_params.limit_lower);
	revolute->setLimit(limit);
}

// Builds the D6 drive target from the cached per-axis spring equilibrium
// points and applies it (see the header). Godot's per-axis equilibria are
// authored independently; PhysX consumes one drive transform, so the linear
// components collect into the translation and the angular ones into an
// Euler-derived quaternion.
void PhysXJoint3D::_apply_g6dof_drive_position() {
	if (!px_joint || kind != JOINT_KIND_6DOF) {
		return;
	}
	const Vector3 lin(
			g6dof_params[Vector3::AXIS_X].linear_spring_equilibrium_point,
			g6dof_params[Vector3::AXIS_Y].linear_spring_equilibrium_point,
			g6dof_params[Vector3::AXIS_Z].linear_spring_equilibrium_point);
	const Vector3 ang(
			g6dof_params[Vector3::AXIS_X].angular_spring_equilibrium_point,
			g6dof_params[Vector3::AXIS_Y].angular_spring_equilibrium_point,
			g6dof_params[Vector3::AXIS_Z].angular_spring_equilibrium_point);
	// The other way round too (see _apply_g6dof_angular_limit): Godot's
	// angular measures run opposite to the D6's.
	const Quaternion q = Quaternion::from_euler(-ang);
	physx::PxD6Joint *d6 = static_cast<physx::PxD6Joint *>(px_joint);
	d6->setDrivePosition(physx::PxTransform(godot_to_px(lin), godot_to_px(q)), true);
}

void PhysXJoint3D::set_hinge_param(PhysicsServer3D::HingeJointParam p_param, real_t p_value) {
	switch (p_param) {
		case PhysicsServer3D::HINGE_JOINT_BIAS: {
			// No PhysX equivalent — stored for round-trip.
			hinge_params.bias = (float)p_value;
			break;
		}
		case PhysicsServer3D::HINGE_JOINT_LIMIT_UPPER: {
			hinge_params.limit_upper = (float)p_value;
			_apply_hinge_limit();
			break;
		}
		case PhysicsServer3D::HINGE_JOINT_LIMIT_LOWER: {
			hinge_params.limit_lower = (float)p_value;
			_apply_hinge_limit();
			break;
		}
		case PhysicsServer3D::HINGE_JOINT_LIMIT_BIAS: {
			hinge_params.limit_bias = (float)p_value;
			_apply_hinge_limit();
			break;
		}
		case PhysicsServer3D::HINGE_JOINT_LIMIT_SOFTNESS: {
			hinge_params.limit_softness = (float)p_value;
			_apply_hinge_limit();
			break;
		}
		case PhysicsServer3D::HINGE_JOINT_LIMIT_RELAXATION: {
			hinge_params.limit_relaxation = (float)p_value;
			_apply_hinge_limit();
			break;
		}
		case PhysicsServer3D::HINGE_JOINT_MOTOR_TARGET_VELOCITY: {
			hinge_params.motor_target_velocity = (float)p_value;
			if (px_joint && hinge_params.enable_motor) {
				static_cast<physx::PxRevoluteJoint *>(px_joint)->setDriveVelocity(-hinge_params.motor_target_velocity, true); // runs the other way round too (see _apply_hinge_limit)
			}
			break;
		}
		case PhysicsServer3D::HINGE_JOINT_MOTOR_MAX_IMPULSE: {
			hinge_params.motor_max_impulse = (float)p_value;
			if (px_joint && hinge_params.enable_motor) {
				static_cast<physx::PxRevoluteJoint *>(px_joint)->setDriveForceLimit(hinge_params.motor_max_impulse);
			}
			break;
		}
		default:
			break;
	}
}

real_t PhysXJoint3D::get_hinge_param(PhysicsServer3D::HingeJointParam p_param) const {
	switch (p_param) {
		case PhysicsServer3D::HINGE_JOINT_BIAS:
			return (real_t)hinge_params.bias;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_UPPER:
			return (real_t)hinge_params.limit_upper;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_LOWER:
			return (real_t)hinge_params.limit_lower;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_BIAS:
			return (real_t)hinge_params.limit_bias;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_SOFTNESS:
			return (real_t)hinge_params.limit_softness;
		case PhysicsServer3D::HINGE_JOINT_LIMIT_RELAXATION:
			return (real_t)hinge_params.limit_relaxation;
		case PhysicsServer3D::HINGE_JOINT_MOTOR_TARGET_VELOCITY:
			return (real_t)hinge_params.motor_target_velocity;
		case PhysicsServer3D::HINGE_JOINT_MOTOR_MAX_IMPULSE:
			return (real_t)hinge_params.motor_max_impulse;
		default:
			return 0.0f;
	}
}

void PhysXJoint3D::set_hinge_flag(PhysicsServer3D::HingeJointFlag p_flag, bool p_enabled) {
	physx::PxRevoluteJoint *revolute = px_joint ? static_cast<physx::PxRevoluteJoint *>(px_joint) : nullptr;

	switch (p_flag) {
		case PhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT: {
			hinge_params.use_limit = p_enabled;
			if (revolute) {
				revolute->setRevoluteJointFlag(physx::PxRevoluteJointFlag::eLIMIT_ENABLED, p_enabled);
				if (p_enabled) {
					_apply_hinge_limit();
				}
			}
			break;
		}
		case PhysicsServer3D::HINGE_JOINT_FLAG_ENABLE_MOTOR: {
			hinge_params.enable_motor = p_enabled;
			if (revolute) {
				revolute->setRevoluteJointFlag(physx::PxRevoluteJointFlag::eDRIVE_ENABLED, p_enabled);
				if (p_enabled) {
					revolute->setDriveVelocity(-hinge_params.motor_target_velocity, true); // runs the other way round too (see _apply_hinge_limit)
					revolute->setDriveForceLimit(hinge_params.motor_max_impulse);
				}
			}
			break;
		}
		default:
			break;
	}
}

bool PhysXJoint3D::get_hinge_flag(PhysicsServer3D::HingeJointFlag p_flag) const {
	switch (p_flag) {
		case PhysicsServer3D::HINGE_JOINT_FLAG_USE_LIMIT:
			return hinge_params.use_limit;
		case PhysicsServer3D::HINGE_JOINT_FLAG_ENABLE_MOTOR:
			return hinge_params.enable_motor;
		default:
			return false;
	}
}

// ---------------------------------------------------------------------------
// Slider joint — params
// ---------------------------------------------------------------------------
void PhysXJoint3D::set_slider_param(PhysicsServer3D::SliderJointParam p_param, real_t p_value) {
	physx::PxPrismaticJoint *prismatic = px_joint ? static_cast<physx::PxPrismaticJoint *>(px_joint) : nullptr;

	switch (p_param) {
		// Linear limit upper
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_UPPER: {
			slider_params.linear_limit_upper = (float)p_value;
			slider_limit_enabled = true;
			if (prismatic) {
				physx::PxJointLinearLimitPair limit = _slider_limit();
				prismatic->setLimit(limit);
				prismatic->setPrismaticJointFlag(physx::PxPrismaticJointFlag::eLIMIT_ENABLED, true);
			}
			break;
		}
		// Linear limit lower
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_LOWER: {
			slider_params.linear_limit_lower = (float)p_value;
			slider_limit_enabled = true;
			if (prismatic) {
				physx::PxJointLinearLimitPair limit = _slider_limit();
				prismatic->setLimit(limit);
				prismatic->setPrismaticJointFlag(physx::PxPrismaticJointFlag::eLIMIT_ENABLED, true);
			}
			break;
		}
		// Linear limit softness
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS: {
			slider_params.linear_limit_softness = (float)p_value;
			if (prismatic) {
				physx::PxJointLinearLimitPair limit = _slider_limit();
				prismatic->setLimit(limit);
			}
			break;
		}
		// Linear limit restitution
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION: {
			slider_params.linear_limit_restitution = (float)p_value;
			if (prismatic) {
				physx::PxJointLinearLimitPair limit = _slider_limit();
				prismatic->setLimit(limit);
			}
			break;
		}
		// Linear limit damping
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_DAMPING: {
			slider_params.linear_limit_damping = (float)p_value;
			if (prismatic) {
				physx::PxJointLinearLimitPair limit = _slider_limit();
				prismatic->setLimit(limit);
			}
			break;
		}
		// Angular limit upper
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_UPPER: {
			slider_params.angular_limit_upper = (float)p_value;
			// PhysX prismatic joint has no angular limits — store for round-trip.
			break;
		}
		// Angular limit lower
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_LOWER: {
			slider_params.angular_limit_lower = (float)p_value;
			break;
		}
		// Angular limit softness
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS: {
			slider_params.angular_limit_softness = (float)p_value;
			break;
		}
		// Angular limit restitution
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_RESTITUTION: {
			slider_params.angular_limit_restitution = (float)p_value;
			break;
		}
		// Angular limit damping
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_DAMPING: {
			slider_params.angular_limit_damping = (float)p_value;
			break;
		}
		// Linear motion softness
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_MOTION_SOFTNESS: {
			slider_params.linear_motion_softness = (float)p_value;
			break;
		}
		// Linear motion restitution
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_MOTION_RESTITUTION: {
			slider_params.linear_motion_restitution = (float)p_value;
			break;
		}
		// Linear motion damping
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_MOTION_DAMPING: {
			slider_params.linear_motion_damping = (float)p_value;
			break;
		}
		// Linear orthogonal softness
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_ORTHOGONAL_SOFTNESS: {
			slider_params.linear_orthogonal_softness = (float)p_value;
			break;
		}
		// Linear orthogonal restitution
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_ORTHOGONAL_RESTITUTION: {
			slider_params.linear_orthogonal_restitution = (float)p_value;
			break;
		}
		// Linear orthogonal damping
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_ORTHOGONAL_DAMPING: {
			slider_params.linear_orthogonal_damping = (float)p_value;
			break;
		}
		// Angular motion softness
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_MOTION_SOFTNESS: {
			slider_params.angular_motion_softness = (float)p_value;
			break;
		}
		// Angular motion restitution
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_MOTION_RESTITUTION: {
			slider_params.angular_motion_restitution = (float)p_value;
			break;
		}
		// Angular motion damping
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_MOTION_DAMPING: {
			slider_params.angular_motion_damping = (float)p_value;
			break;
		}
		// Angular orthogonal softness
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_ORTHOGONAL_SOFTNESS: {
			slider_params.angular_orthogonal_softness = (float)p_value;
			break;
		}
		// Angular orthogonal restitution
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_ORTHOGONAL_RESTITUTION: {
			slider_params.angular_orthogonal_restitution = (float)p_value;
			break;
		}
		// Angular orthogonal damping
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_ORTHOGONAL_DAMPING: {
			slider_params.angular_orthogonal_damping = (float)p_value;
			break;
		}
		default:
			break;
	}
}

real_t PhysXJoint3D::get_slider_param(PhysicsServer3D::SliderJointParam p_param) const {
	switch (p_param) {
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_UPPER:
			return (real_t)slider_params.linear_limit_upper;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_LOWER:
			return (real_t)slider_params.linear_limit_lower;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_SOFTNESS:
			return (real_t)slider_params.linear_limit_softness;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_RESTITUTION:
			return (real_t)slider_params.linear_limit_restitution;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_LIMIT_DAMPING:
			return (real_t)slider_params.linear_limit_damping;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_MOTION_SOFTNESS:
			return (real_t)slider_params.linear_motion_softness;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_MOTION_RESTITUTION:
			return (real_t)slider_params.linear_motion_restitution;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_MOTION_DAMPING:
			return (real_t)slider_params.linear_motion_damping;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_ORTHOGONAL_SOFTNESS:
			return (real_t)slider_params.linear_orthogonal_softness;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_ORTHOGONAL_RESTITUTION:
			return (real_t)slider_params.linear_orthogonal_restitution;
		case PhysicsServer3D::SLIDER_JOINT_LINEAR_ORTHOGONAL_DAMPING:
			return (real_t)slider_params.linear_orthogonal_damping;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_UPPER:
			return (real_t)slider_params.angular_limit_upper;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_LOWER:
			return (real_t)slider_params.angular_limit_lower;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_SOFTNESS:
			return (real_t)slider_params.angular_limit_softness;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_RESTITUTION:
			return (real_t)slider_params.angular_limit_restitution;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_LIMIT_DAMPING:
			return (real_t)slider_params.angular_limit_damping;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_MOTION_SOFTNESS:
			return (real_t)slider_params.angular_motion_softness;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_MOTION_RESTITUTION:
			return (real_t)slider_params.angular_motion_restitution;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_MOTION_DAMPING:
			return (real_t)slider_params.angular_motion_damping;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_ORTHOGONAL_SOFTNESS:
			return (real_t)slider_params.angular_orthogonal_softness;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_ORTHOGONAL_RESTITUTION:
			return (real_t)slider_params.angular_orthogonal_restitution;
		case PhysicsServer3D::SLIDER_JOINT_ANGULAR_ORTHOGONAL_DAMPING:
			return (real_t)slider_params.angular_orthogonal_damping;
		default:
			return 0.0f;
	}
}

// ---------------------------------------------------------------------------
// Cone/twist joint — params
// ---------------------------------------------------------------------------

// Rebuilds and applies both the swing (cone) and twist (angular pair) limits
// from cone_twist_params. Centralizes the limit construction that was
// previously duplicated across 5 param cases.
void PhysXJoint3D::_apply_cone_twist_limits() {
	if (!px_joint) {
		return;
	}
	physx::PxD6Joint *d6 = static_cast<physx::PxD6Joint *>(px_joint);

	// PhysX validates cone/pair limits strictly (angles must lie in (0, PI],
	// pair min < pair max). Godot allows degenerate values (a 0 span is a
	// locked joint; the stock 0/0 g6dof ranges), so clamp into the valid range
	// — a hair-thin limit reads as locked, which is the Godot semantic.
	const real_t swing = CLAMP(cone_twist_params.swing_span, 1.0e-4, (real_t)Math::PI);
	physx::PxJointLimitCone swing_limit(swing, swing);
	swing_limit.stiffness = cone_twist_params.softness;
	swing_limit.damping = cone_twist_params.relaxation;
	swing_limit.restitution = cone_twist_params.bias;
	d6->setSwingLimit(swing_limit);

	const real_t twist = CLAMP(cone_twist_params.twist_span, 1.0e-4, (real_t)Math::PI);
	physx::PxJointAngularLimitPair twist_limit(-twist, twist);
	twist_limit.stiffness = cone_twist_params.softness;
	twist_limit.damping = cone_twist_params.relaxation;
	twist_limit.bounceThreshold = cone_twist_params.bias;
	d6->setTwistLimit(twist_limit);
}

void PhysXJoint3D::set_cone_twist_param(PhysicsServer3D::ConeTwistJointParam p_param, real_t p_value) {
	switch (p_param) {
		case PhysicsServer3D::CONE_TWIST_JOINT_SWING_SPAN:
			cone_twist_params.swing_span = (float)p_value;
			cone_limits_set = true;
			_apply_cone_twist_limits();
			break;
		case PhysicsServer3D::CONE_TWIST_JOINT_TWIST_SPAN:
			cone_twist_params.twist_span = (float)p_value;
			cone_limits_set = true;
			_apply_cone_twist_limits();
			break;
		case PhysicsServer3D::CONE_TWIST_JOINT_BIAS:
			cone_twist_params.bias = (float)p_value;
			cone_limits_set = true;
			_apply_cone_twist_limits();
			break;
		case PhysicsServer3D::CONE_TWIST_JOINT_SOFTNESS:
			cone_twist_params.softness = (float)p_value;
			cone_limits_set = true;
			_apply_cone_twist_limits();
			break;
		case PhysicsServer3D::CONE_TWIST_JOINT_RELAXATION:
			cone_twist_params.relaxation = (float)p_value;
			cone_limits_set = true;
			_apply_cone_twist_limits();
			break;
		default:
			break;
	}
}

real_t PhysXJoint3D::get_cone_twist_param(PhysicsServer3D::ConeTwistJointParam p_param) const {
	switch (p_param) {
		case PhysicsServer3D::CONE_TWIST_JOINT_SWING_SPAN:
			return (real_t)cone_twist_params.swing_span;
		case PhysicsServer3D::CONE_TWIST_JOINT_TWIST_SPAN:
			return (real_t)cone_twist_params.twist_span;
		case PhysicsServer3D::CONE_TWIST_JOINT_BIAS:
			return (real_t)cone_twist_params.bias;
		case PhysicsServer3D::CONE_TWIST_JOINT_SOFTNESS:
			return (real_t)cone_twist_params.softness;
		case PhysicsServer3D::CONE_TWIST_JOINT_RELAXATION:
			return (real_t)cone_twist_params.relaxation;
		default:
			return 0.0f;
	}
}

// ---------------------------------------------------------------------------
// 6DOF joint — params and flags (per-axis)
// ---------------------------------------------------------------------------

// Applies the per-axis linear limit from the cached params. godot_physics
// gates its limit constraint on isLimited (upper >= lower — an equal range is
// a valid, effectively-locked pair) and solveLinearAxis handles the rest; a
// STRICTLY REVERSED range (upper < lower) is unconstrained — the solve is
// never called (godot_generic_6dof_joint_3d solveLinearAxis/solve
// isLimited). Mirroring that keeps the pair PhysX-valid too:
// PxJointLinearLimitPair requires upper >= lower.
void PhysXJoint3D::_apply_g6dof_linear_limit(physx::PxD6Joint *p_d6, Vector3::Axis p_axis) {
	if (!p_d6) {
		return;
	}
	const G6DOFJointAxisParams &params = g6dof_params[p_axis];
	const physx::PxD6Axis::Enum lin_axis = _px_linear_axis(p_axis);

	if (!g6dof_flags[p_axis].linear_limit || params.linear_lower_limit > params.linear_upper_limit) {
		p_d6->setMotion(lin_axis, physx::PxD6Motion::eFREE);
		return;
	}

	if (params.linear_lower_limit == params.linear_upper_limit) {
		// Godot's built-in treats an EQUAL range as effectively locked (the
		// solve clamps to [lower, upper] == a single point — the node's stock
		// 0/0 defaults mean "rigid linear connection"). PhysX's zero-extent
		// LIMITED pair does not actually hold under load (a 6-link chain with
		// 0/0 linear links pulled apart), so map it to a true lock. A spring
		// on the same axis is freed again by _apply_g6dof_axis_drive.
		p_d6->setMotion(lin_axis, physx::PxD6Motion::eLOCKED);
		return;
	}

	physx::PxJointLinearLimitPair limit(physx::PxTolerancesScale(), params.linear_lower_limit, params.linear_upper_limit);
	limit.stiffness = params.linear_limit_softness;
	limit.damping = params.linear_damping;
	limit.restitution = params.linear_restitution;
	p_d6->setLinearLimit(lin_axis, limit);
	p_d6->setMotion(lin_axis, physx::PxD6Motion::eLIMITED);
}

// Applies the per-axis angular limit to the correct PhysX slot:
//   AXIS_X (twist)  -> setTwistLimit (PxJointAngularLimitPair, lower/upper)
//   AXIS_Y (swing1) } setPyramidSwingLimit (per-axis spans preserved)
//   AXIS_Z (swing2) }
// The pyramid limit keeps each swing axis's own span; an axis whose limit
// flag is off contributes the near-full range and stays effectively free.
void PhysXJoint3D::_apply_g6dof_angular_limit(physx::PxD6Joint *p_d6, Vector3::Axis p_axis) {
	if (!p_d6) {
		return;
	}
	const G6DOFJointAxisParams &params = g6dof_params[p_axis];

	if (p_axis == Vector3::AXIS_X) {
		// Twist: angular limit pair. PhysX requires lower < upper; a degenerate
		// Godot range (the stock 0/0 defaults on an axis whose limit is off) is
		// nudged open so setTwistLimit stays valid. Whether the axis actually
		// constrains is governed separately by setMotion.
		float lo = (float)params.angular_lower_limit;
		float hi = (float)params.angular_upper_limit;
		if (hi <= lo) {
			hi = lo + 1.0e-4f;
		}
		// Godot's 6DOF angles run the other way round to PhysX's D6 (as for the
		// hinge, and as Jolt maps them): [lower, upper] is [-upper, -lower].
		// Passed straight through, a ragdoll's asymmetric limits (knees, elbows)
		// bent the wrong way. The twist sits inside (-2pi, 2pi): a limit at
		// +-180 deg was silently dropped.
		const float twist_max = (float)(Math::TAU - 0.001);
		physx::PxJointAngularLimitPair limit(
				CLAMP(-hi, -twist_max, twist_max),
				CLAMP(-lo, -twist_max, twist_max));
		limit.stiffness = params.angular_limit_softness;
		limit.damping = params.angular_damping;
		limit.restitution = params.angular_restitution;
		p_d6->setTwistLimit(limit);
	} else {
		// Swing1 (Y) / Swing2 (Z): a per-axis pyramid limit preserves each
		// axis's own span (a shared cone collapsed them into one symmetric
		// extent — the MIN of the two lowers). An axis whose limit flag is
		// OFF gets the near-full pyramid range so it stays effectively free
		// while the other axis constrains alone; a limited axis uses its own
		// cached span, nudged into PhysX's valid window (min/max strictly
		// inside (-PI, PI), max >= min — a degenerate Godot range reads as
		// locked, which is the Godot semantic).
		auto axis_span = [&](Vector3::Axis axis) -> physx::PxVec2 {
			if (!g6dof_flags[axis].angular_limit) {
				return physx::PxVec2(-physx::PxPi + 1.0e-4f, physx::PxPi - 1.0e-4f);
			}
			const G6DOFJointAxisParams &ap = g6dof_params[axis];
			// The other way round too (see the twist): Godot's [lower, upper]
			// is [-upper, -lower] on the D6, clamped inside (-PI, PI).
			float lo = CLAMP(-(float)ap.angular_upper_limit, -physx::PxPi + 1.0e-4f, physx::PxPi - 1.0e-4f);
			float hi = CLAMP(-(float)ap.angular_lower_limit, -physx::PxPi + 1.0e-4f, physx::PxPi - 1.0e-4f);
			if (hi < lo) {
				hi = lo;
			}
			return physx::PxVec2(lo, hi);
		};
		const physx::PxVec2 y_span = axis_span(Vector3::AXIS_Y);
		const physx::PxVec2 z_span = axis_span(Vector3::AXIS_Z);
		physx::PxJointLimitPyramid pyramid(y_span.x, y_span.y, z_span.x, z_span.y);
		pyramid.stiffness = params.angular_limit_softness;
		pyramid.damping = params.angular_damping;
		pyramid.restitution = params.angular_restitution;
		p_d6->setPyramidSwingLimit(pyramid);
	}
}

// Resolves the axis's single PxD6Drive slot: a Godot 6DOF spring and motor
// share one PhysX drive per axis, and the node pushes its configuration in a
// fixed order (all params, then all flags — with ENABLE_LINEAR_MOTOR last),
// so "last property push wins" let a DISABLED motor's zero-force-limit
// velocity servo clobber an installed spring (stiffness 0 + forceLimit 0 =
// a drive that can never exert force). The winner is resolved from the
// cached flags: spring > motor > no drive. A drive also only acts on an
// axis whose motion allows it: a spring on an axis limited to a degenerate
// range (the node defaults push ENABLE_*_LIMIT with a 0/0 range) gets that
// axis freed, while a springless axis keeps Godot's "0/0 = rigid" semantic.
void PhysXJoint3D::_apply_g6dof_axis_drive(physx::PxD6Joint *p_d6, Vector3::Axis p_axis, bool p_angular) {
	if (!p_d6) {
		return;
	}
	const G6DOFJointAxisFlags &f = g6dof_flags[p_axis];
	const G6DOFJointAxisParams &params = g6dof_params[p_axis];
	const bool spring = p_angular ? f.angular_spring : f.linear_spring;
	const bool motor = p_angular ? f.angular_motor : f.linear_motor;
	const bool limited = p_angular ? f.angular_limit : f.linear_limit;
	const real_t lower = p_angular ? params.angular_lower_limit : params.linear_lower_limit;
	const real_t upper = p_angular ? params.angular_upper_limit : params.linear_upper_limit;
	const physx::PxD6Axis::Enum axis = p_angular ? _px_angular_axis(p_axis) : _px_linear_axis(p_axis);

	physx::PxD6JointDrive drive;
	if (spring) {
		drive.stiffness = p_angular ? params.angular_spring_stiffness : params.linear_spring_stiffness;
		drive.damping = p_angular ? params.angular_spring_damping : params.linear_spring_damping;
		// Springs are not clamped by the motor force limit (Godot applies
		// spring force unlimited; the motor keeps its own limit).
		drive.forceLimit = PX_MAX_F32;
	} else if (motor) {
		drive.stiffness = 0.0f;
		drive.damping = G6DOF_MOTOR_DAMPING;
		drive.forceLimit = p_angular ? params.angular_motor_force_limit : params.linear_motor_force_limit;
	} else {
		// Neither: no drive on this axis.
		drive.stiffness = 0.0f;
		drive.damping = 0.0f;
		drive.forceLimit = 0.0f;
	}
	p_d6->setDrive(static_cast<physx::PxD6Drive::Enum>(axis), drive);

	if (spring && !(limited && upper > lower)) {
		p_d6->setMotion(axis, physx::PxD6Motion::eFREE);
	}
}

void PhysXJoint3D::set_g6dof_param(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param, real_t p_value) {
	G6DOFJointAxisParams &params = g6dof_params[p_axis];
	// Store first: the cache is the source of truth and must stay complete
	// even while the joint is dormant (no PxJoint yet).
	_store_g6dof_param(params, p_param, p_value);

	physx::PxD6Joint *d6 = px_joint ? static_cast<physx::PxD6Joint *>(px_joint) : nullptr;
	if (!d6) {
		return;
	}
	physx::PxD6Axis::Enum ang_axis = _px_angular_axis(p_axis);

	switch (p_param) {
		// Linear limits
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_LOWER_LIMIT: {
			g6dof_flags[p_axis].linear_limit = true;
			_apply_g6dof_linear_limit(d6, p_axis);
			break;
		}
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_UPPER_LIMIT: {
			g6dof_flags[p_axis].linear_limit = true;
			_apply_g6dof_linear_limit(d6, p_axis);
			break;
		}
		// Linear limit softness
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS: {
			_apply_g6dof_linear_limit(d6, p_axis);
			break;
		}
		// Linear restitution
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_RESTITUTION: {
			_apply_g6dof_linear_limit(d6, p_axis);
			break;
		}
		// Linear damping
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_DAMPING: {
			_apply_g6dof_linear_limit(d6, p_axis);
			break;
		}
		// Linear motor target velocity — accumulate into cached drive velocity
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_MOTOR_TARGET_VELOCITY: {
			cached_g6dof_lin_drive_vel[p_axis] = params.linear_motor_target_velocity;
			if (!g6dof_flags[p_axis].linear_spring) {
				d6->setDriveVelocity(cached_g6dof_lin_drive_vel, cached_g6dof_ang_drive_vel, true);
			}
			g6dof_lin_drives[p_axis] = { true, 0.0f, G6DOF_MOTOR_DAMPING, params.linear_motor_force_limit };
			// Velocity servo: zero stiffness, large damping (see
			// G6DOF_MOTOR_DAMPING) — the Godot motor, force-limited. Resolved
			// through the slot winner: a configured spring is not clobbered.
			_apply_g6dof_axis_drive(d6, p_axis, false);
			break;
		}
		// Linear motor force limit
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_MOTOR_FORCE_LIMIT: {
			g6dof_lin_drives[p_axis] = { true, 0.0f, G6DOF_MOTOR_DAMPING, params.linear_motor_force_limit };
			_apply_g6dof_axis_drive(d6, p_axis, false);
			break;
		}
		// Linear spring stiffness
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_STIFFNESS: {
			g6dof_lin_drives[p_axis] = { true, params.linear_spring_stiffness, params.linear_spring_damping, PX_MAX_F32 };
			_apply_g6dof_axis_drive(d6, p_axis, false);
			break;
		}
		// Linear spring damping
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_DAMPING: {
			g6dof_lin_drives[p_axis] = { true, params.linear_spring_stiffness, params.linear_spring_damping, PX_MAX_F32 };
			_apply_g6dof_axis_drive(d6, p_axis, false);
			break;
		}
		// Linear spring equilibrium point
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT: {
			_apply_g6dof_drive_position();
			break;
		}
		// Angular limits
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_LOWER_LIMIT: {
			g6dof_flags[p_axis].angular_limit = true;
			_apply_g6dof_angular_limit(d6, p_axis);
			d6->setMotion(ang_axis, physx::PxD6Motion::eLIMITED);
			break;
		}
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_UPPER_LIMIT: {
			// Mirror the lower-limit case: setting a limit bound marks the axis
			// limited so the constraint is live now AND survives a wrapper
			// rebuild (the cache is the source of truth for _apply_params).
			// Godot's own Generic6DOFJoint3D pushes FLAG_ENABLE_ANGULAR_LIMIT
			// true by default (generic_6dof_joint_3d.cpp _create_joint), so
			// node-driven joints are unaffected either way.
			g6dof_flags[p_axis].angular_limit = true;
			_apply_g6dof_angular_limit(d6, p_axis);
			d6->setMotion(ang_axis, physx::PxD6Motion::eLIMITED);
			break;
		}
		// Angular limit softness
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS: {
			_apply_g6dof_angular_limit(d6, p_axis);
			break;
		}
		// Angular restitution
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_RESTITUTION: {
			_apply_g6dof_angular_limit(d6, p_axis);
			break;
		}
		// Angular damping
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_DAMPING: {
			_apply_g6dof_angular_limit(d6, p_axis);
			break;
		}
		// Angular force limit (max torque for motor/limit correction)
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_FORCE_LIMIT: {
			// PxD6Joint doesn't have a direct max torque parameter for limits.
			// This is stored for round-trip but has no effect on the solver.
			break;
		}
		// Angular ERP (error reduction parameter for positional drift)
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_ERP: {
			// PxD6Joint doesn't have an ERP parameter — stored for round-trip.
			break;
		}
		// Angular motor target velocity — accumulate into cached drive velocity
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_MOTOR_TARGET_VELOCITY: {
			// The other way round too (see _apply_g6dof_angular_limit): the D6
			// measures the axis opposite to Godot.
			cached_g6dof_ang_drive_vel[p_axis] = -params.angular_motor_target_velocity;
			if (!g6dof_flags[p_axis].angular_spring) {
				d6->setDriveVelocity(cached_g6dof_lin_drive_vel, cached_g6dof_ang_drive_vel, true);
			}
			g6dof_ang_drives[p_axis] = { true, 0.0f, G6DOF_MOTOR_DAMPING, params.angular_motor_force_limit };
			// Velocity servo (see G6DOF_MOTOR_DAMPING). Resolved through the
			// slot winner: a configured spring is not clobbered.
			_apply_g6dof_axis_drive(d6, p_axis, true);
			break;
		}
		// Angular motor force limit
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_MOTOR_FORCE_LIMIT: {
			g6dof_ang_drives[p_axis] = { true, 0.0f, G6DOF_MOTOR_DAMPING, params.angular_motor_force_limit };
			_apply_g6dof_axis_drive(d6, p_axis, true);
			break;
		}
		// Angular spring stiffness
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS: {
			g6dof_ang_drives[p_axis] = { true, params.angular_spring_stiffness, params.angular_spring_damping, PX_MAX_F32 };
			_apply_g6dof_axis_drive(d6, p_axis, true);
			break;
		}
		// Angular spring damping
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_DAMPING: {
			g6dof_ang_drives[p_axis] = { true, params.angular_spring_stiffness, params.angular_spring_damping, PX_MAX_F32 };
			_apply_g6dof_axis_drive(d6, p_axis, true);
			break;
		}
		// Angular spring equilibrium point
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT: {
			_apply_g6dof_drive_position();
			break;
		}
		default:
			break;
	}
}

real_t PhysXJoint3D::get_g6dof_param(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param) const {
	const G6DOFJointAxisParams &params = g6dof_params[p_axis];

	switch (p_param) {
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_LOWER_LIMIT:
			return (real_t)params.linear_lower_limit;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_UPPER_LIMIT:
			return (real_t)params.linear_upper_limit;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_LIMIT_SOFTNESS:
			return (real_t)params.linear_limit_softness;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_RESTITUTION:
			return (real_t)params.linear_restitution;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_DAMPING:
			return (real_t)params.linear_damping;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_MOTOR_TARGET_VELOCITY:
			return (real_t)params.linear_motor_target_velocity;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_MOTOR_FORCE_LIMIT:
			return (real_t)params.linear_motor_force_limit;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_STIFFNESS:
			return (real_t)params.linear_spring_stiffness;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_DAMPING:
			return (real_t)params.linear_spring_damping;
		case PhysicsServer3D::G6DOF_JOINT_LINEAR_SPRING_EQUILIBRIUM_POINT:
			return (real_t)params.linear_spring_equilibrium_point;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_LOWER_LIMIT:
			return (real_t)params.angular_lower_limit;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_UPPER_LIMIT:
			return (real_t)params.angular_upper_limit;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_LIMIT_SOFTNESS:
			return (real_t)params.angular_limit_softness;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_RESTITUTION:
			return (real_t)params.angular_restitution;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_DAMPING:
			return (real_t)params.angular_damping;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_FORCE_LIMIT:
			return (real_t)params.angular_force_limit;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_ERP:
			return (real_t)params.angular_erp;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_MOTOR_TARGET_VELOCITY:
			return (real_t)params.angular_motor_target_velocity;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_MOTOR_FORCE_LIMIT:
			return (real_t)params.angular_motor_force_limit;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_STIFFNESS:
			return (real_t)params.angular_spring_stiffness;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_DAMPING:
			return (real_t)params.angular_spring_damping;
		case PhysicsServer3D::G6DOF_JOINT_ANGULAR_SPRING_EQUILIBRIUM_POINT:
			return (real_t)params.angular_spring_equilibrium_point;
		default:
			return 0.0f;
	}
}

void PhysXJoint3D::set_g6dof_flag(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag, bool p_enable) {
	G6DOFJointAxisFlags &flags = g6dof_flags[p_axis];
	const G6DOFJointAxisParams &params = g6dof_params[p_axis];

	physx::PxD6Joint *d6 = px_joint ? static_cast<physx::PxD6Joint *>(px_joint) : nullptr;
	physx::PxD6Axis::Enum ang_axis = _px_angular_axis(p_axis);

	switch (p_flag) {
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT: {
			flags.linear_limit = p_enable;
			// Godot semantics: the flag alone governs the axis — enabled means
			// limited (while the range is valid, see _apply_g6dof_linear_limit),
			// disabled means FREE (not locked).
			if (d6) {
				_apply_g6dof_linear_limit(d6, p_axis);
				_apply_g6dof_axis_drive(d6, p_axis, false); // re-free spring axes
			}
			break;
		}
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT: {
			flags.angular_limit = p_enable;
			if (d6) {
				d6->setMotion(ang_axis, p_enable ? physx::PxD6Motion::eLIMITED : physx::PxD6Motion::eFREE);
				_apply_g6dof_axis_drive(d6, p_axis, true); // re-free spring axes
			}
			break;
		}
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING: {
			flags.angular_spring = p_enable;
			g6dof_ang_drives[p_axis] = { p_enable, params.angular_spring_stiffness, params.angular_spring_damping, PX_MAX_F32 };
			if (d6) {
				_apply_g6dof_axis_drive(d6, p_axis, true);
				_apply_g6dof_drive_position();
			}
			break;
		}
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING: {
			flags.linear_spring = p_enable;
			g6dof_lin_drives[p_axis] = { p_enable, params.linear_spring_stiffness, params.linear_spring_damping, PX_MAX_F32 };
			if (d6) {
				_apply_g6dof_axis_drive(d6, p_axis, false);
				_apply_g6dof_drive_position();
			}
			break;
		}
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_MOTOR: {
			flags.angular_motor = p_enable;
			// Update cached angular drive velocity for this axis
			// (negated: the D6 measures the axis opposite to Godot, see
			// _apply_g6dof_angular_limit).
			cached_g6dof_ang_drive_vel[p_axis] = -params.angular_motor_target_velocity;
			g6dof_ang_drives[p_axis] = { p_enable, 0.0f, G6DOF_MOTOR_DAMPING, params.angular_motor_force_limit };
			if (d6) {
				// The motor velocity only applies when the motor owns the
				// axis's drive slot (no spring configured -- see
				// _apply_g6dof_axis_drive).
				if (!flags.angular_spring) {
					d6->setDriveVelocity(cached_g6dof_lin_drive_vel, cached_g6dof_ang_drive_vel, true);
				}
				_apply_g6dof_axis_drive(d6, p_axis, true);
			}
			break;
		}
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_MOTOR: {
			flags.linear_motor = p_enable;
			// Update cached linear drive velocity for this axis
			cached_g6dof_lin_drive_vel[p_axis] = params.linear_motor_target_velocity;
			g6dof_lin_drives[p_axis] = { p_enable, 0.0f, G6DOF_MOTOR_DAMPING, params.linear_motor_force_limit };
			if (d6) {
				// The motor velocity only applies when the motor owns the
				// axis's drive slot (no spring configured -- see
				// _apply_g6dof_axis_drive).
				if (!flags.linear_spring) {
					d6->setDriveVelocity(cached_g6dof_lin_drive_vel, cached_g6dof_ang_drive_vel, true);
				}
				_apply_g6dof_axis_drive(d6, p_axis, false);
			}
			break;
		}
		default:
			break;
	}
}

bool PhysXJoint3D::get_g6dof_flag(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag) const {
	// Round-trip what was SET: the stored flag, not a value derived from the
	// params (Godot's node keeps its own flag state; e.g. a motor enabled with
	// a zero target velocity must still read back enabled).
	const G6DOFJointAxisFlags &flags = g6dof_flags[p_axis];

	switch (p_flag) {
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_LIMIT:
			return flags.linear_limit;
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_LIMIT:
			return flags.angular_limit;
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_ANGULAR_SPRING:
			return flags.angular_spring;
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_SPRING:
			return flags.linear_spring;
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_MOTOR:
			return flags.angular_motor;
		case PhysicsServer3D::G6DOF_JOINT_FLAG_ENABLE_LINEAR_MOTOR:
			return flags.linear_motor;
		default:
			return false;
	}
}

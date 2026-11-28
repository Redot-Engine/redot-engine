/**************************************************************************/
/*  physx_joint_3d.h                                                      */
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

#pragma once

/**
 * @file physx_joint_3d.h
 * @brief Joint wrapper — bridges Godot's PhysicsServer3D joint API to PhysX.
 *
 * PhysXJoint3D is a single class that wraps a PxJoint* (either PxRevoluteJoint,
 * PxPrismaticJoint, PxSphericalJoint, PxD6Joint, etc.) and dispatches parameter
 * access based on the Godot-side JointType. This avoids the boilerplate of
 * maintaining one class per joint type while keeping PhysX API usage idiomatic.
 *
 * Joint type mapping:
 *   JOINT_TYPE_PIN       → PxD6Joint (linear locked, angular free — pure ball joint)
 *   JOINT_TYPE_HINGE     → PxRevoluteJoint
 *   JOINT_TYPE_SLIDER    → PxPrismaticJoint
 *   JOINT_TYPE_CONE_TWIST→ PxD6Joint (swing/twist configured)
 *   JOINT_TYPE_6DOF      → PxD6Joint (fully configurable)
 *
 * Lifecycle model: the wrapper references the two Godot-side bodies
 * (PhysXBody3D, not raw actors) and keeps the COMPLETE Godot-side joint
 * configuration cached. Every setter stores into that cache first, then pushes
 * to the live PxJoint if one exists — so parameters set while the joint is
 * detached (body freed, static↔dynamic actor recreation, transient single-body
 * configuration) are preserved and re-applied verbatim when the PxJoint can be
 * recreated (bodies notify their joints on actor recreation / scene entry).
 * The wrapper's RID therefore stays valid for its whole lifetime regardless of
 * what happens to the connected bodies.
 *
 * Godot-specific parameters that have no PhysX equivalent:
 *   - PinJoint: PIN_JOINT_BIAS, PIN_JOINT_DAMPING, PIN_JOINT_IMPULSE_CLAMP
 *     Stored on the wrapper for round-trip get_param, but have no/negligible
 *     effect (PhysX solver stiffness is global, not per-joint).
 *   - HingeJoint: HINGE_JOINT_BIAS — no PhysX equivalent.
 *   - HingeJoint: HINGE_JOINT_LIMIT_BIAS — maps to PxJointLimitParameters::bounceThreshold.
 *   - HingeJoint: HINGE_JOINT_LIMIT_SOFTNESS — maps to PxJointLimitParameters::stiffness.
 *   - HingeJoint: HINGE_JOINT_LIMIT_RELAXATION — maps to PxJointLimitParameters::damping.
 *   - SliderJoint: linear/angular SLIDER_JOINT_* parameters map to
 *     PxJointLinearLimitPair/PxJointAngularLimitPair equivalents.
 *   - ConeTwistJoint: CONE_TWIST_JOINT_BIAS — maps to PxJointLimitCone::bounceThreshold.
 *   - ConeTwistJoint: CONE_TWIST_JOINT_SOFTNESS — maps to PxJointLimitCone::stiffness.
 *   - ConeTwistJoint: CONE_TWIST_JOINT_RELAXATION — maps to PxJointLimitCone::damping.
 */

#ifndef PHYSX_JOINT_3D_H
#define PHYSX_JOINT_3D_H

#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/templates/rid.h"
#include "core/variant/variant.h"
#include "servers/physics_3d/physics_server_3d.h"

#include "physx_rid_owner.h"

#include "extensions/PxD6Joint.h"
#include "extensions/PxJointLimit.h"
#include "extensions/PxPrismaticJoint.h"
#include "extensions/PxRevoluteJoint.h"
#include "extensions/PxSphericalJoint.h"
#include <PxPhysicsAPI.h>

class PhysXBody3D;

namespace physx {
class PxJoint;
class PxRigidActor;
class PxPhysics;
} //namespace physx

/**
 * @brief Godot-side metadata for a pin joint — stored on wrapper, no PhysX equivalent.
 */
struct PinJointParams {
	real_t bias = 0.0f;
	real_t damping = 0.0f;
	real_t impulse_clamp = 0.0f;
};

/**
 * @brief Godot-side metadata for a hinge joint — stored for round-trip access.
 */
struct HingeJointParams {
	real_t bias = 0.0f;
	real_t limit_upper = 0.0f;
	real_t limit_lower = 0.0f;
	real_t limit_bias = 0.0f;
	real_t limit_softness = 0.0f;
	real_t limit_relaxation = 0.0f;
	real_t motor_target_velocity = 0.0f;
	real_t motor_max_impulse = 0.0f;
	bool use_limit = false;
	bool enable_motor = false;
};

/**
 * @brief Godot-side metadata for a slider joint — stored for round-trip access.
 */
struct SliderJointParams {
	// Linear
	real_t linear_limit_upper = 0.0f;
	real_t linear_limit_lower = 0.0f;
	real_t linear_limit_softness = 0.0f;
	real_t linear_limit_restitution = 0.0f;
	real_t linear_limit_damping = 0.0f;
	real_t linear_motion_softness = 0.0f;
	real_t linear_motion_restitution = 0.0f;
	real_t linear_motion_damping = 0.0f;
	real_t linear_orthogonal_softness = 0.0f;
	real_t linear_orthogonal_restitution = 0.0f;
	real_t linear_orthogonal_damping = 0.0f;

	// Angular
	real_t angular_limit_upper = 0.0f;
	real_t angular_limit_lower = 0.0f;
	real_t angular_limit_softness = 0.0f;
	real_t angular_limit_restitution = 0.0f;
	real_t angular_limit_damping = 0.0f;
	real_t angular_motion_softness = 0.0f;
	real_t angular_motion_restitution = 0.0f;
	real_t angular_motion_damping = 0.0f;
	real_t angular_orthogonal_softness = 0.0f;
	real_t angular_orthogonal_restitution = 0.0f;
	real_t angular_orthogonal_damping = 0.0f;
};

/**
 * @brief Godot-side metadata for a cone/twist joint — stored for round-trip access.
 */
struct ConeTwistJointParams {
	real_t swing_span = 0.0f;
	real_t twist_span = 0.0f;
	real_t bias = 0.0f;
	real_t softness = 0.0f;
	real_t relaxation = 0.0f;
};

/**
 * @brief Godot-side metadata for a 6DOF joint — stored for round-trip access.
 */
struct G6DOFJointAxisParams {
	real_t linear_lower_limit = 0.0f;
	real_t linear_upper_limit = 0.0f;
	real_t linear_limit_softness = 0.0f;
	real_t linear_restitution = 0.0f;
	real_t linear_damping = 0.0f;
	real_t linear_motor_target_velocity = 0.0f;
	real_t linear_motor_force_limit = 0.0f;
	real_t linear_spring_stiffness = 0.0f;
	real_t linear_spring_damping = 0.0f;
	real_t linear_spring_equilibrium_point = 0.0f;

	real_t angular_lower_limit = 0.0f;
	real_t angular_upper_limit = 0.0f;
	real_t angular_limit_softness = 0.0f;
	real_t angular_restitution = 0.0f;
	real_t angular_damping = 0.0f;
	real_t angular_force_limit = 0.0f;
	real_t angular_erp = 0.0f;
	real_t angular_motor_target_velocity = 0.0f;
	real_t angular_motor_force_limit = 0.0f;
	real_t angular_spring_stiffness = 0.0f;
	real_t angular_spring_damping = 0.0f;
	real_t angular_spring_equilibrium_point = 0.0f;
};

/**
 * @brief Per-axis 6DOF enable flags (Godot's G6DOF_JOINT_FLAG_*).
 *
 * These mirror the D6 motion/drive state the flags produce, so a rebuild can
 * restore the exact axis configuration from cache alone.
 */
struct G6DOFJointAxisFlags {
	bool linear_limit = false;
	bool angular_limit = false;
	bool linear_motor = false;
	bool angular_motor = false;
	bool linear_spring = false;
	bool angular_spring = false;
};

/**
 * @brief Last-pushed per-axis D6 drive configuration, so a rebuild restores
 * the same drive (spring- or motor-style) the setters last applied.
 */
struct G6DOFDriveState {
	bool active = false;
	float stiffness = 0.0f;
	float damping = 0.0f;
	float force = 0.0f;
};

class PhysXJoint3D : public PhysXRIDOwner {
public:
	/// Identifies which PhysX joint subclass we wrap.
	enum JointKind {
		JOINT_KIND_NONE = -1,
		JOINT_KIND_PIN,
		JOINT_KIND_HINGE,
		JOINT_KIND_SLIDER,
		JOINT_KIND_CONE_TWIST,
		JOINT_KIND_6DOF,
	};

private:
	physx::PxJoint *px_joint = nullptr;
	// Survives body teardown / actor recreation (release() is what resets it).
	JointKind kind = JOINT_KIND_NONE;
	PhysXBody3D *body_a = nullptr;
	PhysXBody3D *body_b = nullptr;
	// Cached make_* frames — restored verbatim on rebuild. For hinge joints the
	// server composes the Godot Z→PhysX X axis rotation into these, so the
	// cached frames are always exactly what the PxJoint needs.
	physx::PxTransform frame_a = physx::PxTransform(physx::PxIdentity);
	physx::PxTransform frame_b = physx::PxTransform(physx::PxIdentity);

	// --- Godot-side parameter cache: the source of truth for every value the
	// setters receive; pushed to the live PxJoint by _apply_params(). ---
	PinJointParams pin_params;
	HingeJointParams hinge_params;
	SliderJointParams slider_params;
	// The slider's limit constraint is enabled the first time a linear limit
	// upper/lower is set (matching the setter behavior); tracked so a rebuild
	// doesn't enable limits on a fresh slider that never had them.
	bool slider_limit_enabled = false;
	ConeTwistJointParams cone_twist_params;
	// Fresh cone-twist D6 motions come LIMITED with wide SDK-default cones;
	// the clamped limits are only pushed once a parameter was actually set.
	bool cone_limits_set = false;
	G6DOFJointAxisParams g6dof_params[6];
	G6DOFJointAxisFlags g6dof_flags[6];
	G6DOFDriveState g6dof_lin_drives[6];
	G6DOFDriveState g6dof_ang_drives[6];
	// Cached 6DOF drive velocity — accumulated across all axes
	physx::PxVec3 cached_g6dof_lin_drive_vel{ 0.0f, 0.0f, 0.0f };
	physx::PxVec3 cached_g6dof_ang_drive_vel{ 0.0f, 0.0f, 0.0f };

	// true = Godot's Joint3D default (exclude_nodes_from_collision) and the
	// PhysX constraint default; _apply_params() pushes it on (re)creation.
	bool collisions_disabled = true;

	/// Solver priority, stored for round-trip access. PhysX 5.x exposes no
	/// per-constraint solver priority, so the value is not applied to the
	/// PxConstraint.
	int solver_priority = 0;

	// --- Private helpers (deduplicate limit/drive reconfiguration) ---
	/// Builds the slider linear limit pair from the cached params (softness /
	/// damping / restitution included); deduplicates the switch-case blocks.
	physx::PxJointLinearLimitPair _slider_limit() const;
	/// Rebuilds and applies the hinge angular limit from hinge_params.
	/// No-op if use_limit is false.
	void _apply_hinge_limit();
	/// Applies the per-axis G6DOF linear limit: pushes the cached range to
	/// setLinearLimit and marks the axis LIMITED, but only while the range is
	/// valid (lower < upper) — godot_physics gates its whole limit constraint
	/// on minLimit < maxLimit (solveLinearAxis), so a reversed or equal range
	/// leaves the axis unconstrained and the invalid pair never reaches PhysX.
	void _apply_g6dof_linear_limit(physx::PxD6Joint *p_d6, Vector3::Axis p_axis);
	/// Builds the D6 drive target from the cached per-axis spring equilibrium
	/// points and applies it via PxD6Joint::setDrivePosition: linear equilibria
	/// map to the drive translation, angular equilibria (radians about the
	/// joint X/Y/Z) to the drive rotation as an Euler-derived quaternion that
	/// PhysX decomposes into twist/swing.
	void _apply_g6dof_drive_position();
	/// Resolves the axis's single PxD6Drive slot: a Godot 6DOF spring and
	/// motor share one PhysX drive per axis, and the node pushes its
	/// configuration in a fixed order (params, then flags — with
	/// ENABLE_LINEAR_MOTOR last), so "last push wins" let a DISABLED motor's
	/// zero-force-limit velocity servo clobber an installed spring. Resolves
	/// the winner from the cached flags (spring > motor > no drive) and frees
	/// the axis motion when a spring sits on a degenerate limit range (a
	/// drive can only act on an axis that is not locked).
	void _apply_g6dof_axis_drive(physx::PxD6Joint *p_d6, Vector3::Axis p_axis, bool p_angular);
	/// Rebuilds and applies the cone-twist swing + twist limits from cone_twist_params.
	void _apply_cone_twist_limits();
	/// Applies the g6dof angular limit to the correct PhysX limit slot
	/// (twist -> setTwistLimit, swing -> setPyramidSwingLimit) for the given axis.
	void _apply_g6dof_angular_limit(physx::PxD6Joint *p_d6, Vector3::Axis p_axis);
	/// Godot axis (X/Y/Z) -> D6 linear axis (eX/eY/eZ).
	static physx::PxD6Axis::Enum _px_linear_axis(Vector3::Axis p_axis);
	/// Godot axis (X/Y/Z) -> D6 angular axis (eTWIST/eSWING1/eSWING2).
	static physx::PxD6Axis::Enum _px_angular_axis(Vector3::Axis p_axis);
	/// Pushes the full cached configuration into the live PxJoint (no-op
	/// without one). Called after creation/rebuild.
	void _apply_params();
	/// Zeroes every per-kind parameter cache (params, flags, drive states,
	/// cached drive velocities). Called by make(): a fresh configuration must
	/// not resurrect parameters from the joint that previously occupied this
	/// RID — matching godot_physics, where joint_make_* replaces the joint
	/// object with fresh defaults.
	void _reset_param_caches();
	/// Releases the live PxJoint (fetch-safe via the bodies' spaces, waking the
	/// connected dynamics first). Cache and body links are untouched.
	void _destroy_px_joint();
	/// Creates the PxJoint from the cached kind/body/frame configuration.
	/// Returns null (quietly — the joint stays dormant) when the current
	/// configuration can't drive a PhysX constraint yet: no bodies, no dynamic
	/// actor (the transient static-only state while a Joint3D node assigns its
	/// node paths one at a time).
	physx::PxJoint *_create();

public:
	PhysXJoint3D() = default;
	~PhysXJoint3D();

	// --- Factory ---
	static physx::PxJoint *create_px_joint(physx::PxPhysics &p_physics,
			PhysicsServer3D::JointType p_type,
			physx::PxRigidActor *p_body_a,
			const physx::PxTransform &p_local_a,
			physx::PxRigidActor *p_body_b,
			const physx::PxTransform &p_local_b);

	// --- Configuration entry (server: joint_make_*) ---
	/// (Re)configures this RID for a type/body pair and tries to create the
	/// PxJoint. Every per-kind parameter cache is RESET first (a fresh
	/// configuration must not inherit state from the joint that previously
	/// held this RID — matching godot_physics's joint_make_* replacement
	/// semantics); configure via the set_* setters after make(). A
	/// configuration without a dynamic actor stays dormant until a body
	/// notifies (rebuild) or the next make() call.
	void make(PhysicsServer3D::JointType p_type, JointKind p_kind,
			PhysXBody3D *p_body_a, const physx::PxTransform &p_local_a,
			PhysXBody3D *p_body_b, const physx::PxTransform &p_local_b);
	/// Recreates the PxJoint from the cached configuration when possible.
	/// Idempotent: no-op while a PxJoint is live. Called by the connected
	/// bodies on actor recreation and scene entry.
	void rebuild();
	/// The given body is being deleted: drop the PxJoint, unlink that body and
	/// keep the wrapper (RID + cached params) fully valid. The joint stays
	/// dormant until a future make() call reconfigures it.
	void body_removed(PhysXBody3D *p_body);
	/// The given body's PxActor is about to be destroyed and re-created (the
	/// static<->dynamic flip in set_mode): the PxJoint references the old
	/// actor, so the constraint must go with it. Unlike body_removed the body
	/// links stay, and the joint rebuilds against the fresh actor when the
	/// body re-enters its scene (PhysXBody3D::_add_to_scene).
	void actor_invalidated(PhysXBody3D *p_body);
	// --- Release ---
	/// Full teardown: PxJoint, body links and kind. Cached parameters are
	/// reset by reconfiguration (make), matching Godot's joint_clear semantics.
	void release();

	// --- Identity ---
	JointKind get_kind() const { return kind; }
	physx::PxJoint *get_px_joint() const { return px_joint; }
	PhysXBody3D *get_body_a() const { return body_a; }
	PhysXBody3D *get_body_b() const { return body_b; }

	// --- 6DOF drive velocity caching (per-axis motors must not overwrite each other) ---
	void cache_g6dof_drive_velocity(const physx::PxVec3 &p_lin, const physx::PxVec3 &p_ang);
	void apply_cached_g6dof_drive_velocity(physx::PxD6Joint *p_d6);

	/// True while a joint motor (enabled, non-zero target) actively drives this
	/// constraint: PhysX drives do not wake a sleeping body, so the body
	/// pre-step keeps the connected dynamics awake while this reports true.
	bool has_running_motor() const;

	// --- Static helpers for joint creation (used by physx_server.cpp) ---
	static physx::PxTransform compute_joint_frame(const physx::PxVec3 &p_pivot, const physx::PxVec3 &p_axis);
	static physx::PxTransform to_physx_transform(const Transform3D &p_transform);
	static physx::PxVec3 to_physx_vec3(const Vector3 &p_vec);
	static Vector3 to_godot_vec3(const physx::PxVec3 &p_vec);

	// --- Solver priority (stored; PhysX has no per-constraint priority) ---
	void set_solver_priority(int p_priority);
	int get_solver_priority() const;

	// --- Collision between bodies ---
	void set_disable_collisions(bool p_disable);
	bool is_disabled_collisions() const;

	// --- Pin joint params ---
	void set_pin_param(PhysicsServer3D::PinJointParam p_param, real_t p_value);
	real_t get_pin_param(PhysicsServer3D::PinJointParam p_param) const;
	void set_local_a(const Vector3 &p_local_a);
	Vector3 get_local_a() const;
	void set_local_b(const Vector3 &p_local_b);
	Vector3 get_local_b() const;

	// --- Hinge joint params ---
	void set_hinge_param(PhysicsServer3D::HingeJointParam p_param, real_t p_value);
	real_t get_hinge_param(PhysicsServer3D::HingeJointParam p_param) const;
	void set_hinge_flag(PhysicsServer3D::HingeJointFlag p_flag, bool p_enabled);
	bool get_hinge_flag(PhysicsServer3D::HingeJointFlag p_flag) const;

	// --- Slider joint params ---
	void set_slider_param(PhysicsServer3D::SliderJointParam p_param, real_t p_value);
	real_t get_slider_param(PhysicsServer3D::SliderJointParam p_param) const;

	// --- Cone/twist joint params ---
	void set_cone_twist_param(PhysicsServer3D::ConeTwistJointParam p_param, real_t p_value);
	real_t get_cone_twist_param(PhysicsServer3D::ConeTwistJointParam p_param) const;

	// --- 6DOF joint params ---
	void set_g6dof_param(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param, real_t p_value);
	real_t get_g6dof_param(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param) const;
	void set_g6dof_flag(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag, bool p_enable);
	bool get_g6dof_flag(Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag) const;
};

#endif // PHYSX_JOINT_3D_H

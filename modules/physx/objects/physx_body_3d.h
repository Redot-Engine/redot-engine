/**************************************************************************/
/*  physx_body_3d.h                                                       */
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
 * @file physx_body_3d.h
 * @brief Rigid body wrapper — the core physics object.
 *
 * PhysXBody3D owns a PxRigidActor (either PxRigidStatic or PxRigidDynamic,
 * depending on BodyMode) and manages its full lifecycle: creation, mode
 * switching, state (transform/velocity/sleep), parameters (mass/friction/
 * damping/gravity), forces/impulses, CCD, axis locks, collision exceptions,
 * contact reporting, and per-step hooks.
 *
 * The body also owns a PhysXActorUserData that is attached to the PxActor's
 * userData field, bridging PhysX query/callback results back to Godot.
 */

#ifndef PHYSX_BODY_3D_H
#define PHYSX_BODY_3D_H
#include "../shapes/physx_user_data.h"
#include "physx_shaped_object_3d.h"
#include "servers/physics_3d/physics_server_3d.h"

#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"

namespace physx {
class PxRigidActor;
}

class PhysXArea3D;
class PhysXJoint3D;
class PhysXVehicleServer;
class PhysXSpace3D;
class PhysXDirectBodyState3D;

/**
 * @brief One contact point recorded against this body during the last step.
 *
 * Populated by PhysXSimulationEventCallback::onContact and read by
 * PhysXDirectBodyState3D's contact getters (get_contact_local_position, etc.).
 */
struct PhysXBodyContact {
	Vector3 local_position; ///< World-space contact point on the local body side.
	Vector3 local_normal; ///< Contact normal pointing toward the local body.
	Vector3 impulse; ///< Solver impulse applied along the normal.
	int local_shape = 0; ///< Shape index of the local body.
	RID collider; ///< RID of the other body.
	ObjectID collider_id; ///< ObjectID of the other body's Godot node.
	int collider_shape = 0; ///< Shape index of the other body.
	Vector3 collider_velocity; ///< Velocity of the collider at the contact point.
	Vector3 local_velocity; ///< Velocity of the local body at the contact point.
	Vector3 collider_position; ///< World-space contact point on the collider side.
	real_t depth = 0.0f; ///< Penetration depth at this contact (Godot's replacement policy ranks by it).
};

class PhysXBody3D : public PhysXShapedObject3D {
public:
	PhysXBody3D();
	virtual ~PhysXBody3D();

	// --- Space Management ---
	virtual void set_space(PhysXSpace3D *p_space) override;

	// --- Body Mode ---
	PhysicsServer3D::BodyMode get_mode() const { return mode; }
	void set_mode(PhysicsServer3D::BodyMode p_mode);

	// --- PhysX Internal Access ---
	physx::PxRigidActor *get_px_actor() const { return get_rigid_actor(); }
	/// Returns the actor as PxRigidDynamic, or nullptr if this is a static body.
	physx::PxRigidDynamic *get_px_dynamic() const;
	/// PxMaterial used by this body's shapes (lazily created on first call) —
	/// the key side of the vehicle surface-grip table.
	physx::PxMaterial *get_shape_material() { return _get_shape_material(); }

	/// True while this body is borrowed as a vehicle2 chassis. When set,
	/// on_pre_step() skips its manual gravity application — the vehicle2
	/// simulation context owns gravity for the chassis (the actor also has
	/// eDISABLE_GRAVITY set, per the PxVehiclePhysXActor requirement).
	bool is_vehicle_chassis = false;
	bool get_is_vehicle_chassis() const { return is_vehicle_chassis; }
	void set_is_vehicle_chassis(bool p_enabled) { is_vehicle_chassis = p_enabled; }

	/// Re-syncs actor_user_data with the body's current RID/ObjectID.
	/// Call after set_instance_id() so query results stay correct.
	void refresh_user_data();

	// --- State (transform, velocities, sleep) ---
	void set_state(PhysicsServer3D::BodyState p_state, const Variant &p_value);
	Variant get_state(PhysicsServer3D::BodyState p_state) const;

	// --- Parameters (mass, friction, bounce, damping, gravity scale) ---
	void set_param(PhysicsServer3D::BodyParameter p_param, const Variant &p_value);
	Variant get_param(PhysicsServer3D::BodyParameter p_param) const;
	void reset_mass_properties();
	/// Recomputes the inertia tensor from the current mass and attached
	/// simulation shapes (density=0 preserves the mass).  Returns false if
	/// there are no simulation shapes — the inertia tensor is left unchanged.
	bool update_inertia();

	// --- Forces / impulses (instantaneous) ---
	void apply_central_impulse(const Vector3 &p_impulse);
	void apply_impulse(const Vector3 &p_impulse, const Vector3 &p_position);
	void apply_torque_impulse(const Vector3 &p_impulse);
	void apply_central_force(const Vector3 &p_force);
	void apply_force(const Vector3 &p_force, const Vector3 &p_position);
	void apply_torque(const Vector3 &p_torque);

	// --- Constant forces (persist across steps until cleared) ---
	void add_constant_central_force(const Vector3 &p_force);
	void add_constant_force(const Vector3 &p_force, const Vector3 &p_position);
	void add_constant_torque(const Vector3 &p_torque);
	void set_constant_force(const Vector3 &p_force);
	Vector3 get_constant_force() const { return constant_force; }
	void set_constant_torque(const Vector3 &p_torque);
	Vector3 get_constant_torque() const { return constant_torque; }

	// --- Axis velocity (Godot-specific: adds velocity along one axis) ---
	void set_axis_velocity(const Vector3 &p_axis_velocity);

	// --- Axis lock ---
	void set_axis_lock(PhysicsServer3D::BodyAxis p_axis, bool p_lock);
	bool is_axis_locked(PhysicsServer3D::BodyAxis p_axis) const;

	// --- Continuous Collision Detection ---
	void set_continuous_collision_detection(bool p_enable);
	bool is_continuous_collision_detection_enabled() const;

	// --- Collision exceptions ---
	void add_collision_exception(const RID &p_excepted_body);
	void remove_collision_exception(const RID &p_excepted_body);
	void get_collision_exceptions(List<RID> *p_exceptions) const;
	const HashSet<RID> &get_collision_exception_set() const;

	// --- Joint links (joints register/unregister themselves; the body
	// notifies them when its actor is recreated or the body is destroyed) ---
	void add_joint(PhysXJoint3D *p_joint) {
		if (!joints.has(p_joint)) {
			joints.push_back(p_joint);
		}
	}
	void remove_joint(PhysXJoint3D *p_joint) {
		const int64_t idx = joints.find(p_joint);
		if (idx != -1) {
			joints.remove_at_unordered(idx);
		}
	}

	// --- Chassis links (vehicles borrowing this body as chassis register
	// themselves; the body notifies them when its space membership changes) ---
	void add_chassis_vehicle(PhysXVehicleServer *p_vehicle) {
		if (!chassis_vehicles.has(p_vehicle)) {
			chassis_vehicles.push_back(p_vehicle);
		}
	}
	void remove_chassis_vehicle(PhysXVehicleServer *p_vehicle) {
		const int64_t idx = chassis_vehicles.find(p_vehicle);
		if (idx != -1) {
			chassis_vehicles.remove_at_unordered(idx);
		}
	}

	// --- Kinematic body velocity ---
	Vector3 get_kinematic_linear_velocity() const { return kinematic_linear_velocity; }
	Vector3 get_kinematic_angular_velocity() const { return kinematic_angular_velocity; }

	// --- Contact reporting ---
	void set_max_contacts_reported(int p_amount);
	void set_collision_priority(real_t p_priority) { collision_priority = p_priority; }
	real_t get_collision_priority() const { return collision_priority; }
	void set_user_flags(uint32_t p_flags) { user_flags = p_flags; }
	uint32_t get_user_flags() const { return user_flags; }
	void set_contacts_depth_threshold(real_t p_threshold) { contacts_depth_threshold = p_threshold; }
	real_t get_contacts_depth_threshold() const { return contacts_depth_threshold; }
	int get_max_contacts_reported() const { return max_contacts_reported; }

	// --- Custom force integration (Godot's "custom integrator") ---
	void set_omit_force_integration(bool p_enable);
	bool is_omitting_force_integration() const { return omit_force_integration; }

	// --- Ray pickable ---
	void set_ray_pickable(bool p_enable) { ray_pickable = p_enable; }
	bool is_ray_pickable() const override { return ray_pickable; }

	// --- Shape lifecycle overrides (update inertia tensor) ---
	void _on_shape_added() override;
	void _on_shape_removed() override;
	void _on_shape_geometry_changed() override;
	void _on_shape_transform_changed() override;

	// --- Shape observer override (wake only when body is in a scene) ---
	void shape_changed(PhysXShape3D *p_shape) override;

	// --- Scene management (deferred until shapes are attached) ---
	void _add_to_scene();

	/// Queues AREA_BODY_REMOVED for every overlapping area and clears the
	/// overlap list. Used when the body's actor is torn down — destruction,
	/// leaving a space, or the static<->dynamic mode flip — since the trigger
	/// pairs then return as REMOVED, which the simulation event callback
	/// skips, and Godot would never see the body leave.
	void _emit_all_area_exits();

	// Forces/impulses applied while the actor is not in a PxScene yet (e.g.
	// from SceneTree._initialize(), before the body enters a world) — PhysX
	// crashes in addForce without a BodySim, while godot_physics_3d applies
	// them once simulation starts. Buffered here and flushed by
	// _flush_pending_force_ops() from _add_to_scene().
	struct PendingForceOp {
		enum Kind {
			CENTRAL_IMPULSE,
			IMPULSE_AT,
			TORQUE_IMPULSE,
			CENTRAL_FORCE,
			FORCE_AT,
			TORQUE,
		};
		Kind kind;
		Vector3 vec; // impulse / force / torque
		Vector3 offset; // body-origin offset, for the *_AT kinds
	};
	LocalVector<PendingForceOp> pending_force_ops;
	bool _apply_force_op(const PendingForceOp &p_op);
	void _flush_pending_force_ops();

	// --- Callbacks ---
	void set_state_sync_callback(const Callable &p_callable) { state_sync_callback = p_callable; }
	void set_force_integration_callback(const Callable &p_callable, const Variant &p_userdata) {
		force_integration_callback = p_callable;
		force_integration_userdata = p_userdata;
	}
	//
	virtual void set_shape_disabled(int p_shape_idx, bool p_disabled);

	/// Re-applies the sleep policy from the current space (sleep thresholds /
	/// wake counter / allow_sleep). Called by the space when a sleep-related
	/// SpaceParameter changes and when the body registers with a space.
	void refresh_sleep_policy();

	/// Replace the shape resource at p_shape_idx, preserving the slot's
	/// transform and disabled state (Godot's body_set_shape contract).
	void set_shape(int p_shape_idx, PhysXShape3D *p_shape);

	// --- Per-step hooks (called by PhysXSpace3D::step) ---
	/// Pre-step: clear contacts, apply constant forces + gravity scale, run
	/// the custom integrator if set. Also handles area gravity/damping
	/// overrides by querying overlapping areas and applying effective values.
	void on_pre_step(float p_step);
	/// Post-step: fire the state-sync callback so Godot nodes read new state.
	void on_post_step(float p_step);

	// --- Contact buffer (populated by onContact, read by DirectBodyState) ---
	LocalVector<PhysXBodyContact> &get_contacts() { return contacts; }
	const LocalVector<PhysXBodyContact> &get_contacts() const { return contacts; }
	void clear_contacts() { contacts.clear(); }
	/// Effective contact list for readers: while a body sleeps no onContact
	/// events arrive (the pair stops running narrowphase), so the live buffer
	/// stays empty — serve the last pre-sleep snapshot instead, matching
	/// Godot's "resting bodies report their touching contacts" semantics.
	const LocalVector<PhysXBodyContact> &get_report_contacts() const {
		if (contacts.is_empty() && !last_step_contacts.is_empty()) {
			const physx::PxRigidDynamic *dyn = get_px_dynamic();
			if (dyn && dyn->isSleeping()) {
				return last_step_contacts;
			}
		}
		return contacts;
	}

	// --- Direct body state (lazily created, owned by the body) ---
	PhysXDirectBodyState3D *get_direct_state();

	// --- Cached resolved totals (recomputed in on_pre_step; read by
	// PhysicsDirectBodyState3D::get_total_gravity / get_total_*_damp). ---
	Vector3 get_cached_total_gravity() const { return cached_total_gravity; }
	real_t get_cached_total_linear_damp() const { return cached_total_linear_damp; }
	real_t get_cached_total_angular_damp() const { return cached_total_angular_damp; }

	// --- Overlap tracking (called by onTrigger or set_space cleanup) ---
	/// Adds an area to this body's overlap list. Called by onTrigger() or
	/// PhysXArea3D::set_space() cleanup.
	void add_overlapping_area(PhysXArea3D *p_area);
	/// Removes an area from this body's overlap list. Called by onTrigger() or
	/// PhysXArea3D::set_space() cleanup.
	void remove_overlapping_area(PhysXArea3D *p_area);

protected:
	virtual void _update_shapes() override;
	/// Lazily creates and returns a body-private PxMaterial so that per-body
	/// bounce/friction don't leak through the shared server default material.
	virtual physx::PxMaterial *_get_shape_material() override;

private:
	PhysicsServer3D::BodyMode mode = PhysicsServer3D::BODY_MODE_RIGID;

	/// Bridges PxActor->userData back to this object's RID/ObjectID.
	PhysXActorUserData actor_user_data;

	// Cached parameters (reapplied on actor recreation via _apply_params_to_actor).
	real_t bounce = 0.0f;
	real_t friction = 1.0f;
	real_t mass = 1.0f;
	real_t gravity_scale = 1.0f;
	real_t linear_damp = 0.0f;
	real_t angular_damp = 0.0f;
	real_t cached_total_linear_damp = 0.0f;
	real_t cached_total_angular_damp = 0.0f;
	Vector3 cached_total_gravity;
	PhysicsServer3D::BodyDampMode linear_damp_mode = PhysicsServer3D::BODY_DAMP_MODE_COMBINE;
	PhysicsServer3D::BodyDampMode angular_damp_mode = PhysicsServer3D::BODY_DAMP_MODE_COMBINE;

	// Sleep policy (BODY_STATE_CAN_SLEEP). PhysX has no "disable sleeping" flag;
	// a sleep threshold of 0 makes the body's speed never drop below it.
	bool can_sleep = true;
	real_t cached_sleep_threshold = -1.0f; ///< Captured before zeroing; -1 = not captured.

	// User overrides (BODY_PARAM_INERTIA / BODY_PARAM_CENTER_OF_MASS). These
	// must be re-applied after update_inertia(), which recomputes both from
	// the shape geometry.
	bool inertia_set = false;
	Vector3 inertia_override;
	bool center_of_mass_set = false;
	Vector3 center_of_mass_override;

	// Constant force/torque accumulators (Godot semantics: persist across steps).
	Vector3 constant_force;
	Vector3 constant_torque;
	/// Collision exceptions (consulted by query filter callback).
	HashSet<RID> collision_exceptions;

	// Flags / state.
	int max_contacts_reported = 0;
	real_t collision_priority = 1.0; // Godot default; weights test-motion recovery upstream
	uint32_t user_flags = 0;
	real_t contacts_depth_threshold = 0.0;
	bool omit_force_integration = false;
	bool ray_pickable = true;
	bool ccd_enabled = false;
	physx::PxRigidDynamicLockFlags axis_lock_flags;

	// Callbacks.
	Callable state_sync_callback;
	Callable force_integration_callback;
	Variant force_integration_userdata;

	/// Per-step contacts (see PhysXBodyContact).
	LocalVector<PhysXBodyContact> contacts;
	/// Snapshot of the previous step's contacts, kept so a body that falls
	/// asleep mid-contact still reports it (see get_report_contacts()).
	LocalVector<PhysXBodyContact> last_step_contacts;

	/// Lazily-created DirectBodyState wrapper.
	PhysXDirectBodyState3D *direct_state = nullptr;

	/// Areas overlapping this body, maintained by onTrigger().
	LocalVector<PhysXArea3D *> overlapping_areas;
	/// Reused priority-order index buffer for the area-override resolution in
	/// on_pre_step (was a fresh LocalVector per body per step).
	LocalVector<int> _area_order_scratch;

	/// Joints connected to this body (they register on make and unlink on
	/// release/destruction). Notified so the PxJoint can be rebuilt when this
	/// body's actor is recreated (static<->dynamic mode switch, scene entry)
	/// and destroyed when the body goes away.
	LocalVector<PhysXJoint3D *> joints;

	/// Vehicles borrowing this body as chassis. Notified on set_space so the
	/// vehicle re-homes into the chassis's scene (road geometry, gravity and
	/// the step loop must come from the scene the chassis actually lives in).
	LocalVector<PhysXVehicleServer *> chassis_vehicles;

	// Kinematic velocity tracking (AnimatableBody3D / sync_to_physics)
	Vector3 kinematic_linear_velocity;
	Vector3 kinematic_angular_velocity;
	physx::PxTransform previous_kinematic_pose{ physx::PxIdentity };
	bool has_previous_kinematic_pose = false;

	// True once this body has been added to the PhysX scene.
	// Used to defer scene addition until shapes are attached.
	bool body_added_to_scene = false;

	// Mirrors the chassis actor's eDISABLE_GRAVITY flag so on_pre_step only
	// writes it on an actual flip (flag writes dirty the actor in PhysX).
	// Recreated actors start with gravity enabled, matching the initial value.
	bool gravity_disabled_cached = false;

	void _create_actor();
	/// Destroys the actor. p_space routes the release when the body's own
	/// space pointer is already gone (destructor path after set_space(nullptr))
	/// — routing through the (former) space queues the remove+release until
	/// after an in-flight fetch instead of releasing into a live solve.
	void _destroy_actor(PhysXSpace3D *p_space = nullptr);
	/// Full re-apply: surface params + mass properties (setMass + inertia/COM
	/// re-derived from the shapes). Used by creation, mode switches and the
	/// mass-affecting parameters.
	void _apply_params_to_actor();
	/// Cheap re-apply: bounce/friction (userData + private material),
	/// damping, CCD, axis locks, sleep policy. Everything except mass
	/// properties — used by set_param for parameters that cannot change the
	/// inertia tensor or center of mass.
	void _apply_surface_params_to_actor();
	void _apply_sleep_policy(physx::PxRigidDynamic *p_dyn);
	void _update_kinematic_velocity(float p_step);
	/// Lock flags actually enforced: user axis_lock_flags plus mode-derived
	/// locks (BODY_MODE_RIGID_LINEAR locks all angular axes).
	physx::PxRigidDynamicLockFlags _effective_lock_flags() const;
};
#endif // PHYSX_BODY_3D_H

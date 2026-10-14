/**************************************************************************/
/*  physx_area_3d.h                                                       */
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

#include "core/math/transform_3d.h"
#include "core/object/object_id.h"
#include "core/variant/callable.h"
#include "core/variant/variant.h"
#include "servers/physics_3d/physics_server_3d.h"

#include "../shapes/physx_user_data.h"
#include "physx_shaped_object_3d.h"

// Forward declarations
class PhysXBody3D;
namespace physx {
class PxRigidActor;
}

class PhysXArea3D : public PhysXShapedObject3D {
public:
	PhysXArea3D();
	~PhysXArea3D();

	// --- Space Management (overrides PhysXObject3D pure virtual) ---
	virtual void set_space(PhysXSpace3D *p_space) override;

	// --- Transform ---
	void set_transform(const Transform3D &p_transform);
	Transform3D get_transform() const;

	// --- Shape mutators with area-specific trigger flag configuration ---
	void add_shape(PhysXShape3D *p_shape, const Transform3D &p_transform = Transform3D(), bool p_disabled = false);
	void set_shape(int p_shape_idx, PhysXShape3D *p_shape);
	void set_shape_transform(int p_shape_idx, const Transform3D &p_transform);
	void set_shape_disabled(int p_shape_idx, bool p_disabled);
	void remove_shape(int p_shape_idx);
	void clear_shapes();

	// --- Area Parameters (Gravity, Damping, Priority, Wind) ---
	void set_param(PhysicsServer3D::AreaParameter p_param, const Variant &p_value);
	Variant get_param(PhysicsServer3D::AreaParameter p_param) const;

	// --- Monitoring & Callbacks ---
	void set_monitorable(bool p_monitorable);
	bool get_monitorable() const;

	// Monitoring is controlled purely by callback validity — see the PhysX
	// module-local fix: Area3D::set_monitoring() installs the callback and
	// Godot never tells the server about a separate flag.
	void set_monitoring(bool p_enable);
	bool get_monitoring() const;

	void set_ray_pickable(bool p_enable);
	bool is_ray_pickable() const override;

	void set_monitor_callback(const Callable &p_callback) {
		monitor_callback = p_callback;
		_update_shapes(); // Trigger flags depend on whether this area detects.
	}
	void set_area_monitor_callback(const Callable &p_callback) {
		area_monitor_callback = p_callback;
		_update_shapes(); // Trigger flags depend on whether this area detects.
	}

	/**
	 * @brief Dispatches a body-monitor event (AREA_BODY_ADDED / AREA_BODY_REMOVED).
	 *
	 * Called by PhysXSimulationEventCallback::onTrigger. Matches the Godot
	 * Area3D::_body_inout signature: callable(status, body_rid, body_object_id,
	 * body_shape, area_shape). No-op if monitoring is disabled or no callback
	 * is set.
	 */
	void dispatch_body_monitor(int p_status, const RID &p_body_rid, ObjectID p_body_id, int p_body_shape, int p_area_shape);

	/**
	 * @brief Dispatches an area-monitor event (for area-vs-area overlaps).
	 * Matches Area3D::_area_inout: callable(status, area_rid, area_object_id,
	 * area_shape, self_shape).
	 */
	void dispatch_area_monitor(int p_status, const RID &p_area_rid, ObjectID p_area_id, int p_area_shape, int p_self_shape);

	/// Re-syncs actor_user_data with the area's current RID/ObjectID.
	void refresh_user_data();

	// --- Area parameter getters (for the resolver) ---
	/// Returns the area's gravity override mode.
	PhysicsServer3D::AreaSpaceOverrideMode get_gravity_override_mode() const { return gravity_override_mode; }
	/// Returns the area's gravity magnitude.
	real_t get_gravity() const { return gravity; }
	/// Returns the area's gravity direction vector.
	Vector3 get_gravity_vector() const { return gravity_vector; }
	/// Returns whether this area uses point gravity.
	bool get_gravity_is_point() const { return gravity_is_point; }
	/// Returns the point gravity unit distance.
	real_t get_gravity_point_unit_distance() const { return gravity_point_unit_distance; }
	/// Returns the area's linear damping value.
	real_t get_linear_damp() const { return linear_damp; }
	/// Returns the area's linear damping override mode.
	PhysicsServer3D::AreaSpaceOverrideMode get_linear_damp_override_mode() const { return linear_damp_override_mode; }
	/// Returns the area's angular damping value.
	real_t get_angular_damp() const { return angular_damp; }
	/// Returns the area's angular damping override mode.
	PhysicsServer3D::AreaSpaceOverrideMode get_angular_damp_override_mode() const { return angular_damp_override_mode; }
	/// Returns the area's priority.
	int get_priority() const { return priority; }

	/// True when this area carries a wind force (wind_force_magnitude != 0).
	bool has_wind() const { return wind_force_magnitude != 0.0f; }
	/// Wind force contribution at a world position: wind_direction scaled by
	/// wind_force_magnitude, attenuated over downwind distance from
	/// wind_source by wind_attenuation_factor (zero unless the wind params
	/// are set).
	Vector3 wind_at(const Vector3 &p_position) const;

	/// Returns true if this area has any area override enabled.
	bool has_gravity_override() const { return gravity_override_mode != PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED; }
	bool has_linear_damp_override() const { return linear_damp_override_mode != PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED; }
	bool has_angular_damp_override() const { return angular_damp_override_mode != PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED; }

	// --- Overlap tracking (maintained by PhysXSimulationEventCallback) ---
	/// Adds a body to this area's overlap list. Called by onTrigger().
	void add_overlapping_body(PhysXBody3D *p_body);
	/// Removes a body from this area's overlap list. Called by onTrigger().
	void remove_overlapping_body(PhysXBody3D *p_body);
	/// Returns the list of bodies currently overlapping this area.
	const LocalVector<PhysXBody3D *> &get_overlapping_bodies() const { return overlapping_bodies; }

	/// Adds an area to this area's overlap list. Called by onTrigger().
	void add_overlapping_area(PhysXArea3D *p_area);
	/// Removes an area from this area's overlap list. Called by onTrigger().
	void remove_overlapping_area(PhysXArea3D *p_area);
	/// Returns the list of areas currently overlapping this area.
	const LocalVector<PhysXArea3D *> &get_overlapping_areas() const { return overlapping_areas; }

protected:
	virtual void _update_shapes() override;

private:
	/// Bridges PxActor->userData back to this object's RID/ObjectID/type.
	PhysXActorUserData actor_user_data;

	Transform3D transform;
	bool monitorable = false;
	// Reference default is pickable for every collision object (bodies and
	// areas alike: godot_physics_3d godot_collision_object_3d.h). The Area3D
	// node pushes its own value on ready; this only governs raw-server areas.
	bool ray_pickable = true;

	Callable monitor_callback;
	Callable area_monitor_callback;

	// Gravity parameters
	PhysicsServer3D::AreaSpaceOverrideMode gravity_override_mode = PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED;
	real_t gravity = 9.8;
	Vector3 gravity_vector = Vector3(0, -1, 0);
	bool gravity_is_point = false;
	real_t gravity_point_unit_distance = 0.0;

	// Damping parameters
	PhysicsServer3D::AreaSpaceOverrideMode linear_damp_override_mode = PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED;
	real_t linear_damp = 0.1;
	PhysicsServer3D::AreaSpaceOverrideMode angular_damp_override_mode = PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED;
	real_t angular_damp = 0.1;

	// Area Priority
	int priority = 0;

	// Wind parameters (Godot 4 specific)
	real_t wind_force_magnitude = 0.0;
	Vector3 wind_source = Vector3(0, 0, 0);
	Vector3 wind_direction = Vector3(0, 0, 0);
	real_t wind_attenuation_factor = 0.0;

	// --- Overlap tracking (maintained by PhysXSimulationEventCallback) ---
	/// Bodies currently overlapping this area. Populated by onTrigger().
	LocalVector<PhysXBody3D *> overlapping_bodies;
	/// Areas currently overlapping this area. Populated by onTrigger().
	LocalVector<PhysXArea3D *> overlapping_areas;

	/// Applies trigger flag configuration to a newly attached PxShape.
	void _configure_shape_as_trigger(physx::PxShape *p_shape) const;

	/// Creates or destroys the per-shape non-trigger "detection" PxShape that
	/// makes a monitorable area detectable by other areas (REG-0011): PhysX 5
	/// does not report trigger-vs-trigger pairs, so area-vs-area overlap is
	/// generated by pairing the other area's trigger with this area's plain
	/// simulation shape. eSIMULATION_SHAPE and eTRIGGER_SHAPE are mutually
	/// exclusive on one PxShape, hence the second shape instance.
	void _sync_detection_shape(AttachedShape &p_record);

	/// Manually emits area-vs-area AREA_BODY_REMOVED events for all recorded
	/// overlaps and clears this area's overlap list. Needed when the shape
	/// state that would generate the exits is destroyed (monitorable turned
	/// off, shape removed) — PhysX exit events for pairs removed with
	/// eREMOVED_SHAPE_* are skipped in onTrigger, so without this the
	/// overlaps would leak and the lists would keep dangling pointers.
	void _emit_area_exit_events();

	/// Wakes dynamic bodies overlapping the area's world AABB at the given
	/// (target) pose. Called after set_transform so overrides and contact-
	/// driven state for bodies at the destination take effect this step.
	void _wake_overlapping_dynamic_bodies(const physx::PxTransform &p_target_pose) const;

public:
	/// World-space bounds of the area's shapes (empty when it has none).
	physx::PxBounds3 get_world_bounds() const;
};

// ---------------------------------------------------------------------------
// Shared area-resolution helpers (used by the rigid-body pre-step AND the
// soft-body gravity resolver — same semantics as godot_physics_3d).
// ---------------------------------------------------------------------------

// Godot's override-mode resolver: applies p_getter() to r_value per the mode;
// returns true when the channel is resolved (REPLACE / COMBINE_REPLACE stop
// the search, COMBINE continues).
template <typename TValue, typename TGetter>
inline bool physx_apply_area_override(TValue &r_value,
		PhysicsServer3D::AreaSpaceOverrideMode p_mode,
		TGetter &&p_getter) {
	switch (p_mode) {
		case PhysicsServer3D::AREA_SPACE_OVERRIDE_DISABLED:
			return false;
		case PhysicsServer3D::AREA_SPACE_OVERRIDE_COMBINE:
			r_value += p_getter();
			return false;
		case PhysicsServer3D::AREA_SPACE_OVERRIDE_COMBINE_REPLACE:
			r_value += p_getter();
			return true;
		case PhysicsServer3D::AREA_SPACE_OVERRIDE_REPLACE:
			r_value = p_getter();
			return true;
		case PhysicsServer3D::AREA_SPACE_OVERRIDE_REPLACE_COMBINE:
			r_value = p_getter();
			return false;
	}
	return false;
}

// The area's gravity contribution at a world position (handles point gravity
// and the area's world transform).
Vector3 physx_area_gravity_at(const PhysXArea3D &p_area, const Vector3 &p_position);

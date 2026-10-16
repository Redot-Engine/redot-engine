/**************************************************************************/
/*  physx_vehicle_3d.h                                                    */
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

#include "core/math/vector3.h"
#include "core/templates/local_vector.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/physics_material.h"

class PhysXVehicleWheel3D;

// A real, PhysX-specific 4-wheel vehicle -- offers PxVehicle2 capability
// stock VehicleBody3D/VehicleWheel3D structurally can't (real engine torque
// response, Ackermann steering correction, a real slip-based tire friction
// curve), alongside (not replacing) this module's VehicleBody3D support.
// Wraps the same Vehicle4W/configure_vehicle4w() composition
// PhysXVehicleProbe proved out headlessly (vehicle/physx_vehicle4w.h)
// -- shared directly, not duplicated, since that layer is pure PxVehicle2
// composition with no project-specific behavior in it (unlike
// PhysXDestructible3D's probe, whose damage/fracture logic is genuinely
// node-specific and was deliberately duplicated instead).
//
// Node structure mirrors VehicleBody3D/VehicleWheel3D exactly, for a direct
// 1:1 comparison: a body node with a real CollisionShape3D (BoxShape3D only)
// child for the chassis, and exactly 4 PhysXVehicleWheel3D children for the
// wheels -- each wheel's own `position` is the suspension attachment
// hardpoint, so a MeshInstance3D child under it lines up with the real
// physics automatically instead of needing hand-guessed offsets (an earlier,
// flat-scalar-properties version of this node needed exactly that, and the
// offsets were wrong -- see the demo's own commit history).
//
// Owns its own PxRigidDynamic directly (via configure_vehicle4w), not a
// PhysXBody3D -- PxVehicle2's own PxVehiclePhysXActorEndComponent writes
// wheel-shape local poses and rigid-body momentum straight onto the actor
// every tick, which needs raw actor/shape pointers, not the generic
// PhysicsServer3D RID abstraction.
class PhysXVehicle3D : public Node3D {
	GDCLASS(PhysXVehicle3D, Node3D);

	friend class PhysXVehicleWheel3D;
	LocalVector<PhysXVehicleWheel3D *> wheels;

public:
	enum CenterOfMassMode {
		CENTER_OF_MASS_MODE_AUTO,
		CENTER_OF_MASS_MODE_CUSTOM,
	};

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_mass(real_t p_mass);
	real_t get_mass() const { return mass; }
	void set_moment_of_inertia(const Vector3 &p_moi);
	Vector3 get_moment_of_inertia() const { return moment_of_inertia; }
	void set_center_of_mass_mode(CenterOfMassMode p_mode);
	CenterOfMassMode get_center_of_mass_mode() const { return center_of_mass_mode; }
	void set_center_of_mass(const Vector3 &p_center_of_mass);
	const Vector3 &get_center_of_mass() const { return center_of_mass; }
	void set_can_sleep(bool p_can_sleep);
	bool is_able_to_sleep() const { return can_sleep; }
	bool is_sleeping() const;

	void set_max_engine_torque(real_t p_v);
	real_t get_max_engine_torque() const { return max_engine_torque; }
	void set_max_brake_torque(real_t p_v);
	real_t get_max_brake_torque() const { return max_brake_torque; }
	void set_max_steer_angle(real_t p_v);
	real_t get_max_steer_angle() const { return max_steer_angle; }
	void set_use_gearbox(bool p_enabled);
	bool get_use_gearbox() const { return use_gearbox; }
	void set_engine_peak_torque(real_t p_v);
	real_t get_engine_peak_torque() const { return engine_peak_torque; }
	void set_engine_idle_omega(real_t p_v);
	real_t get_engine_idle_omega() const { return engine_idle_omega; }
	void set_engine_max_omega(real_t p_v);
	real_t get_engine_max_omega() const { return engine_max_omega; }
	void set_clutch_strength(real_t p_v);
	real_t get_clutch_strength() const { return clutch_strength; }
	void set_gear_ratios(const PackedFloat32Array &p_ratios);
	PackedFloat32Array get_gear_ratios() const;
	void set_gear_final_ratio(real_t p_v);
	real_t get_gear_final_ratio() const { return gear_final_ratio; }
	void set_gear_switch_time(real_t p_v);
	real_t get_gear_switch_time() const { return gear_switch_time; }
	void set_autobox_latency(real_t p_v);
	real_t get_autobox_latency() const { return autobox_latency; }
	void set_target_gear(int p_gear);
	int get_target_gear() const { return target_gear; }
	void set_use_autobox(bool p_enabled);
	bool get_use_autobox() const { return use_autobox; }
	// Autobox shift thresholds, as fractions of (engine omega / max omega):
	// above up_ratio the autobox shifts up, below down_ratio it shifts down.
	void set_autobox_up_ratio(real_t p_v) { autobox_up_ratio = CLAMP(p_v, 0.0f, 1.0f); }
	real_t get_autobox_up_ratio() const { return autobox_up_ratio; }
	void set_autobox_down_ratio(real_t p_v) { autobox_down_ratio = CLAMP(p_v, 0.0f, 1.0f); }
	real_t get_autobox_down_ratio() const { return autobox_down_ratio; }
	void set_ackermann_strength(real_t p_v);
	real_t get_ackermann_strength() const { return ackermann_strength; }
	void set_front_anti_roll_stiffness(real_t p_v);
	real_t get_front_anti_roll_stiffness() const { return front_anti_roll_stiffness; }
	void set_rear_anti_roll_stiffness(real_t p_v);
	real_t get_rear_anti_roll_stiffness() const { return rear_anti_roll_stiffness; }

	// Runtime control inputs, same convention as PxVehicleCommandState: throttle/
	// brake in [0,1], steer in [-1,1]. Set every tick from script, same pattern
	// as VehicleBody3D.engine_force/brake/steering -- different units (PxVehicle2's
	// own normalized commands, not a raw force/raw angle), since that's what the
	// underlying SDK actually takes.
	void set_throttle(real_t p_v) { throttle = CLAMP(p_v, 0.0, 1.0); }
	real_t get_throttle() const { return throttle; }
	void set_brake(real_t p_v) { brake = CLAMP(p_v, 0.0, 1.0); }
	real_t get_brake() const { return brake; }
	void set_steer(real_t p_v) { steer = CLAMP(p_v, -1.0, 1.0); }
	real_t get_steer() const { return steer; }
	// Handbrake input [0,1] -- PhysX brake channel 1 (rear wheels only).
	void set_handbrake(real_t p_v) { handbrake = CLAMP(p_v, 0.0, 1.0); }
	real_t get_handbrake() const { return handbrake; }
	// Handbrake channel torque (Nm). 0 = reuse max_brake_torque.
	void set_handbrake_torque(real_t p_v);
	real_t get_handbrake_torque() const { return handbrake_torque; }
	// Direct-drive has no gearbox, just a fixed forward/neutral/reverse
	// multiplier on throttle response (PxVehicleDirectDriveTransmissionCommandState) --
	// this is that switch, not a raw property on Vehicle4WConfig, since it's
	// a per-tick control input like throttle/brake/steer, not a build-time
	// tuning value.
	void set_reverse(bool p_v) { reverse = p_v; }
	bool is_reverse() const { return reverse; }

	void set_collision_layer(uint32_t p_layer);
	uint32_t get_collision_layer() const { return collision_layer; }
	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_mask() const { return collision_mask; }
	// Chassis contact material, the property RigidBody3D/VehicleBody3D use.
	// Applies to the chassis box only; tire grip is the wheels' own. Changes
	// (here or to the assigned material) apply in place, no rebuild.
	void set_physics_material_override(const Ref<PhysicsMaterial> &p_material);
	Ref<PhysicsMaterial> get_physics_material_override() const { return physics_material_override; }

	Vector3 get_linear_velocity() const;
	real_t get_forward_speed() const;

	// Diagnostics -- see PhysXVehicleProbe's own identical methods for
	// what these mean. wheel index here matches this node's own `wheels`
	// child-registration order (NOT the FL/FR/RL/RR canonical order the
	// underlying Vehicle4W uses internally).
	real_t get_wheel_jounce(int p_wheel) const;
	real_t get_wheel_separation(int p_wheel) const;
	Vector3 get_actor_position() const;

	// --- Gearbox-mode telemetry (meaningful only when use_gearbox is on) ----
	float get_engine_rpm() const; // engine omega as revolutions/minute
	int get_engine_gear() const; // current gear: 0 = R, 1 = N, 2.. = 1st+
	float get_clutch() const; // clutch response [0,1]
	float get_wheel_rpm(int p_wheel) const; // wheel spin as revolutions/minute
	float get_wheel_skid(int p_wheel) const; // longitudinal slip ratio
	Dictionary get_wheel_contact(int p_wheel) const; // { contact: bool, normal: Vector3 }
	float get_wheel_steer_angle(int p_wheel) const; // Ackermann-resolved angle (rad)

	// Live tire-friction update from a wheel child (called by
	// PhysXVehicleWheel3D::set_tire_friction): rewrites that wheel's
	// road-friction default and its tire force curve's grip scale IN the
	// built composition -- no rebuild, so surface changes (wet/ice/gravel
	// zones) can be applied mid-drive. Safe from _physics_process: same
	// window as the command writes. If the vehicle is not built yet the
	// authored value is simply picked up at the next build.
	void _update_live_wheel_tire_friction(PhysXVehicleWheel3D *p_wheel);

	// Transform contract (RigidBody3D-style, two-way): the node's transform is
	// driven FROM the chassis every physics tick, but a script-side write to
	// global_transform while the vehicle is live is pushed INTO the chassis --
	// pose hard-set and all velocities zeroed (reset/respawn semantics). The
	// chassis itself is a raw PxRigidDynamic owned here, invisible to
	// PhysicsServer3D, so this node transform IS the only script-facing way to
	// reposition it.

	PackedStringArray get_configuration_warnings() const override;

	PhysXVehicle3D();
	~PhysXVehicle3D();

private:
	real_t mass = 1500.0f;
	Vector3 moment_of_inertia = Vector3(2000.0f, 2200.0f, 1000.0f);
	CenterOfMassMode center_of_mass_mode = CENTER_OF_MASS_MODE_AUTO;
	Vector3 center_of_mass;
	bool can_sleep = true;
	real_t max_engine_torque = 700.0f;
	real_t max_brake_torque = 6000.0f;
	// --- Engine drive (use_gearbox = true) ----------------------------------
	// The EngineDrive drivetrain: throttle drives an engine (torque curve,
	// idle/max omega) through a clutch and gearbox (reverse + neutral +
	// forward ratios, autobox shifting). DirectDrive (default) applies wheel
	// torque directly. Gear ratio signs follow the SDK's own requirement
	// (reverse < 0, neutral == 0, forward > 0 -- PxVehicleGearboxParams
	// ::isValid rejects anything else); with the composition's right-handed
	// frame (see configure_vehicle4w's axis note) positive forward ratios
	// drive the vehicle nose-first (-Z) and the negative reverse ratio
	// drives it backward (+Z).
	bool use_gearbox = false;
	bool use_autobox = false; // gearbox mode only: automatic shifting (DRIVE) vs manual target_gear
	float engine_peak_torque = 500.0f;
	float engine_idle_omega = 80.0f;
	float engine_max_omega = 600.0f;
	float clutch_strength = 40.0f;
	PackedFloat32Array gear_ratios = PackedFloat32Array(); // empty = composition default
	float gear_final_ratio = 3.5f;
	float gear_switch_time = 0.5f;
	float autobox_latency = 0.5f;
	float autobox_up_ratio = 0.65f;
	float autobox_down_ratio = 0.40f;
	// Target gear for gearbox mode: 255 = automatic (DRIVE), 0 = reverse,
	// 1 = neutral, 2.. = forward gears (see the GEAR_* constants).
	int target_gear = 255;

	real_t max_steer_angle = 0.6f;
	real_t ackermann_strength = 1.0f;
	real_t front_anti_roll_stiffness = 0.0f;
	real_t rear_anti_roll_stiffness = 0.0f;

	real_t throttle = 0.0f;
	real_t brake = 0.0f;
	real_t handbrake = 0.0f;
	real_t handbrake_torque = 0.0f; // 0 = reuse max_brake_torque
	real_t steer = 0.0f;
	bool reverse = false;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	Ref<PhysicsMaterial> physics_material_override;

	// Opaque pointer to the real PxVehicle2 composition (kept out of this
	// header so nothing outside physx_vehicle_3d.cpp needs vehicle/PxVehicleAPI.h).
	struct Impl;
	Impl *impl = nullptr;
	// Last pose written from the chassis into this node (two-way transform
	// contract): NOTIFICATION_TRANSFORM_CHANGED is delivered DEFERRED, so the
	// handler can't tell its own sync writes apart with a flag -- it compares
	// against this instead and only pushes genuine script writes back.
	Transform3D _last_synced_xform;
	bool _last_synced_valid = false;

	// Any exported-property setter, or a child PhysXVehicleWheel3D's own
	// property setter, calls this if the vehicle is already built (editing in
	// the Inspector while Playing) -- full rebuild, same "simple; optimize
	// later" convention PhysXBody3D's own shape/mode-change path already
	// uses. No-op if not yet built.
	void _rebuild_if_live();
	bool _build();
	void _destroy();
	// Effective chassis friction/bounce (SIGNED, Godot-style: negative =
	// rough/absorbent), from physics_material_override or the no-override
	// defaults. Written into the chassis actor's userData (read by the
	// contact-modify combiner) and the chassis PxMaterial (GPU fallback).
	void _chassis_material(real_t &r_friction, real_t &r_bounce) const;
	void _chassis_material_changed();
};

VARIANT_ENUM_CAST(PhysXVehicle3D::CenterOfMassMode);

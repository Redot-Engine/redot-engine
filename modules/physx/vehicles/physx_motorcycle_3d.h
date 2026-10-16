/**************************************************************************/
/*  physx_motorcycle_3d.h                                                 */
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

// A real, PhysX-specific 2-wheel vehicle (motorcycle-style: one front
// steering wheel, one rear driven wheel) -- offers the same real PxVehicle2
// per-wheel suspension/tire/drivetrain composition PhysXVehicle3D offers for
// 4 wheels, ported to Vehicle2W (vehicle/physx_vehicle2w.h) since
// PhysXVehicle3D's own Vehicle4W composition hard-requires exactly 2
// steering + 2 non-steering wheels and can't represent a 2-wheel vehicle at
// all.
//
// PxVehicle2 provides no balancing mechanism for a 2-wheel vehicle (unlike
// Jolt's dedicated MotorcycleController, confirmed by reading the whole
// vehicle/ SDK header tree -- zero Motorcycle/Lean/Balance-related types
// anywhere in it) -- staying upright is left entirely to script: this node
// exposes apply_torque_impulse()/set_angular_velocity() plus the orientation
// readouts (get_up/get_forward/get_angular_velocity/get_roll_angle) a
// script-side lean controller needs, the same API surface
// PhysXMotorcycleProbe proved a working velocity-blend lean controller
// against headlessly (see vehicle/physx_motorcycle_probe.h and
// test/cpu/motorcycle_probe_test.gd in the demo repo for why a direct
// torque-impulse PD controller was tried first and found to diverge).
//
// Node structure mirrors PhysXVehicle3D: a body node with a real
// CollisionShape3D (BoxShape3D) child for the chassis, and exactly 2
// PhysXVehicleWheel3D children -- the same wheel class PhysXVehicle3D uses,
// shared rather than duplicated (see that class's own doc comment for why: a
// motorcycle's fixed front-steers/rear-drives roles are already exactly what
// use_as_steering/use_as_traction express, so a dedicated wheel class would
// have added nothing). Exactly one wheel must have use_as_steering=true
// (the front) and exactly one use_as_traction=true (the rear, matching a
// real motorcycle's RWD convention -- Vehicle2W's drivetrain is fixed
// rear-only regardless of use_as_traction's value on either wheel, so that
// flag is read for the front/rear classification only, not to select which
// wheel actually receives engine torque).
//
// Owns its own PxRigidDynamic directly (via configure_vehicle2w), not a
// PhysXBody3D -- same reasoning as PhysXVehicle3D's own doc comment.
class PhysXMotorcycle3D : public Node3D {
	GDCLASS(PhysXMotorcycle3D, Node3D);

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

	// Runtime control inputs, same convention as PhysXVehicle3D.
	void set_throttle(real_t p_v) { throttle = p_v; }
	real_t get_throttle() const { return throttle; }
	void set_brake(real_t p_v) { brake = p_v; }
	real_t get_brake() const { return brake; }
	void set_steer(real_t p_v) { steer = p_v; }
	real_t get_steer() const { return steer; }
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

	// Lean-control API -- see this class's own doc comment. A script-side
	// controller (e.g. a lean_controller.gd attached to this node) reads the
	// orientation getters and calls apply_torque_impulse()/
	// set_angular_velocity() every physics tick to keep the bike upright.
	void apply_torque_impulse(const Vector3 &p_impulse);
	void apply_force_at_local_position(const Vector3 &p_force, const Vector3 &p_local_position);
	void set_angular_velocity(const Vector3 &p_angular_velocity);
	Vector3 get_angular_velocity() const;
	Vector3 get_up() const;
	Vector3 get_forward() const;
	real_t get_roll_angle() const;

	real_t get_wheel_jounce(int p_wheel) const;
	real_t get_wheel_separation(int p_wheel) const;
	Vector3 get_actor_position() const;
	// Ground-truth diagnostic: the raw PxVehicle2 tire lateral force PhysX
	// itself computed for a wheel this tick (world-space, dotted with the
	// body's current right vector) -- lets a script check whether the SDK's
	// own tire model is behind a "wrong way" slide independent of any
	// script-side controller logic.
	real_t get_wheel_lateral_force(int p_wheel) const;
	real_t get_wheel_lateral_speed(int p_wheel) const;
	Vector3 get_wheel_lateral_direction(int p_wheel) const;
	Vector3 get_wheel_lateral_force_vector(int p_wheel) const;
	Vector3 get_wheel_lateral_velocity(int p_wheel) const;
	real_t get_wheel_camber_angle(int p_wheel) const;

	PackedStringArray get_configuration_warnings() const override;

	PhysXMotorcycle3D();
	~PhysXMotorcycle3D();

private:
	real_t mass = 220.0f;
	Vector3 moment_of_inertia = Vector3(40.0f, 55.0f, 20.0f);
	CenterOfMassMode center_of_mass_mode = CENTER_OF_MASS_MODE_AUTO;
	Vector3 center_of_mass;
	bool can_sleep = true;
	real_t max_engine_torque = 250.0f;
	real_t max_brake_torque = 1200.0f;
	real_t max_steer_angle = 0.5f;

	real_t throttle = 0.0f;
	real_t brake = 0.0f;
	real_t steer = 0.0f;
	bool reverse = false;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	Ref<PhysicsMaterial> physics_material_override;

	// Opaque pointer to the real Vehicle2W composition (kept out of this
	// header so nothing outside physx_motorcycle_3d.cpp needs
	// vehicle/PxVehicleAPI.h).
	struct Impl;
	Impl *impl = nullptr;

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

VARIANT_ENUM_CAST(PhysXMotorcycle3D::CenterOfMassMode);

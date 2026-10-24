/**************************************************************************/
/*  physx_tank_3d.h                                                       */
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

// A real, PhysX-specific N-wheel tracked (tank-style) vehicle -- driven
// entirely by skid-steer, no steering wheels at all. Neither PhysXVehicle3D
// (fixed 4 wheels, steering axle) nor PhysXMotorcycle3D (fixed 2 wheels,
// steering front) can represent this, so it's a separate composition,
// VehicleTrack (vehicle/physx_vehicle_track.h), built from the same
// raw per-wheel suspension/tire/drivetrain components the others use.
//
// PxVehicle2 has no track/tank primitive whatsoever (confirmed: zero track/
// tank-related types anywhere in the vehicle/ SDK header tree -- PhysX 5
// dropped the old PhysX-3.4-era PxVehicleDriveTank class entirely). Jolt, by
// contrast, ships a real, tested TrackedVehicleController -- but wrapping
// that would mean new code in modules/jolt_physics, breaking this project's
// standing "vanilla engine + one extra module" line, so this is a
// from-scratch skid-steer composition entirely inside godot_physx instead
// (see physx_vehicle_track.h's own doc comment for the mechanism).
//
// Node structure: a body node with a real CollisionShape3D (BoxShape3D)
// child for the hull, and 2 to VehicleTrack::MAX_WHEELS PhysXVehicleWheel3D
// children -- the same wheel class PhysXVehicle3D/PhysXMotorcycle3D use,
// shared rather than duplicated (a tank wheel needs nothing beyond what it
// already exposes -- see VehicleTrackWheelConfig's own doc comment). Every
// wheel is driven and none steer; use_as_steering/use_as_traction are simply
// unused here. Which TRACK a wheel belongs to is inferred automatically from
// its own local X position sign (negative = left, positive = right) by
// configure_vehicle_track(), the same "the vehicle inspects its own wheel
// children" pattern PhysXMotorcycle3D already uses for front/rear
// classification -- no explicit per-wheel track-side property needed.
//
// Owns its own PxRigidDynamic directly (via configure_vehicle_track), not a
// PhysXBody3D -- same reasoning as PhysXVehicle3D's own doc comment.
class PhysXTank3D : public Node3D {
	GDCLASS(PhysXTank3D, Node3D);

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

	// Runtime control inputs -- see VehicleTrack::setDriverInput()'s own doc
	// comment for the skid-steer convention: sign is direction, magnitude is
	// speed, independently per track (a real pivot turn needs the two
	// tracks able to spin in opposite directions at once, which a single
	// throttle+steer pair -- PhysXVehicle3D/PhysXMotorcycle3D's own
	// convention -- can't express).
	void set_left_ratio(real_t p_v) { left_ratio = CLAMP(p_v, -1.0, 1.0); }
	real_t get_left_ratio() const { return left_ratio; }
	void set_right_ratio(real_t p_v) { right_ratio = CLAMP(p_v, -1.0, 1.0); }
	real_t get_right_ratio() const { return right_ratio; }
	void set_brake(real_t p_v) { brake = p_v; }
	real_t get_brake() const { return brake; }

	void set_collision_layer(uint32_t p_layer);
	uint32_t get_collision_layer() const { return collision_layer; }
	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_mask() const { return collision_mask; }
	// Chassis (hull) contact material, the property RigidBody3D/VehicleBody3D
	// use. Applies to the hull box only; track grip is the wheels' own.
	// Changes (here or to the assigned material) apply in place, no rebuild.
	void set_physics_material_override(const Ref<PhysicsMaterial> &p_material);
	Ref<PhysicsMaterial> get_physics_material_override() const { return physics_material_override; }

	Vector3 get_linear_velocity() const;
	Vector3 get_angular_velocity() const;
	real_t get_forward_speed() const;
	Vector3 get_up() const;
	Vector3 get_forward() const;

	real_t get_wheel_jounce(int p_wheel) const;
	real_t get_wheel_separation(int p_wheel) const;
	Vector3 get_actor_position() const;

	PackedStringArray get_configuration_warnings() const override;

	PhysXTank3D();
	~PhysXTank3D();

private:
	real_t mass = 4000.0f;
	Vector3 moment_of_inertia = Vector3(8000.0f, 9000.0f, 9000.0f);
	CenterOfMassMode center_of_mass_mode = CENTER_OF_MASS_MODE_AUTO;
	Vector3 center_of_mass;
	bool can_sleep = true;
	real_t max_engine_torque = 4000.0f;
	real_t max_brake_torque = 8000.0f;

	real_t left_ratio = 0.0f;
	real_t right_ratio = 0.0f;
	real_t brake = 0.0f;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	Ref<PhysicsMaterial> physics_material_override;

	// Opaque pointer to the real VehicleTrack composition (kept out of this
	// header so nothing outside physx_tank_3d.cpp needs vehicle/PxVehicleAPI.h).
	struct Impl;
	Impl *impl = nullptr;

	// Unlike PhysXVehicle3D/PhysXMotorcycle3D (exactly 4 or 2 wheels
	// required, so they only ever build once), a tank's wheel count varies
	// (2 to VehicleTrack::MAX_WHEELS) -- each wheel child's own
	// NOTIFICATION_ENTER_TREE fires separately as Godot cascades tree-entry
	// down a subtree, even when the whole subtree (tank + all its wheels)
	// enters the live tree in one add_child() call, so _rebuild_if_live()
	// gets called once per wheel as `wheels` grows one element at a time.
	// Found via a real repro: an 8-wheel tank hit ~69 m/s and fell through
	// the floor versus the identically-configured headless probe's clean
	// ~14 m/s -- rebuilding (destroying and recreating the real PxRigidDynamic
	// actor) 7 times in a row during setup left the SDK in a bad state the
	// probe (built once) never hit. Coalescing multiple rebuild requests
	// within the same frame into one deferred call -- so it only actually
	// rebuilds once, after every wheel has finished entering the tree --
	// fixed it.
	bool rebuild_scheduled = false;
	void _rebuild_if_live();
	void _do_deferred_rebuild();
	bool _build();
	void _destroy();
	// Effective chassis friction/bounce (SIGNED, Godot-style: negative =
	// rough/absorbent), from physics_material_override or the no-override
	// defaults. Written into the chassis actor's userData (read by the
	// contact-modify combiner) and the chassis PxMaterial (GPU fallback).
	void _chassis_material(real_t &r_friction, real_t &r_bounce) const;
	void _chassis_material_changed();
};

VARIANT_ENUM_CAST(PhysXTank3D::CenterOfMassMode);

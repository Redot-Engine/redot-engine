/**************************************************************************/
/*  physx_vehicle_wheel_3d.h                                              */
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

#include "scene/3d/node_3d.h"

class PhysXVehicle3D;
class PhysXMotorcycle3D;
class PhysXTank3D;

// One wheel of a PhysXVehicle3D, PhysXMotorcycle3D, OR a PhysXTank3D -- same
// role/registration mechanism as VehicleWheel3D on a VehicleBody3D (this
// node's own `position` IS the suspension attachment hardpoint, exactly like
// VehicleWheel3D's). Shared between all three vehicle types rather than
// split into per-type wheel classes: the only thing that looked different at
// first (a motorcycle's fixed front-steers/rear-drives roles, or a tank's
// every-wheel-driven/none-steer roles) is already exactly what
// use_as_steering/use_as_traction express when left at their natural
// per-wheel values -- a tank wheel simply never has use_as_steering set (see
// PhysXTank3D's own doc comment for how it decides track side instead, which
// doesn't use either flag at all). Only one of `vehicle`/`motorcycle`/`tank`
// is ever non-null on a given instance.
class PhysXVehicleWheel3D : public Node3D {
	GDCLASS(PhysXVehicleWheel3D, Node3D);

	friend class PhysXVehicle3D;
	friend class PhysXMotorcycle3D;
	friend class PhysXTank3D;
	PhysXVehicle3D *vehicle = nullptr;
	PhysXMotorcycle3D *motorcycle = nullptr;
	PhysXTank3D *tank = nullptr;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_radius(real_t p_v);
	real_t get_radius() const { return radius; }
	void set_half_width(real_t p_v);
	real_t get_half_width() const { return half_width; }
	void set_wheel_mass(real_t p_v);
	real_t get_wheel_mass() const { return wheel_mass; }
	void set_wheel_moment_of_inertia(real_t p_v);
	real_t get_wheel_moment_of_inertia() const { return wheel_moment_of_inertia; }
	void set_damping_rate(real_t p_v);
	real_t get_damping_rate() const { return damping_rate; }

	void set_suspension_travel(real_t p_v);
	real_t get_suspension_travel() const { return suspension_travel; }
	void set_suspension_stiffness(real_t p_v);
	real_t get_suspension_stiffness() const { return suspension_stiffness; }
	void set_suspension_damping(real_t p_v);
	real_t get_suspension_damping() const { return suspension_damping; }

	void set_tire_lateral_stiffness(real_t p_v);
	real_t get_tire_lateral_stiffness() const { return tire_lateral_stiffness; }
	void set_tire_longitudinal_stiffness(real_t p_v);
	real_t get_tire_longitudinal_stiffness() const { return tire_longitudinal_stiffness; }
	void set_tire_camber_stiffness(real_t p_v);
	real_t get_tire_camber_stiffness() const { return tire_camber_stiffness; }
	void set_tire_friction(real_t p_v);
	real_t get_tire_friction() const { return tire_friction; }
	void set_tire_rest_grip(real_t p_v);
	real_t get_tire_rest_grip() const { return tire_rest_grip; }
	void set_tire_slide_grip(real_t p_v);
	real_t get_tire_slide_grip() const { return tire_slide_grip; }

	void set_use_as_steering(bool p_v);
	bool is_used_as_steering() const { return use_as_steering; }
	void set_use_as_traction(bool p_v);
	bool is_used_as_traction() const { return use_as_traction; }

	PackedStringArray get_configuration_warnings() const override;

	// Authored suspension hardpoint (see the cache comment): what the parent
	// vehicles configure from.
	Vector3 get_authored_position() const { return _authored_position; }
	Basis get_authored_basis() const { return _authored_basis; }

private:
	real_t radius = 0.35f;
	real_t half_width = 0.15f;
	real_t wheel_mass = 20.0f;
	real_t wheel_moment_of_inertia = 1.2f;
	real_t damping_rate = 0.25f;
	real_t suspension_travel = 0.22f;
	real_t suspension_stiffness = 22000.0f;
	real_t suspension_damping = 5200.0f;
	real_t tire_lateral_stiffness = 20000.0f;
	real_t tire_longitudinal_stiffness = 20000.0f;
	real_t tire_camber_stiffness = 0.0f;
	real_t tire_friction = 1.0f;
	real_t tire_rest_grip = 0.9f;
	real_t tire_slide_grip = 0.7f;
	bool use_as_steering = false;
	bool use_as_traction = true;

	// Any property change rebuilds the parent if it's already live, same as
	// PhysXVehicle3D's own property setters -- this node has no independent
	// physics state, it's just data the parent reads at build time.
	void _rebuild_parent_if_live();

	// The AUTHORED local transform, captured at tree entry. Once the vehicle
	// is live it OWNS this node's transform (it writes the live suspension
	// pose -- jounce, steer and wheel spin -- into it every tick), so any
	// later rebuild must configure from the authored hardpoint, never from
	// the live pose: reading get_transform() post-build folds spin/steer back
	// into the suspension geometry, tilting the travel direction and
	// injecting a phantom drive force (measured: a parked car cruised at a
	// constant 2.5 m/s after its tuning was touched at runtime).
	Vector3 _authored_position;
	Basis _authored_basis;
	bool _authored_valid = false;
};

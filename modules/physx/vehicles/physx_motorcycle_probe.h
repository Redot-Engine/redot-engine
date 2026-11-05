/**************************************************************************/
/*  physx_motorcycle_probe.h                                              */
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

#include "core/math/basis.h"
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/templates/rid.h"

// Runtime-bridge MVP probe for a PxVehicle2 direct-drive 2-wheel vehicle
// (vehicle/physx_vehicle2w.h) -- not a real node, exists so a headless
// GDScript test can prove the composition works AND prove a script-side lean
// controller can keep it upright, before building the real PhysXMotorcycle3D
// node. Same precedent as PhysXVehicleProbe for the 4-wheel car.
//
// PxVehicle2 has no built-in balancing mechanism for a 2-wheel vehicle
// (unlike Jolt's MotorcycleController) -- apply_torque_impulse() is what lets
// a GDScript lean controller supply that missing piece itself, applying a
// corrective torque every tick the same way Jolt's lean spring does
// internally, just at the script layer instead of inside the SDK.
class PhysXMotorcycleProbe : public RefCounted {
	GDCLASS(PhysXMotorcycleProbe, RefCounted);

protected:
	static void _bind_methods();

public:
	bool initialize(RID p_space, const Vector3 &p_position);

	// p_throttle/p_brake in [0,1], p_steer in [-1,1]. Same convention/ordering
	// as PhysXVehicleProbe::step() -- does not call PxScene::simulate().
	void step(real_t p_dt, real_t p_throttle, real_t p_brake, real_t p_steer);

	// World-space torque impulse applied directly to the chassis actor this
	// tick -- the hook a script-side lean controller uses to keep the bike
	// upright (and lean it into turns), since PxVehicle2 itself never applies
	// any balancing torque on its own.
	void apply_torque_impulse(const Vector3 &p_impulse);
	// Directly overrides the chassis's angular velocity -- a stronger, always-
	// stable tool than apply_torque_impulse() for a lean controller: blending
	// the roll-axis component of angular velocity toward a target each tick
	// is a bounded contraction (it can't diverge the way integrating a PD
	// torque impulse can, which is what a direct torque-based lean controller
	// hit -- the cornering-induced destabilizing torque on a 2-wheel vehicle
	// grows faster per tick than a once-per-tick impulse correction can track,
	// regardless of gain).
	void set_angular_velocity(const Vector3 &p_angular_velocity);

	Vector3 get_position() const;
	Vector3 get_linear_velocity() const;
	real_t get_forward_speed() const;
	// The chassis's own up/forward vectors and angular velocity in world
	// space -- what a lean controller needs: up/roll_angle for the tilt
	// error, angular_velocity (dotted with forward) for the roll-rate
	// damping term of a PD controller.
	Vector3 get_up() const;
	Vector3 get_forward() const;
	real_t get_roll_angle() const;
	Vector3 get_angular_velocity() const;
	// Roll angle around the forward axis, radians, positive = leaning right.
	// Convenience wrapper around get_up()/get_basis() for a simple PD lean
	// controller that doesn't want to re-derive this from the basis by hand.

	real_t get_wheel_jounce(int p_wheel) const;
	real_t get_wheel_separation(int p_wheel) const;
	Vector3 get_actor_position() const;

	PhysXMotorcycleProbe();
	~PhysXMotorcycleProbe();

private:
	struct Impl;
	Impl *impl = nullptr;
};

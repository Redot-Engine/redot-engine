/**************************************************************************/
/*  physx_tank_probe.h                                                    */
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

// Runtime-bridge MVP probe for a PxVehicle2 N-wheel tracked (tank-style)
// vehicle (vehicle/physx_vehicle_track.h) -- not a real node, exists so
// a headless GDScript test can prove skid-steer (independently signed
// left/right track speed) actually turns the vehicle before building the
// real node. Same precedent as PhysXMotorcycleProbe/
// PhysXVehicleProbe before it.
//
// Unlike the car/motorcycle probes, there is no lean/balance concern here --
// a tank is inherently stable (wide, low, many ground contacts), so this
// probe's only real job is proving the skid-steer mechanism itself works.
class PhysXTankProbe : public RefCounted {
	GDCLASS(PhysXTankProbe, RefCounted);

protected:
	static void _bind_methods();

public:
	// p_wheel_positions_local: one entry per road wheel, in the chassis's own
	// local space -- negative X = left track, positive X = right track (see
	// configure_vehicle_track()'s own doc comment). Must be at least 2 and no
	// more than VehicleTrack::MAX_WHEELS entries.
	bool initialize(RID p_space, const Vector3 &p_position, const PackedVector3Array &p_wheel_positions_local);

	// p_left_ratio/p_right_ratio in [-1,1] (sign = direction, magnitude =
	// speed), p_brake in [0,1] -- see VehicleTrack::setDriverInput()'s own
	// doc comment. Does not call PxScene::simulate().
	void step(real_t p_dt, real_t p_left_ratio, real_t p_right_ratio, real_t p_brake);

	Vector3 get_position() const;
	Vector3 get_linear_velocity() const;
	Vector3 get_angular_velocity() const;
	real_t get_forward_speed() const;
	Vector3 get_up() const;
	Vector3 get_forward() const;

	real_t get_wheel_jounce(int p_wheel) const;
	real_t get_wheel_separation(int p_wheel) const;
	Vector3 get_actor_position() const;

	PhysXTankProbe();
	~PhysXTankProbe();

private:
	struct Impl;
	Impl *impl = nullptr;
};

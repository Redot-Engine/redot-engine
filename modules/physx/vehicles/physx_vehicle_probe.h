/**************************************************************************/
/*  physx_vehicle_probe.h                                                 */
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
#include "core/object/ref_counted.h"
#include "core/templates/rid.h"

// Runtime-bridge MVP probe for PxVehicle2 (PhysX 5's component-based vehicle
// SDK), not a real node -- exists purely so a headless GDScript test can drive
// a real 4-wheel direct-drive vehicle end to end, same precedent as
// GodotPhysXBlastProbe from the Blast work: prove the mechanism cheaply
// before building the real PhysXVehicle3D node. Owns its own PxRigidDynamic
// chassis directly (via PxVehiclePhysXActorCreate), not a PhysXBody3D --
// PxVehicle2's PxVehiclePhysXActorEndComponent writes wheel-shape local poses
// and rigid-body momentum straight onto the actor every tick, which needs raw
// actor/shape pointers, not the generic PhysicsServer3D RID abstraction.
class PhysXVehicleProbe : public RefCounted {
	GDCLASS(PhysXVehicleProbe, RefCounted);

protected:
	static void _bind_methods();

public:
	// p_space: a PhysicsServer3D space RID already running on the PhysX
	// backend (e.g. get_world_3d().space) -- the probe's chassis/wheels are
	// added to that space's real PxScene.
	// p_wheel_radius <= 0 uses Vehicle4WWheelConfig's own default (0.35).
	bool initialize(RID p_space, const Vector3 &p_position, real_t p_wheel_radius = -1.0);

	// p_throttle/p_brake in [0,1], p_steer in [-1,1]. Runs the vehicle's own
	// PxVehicleComponentSequence for this tick; does NOT call
	// PxScene::simulate() -- the caller's normal physics step still does that,
	// same real ordering PxVehicle2's own snippets use (vehicle.step() writes
	// forces/poses onto the actor, then the space's own simulate() picks them
	// up alongside every other body).
	void step(real_t p_dt, real_t p_throttle, real_t p_brake, real_t p_steer);

	Vector3 get_position() const;
	Vector3 get_linear_velocity() const;
	real_t get_forward_speed() const;

	// Diagnostics for suspension tuning (0=FL, 1=FR, 2=RL, 3=RR). jounce is
	// how compressed the suspension is right now, in [0, suspension_travel]
	// (0 = max droop, suspension_travel = fully bottomed out). separation < 0
	// means the wheel is penetrating the ground even at max compression --
	// the suspension travel isn't enough to reach the ground at all.
	real_t get_wheel_jounce(int p_wheel) const;
	real_t get_wheel_separation(int p_wheel) const;

	// Raw PxRigidDynamic::getGlobalPose() -- the actor's own origin, NOT
	// rigidBodyState.pose (which may be CoM-relative; used to check this
	// directly rather than re-deriving the frame convention by hand).
	Vector3 get_actor_position() const;

	// World-space wheel HUB CENTER (actor pose composed with
	// wheelLocalPoses[i].localPose) -- for directly measuring ground
	// clearance (wheel_position.y - radius) instead of re-deriving the
	// suspension attachment/jounce math by hand.
	Vector3 get_wheel_position(int p_wheel) const;

	PhysXVehicleProbe();
	~PhysXVehicleProbe();

private:
	// Opaque pointer to the actual PxVehicle2 composition (kept out of this
	// header so nothing outside godot_physx_vehicle_probe.cpp needs
	// <PxPhysicsAPI.h>/vehicle/PxVehicleAPI.h).
	struct Impl;
	Impl *impl = nullptr;
};

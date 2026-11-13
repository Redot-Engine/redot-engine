/**************************************************************************/
/*  physx_vehicle_shared.h                                                */
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
 * @file physx_vehicle_shared.h
 * @brief Composition helpers shared by BOTH vehicle stacks (the server-RID
 *        assembly in physx_vehicle_server.cpp and the node-level PxVehicle2
 *        compositions in physx_vehicle4w/2w/track.h).
 *
 * This exists to keep the two stacks' vehicle conventions from drifting again:
 * the left-handed-frame regression (VEHN-2) happened exactly because the 4W
 * composition fixed its frame while 2W/track kept their own copies. Anything
 * here must stay a pure inline helper — no Godot node types, no state.
 *
 * Conventions (Godot, matching physx_conversions.h's 1:1 axis mapping):
 *   forward = -Z, right = +X, up = +Y.
 */

#ifndef PHYSX_VEHICLE_SHARED_H
#define PHYSX_VEHICLE_SHARED_H

#include "core/math/vector3.h"

#include <PxPhysicsAPI.h>
#include <vehicle/PxVehicleAPI.h>

// ---------------------------------------------------------------------------
// Frame: Godot's forward = -Z requires the RIGHT-handed SDK triple
// (lng x lat = vrt): lngAxis = eNegZ, latAxis = eNegX, vrtAxis = ePosY.
// latAxis = ePosX makes a left-handed triple — PxVehicleFrame::isValid()
// builds a quaternion from the basis and a mirrored basis has determinant -1,
// which is not a unit rotation, so the frame is illegal (checked builds warn;
// unchecked builds run with mirrored drive/lateral behavior).
// ---------------------------------------------------------------------------
inline physx::PxVehicleFrame physx_vehicle_frame() {
	physx::PxVehicleFrame f;
	f.setToDefault();
	f.lngAxis = physx::PxVehicleAxes::eNegZ;
	f.latAxis = physx::PxVehicleAxes::eNegX;
	f.vrtAxis = physx::PxVehicleAxes::ePosY;
	return f;
}

// Simulation-context gravity from the owning scene (VEHN-5 / VEH-5: the
// hardcoded -9.81 made custom scene gravity wrong on the node stack; the
// server path already read the scene).
inline physx::PxVec3 physx_vehicle_scene_gravity(const physx::PxScene &p_scene, const physx::PxVehicleFrame &p_frame) {
	const physx::PxVec3 g = p_scene.getGravity();
	return p_frame.getVrtAxis() * -g.magnitude();
}

// ---------------------------------------------------------------------------
// Command clamps (VEHN-7): Godot exposes [0,1] throttle/brake and [-1,1]
// steer/ratio conventions; out-of-range script values otherwise produce
// proportionally out-of-range torque through the linear response model.
// ---------------------------------------------------------------------------
inline physx::PxReal physx_vehicle_clamp01(real_t p_v) {
	return (physx::PxReal)CLAMP(p_v, 0.0, 1.0);
}

inline physx::PxReal physx_vehicle_clamp_sym(real_t p_v) {
	return (physx::PxReal)CLAMP(p_v, -1.0, 1.0);
}

// Explicit chassis gravity-off (VEHN-4): PxVehicle2 applies gravity itself
// through the simulation context (PxVehicleRigidBodyComponent), so the chassis
// actor must have scene gravity disabled or it is applied twice. The node
// stack relied on PxVehiclePhysXActorCreate doing this implicitly — set it
// explicitly so an SDK behavior change cannot silently double-apply gravity.
inline void physx_vehicle_chassis_gravity_off(physx::PxRigidActor &p_actor) {
	p_actor.setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
}

#endif // PHYSX_VEHICLE_SHARED_H

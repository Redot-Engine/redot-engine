/**************************************************************************/
/*  physx_box_shape_3d.cpp                                                */
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

#include "physx_box_shape_3d.h"

PhysXBoxShape3D::PhysXBoxShape3D() {
	// Half-extents of Godot's default (1,1,1) box; set_data documents the
	// half-extents server contract.
	half_extents = Vector3(0.5, 0.5, 0.5);
}

void PhysXBoxShape3D::set_data(const Variant &p_data) {
	// SHAPE_BOX data is HALF-EXTENTS, per the server contract: core
	// BoxShape3D::set_size passes size / 2 to the physics server, and both
	// reference backends (godot_physics_3d's GodotBoxShape3D and
	// jolt_physics_3d) interpret the data as half-extents. Treating it as
	// full size and halving it here shrinks every box to a quarter of its
	// volume (a probe measured boxes resting at exactly half height
	// and a 2 m cube reporting a 1 m cube's inertia).
	const Vector3 new_half_extents = Vector3(p_data);
	// Negative half-extents are invalid data — rejected before they are stored
	// (zero is a degenerate-but-tolerated case: the geometry clamp keeps it a
	// hair-thin slab instead of crashing).
	ERR_FAIL_COND_MSG(new_half_extents.x < 0.0f || new_half_extents.y < 0.0f || new_half_extents.z < 0.0f,
			"PhysX box shape half-extents must be >= 0.");
	half_extents = new_half_extents;
	_notify_shape_changed();
}

Variant PhysXBoxShape3D::get_data() const {
	// Return the half-extents as stored (mirrors set_data and the contract).
	return half_extents;
}

AABB PhysXBoxShape3D::get_aabb() const {
	return AABB(-half_extents, half_extents * 2.0);
}

bool PhysXBoxShape3D::get_physx_geometry(physx::PxGeometryHolder &p_geometry_holder, const physx::PxVec3 &p_scale) const {
	// Calculate final scaled half-extents
	physx::PxVec3 scaled_half_extents;
	scaled_half_extents.x = half_extents.x * p_scale.x;
	scaled_half_extents.y = half_extents.y * p_scale.y;
	scaled_half_extents.z = half_extents.z * p_scale.z;

	// Safety: PhysX doesn't like zero or negative dimensions
	// Using a tiny epsilon ensures the physics engine doesn't crash on flat boxes
	const float EPSILON = 0.0001f;
	scaled_half_extents.x = MAX(scaled_half_extents.x, EPSILON);
	scaled_half_extents.y = MAX(scaled_half_extents.y, EPSILON);
	scaled_half_extents.z = MAX(scaled_half_extents.z, EPSILON);

	// Store in the holder — scaled_half_extents is already half the full size,
	// matching PxBoxGeometry's half-extent expectation.
	p_geometry_holder.storeAny(physx::PxBoxGeometry(scaled_half_extents));

	return true;
}

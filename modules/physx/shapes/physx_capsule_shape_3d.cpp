/**************************************************************************/
/*  physx_capsule_shape_3d.cpp                                            */
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

#include "physx_capsule_shape_3d.h"

void PhysXCapsuleShape3D::set_data(const Variant &p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_height = data.get("height", Variant());
	ERR_FAIL_COND(maybe_height.get_type() != Variant::FLOAT);

	const Variant maybe_radius = data.get("radius", Variant());
	ERR_FAIL_COND(maybe_radius.get_type() != Variant::FLOAT);

	height = maybe_height;
	radius = maybe_radius;

	// PhysX requires halfCylinderHeight >= 0; the geometry clamps a capsule
	// whose height is below 2*radius into a sphere. Godot's node clamps this
	// at the resource level, but raw server data can arrive unclamped — say so
	// instead of silently changing the shape.
	if (height < radius * 2.0f) {
		WARN_PRINT_ONCE("PhysX: capsule height < 2*radius is clamped to a sphere (Godot's resource layer normally prevents this).");
	}

	_notify_shape_changed();
}

Variant PhysXCapsuleShape3D::get_data() const {
	Dictionary data;
	data["height"] = height;
	data["radius"] = radius;
	return data;
}

AABB PhysXCapsuleShape3D::get_aabb() const {
	// Capsule is aligned to the Y axis in Godot
	const Vector3 half_extents(radius, height / 2.0f, radius);
	return AABB(-half_extents, half_extents * 2.0f);
}

physx::PxTransform PhysXCapsuleShape3D::get_local_pose(const physx::PxVec3 &p_scale) const {
	// Rotate PhysX's local X-axis to match Redot's Y-axis
	physx::PxQuat rot_z_90(physx::PxHalfPi, physx::PxVec3(0.0f, 0.0f, 1.0f));
	return physx::PxTransform(physx::PxVec3(0.0f), rot_z_90);
}

bool PhysXCapsuleShape3D::get_physx_geometry(physx::PxGeometryHolder &p_geometry_holder, const physx::PxVec3 &p_scale) const {
	// PhysX doesn't support non-uniform scaling on capsules.
	// We extract a uniform max scale just like the sphere implementation.
	float max_scale = physx::PxMax(physx::PxAbs(p_scale.x), physx::PxMax(physx::PxAbs(p_scale.y), physx::PxAbs(p_scale.z)));

	float scaled_radius = radius * max_scale;
	float scaled_height = height * max_scale;

	// PhysX expects the half-height of the *cylindrical* part, not the whole capsule.
	// Formula: (Total Height / 2) - Radius
	float half_cylinder_height = (scaled_height / 2.0f) - scaled_radius;

	// Failsafe: if height < radius * 2, PhysX will assert/crash. Clamp it.
	if (half_cylinder_height < 0.0f) {
		half_cylinder_height = 0.0f;
	}

	physx::PxCapsuleGeometry capsule_geometry(scaled_radius, half_cylinder_height);
	p_geometry_holder.storeAny(capsule_geometry);

	return true;
}

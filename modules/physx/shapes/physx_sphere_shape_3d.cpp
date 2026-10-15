/**************************************************************************/
/*  physx_sphere_shape_3d.cpp                                             */
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

#include "physx_sphere_shape_3d.h"

void PhysXSphereShape3D::set_data(const Variant &p_data) {
	// Godot sends the radius as a simple float for SHAPE_SPHERE. PxSphereGeometry
	// requires radius > 0 — reject invalid data here instead of failing late at
	// createShape with a shape-less slot.
	ERR_FAIL_COND_MSG(p_data.get_type() != Variant::FLOAT && p_data.get_type() != Variant::INT,
			"PhysX sphere shape data must be a radius (number).");
	const float r = (float)p_data;
	ERR_FAIL_COND_MSG(r <= 0.0f || !Math::is_finite(r),
			"PhysX sphere shape radius must be finite and > 0.");
	radius = r;
	_notify_shape_changed();
}

Variant PhysXSphereShape3D::get_data() const {
	return radius;
}

AABB PhysXSphereShape3D::get_aabb() const {
	return AABB(Vector3(-radius, -radius, -radius), Vector3(radius * 2.0, radius * 2.0, radius * 2.0));
}

bool PhysXSphereShape3D::get_physx_geometry(physx::PxGeometryHolder &p_geometry_holder, const physx::PxVec3 &p_scale) const {
	// PhysX spheres are perfectly round. They do not support non-uniform scaling (ellipsoids).
	// Standard practice is to use the maximum scale component to ensure the collider
	// fully encompasses the visual mesh, or just assume uniform scaling.

	// We use PxAbs to handle negative scales (flipping) correctly.
	float max_scale = physx::PxMax(physx::PxAbs(p_scale.x), physx::PxMax(physx::PxAbs(p_scale.y), physx::PxAbs(p_scale.z)));

	// Create the geometry
	physx::PxSphereGeometry sphere_geometry(radius * max_scale);

	// Store it in the holder
	p_geometry_holder.storeAny(sphere_geometry);

	return true;
}
/**************************************************************************/
/*  physx_separation_ray_shape_3d.cpp                                     */
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

// modules/physx/shapes/physx_separation_ray_shape_3d.cpp
#include "physx_separation_ray_shape_3d.h"
#include <geometry/PxBoxGeometry.h>

void PhysXSeparationRayShape3D::set_data(const Variant &p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_length = data.get("length", Variant());
	ERR_FAIL_COND(maybe_length.get_type() != Variant::FLOAT);

	const Variant maybe_slide_on_slope = data.get("slide_on_slope", Variant());
	ERR_FAIL_COND(maybe_slide_on_slope.get_type() != Variant::BOOL);

	length = maybe_length;
	slide_on_slope = maybe_slide_on_slope;

	_notify_shape_changed();
}

Variant PhysXSeparationRayShape3D::get_data() const {
	Dictionary data;
	data["length"] = length;
	data["slide_on_slope"] = slide_on_slope;
	return data;
}

AABB PhysXSeparationRayShape3D::get_aabb() const {
	// Godot SeparationRays project down the local Z-axis
	constexpr float size_xy = 0.1f;
	constexpr float half_size_xy = size_xy * 0.5f;
	return AABB(Vector3(-half_size_xy, -half_size_xy, 0.0f), Vector3(size_xy, size_xy, length));
}

bool PhysXSeparationRayShape3D::get_physx_geometry(physx::PxGeometryHolder &holder, const physx::PxVec3 &scale) const {
	// Emulate the ray using a very thin Box Geometry.
	// A Box is defined by its half-extents in PhysX. The length scales with
	// the body's Z scale; the cross-section scales with X/Y so a scaled body's
	// ray probes a correspondingly scaled volume.
	float half_length = (length * scale.z) * 0.5f;
	float thickness = 0.01f;

	holder.storeAny(physx::PxBoxGeometry(thickness * scale.x, thickness * scale.y, half_length));

	return true;
}

physx::PxTransform PhysXSeparationRayShape3D::get_local_pose(const physx::PxVec3 &p_scale) const {
	// Godot's separation-ray convention (gizmo, motion-query ray bases) is
	// that the shape extends [0, +length] along +Z from the shape origin. The
	// emulated box is centered on its local pose, so offset it forward by half
	// the SCALED length — a centered volume stuck half a ray out the back of
	// the body, and query/motion code had to un-center it everywhere.
	return physx::PxTransform(physx::PxVec3(0.0f, 0.0f, (length * p_scale.z) * 0.5f));
}

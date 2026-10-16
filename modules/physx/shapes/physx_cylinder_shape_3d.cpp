/**************************************************************************/
/*  physx_cylinder_shape_3d.cpp                                           */
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

#include "physx_cylinder_shape_3d.h"

PhysXCylinderShape3D::PhysXCylinderShape3D() {}

PhysXCylinderShape3D::~PhysXCylinderShape3D() {}

void PhysXCylinderShape3D::set_data(const Variant &p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);
	Dictionary d = p_data;
	// PxCustomGeometryExt::CylinderCallbacks requires positive dimensions —
	// validate here (missing keys / non-positive values) instead of failing
	// late at createShape with a shape-less slot.
	ERR_FAIL_COND(!d.has("radius") || !d.has("height"));
	ERR_FAIL_COND_MSG((float)d["radius"] <= 0.0f || (float)d["height"] <= 0.0f,
			"PhysX cylinder shape radius and height must be > 0.");
	radius = d["radius"];
	height = d["height"];

	// Push the new params into every live per-scale instance.
	_refresh_instances();

	_notify_shape_changed();
}

Variant PhysXCylinderShape3D::get_data() const {
	Dictionary data;
	data["height"] = height;
	data["radius"] = radius;
	return data;
}

AABB PhysXCylinderShape3D::get_aabb() const {
	return AABB(
			Vector3(-radius, -height * 0.5f, -radius),
			Vector3(radius * 2, height, radius * 2));
}

physx::PxCustomGeometryExt::CylinderCallbacks *PhysXCylinderShape3D::_create_callbacks() const {
	return new physx::PxCustomGeometryExt::CylinderCallbacks(height, radius, 1);
}

void PhysXCylinderShape3D::_apply_scale_to_callbacks(physx::PxCustomGeometryExt::CylinderCallbacks &cb, const physx::PxVec3 &scale) const {
	float scaled_radius = radius * physx::PxMax(physx::PxAbs(scale.x), physx::PxAbs(scale.z));
	float scaled_height = height * physx::PxAbs(scale.y);

	cb.setRadius(scaled_radius);
	cb.setHeight(scaled_height);
}

void PhysXCylinderShape3D::_apply_params_to_callbacks(physx::PxCustomGeometryExt::CylinderCallbacks &cb) const {
	cb.setRadius(radius);
	cb.setHeight(height);
}

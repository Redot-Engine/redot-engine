/**************************************************************************/
/*  physx_custom_shape_type.cpp                                           */
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

#include "physx_custom_shape_type.h"

#include "core/templates/local_vector.h"
#include "physx_cone_shape_3d.h" // Register new custom shapes here

PhysXCustomShapeType::PhysXCustomShapeType() {}

PhysXCustomShapeType::~PhysXCustomShapeType() {
	// Destructor BODY, not the base destructor: virtual dispatch still reaches
	// the override here, so owners release their PxShapes while the inner
	// shape (and its callbacks) are alive.
	detach_from_owners();
}

void PhysXCustomShapeType::detach_from_owners() {
	// Snapshot the owners: detach_shape() calls back into remove_owner(),
	// which mutates the map being iterated.
	LocalVector<PhysXShapedObject3D *> snapshot;
	{
		MutexLock lock(owners_mutex);
		for (const KeyValue<PhysXShapedObject3D *, int> &E : owners) {
			snapshot.push_back(E.key);
		}
	}
	for (PhysXShapedObject3D *owner : snapshot) {
		owner->detach_shape(this);
	}
}

// Define the static registry
HashMap<StringName, PhysXCustomShapeType::FactoryFunc> PhysXCustomShapeType::shape_factories;

void PhysXCustomShapeType::register_builtin_shapes() {
	if (!shape_factories.has("cone")) {
		shape_factories["cone"] = []() -> std::unique_ptr<PhysXShape3D, void (*)(PhysXShape3D *)> {
			return std::unique_ptr<PhysXShape3D, void (*)(PhysXShape3D *)>(memnew(PhysXConeShape3D), memdelete_shape);
		};
	}
}

bool PhysXCustomShapeType::is_convex() const {
	if (internal_shape) {
		return internal_shape->is_convex();
	}
	return false;
}

void PhysXCustomShapeType::set_data(const Variant &p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);
	Dictionary d = p_data;

	// Default to empty StringName if not found
	StringName new_type = d.get("type", StringName());

	if (new_type != current_type) {
		current_type = new_type;

		// Instant factory dispatch
		if (shape_factories.has(current_type)) {
			internal_shape = shape_factories[current_type]();
		} else {
			internal_shape.reset();
		}
	}

	if (internal_shape) {
		// Propagate the wrapper's identity/margin policy to the inner shape so
		// it cannot silently bypass wrapper-level settings (margin drives the
		// attached PxShape's contact offset via the wrapper's create_shape,
		// but the inner RID must not be an empty RID for reverse lookups).
		internal_shape->set_rid(get_rid());
		internal_shape->set_margin(margin);
		internal_shape->set_data(p_data);
	}

	_notify_shape_changed();
}

void PhysXCustomShapeType::set_margin(float p_margin) {
	PhysXShape3D::set_margin(p_margin);
	if (internal_shape) {
		// Keep the inner shape's margin in sync (the wrapper's margin governs
		// the attached PxShape; the sync removes the divergence trap if any
		// inner-shape path ever reads it).
		internal_shape->set_margin(p_margin);
	}
}

Variant PhysXCustomShapeType::get_data() const {
	if (internal_shape) {
		Dictionary data = internal_shape->get_data();
		data["type"] = current_type; // Cleanly inserts the StringName
		return data;
	}

	Dictionary empty_data;
	empty_data["type"] = StringName(); // Represents our "Unknown" state
	return empty_data;
}

AABB PhysXCustomShapeType::get_aabb() const {
	if (internal_shape) {
		return internal_shape->get_aabb();
	}
	return AABB();
}

bool PhysXCustomShapeType::get_physx_geometry(physx::PxGeometryHolder &holder, const physx::PxVec3 &scale) const {
	if (internal_shape) {
		return internal_shape->get_physx_geometry(holder, scale);
	}
	return false;
}

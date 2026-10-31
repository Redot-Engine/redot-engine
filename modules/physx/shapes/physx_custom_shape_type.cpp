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
	// detach_from_owners() ends with _purge_retiring(); anything it left
	// parked cannot be referenced anymore (every owner PxShape is detached),
	// and the member destructors (internal_shape, retiring leftovers) must
	// not leak it.
	for (PhysXShape3D *shape : retiring_inner_shapes) {
		memdelete(shape);
	}
	retiring_inner_shapes.clear();
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
	// The owners' PxShape instances are gone: parked inner shapes' callbacks
	// have no possible reference left.
	_purge_retiring();
}

// Destroys every parked inner shape no attached PxShape still embeds
// callbacks from. The reference count goes through the WRAPPER's owners
// (owners register here, not on the inner shape). The wrapper owns the
// parked pointers exclusively.
void PhysXCustomShapeType::_purge_retiring() {
	for (uint32_t i = 0; i < retiring_inner_shapes.size();) {
		PhysXShape3D *shape = retiring_inner_shapes[i];
		if (shape && shape->has_referenced_geometry_instances()) {
			i++;
			continue;
		}
		if (shape) {
			memdelete(shape);
		}
		retiring_inner_shapes.remove_at(i);
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
		_purge_retiring(); // owners may have drained since the last switch
		// SHAPE-2: the owners' attached PxShapes embed the current inner
		// shape's PxCustomGeometry callbacks. Park the inner shape instead of
		// destroying it here: its own owners map is empty (owners register on
		// this wrapper), so destroying it would free callbacks that those
		// PxShapes still reference (use-after-free on the next collision or
		// query, and _notify_shape_changed() below only rebinds them
		// afterwards -- never for an unknown new type, which has no geometry
		// to rebind to at all).
		InnerShapePtr old_shape = std::move(internal_shape);
		current_type = new_type;

		// Instant factory dispatch
		if (shape_factories.has(current_type)) {
			internal_shape = shape_factories[current_type]();
		} else {
			internal_shape.reset();
		}
		if (old_shape) {
			retiring_inner_shapes.push_back(old_shape.release());
		}
	}

	if (internal_shape) {
		// Propagate the wrapper's identity/margin policy to the inner shape so
		// it cannot silently bypass wrapper-level settings (margin drives the
		// attached PxShape's contact offset via the wrapper's create_shape,
		// but the inner RID must not be an empty RID for reverse lookups).
		internal_shape->set_rid(get_rid());
		// Callback liveness is decided by THIS wrapper's owners (see
		// set_owners_source).
		internal_shape->set_owners_source(this);
		internal_shape->set_margin(margin);
		internal_shape->set_data(p_data);
	}

	_notify_shape_changed();

	// The notify above re-created every attached PxShape's geometry from the
	// current inner shape: a valid new type re-embedded fresh callbacks into
	// all of them, so parked inner shapes from this or earlier switches may
	// be unreferenced now. For an unknown new type nothing was rebound and
	// the parked shapes stay until the owners detach (see detach_from_owners)
	// or this wrapper dies.
	_purge_retiring();
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

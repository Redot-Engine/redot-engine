/**************************************************************************/
/*  physx_shape_3d.cpp                                                    */
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

/**
 * @file physx_shape_3d.cpp
 * @brief Shared shape resource implementation (owner tracking, geometry creation).
 */

#include "physx_shape_3d.h"

#include "objects/physx_shaped_object_3d.h"
#include "physx_server.h"

// PhysX
#include "PxMaterial.h"
#include "PxPhysics.h"
#include "PxShape.h"
#include "geometry/PxBoxGeometry.h"
#include "geometry/PxCapsuleGeometry.h"
#include "geometry/PxConvexMeshGeometry.h"
#include "geometry/PxGeometryHelpers.h"
#include "geometry/PxHeightFieldGeometry.h"
#include "geometry/PxPlaneGeometry.h"
#include "geometry/PxSphereGeometry.h"

// ---------------------------------------------------------------------------
// Owner tracking — shapes are shared; we need to notify all owners on change.
// ---------------------------------------------------------------------------

void PhysXShape3D::add_owner(PhysXShapedObject3D *p_owner) {
	MutexLock lock(owners_mutex);
	if (owners.has(p_owner)) {
		owners[p_owner]++;
	} else {
		owners[p_owner] = 1;
	}
}

void PhysXShape3D::remove_owner(PhysXShapedObject3D *p_owner) {
	MutexLock lock(owners_mutex);
	if (owners.has(p_owner)) {
		owners[p_owner]--;
		if (owners[p_owner] <= 0) {
			owners.erase(p_owner);
		}
	}
}

bool PhysXShape3D::has_owner(PhysXShapedObject3D *p_owner) const {
	MutexLock lock(owners_mutex);
	return owners.has(p_owner);
}

int PhysXShape3D::count_owner_shape_references(const void *p_callbacks) const {
	int refs = 0;
	MutexLock lock(owners_mutex);
	for (const KeyValue<PhysXShapedObject3D *, int> &E : owners) {
		refs += E.key->count_shapes_with_callback(p_callbacks);
	}
	return refs;
}

void PhysXShape3D::set_margin(float p_margin) {
	margin = p_margin;
	_notify_shape_changed();
}

void PhysXShape3D::_notify_shape_changed() {
	MutexLock lock(owners_mutex);
	for (const KeyValue<PhysXShapedObject3D *, int> &E : owners) {
		// Notify each owning body/area that this shape's geometry changed,
		// so they can update their attached PxShape instances.
		E.key->shape_changed(this);
	}
}

void PhysXShape3D::detach_from_owners() {
	// Default: no-op. Derived classes (custom-geometry shapes) override to
	// detach their PxShape from owners before their callbacks are destroyed.
}

// ---------------------------------------------------------------------------
// Destructor — nullify shape pointers in all owners before destruction.
//
// When this shape is freed, every body/area that references it still holds
// a raw pointer in AttachedShape::shape_blueprint. If the body is destroyed
// after the shape, its destructor calls remove_owner() on the freed pointer
// → use-after-free. Nullifying the pointer prevents that.
// ---------------------------------------------------------------------------

PhysXShape3D::~PhysXShape3D() {
	// Detach PxShape instances from owners before callbacks are destroyed.
	// For custom-geometry shapes (cylinder/cone), the PxShape holds a
	// PxCustomGeometry whose callback pointer would dangle if the PxShape
	// remains attached after the callback is destroyed.
	detach_from_owners();

	MutexLock lock(owners_mutex);
	for (const KeyValue<PhysXShapedObject3D *, int> &E : owners) {
		E.key->nullify_shape(this);
	}
}

// ---------------------------------------------------------------------------
// PxShape creation — produces a per-actor instance from this shape_blueprint.
// ---------------------------------------------------------------------------

physx::PxShape *PhysXShape3D::create_shape(physx::PxPhysics &p_physics, const physx::PxVec3 &p_scale, const physx::PxMaterial *p_material, physx::PxShapeFlags p_flags) {
	// Generate the concrete geometry (box, sphere, capsule, etc.).
	physx::PxGeometryHolder geom_holder;
	if (!get_physx_geometry(geom_holder, p_scale)) {
		ERR_PRINT("PhysX: Failed to generate geometry for shape.");
		return nullptr;
	}
	// Handle Material (use server default if null)
	const physx::PxMaterial *mat = p_material;
	if (!mat) {
		mat = PhysXServer3D::get_singleton()->get_default_material();
		if (!mat) {
			ERR_PRINT("PhysX: No default material available; server not initialized.");
			return nullptr;
		}
	}

	// Create Shape
	// In PhysX 5, createShape is thread-safe
	physx::PxShape *shape = p_physics.createShape(geom_holder.any(), *mat, true, p_flags);

	if (!shape) {
		ERR_PRINT("PhysX: createShape failed.");
		return nullptr;
	}

	// Apply the coordinate alignment for Capsules/Planes (scale-dependent for
	// heightfields / separation rays — the attach-time scale is the same one
	// the geometry received).
	shape->setLocalPose(get_local_pose(p_scale));

	// Apply the contact offset (from the margin) and store a back-pointer
	// for reverse lookups in query results and simulation callbacks.
	shape->setContactOffset(margin);
	shape->userData = this;

	return shape;
}

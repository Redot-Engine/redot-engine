/**************************************************************************/
/*  physx_custom_geometry_callback.h                                      */
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
 * @file physx_custom_geometry_callback.h
 * @brief Template base for shapes using PhysX's PxCustomGeometry system.
 *
 * PhysX 5.x supports PxCustomGeometry, which delegates collision and query
 * operations to a user-provided callback object (PxCustomGeometryExt::Callbacks).
 * This template provides RAII management of those callbacks: it creates one
 * instance per distinct (quantized) scale, and releases every instance once no
 * owner holds a live PxShape referencing them.
 *
 * Concrete subclasses (e.g. the cylinder shape) specialize this template with
 * their specific callback type and implement _create_callbacks() /
 * _apply_scale_to_callbacks().
 *
 * LIFETIME CONTRACT (SHAPE-2): the callbacks are owned exclusively by this
 * shape; a live PxCustomGeometry embedded in an attached PxShape points at
 * them. The DESTRUCTOR calls detach_from_owners() from the most-derived
 * destructor body — where virtual dispatch still reaches this override — so
 * every owner's PxShape is released while the callbacks are alive. (Calling it
 * from the BASE destructor cannot work: by then the override is unreachable
 * and the callbacks are already freed.) After detachment releases all owner
 * shapes, no PxCustomGeometry references the instances and they are freed.
 *
 * GPU DYNAMICS NOTE (verified — do not re-investigate without cause):
 * PhysX documentation historically described PxCustomGeometry as unsupported
 * by the GPU pipeline, suggesting bodies with custom geometry could lose
 * collision in GODOT_PHYSX_GPU scenes. Tested against the vendored SDK with
 * GPU dynamics active (CUDA device present, scene with eENABLE_GPU_DYNAMICS +
 * eGPU broadphase): a dynamic cylinder and a dynamic cone both fall, contact,
 * and rest at their exact half-heights on a static floor — PhysX generates
 * such pairs' contacts on the CPU while they remain in the GPU broadphase.
 * The suite's shape-family pair matrix (PHYSX-SHAPE-P-*) guards this
 * permanently. If custom-geometry contact cost ever matters in GPU scenes,
 * cooking a convex approximation is the available optimization — a behavior
 * change (faceted silhouette), not a correctness fix.
 */

#ifndef PHYSX_CUSTOM_GEOMETRY_CALLBACK_H
#define PHYSX_CUSTOM_GEOMETRY_CALLBACK_H

#include "../objects/physx_shaped_object_3d.h"
#include "physx_shape_3d.h"
#include <extensions/PxCustomGeometryExt.h>

#include "core/templates/local_vector.h"

template <typename CallbackT>
class PhysXCustomGeometryCallback : public PhysXShape3D {
public:
	PhysXCustomGeometryCallback() :
			PhysXShape3D() {}

	virtual ~PhysXCustomGeometryCallback() override {
		// Most-derived destructor body: dynamic type is the concrete class, so
		// detach_from_owners() dispatches to the override below (the base
		// destructor's virtual call could not). PxShapes are released while
		// the callbacks are alive; the instances go with _release_geometry().
		detach_from_owners();
		_release_geometry();
	}

	virtual bool get_physx_geometry(physx::PxGeometryHolder &holder, const physx::PxVec3 &scale) const override {
		// One callback instance per distinct (quantized) scale: the shape
		// shape_blueprint is shared across bodies, and a single shared callback
		// mutated per request made two differently-scaled owners fight over
		// its radius/height (last write won for both). Each PxShape holds a
		// PxCustomGeometry bound to its own scale's instance.
		CallbackT *cb = _callback_for_scale(scale);
		if (!cb) {
			return false;
		}
		holder.storeAny(physx::PxCustomGeometry(*cb));
		return true;
	}

protected:
	// One heap instance per distinct quantized scale, alive while any owner's
	// attached PxShape references it (released when the last owner detaches).
	mutable LocalVector<std::pair<physx::PxVec3, CallbackT *>> scaled_instances;

	// Subclasses return a dynamically allocated, strongly-typed callback pointer
	virtual CallbackT *_create_callbacks() const = 0;
	virtual void _apply_scale_to_callbacks(CallbackT &cb, const physx::PxVec3 &scale) const = 0;
	// Unscaled params (radius/height) from the current data members.
	virtual void _apply_params_to_callbacks(CallbackT &cb) const = 0;

	// Quantize scale components to a 1e-4 grid for the cache key: float-exact
	// lookup recomputed an instance for every float jitter of an animated
	// node scale; the quantized key reuses the instance across sub-0.1 um
	// changes (geometry differences at that grid are far below PhysX
	// tolerances).
	static physx::PxVec3 _quantize_scale(const physx::PxVec3 &p_scale) {
		return physx::PxVec3(
				Math::round(p_scale.x * 1.0e4f) * 1.0e-4f,
				Math::round(p_scale.y * 1.0e4f) * 1.0e-4f,
				Math::round(p_scale.z * 1.0e4f) * 1.0e-4f);
	}

	CallbackT *_callback_for_scale(const physx::PxVec3 &scale) const {
		const physx::PxVec3 key = _quantize_scale(scale);
		for (uint32_t i = 0; i < scaled_instances.size(); i++) {
			if (scaled_instances[i].first == key) {
				return scaled_instances[i].second;
			}
		}
		// A new quantized scale: instances no attached PxShape references
		// anymore (a scale change left them behind) would otherwise
		// accumulate for as long as the shape stays attached. The sweep
		// counts references off the owners' live PxShapes; every caller of
		// get_physx_geometry that can have dropped a reference
		// (add/remove_shape, set_shape_transform) runs ensure_synced first,
		// and a zero-reference instance cannot be part of a solve.
		_release_unreferenced_instances();
		CallbackT *cb = _create_callbacks();
		if (!cb) {
			return nullptr;
		}
		_apply_scale_to_callbacks(*cb, key);
		scaled_instances.push_back({ key, cb });
		return cb;
	}

	// Deletes every instance no owner's PxShape references (both the
	// simulation shape and the area detection shape count).
	void _release_unreferenced_instances() const {
		for (uint32_t i = 0; i < scaled_instances.size();) {
			CallbackT *cb = scaled_instances[i].second;
			int refs = 0;
			{
				MutexLock lock(owners_mutex);
				for (const KeyValue<PhysXShapedObject3D *, int> &E : owners) {
					refs += E.key->count_shapes_with_callback(cb);
				}
			}
			if (refs == 0) {
				delete cb;
				scaled_instances.remove_at(i);
			} else {
				i++;
			}
		}
	}

	// Re-push the current data (then each instance's own scale) into every
	// live instance -- called by derived set_data() implementations.
	void _refresh_instances() const {
		for (uint32_t i = 0; i < scaled_instances.size(); i++) {
			_apply_scale_to_callbacks(*scaled_instances[i].second, scaled_instances[i].first);
		}
	}

	void _release_geometry() const {
		for (uint32_t i = 0; i < scaled_instances.size(); i++) {
			delete scaled_instances[i].second;
		}
		scaled_instances.clear();
	}

	// PhysXShape3D interface — detach PxShape from owners before the callbacks
	// are destroyed (see the lifetime contract in the file header). After the
	// last owner detaches, no live PxCustomGeometry references the instances
	// and they are released.
	virtual void detach_from_owners() override {
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

		{
			MutexLock lock(owners_mutex);
			if (owners.is_empty()) {
				_release_geometry();
			}
		}
	}
};
#endif // PHYSX_CUSTOM_GEOMETRY_CALLBACK_H

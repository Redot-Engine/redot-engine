/**************************************************************************/
/*  physx_filter_shader.h                                                 */
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

#include "PxFiltering.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include <atomic>

// PxFilterData word conventions for this module:
//   word0 = collision_layer   (Godot layer bits, "I belong to these layers")
//   word1 = collision_mask    (Godot mask bits, "I collide with these layers")
//   word2 = unused (reserved for future use, e.g. group indices)
//   word3 = module flags:
//           bit 0 (0x1): contact notification requested
//                        (set when body has max_contacts_reported > 0)
//           bit 1 (0x2): shape belongs to an area (trigger or detection
//                        shape; areas must never generate solver contacts)
#define PHYSX_FILTER_FLAG_CONTACT_NOTIFY 0x1u
#define PHYSX_FILTER_FLAG_IS_AREA 0x2u

// Module-global flag: when true (debug-contacts enabled on any space), the
// simulation filter shader requests contact-point notifications for every
// colliding pair so onContact can record them for the "Visible Collision
// Shapes" overlay — even when no body has max_contacts_reported > 0.
// Toggled by PhysXSpace3D::set_debug_contacts via the server.
extern std::atomic<bool> g_physx_debug_contacts_enabled;

// ---------------------------------------------------------------------------
// Collision-exception registry (rigid bodies and GPU deformables).
//
// PxFilterData.word2 carries an "exception slot" on shapes whose owner
// participates in at least one collision exception — soft-body OR rigid-body
// (0 = none). The filter shader cannot see actors/userData, so a pair where
// BOTH sides carry slots is checked against this registry in both directions;
// a match kills the pair. This is the enforcement path on GPU dynamics scenes,
// where the pair filter callback does not run; on the CPU the
// PhysXPairFilterCallback enforces the same rigid exceptions from the
// wrappers' exception sets. Verified against the vendored SDK with GPU
// dynamics active: the shader's eKILL fully controls deformable-vs-rigid
// pairs (a layer-test kill made a GPU PxDeformableVolume fall through a
// platform).
//
// Threading: written only between steps on the server thread (add/remove
// exception, body/soft-body free); read by the filter shader on PhysX workers
// during simulate(). No writes occur while the simulation runs (the server
// thread blocks in fetchResults under the synchronous step), so concurrent
// reads need no lock — the same contract as the actor userData floats.
// ---------------------------------------------------------------------------
class PhysXSoftExceptionRegistry {
public:
	bool has(uint32_t p_a_slot, uint32_t p_b_slot) const;
	void add(uint32_t p_a_slot, uint32_t p_b_slot);
	void remove(uint32_t p_a_slot, uint32_t p_b_slot);
	/// Clears every pair keyed by one participant's slot (called when a body
	/// or soft body is freed; slots are monotonic and never reused).
	void remove_soft(uint32_t p_slot);

private:
	HashMap<uint32_t, HashSet<uint32_t>> pairs; // participant slot -> paired slots
};

extern PhysXSoftExceptionRegistry g_physx_soft_exceptions;

/// Allocates a nonzero exception slot (monotonic; 0 is reserved for "no
/// slot"). Called on the server thread only.
uint32_t physx_alloc_soft_exception_slot();

// Custom simulation filter shader implementing Godot's collision semantics.
//
// Implements the Godot layer/mask test directly — an asymmetric OR:
//     collide iff (layer0 & mask1) | (layer1 & mask0) != 0
// The stock PxDefaultSimulationFilterShader cannot be used because it applies
// a symmetric AND plus reserved-bit conventions that conflict with this
// module's word0=layer / word1=mask packing.
//
// Also: marks trigger pairs for eTRIGGER_DEFAULT, and requests contact-point
// notifications (eNOTIFY_TOUCH_FOUND/LOST + eNOTIFY_CONTACT_POINTS) for any
// pair where at least one side carries PHYSX_FILTER_FLAG_CONTACT_NOTIFY.
//
physx::PxFilterFlags physx_simulation_filter_shader(
		physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0,
		physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
		physx::PxPairFlags &pairFlags,
		const void *constantBlock, physx::PxU32 constantBlockSize);

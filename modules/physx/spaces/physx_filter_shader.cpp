/**************************************************************************/
/*  physx_filter_shader.cpp                                               */
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

#include "physx_filter_shader.h"

#include "PxRigidActor.h"
#include "PxShape.h"

// Module-global debug-contacts flag. Read by the (stateless) filter shader on
// the simulation thread; toggled by the server when a space enables debug contacts.
std::atomic<bool> g_physx_debug_contacts_enabled{ false };

// ---------------------------------------------------------------------------
// Soft-body collision-exception registry (see the header for the contract).
// ---------------------------------------------------------------------------

uint32_t physx_alloc_soft_exception_slot() {
	static uint32_t next_slot = 0;
	return ++next_slot;
}

bool PhysXSoftExceptionRegistry::has(uint32_t p_soft_slot, uint32_t p_body_slot) const {
	const HashSet<uint32_t> *set = pairs.getptr(p_soft_slot);
	return set && set->has(p_body_slot);
}

void PhysXSoftExceptionRegistry::add(uint32_t p_soft_slot, uint32_t p_body_slot) {
	pairs[p_soft_slot].insert(p_body_slot);
}

void PhysXSoftExceptionRegistry::remove(uint32_t p_soft_slot, uint32_t p_body_slot) {
	HashSet<uint32_t> *set = pairs.getptr(p_soft_slot);
	if (set) {
		set->erase(p_body_slot);
	}
}

void PhysXSoftExceptionRegistry::remove_soft(uint32_t p_soft_slot) {
	pairs.erase(p_soft_slot);
}

PhysXSoftExceptionRegistry g_physx_soft_exceptions;

/**
 * Custom simulation filter shader implementing Godot's collision semantics.
 *
 * Godot rule (asymmetric OR): two objects collide if EITHER side wants the
 * other, i.e.
 *
 *     (layer0 & mask1) | (layer1 & mask0)   !=  0
 *
 * This is deliberately an OR, not an AND — A can detect B even when B does not
 * detect A. The stock PxDefaultSimulationFilterShader uses a symmetric AND of
 * both directions and applies reserved-bit conventions that do not match the
 * way this module packs word0=layer / word1=mask, so it cannot be used here.
 *
 * PxFilterData word layout (set by PhysXShapedObject3D::update_shapes_collision_filter):
 *   word0 = collision_layer   ("I belong to these layers")
 *   word1 = collision_mask    ("I collide with these layers")
 *   word2 = unused (reserved)
 *   word3 = module flags — bit 0 (PHYSX_FILTER_FLAG_CONTACT_NOTIFY) requests
 *           contact-point notifications for this shape.
 *
 * The shader is stateless and runs on the simulation thread; it must not touch
 * Godot objects. Pair flags are set so that:
 *   - colliding simulation shapes get solver contacts,
 *   - any pair where at least one side is a trigger gets trigger notifications,
 *   - pairs where at least one side requested contact reporting get the
 *     NOTIFY_TOUCH_* + NOTIFY_CONTACT_POINTS flags consumed by onContact,
 *   - any non-trigger pair involving an area shape is killed: monitorable
 *     areas carry a plain (non-trigger) "detection" simulation shape whose
 *     only purpose is to let another area's trigger fire against it (PhysX 5
 *     does not report trigger-vs-trigger pairs), and areas are never solid.
 *
 * NOTE: area actors are kinematic rigid bodies, not static —
 * PhysX never calls this shader for pairs of two static rigid actors, so
 * static areas could never produce area-vs-area trigger events.
 */
physx::PxFilterFlags physx_simulation_filter_shader(
		physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0,
		physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
		physx::PxPairFlags &pairFlags,
		const void *constantBlock, physx::PxU32 constantBlockSize) {
	// --- Object types (needed by both the exception gate and the GPU pair check) ---
	const physx::PxFilterObjectType::Enum obj_type0 = physx::PxGetFilterObjectType(attributes0);
	const physx::PxFilterObjectType::Enum obj_type1 = physx::PxGetFilterObjectType(attributes1);

	// --- Godot layer/mask test (asymmetric OR) ---
	const bool layer_match = (filterData0.word0 & filterData1.word1) || (filterData1.word0 & filterData0.word1);

	// --- Trigger detection (either side marked as a trigger shape) ---
	const bool is_trigger_pair = physx::PxFilterObjectIsTrigger(attributes0) || physx::PxFilterObjectIsTrigger(attributes1);

	// --- Areas never collide ---
	// A monitorable area owns a second, non-trigger simulation "detection"
	// shape so that another area's trigger shape can fire against it (PhysX 5
	// does not report trigger-vs-trigger pairs). Those detection shapes must
	// never produce solver contacts — areas are non-solid — so any pair where
	// NEITHER side is a trigger but one side is an area shape is killed.
	// (Trigger pairs involving an area are handled by the trigger branch below,
	// which still applies the layer/mask test.)
	const bool area0 = (filterData0.word3 & PHYSX_FILTER_FLAG_IS_AREA) != 0;
	const bool area1 = (filterData1.word3 & PHYSX_FILTER_FLAG_IS_AREA) != 0;
	if (!is_trigger_pair && (area0 || area1)) {
		pairFlags = physx::PxPairFlags();
		return physx::PxFilterFlag::eKILL;
	}

	// Pairs that neither match nor are triggers never interact.
	if (!layer_match && !is_trigger_pair) {
		pairFlags = physx::PxPairFlags();
		return physx::PxFilterFlag::eKILL;
	}

	// --- Collision exceptions (body-body and deformable-vs-body) ---
	// Pairs where BOTH sides carry an exception slot (PxFilterData.word2) are
	// checked against the module registry in both directions; a match kills
	// the pair. The shader cannot see actors/userData, hence the slot
	// indirection. This is the enforcement path on GPU dynamics scenes, where
	// the pair filter callback does not run; on the CPU the
	// PhysXPairFilterCallback enforces the same exceptions from the wrappers'
	// sets — both paths kill the identical pair. Slots are only assigned to
	// exception participants (monotonic, never reused), so scenes without
	// collision exceptions never reach the registry and stale entries cannot
	// produce false matches. Verified: the shader's eKILL fully controls GPU
	// PxDeformableVolume pairs.
	if (filterData0.word2 != 0 && filterData1.word2 != 0) {
		if (g_physx_soft_exceptions.has(filterData0.word2, filterData1.word2) ||
				g_physx_soft_exceptions.has(filterData1.word2, filterData0.word2)) {
			pairFlags = physx::PxPairFlags();
			return physx::PxFilterFlag::eKILL;
		}
	}

	if (is_trigger_pair) {
		// Trigger pairs do not solve; they only notify on overlap begin/end.
		// Godot's area detection is one-directional: the area (trigger) checks
		// its mask against the other side's layer. Keep the pair only when the
		// trigger side's mask matches the other side's layer; otherwise kill it.
		// Area-vs-area: the trigger side is one area's trigger shape, the other
		// side the other area's non-trigger detection shape — same one-directional
		// rule (the reverse direction is its own pair/event).
		bool trigger_match = false;
		if (physx::PxFilterObjectIsTrigger(attributes0)) {
			trigger_match = (filterData0.word1 & filterData1.word0) != 0;
		}
		if (!trigger_match && physx::PxFilterObjectIsTrigger(attributes1)) {
			trigger_match = (filterData1.word1 & filterData0.word0) != 0;
		}
		if (!trigger_match) {
			pairFlags = physx::PxPairFlags();
			return physx::PxFilterFlag::eKILL;
		}
		pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
		return physx::PxFilterFlags();
	}

	// --- Simulation (solver) pair ---
	pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;
	pairFlags |= physx::PxPairFlag::eDETECT_DISCRETE_CONTACT;
	// Route simulation pairs through the contact-modify callback so the Godot
	// material combiner (absorbent/rough → sum-clamped / min-abs) can override
	// the per-contact restitution/friction. PhysX's built-in combine modes
	// can't express Godot's "sum, clamped to [0,1]" rule.
	//
	// GPU-solved pairs — deformables (PxDeformableSurface/PxDeformableVolume)
	// and particle systems (PxPBDParticleSystem) — do not support contact
	// modification: flagging such a pair drops it entirely, so the soft body /
	// particles fall through the world. They are excluded here and always use
	// their own material's friction/damping.
	const bool gpu_solved_pair =
			obj_type0 == physx::PxFilterObjectType::eDEFORMABLE_SURFACE ||
			obj_type0 == physx::PxFilterObjectType::eDEFORMABLE_VOLUME ||
			obj_type0 == physx::PxFilterObjectType::ePARTICLESYSTEM ||
			obj_type1 == physx::PxFilterObjectType::eDEFORMABLE_SURFACE ||
			obj_type1 == physx::PxFilterObjectType::eDEFORMABLE_VOLUME ||
			obj_type1 == physx::PxFilterObjectType::ePARTICLESYSTEM;
	if (!gpu_solved_pair) {
		pairFlags |= physx::PxPairFlag::eMODIFY_CONTACTS;
	}
	// CCD pairs are only resolved if the pair flag is set; the per-body
	// eENABLE_CCD flag on the actor gates which bodies actually sweep.
	pairFlags |= physx::PxPairFlag::eDETECT_CCD_CONTACT;

	// Contact-point notifications if either side asked for them
	// (i.e. a body with max_contacts_reported > 0 is involved), OR when debug
	// contacts are globally enabled (the "Visible Collision Shapes" overlay
	// wants every contact point regardless of per-body reporting).
	const bool want_contact_notify =
			(filterData0.word3 & PHYSX_FILTER_FLAG_CONTACT_NOTIFY) ||
			(filterData1.word3 & PHYSX_FILTER_FLAG_CONTACT_NOTIFY) ||
			g_physx_debug_contacts_enabled.load(std::memory_order_relaxed);
	if (want_contact_notify) {
		pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
		pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_LOST;
		// PERSIST reports contacts every step while shapes stay in contact —
		// Godot's contact list is live while touching, not transition-only.
		pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS;
		pairFlags |= physx::PxPairFlag::eNOTIFY_CONTACT_POINTS;
	}

	// eNOTIFY routes the pair through PhysXPairFilterCallback::pairFound, the
	// CPU-side defense net for collision exceptions (which the stateless
	// shader cannot check via userData). pairFound can only kill pairs whose
	// participants carry an exception slot (word2) — the server allocates a
	// slot on every exception participant when an exception is added — so
	// request the callback only for those pairs. Scenes without exceptions
	// skip pairFound entirely; the registry check above still kills exception
	// pairs on both the CPU and GPU paths.
	if (filterData0.word2 != 0 || filterData1.word2 != 0) {
		return physx::PxFilterFlag::eNOTIFY;
	}
	return physx::PxFilterFlags();
}

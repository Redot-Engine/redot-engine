/**************************************************************************/
/*  physx_simulation_event_callback.cpp                                   */
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

#include "physx_simulation_event_callback.h"
#include "../objects/physx_area_3d.h"
#include "../objects/physx_body_3d.h"
#include "../physx_server.h"
#include "../shapes/physx_user_data.h"
#include "physx_space_3d.h"

#include "PxPhysicsAPI.h"
#include "extensions/PxRigidBodyExt.h"

// ---------------------------------------------------------------------------
// onConstraintBreak / onWake / onSleep / onAdvance
// ---------------------------------------------------------------------------

void PhysXSimulationEventCallback::onConstraintBreak(physx::PxConstraintInfo *constraints, physx::PxU32 count) {
	// Intentionally empty: Godot's joint API has no breakage support and
	// PhysXJoint3D never enables eBREAKABLE, so PhysX cannot report a broken
	// constraint here.
}

void PhysXSimulationEventCallback::onWake(physx::PxActor **actors, physx::PxU32 count) {
	// Body wake/sleep notifications could fire body state-sync callbacks here.
	// Not required for correctness; left as a no-op for now.
}

void PhysXSimulationEventCallback::onSleep(physx::PxActor **actors, physx::PxU32 count) {
	// See onWake.
}

void PhysXSimulationEventCallback::onAdvance(const physx::PxRigidBody *const *bodyBuffer, const physx::PxTransform *poseBuffer, physx::PxU32 count) {
	// Called before the solver when CCD is enabled, providing predicted poses.
	// Not needed for Godot integration.
}

// ---------------------------------------------------------------------------
// onContact — populate per-body contact buffers
// ---------------------------------------------------------------------------

// Fills one body's contact list from one side of a contact pair. The two call
// sites below are the same contact seen from each body's perspective: the
// second body's normal and impulse are negated, and the local/collider roles
// (shape indices, userData, dynamic actors) are swapped.
static void _fill_body_contact(PhysXBody3D *p_body, const physx::PxContactPairPoint &p_cp,
		bool p_second_perspective, int p_local_shape, int p_collider_shape,
		const PhysXActorUserData *p_collider_data,
		physx::PxRigidDynamic *p_local_dyn, physx::PxRigidDynamic *p_collider_dyn) {
	const float sign = p_second_perspective ? -1.0f : 1.0f;

	LocalVector<PhysXBodyContact> &contacts = p_body->get_contacts();
	PhysXBodyContact bc;
	bc.local_position = Vector3(p_cp.position.x, p_cp.position.y, p_cp.position.z);
	bc.local_normal = Vector3(sign * p_cp.normal.x, sign * p_cp.normal.y, sign * p_cp.normal.z);
	bc.impulse = Vector3(sign * p_cp.impulse.x, sign * p_cp.impulse.y, sign * p_cp.impulse.z);
	bc.local_shape = p_local_shape >= 0 ? p_local_shape : 0;
	bc.collider = p_collider_data->rid;
	bc.collider_id = p_collider_data->object_id;
	bc.collider_shape = p_collider_shape >= 0 ? p_collider_shape : 0;
	bc.collider_position = Vector3(p_cp.position.x, p_cp.position.y, p_cp.position.z);
	bc.penetration_depth = -p_cp.separation; // negative separation == penetration depth
	// Velocities at the contact point.
	if (p_collider_dyn) {
		const physx::PxVec3 v = physx::PxRigidBodyExt::getVelocityAtPos(*p_collider_dyn, p_cp.position);
		bc.collider_contact_velocity = Vector3(v.x, v.y, v.z);
	}
	if (p_local_dyn) {
		const physx::PxVec3 v = physx::PxRigidBodyExt::getVelocityAtPos(*p_local_dyn, p_cp.position);
		bc.local_contact_velocity = Vector3(v.x, v.y, v.z);
	}
	// Godot's replacement policy: at capacity, a DEEPER contact replaces the
	// shallowest stored one instead of being dropped.
	if ((int)contacts.size() < p_body->get_max_contacts_reported()) {
		contacts.push_back(bc);
	} else {
		int shallowest = 0;
		for (int ci = 1; ci < (int)contacts.size(); ci++) {
			if (contacts[ci].penetration_depth < contacts[shallowest].penetration_depth) {
				shallowest = ci;
			}
		}
		if (bc.penetration_depth > contacts[shallowest].penetration_depth) {
			contacts[shallowest] = bc;
		}
	}
}

void PhysXSimulationEventCallback::onContact(const physx::PxContactPairHeader &pairHeader, const physx::PxContactPair *pairs, physx::PxU32 nbPairs) {
	// Resolve both actors to our wrapper objects via userData.
	if (!pairHeader.actors[0] || !pairHeader.actors[1]) {
		return;
	}

	// When a pair requested eNOTIFY_TOUCH_LOST / eNOTIFY_THRESHOLD_FORCE_LOST,
	// PhysX keeps notifying it after one side is deleted and passes the now
	// dangling actor pointer with eREMOVED_ACTOR_0/1 set. The SDK contract
	// (PxContactPairHeader::actors) forbids dereferencing a deleted actor's
	// pointer, and the cross-referenced data (rid, shape index, velocity) is
	// unreadable once the wrapper is freed, so such pairs are skipped whole.
	const physx::PxU32 header_flags = pairHeader.flags;
	if (header_flags & (physx::PxU32)(physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_0 | physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_1)) {
		return;
	}

	PhysXActorUserData *a0 = static_cast<PhysXActorUserData *>(pairHeader.actors[0]->userData);
	PhysXActorUserData *a1 = static_cast<PhysXActorUserData *>(pairHeader.actors[1]->userData);
	if (!a0 || !a1) {
		return;
	}

	PhysXBody3D *body0 = (a0->object && a0->object->get_type() == PhysXObject3D::OBJECT_TYPE_BODY) ? static_cast<PhysXBody3D *>(a0->object) : nullptr;
	PhysXBody3D *body1 = (a1->object && a1->object->get_type() == PhysXObject3D::OBJECT_TYPE_BODY) ? static_cast<PhysXBody3D *>(a1->object) : nullptr;

	// Debug-contact recording is independent of per-body contact reporting:
	// "Visible Collision Shapes" wants every contact point, even when neither
	// body has max_contacts_reported > 0. Only skip the rest if there is also
	// nothing to record for debug.
	const bool debug_contacts = space && space->is_debugging_contacts();
	// Neither side is a reporting body — nothing to do unless we're debugging.
	if (!body0 && !body1 && !debug_contacts) {
		return;
	}

	// Pre-resolve the dynamic actors once per pair header for velocity lookups.
	physx::PxRigidDynamic *dyn0 = pairHeader.actors[0]->is<physx::PxRigidDynamic>();
	physx::PxRigidDynamic *dyn1 = pairHeader.actors[1]->is<physx::PxRigidDynamic>();

	for (physx::PxU32 p = 0; p < nbPairs; p++) {
		const physx::PxContactPair &pair = pairs[p];
		if (pair.flags & physx::PxContactPairFlag::eREMOVED_SHAPE_0 || pair.flags & physx::PxContactPairFlag::eREMOVED_SHAPE_1) {
			continue;
		}

		// Resolve the per-instance shape indices ONCE per pair (not per contact).
		// PxShape::userData points at the shared shape_blueprint, so the body-local
		// index must be recovered via the owning wrapper's find_shape_index().
		const int shape0_idx = body0 ? body0->find_shape_index(pair.shapes[0]) : -1;
		const int shape1_idx = body1 ? body1->find_shape_index(pair.shapes[1]) : -1;

		// Extract contact points. Limit to a reasonable batch.
		constexpr physx::PxU32 MAX_CONTACTS_PER_PAIR = 16;
		physx::PxContactPairPoint contact_points[MAX_CONTACTS_PER_PAIR];
		physx::PxU32 nb_contacts = pair.extractContacts(contact_points, MAX_CONTACTS_PER_PAIR);

		// Record debug contacts for the "Visible Collision Shapes" overlay.
		// Independent of per-body contact reporting; gated on the debug flag.
		if (debug_contacts) {
			for (physx::PxU32 c = 0; c < nb_contacts; c++) {
				space->add_debug_contact(Vector3(contact_points[c].position.x,
						contact_points[c].position.y,
						contact_points[c].position.z));
			}
		}

		for (physx::PxU32 c = 0; c < nb_contacts; c++) {
			const physx::PxContactPairPoint &cp = contact_points[c];

			// Body 0's perspective: normal points from shape[1] to shape[0]
			// (i.e., toward body0). Godot's local normal points toward self.
			if (body0 && body0->get_max_contacts_reported() > 0) {
				_fill_body_contact(body0, cp, false, shape0_idx, shape1_idx, a1, dyn0, dyn1);
			}

			// Body 1's perspective: normal is flipped.
			if (body1 && body1->get_max_contacts_reported() > 0) {
				_fill_body_contact(body1, cp, true, shape1_idx, shape0_idx, a0, dyn1, dyn0);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// onTrigger — dispatch Area3D monitor callbacks
// ---------------------------------------------------------------------------

void PhysXSimulationEventCallback::onTrigger(physx::PxTriggerPair *pairs, physx::PxU32 count) {
	for (physx::PxU32 i = 0; i < count; i++) {
		const physx::PxTriggerPair &tp = pairs[i];
		// Pairs flagged eREMOVED_SHAPE_* reference shapes/actors that were
		// removed (and possibly released) after the pair was created — their
		// pointers must not be dereferenced. Exit events for freed bodies are
		// emitted from our own bookkeeping instead (PhysXBody3D::set_space
		// and ~PhysXBody3D queue AREA_BODY_REMOVED directly).
		if (tp.flags & physx::PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER || tp.flags & physx::PxTriggerPairFlag::eREMOVED_SHAPE_OTHER) {
			continue;
		}

		// The trigger actor should be an Area; the other actor is a Body/Area.
		// Resolve both via userData.
		if (!tp.triggerActor || !tp.otherActor) {
			continue;
		}

		PhysXActorUserData *trigger_data = static_cast<PhysXActorUserData *>(tp.triggerActor->userData);
		PhysXActorUserData *other_data = static_cast<PhysXActorUserData *>(tp.otherActor->userData);
		if (!trigger_data || !other_data || !trigger_data->object) {
			continue;
		}

		// Only Areas dispatch monitor callbacks and maintain overlap lists.
		if (trigger_data->object->get_type() != PhysXObject3D::OBJECT_TYPE_AREA) {
			continue;
		}

		PhysXArea3D *area = static_cast<PhysXArea3D *>(trigger_data->object);
		const bool body_entered = (tp.status == physx::PxPairFlag::eNOTIFY_TOUCH_FOUND);
		const bool body_exited = (tp.status == physx::PxPairFlag::eNOTIFY_TOUCH_LOST);

		// Resolve shape indices from the owning wrappers' shapes[] vectors.
		// (PxShape::userData points at the shared shape_blueprint, not a per-instance
		// index, so we look the pointer up in the owner instead.)
		const int area_shape_raw = area->find_shape_index(tp.triggerShape);
		const int area_shape = area_shape_raw >= 0 ? area_shape_raw : 0;
		int other_shape = 0;
		if (other_data->object) {
			const int found = static_cast<PhysXShapedObject3D *>(other_data->object)->find_shape_index(tp.otherShape);
			if (found >= 0) {
				other_shape = found;
			}
		}

		// --- Maintain overlap lists ---
		// This bookkeeping runs in-step because the next step's gravity/damp
		// resolution reads overlapping_areas/overlapping_bodies. Only the Godot
		// monitor *callbacks* are deferred to flush_queries().
		const PhysicsServer3D::AreaBodyStatus status = (tp.status == physx::PxPairFlag::eNOTIFY_TOUCH_FOUND) ? PhysicsServer3D::AREA_BODY_ADDED : PhysicsServer3D::AREA_BODY_REMOVED;

		if (other_data->object && other_data->object->get_type() == PhysXObject3D::OBJECT_TYPE_BODY) {
			// Body-area overlap: add/remove body from area's list and vice versa.
			PhysXBody3D *body = static_cast<PhysXBody3D *>(other_data->object);
			if (body_entered) {
				area->add_overlapping_body(body);
				body->add_overlapping_area(area);
			} else if (body_exited) {
				area->remove_overlapping_body(body);
				body->remove_overlapping_area(area);
			}
			// Queue the body_monitor dispatch for flush_queries().
			if (space) {
				PhysXSpace3D::TriggerEvent ev;
				ev.trigger_area = area;
				ev.body = body;
				ev.body_rid = body->get_rid();
				ev.body_id = body->get_instance_id();
				ev.status = status;
				ev.other_shape_idx = other_shape;
				ev.area_shape_idx = area_shape;
				ev.is_area_vs_area = false;
				space->queue_trigger(ev);
			}
		} else if (other_data->object && other_data->object->get_type() == PhysXObject3D::OBJECT_TYPE_AREA) {
			// Area-vs-area overlap: maintain BOTH sides' overlap lists (the
			// lists drive gravity/exit bookkeeping and must be symmetric).
			PhysXArea3D *other_area = static_cast<PhysXArea3D *>(other_data->object);
			if (body_entered) {
				area->add_overlapping_area(other_area);
				other_area->add_overlapping_area(area);
			} else if (body_exited) {
				area->remove_overlapping_area(other_area);
				other_area->remove_overlapping_area(area);
			}
			// Each direction of an area-vs-area overlap arrives as its own
			// PhysX trigger event: the pair that fires is (this
			// area's trigger shape, other area's non-trigger detection shape),
			// so "area" is always the detector and "other_area" the detected
			// one. Queue only THIS direction — the reverse direction arrives
			// (or does not, when that side is not monitorable) as its own event.
			// Exits always dispatch so overlap state can be cleaned up.
			if (space && (body_exited || other_area->get_monitorable())) {
				PhysXSpace3D::TriggerEvent ev;
				ev.trigger_area = area;
				ev.other_area = other_area;
				ev.other_area_rid = other_area->get_rid();
				ev.other_area_id = other_area->get_instance_id();
				ev.status = status;
				ev.other_shape_idx = other_shape;
				ev.area_shape_idx = area_shape;
				ev.is_area_vs_area = true;
				space->queue_trigger(ev);
			}
		}
	}
}

/**************************************************************************/
/*  physx_vehicle_exceptions.h                                            */
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

#include "../spaces/physx_filter_shader.h"

#include "core/object/object_id.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"

#include "PxPhysicsAPI.h"

class Node;

// Collision exceptions for the node-level vehicles (PhysXVehicle3D,
// PhysXTank3D, PhysXMotorcycle3D). Their chassis is a raw PxRigidDynamic with
// no PhysicsServer3D identity, so exceptions ride the shared word2 slot
// registry (g_physx_soft_exceptions -- the same mechanism body exceptions use
// on the GPU filter shader path): this vehicle's own slot is stamped into the
// chassis's simulation shapes on first use, each exception registers that
// slot against the other side's, and the wheels' road-geometry raycast filter
// (physx_wheel_query_filter.h) skips shapes whose slot is paired with the
// vehicle's. Covers real bodies and other vehicles alike -- neither side
// needs a server RID.
class PhysXVehicleExceptions {
public:
	// Registers an exception against p_other (a PhysicsBody3D or another
	// node-level vehicle). p_chassis is this vehicle's chassis actor.
	void add(physx::PxRigidDynamic *p_chassis, Node *p_other);
	void remove(physx::PxRigidDynamic *p_chassis, Node *p_other);
	Array get_nodes() const;
	// Vehicle teardown: drop this slot's registry entries. Slots are never
	// reused, so a stale pair left on another side is inert.
	void dispose();
	// This vehicle's own slot, stamped into the chassis shapes on first
	// allocation; the wheel filter reads the same value.
	uint32_t get_or_alloc_slot(physx::PxRigidDynamic *p_chassis);
	uint32_t get_slot() const { return slot; }
	// For the wheel filter's live read (a lazily allocated slot must be seen
	// without rewiring).
	const uint32_t *get_slot_ptr() const { return &slot; }

private:
	struct Entry {
		RID body; // a PhysicsBody3D
		ObjectID vehicle; // another node-level vehicle
		uint32_t slot = 0;
	};
	uint32_t slot = 0;
	LocalVector<Entry> entries;

	void _stamp_shapes(physx::PxRigidDynamic *p_chassis);
};

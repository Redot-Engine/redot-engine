/**************************************************************************/
/*  physx_vehicle_exceptions.cpp                                          */
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

#include "physx_vehicle_exceptions.h"

#include "../objects/physx_body_3d.h"
#include "../physx_server.h"
#include "../spaces/physx_space_3d.h"
#include "core/object/object.h"
#include "core/templates/local_vector.h"
#include "physx_motorcycle_3d.h"
#include "physx_tank_3d.h"
#include "physx_vehicle_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/physics/physics_body_3d.h"

uint32_t PhysXVehicleExceptions::get_or_alloc_slot(physx::PxRigidDynamic *p_chassis) {
	if (slot == 0) {
		slot = physx_alloc_soft_exception_slot();
	}
	// Always re-stamp: a rebuild replaced the chassis' shapes and their word2
	// slots are back to 0. The stamp is idempotent.
	_stamp_shapes(p_chassis);
	return slot;
}

// word2 of every simulation shape's filter data carries this vehicle's slot;
// the filter shader (CPU and GPU) and the wheel filter read it.
void PhysXVehicleExceptions::_stamp_shapes(physx::PxRigidDynamic *p_chassis) {
	if (p_chassis == nullptr) {
		return;
	}
	const physx::PxU32 nb_shapes = p_chassis->getNbShapes();
	if (nb_shapes == 0) {
		return;
	}
	// PxVehiclePhysXActorCreate attaches one chassis box plus one shape per
	// wheel: an 8-wheel tank carries 9 shapes. Size from nb_shapes so every
	// chassis shape gets the slot -- an unstamped chassis contact pair never
	// matches the filter shader and the exception silently does nothing.
	LocalVector<physx::PxShape *> shapes;
	shapes.resize(nb_shapes);
	p_chassis->getShapes(shapes.ptr(), nb_shapes);
	for (physx::PxU32 i = 0; i < nb_shapes; i++) {
		if (!(shapes[i]->getFlags() & physx::PxShapeFlag::eSIMULATION_SHAPE)) {
			continue;
		}
		physx::PxFilterData data = shapes[i]->getSimulationFilterData();
		data.word2 = slot;
		shapes[i]->setSimulationFilterData(data);
	}
}

void PhysXVehicleExceptions::add(physx::PxRigidDynamic *p_chassis, Node *p_other) {
	ERR_FAIL_NULL(p_other);
	// p_chassis may be null while the vehicle is not built yet (a deferred
	// _build): the entry is recorded, and _build stamps the fresh shapes once
	// the slot exists. This vehicle's own space is ensure_synced() by the
	// calling node method; a body target syncs its own space below.
	uint32_t other_slot = 0;
	RID body_rid;
	ObjectID vehicle_id;

	if (PhysicsBody3D *body_node = Object::cast_to<PhysicsBody3D>(p_other)) {
		body_rid = body_node->get_rid();
		// The singleton is null when PhysX is not the active physics backend
		// (the wrappers' _ensure_exception_synced bails on exactly that case).
		PhysXServer3D *server = PhysXServer3D::get_singleton();
		ERR_FAIL_NULL_MSG(server, "PhysX vehicle: collision exceptions require the PhysX physics backend.");
		PhysXBody3D *body = server->get_body(body_rid);
		ERR_FAIL_NULL_MSG(body, "PhysX vehicle: collision exception target is not a PhysX body.");
		if (body->get_space()) {
			body->get_space()->ensure_synced();
		}
		other_slot = body->get_or_alloc_exception_slot();
		body->refresh_collision_filters();
	} else if (PhysXVehicle3D *vehicle = Object::cast_to<PhysXVehicle3D>(p_other)) {
		vehicle_id = p_other->get_instance_id();
		other_slot = vehicle->vehicle_exception_slot_or_alloc();
	} else if (PhysXTank3D *tank = Object::cast_to<PhysXTank3D>(p_other)) {
		vehicle_id = p_other->get_instance_id();
		other_slot = tank->vehicle_exception_slot_or_alloc();
	} else if (PhysXMotorcycle3D *motorcycle = Object::cast_to<PhysXMotorcycle3D>(p_other)) {
		vehicle_id = p_other->get_instance_id();
		other_slot = motorcycle->vehicle_exception_slot_or_alloc();
	} else {
		ERR_FAIL_MSG("PhysX vehicle: collision exception target must be a PhysicsBody3D or another PhysX vehicle.");
	}

	const uint32_t own_slot = get_or_alloc_slot(p_chassis);
	g_physx_soft_exceptions.add(own_slot, other_slot);
	entries.push_back({ body_rid, vehicle_id, other_slot });
}

void PhysXVehicleExceptions::remove(physx::PxRigidDynamic *p_chassis, Node *p_other) {
	ERR_FAIL_NULL(p_other);
	if (slot == 0) {
		return;
	}
	for (uint32_t i = 0; i < entries.size(); i++) {
		bool matches = false;
		if (entries[i].body.is_valid()) {
			matches = Object::cast_to<PhysicsBody3D>(p_other) != nullptr &&
					Object::cast_to<PhysicsBody3D>(p_other)->get_rid() == entries[i].body;
		} else if (entries[i].vehicle.is_valid()) {
			matches = p_other->get_instance_id() == entries[i].vehicle;
		}
		if (matches) {
			g_physx_soft_exceptions.remove(slot, entries[i].slot);
			entries.remove_at(i);
			return;
		}
	}
}

Array PhysXVehicleExceptions::get_nodes() const {
	Array out;
	for (uint32_t i = 0; i < entries.size(); i++) {
		Object *node = nullptr;
		if (entries[i].body.is_valid()) {
			PhysXServer3D *server = PhysXServer3D::get_singleton();
			PhysXBody3D *body = server ? server->get_body(entries[i].body) : nullptr;
			if (body) {
				node = ObjectDB::get_instance(body->get_instance_id());
			}
		} else if (entries[i].vehicle.is_valid()) {
			node = ObjectDB::get_instance(entries[i].vehicle);
		}
		if (node) {
			out.push_back(node);
		}
	}
	return out;
}

void PhysXVehicleExceptions::dispose() {
	if (slot != 0) {
		g_physx_soft_exceptions.remove_soft(slot);
		slot = 0;
	}
	entries.clear();
}

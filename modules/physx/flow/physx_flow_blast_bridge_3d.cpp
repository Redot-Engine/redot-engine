/**************************************************************************/
/*  physx_flow_blast_bridge_3d.cpp                                        */
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

#include "physx_flow_blast_bridge_3d.h"

#if defined(GODOT_PHYSX_FLOW) && defined(GODOT_PHYSX_BLAST)

#include "../blast/physx_destructible_3d.h"
#include "physx_flow_simulation_3d.h"

#include "core/object/class_db.h"

void PhysXFlowBlastBridge3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (PhysXDestructible3D *d = _resolve_destructible()) {
				if (!connected) {
					d->connect(SNAME("chunks_fractured"), callable_mp(this, &PhysXFlowBlastBridge3D::_on_fractured));
					connected_id = d->get_instance_id(); // remember WHO we connected to
					connected = true;
				}
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_disconnect_destructible();
		} break;
		default:
			break;
	}
}

void PhysXFlowBlastBridge3D::_disconnect_destructible() {
	if (!connected) {
		return;
	}
	// FLOW-3: disconnect the node we ACTUALLY connected to (by cached
	// ObjectID), not whatever destructible_path currently resolves to -- the
	// old re-wire resolved the NEW path for the disconnect, targeting a node
	// that was never connected while the old one stayed wired forever.
	if (Object *obj = ObjectDB::get_instance(connected_id)) {
		if (PhysXDestructible3D *d = Object::cast_to<PhysXDestructible3D>(obj)) {
			d->disconnect(SNAME("chunks_fractured"), callable_mp(this, &PhysXFlowBlastBridge3D::_on_fractured));
		}
	}
	connected_id = ObjectID();
	connected = false;
}

PhysXDestructible3D *PhysXFlowBlastBridge3D::_resolve_destructible() const {
	if (!is_inside_tree()) {
		return nullptr;
	}
	return Object::cast_to<PhysXDestructible3D>(get_node_or_null(destructible_path));
}

PhysXFlowSimulation3D *PhysXFlowBlastBridge3D::_resolve_simulation() const {
	if (!is_inside_tree()) {
		return nullptr;
	}
	return Object::cast_to<PhysXFlowSimulation3D>(get_node_or_null(simulation_path));
}

void PhysXFlowBlastBridge3D::_on_fractured(const Vector3 &p_world_position, int p_pieces, float p_damage) {
	if (!enabled) {
		return;
	}
	PhysXFlowSimulation3D *sim = _resolve_simulation();
	if (sim == nullptr) {
		return;
	}
	// Effect size scales with the applied damage clamped to the authored
	// max radius; intensity with the piece count (a chip-off is a puff, a
	// full shatter is a cloud).
	const float radius = MIN(radius_scale * (0.5f + 0.1f * p_pieces), max_radius);
	const float intensity = CLAMP((float)p_pieces / 8.0f, 0.25f, 1.0f);
	sim->add_transient_emitter(
			p_world_position,
			radius,
			smoke * intensity,
			temperature * intensity,
			fuel * intensity,
			velocity);
}

PackedStringArray PhysXFlowBlastBridge3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	if (destructible_path.is_empty()) {
		warnings.push_back(RTR("No destructible set: the bridge has nothing to listen to."));
	} else if (is_inside_tree() && _resolve_destructible() == nullptr) {
		warnings.push_back(RTR("The destructible_path node is not a PhysXDestructible3D."));
	}
	if (simulation_path.is_empty()) {
		warnings.push_back(RTR("No Flow simulation set: fracture events have nowhere to go."));
	} else if (is_inside_tree() && _resolve_simulation() == nullptr) {
		warnings.push_back(RTR("The simulation_path node is not a PhysXFlowSimulation3D."));
	}
	return warnings;
}

void PhysXFlowBlastBridge3D::set_destructible_path(const NodePath &p_path) {
	destructible_path = p_path;
	// Re-wire when the target changes while inside the tree.
	if (is_inside_tree()) {
		_disconnect_destructible();
		if (PhysXDestructible3D *d = _resolve_destructible()) {
			d->connect(SNAME("chunks_fractured"), callable_mp(this, &PhysXFlowBlastBridge3D::_on_fractured));
			connected_id = d->get_instance_id();
			connected = true;
		}
	}
}

void PhysXFlowBlastBridge3D::set_simulation_path(const NodePath &p_path) {
	simulation_path = p_path;
}

void PhysXFlowBlastBridge3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &PhysXFlowBlastBridge3D::set_enabled);
	ClassDB::bind_method(D_METHOD("get_enabled"), &PhysXFlowBlastBridge3D::get_enabled);
	ClassDB::bind_method(D_METHOD("set_destructible_path", "path"), &PhysXFlowBlastBridge3D::set_destructible_path);
	ClassDB::bind_method(D_METHOD("get_destructible_path"), &PhysXFlowBlastBridge3D::get_destructible_path);
	ClassDB::bind_method(D_METHOD("set_simulation_path", "path"), &PhysXFlowBlastBridge3D::set_simulation_path);
	ClassDB::bind_method(D_METHOD("get_simulation_path"), &PhysXFlowBlastBridge3D::get_simulation_path);
	ClassDB::bind_method(D_METHOD("set_radius_scale", "scale"), &PhysXFlowBlastBridge3D::set_radius_scale);
	ClassDB::bind_method(D_METHOD("get_radius_scale"), &PhysXFlowBlastBridge3D::get_radius_scale);
	ClassDB::bind_method(D_METHOD("set_max_radius", "radius"), &PhysXFlowBlastBridge3D::set_max_radius);
	ClassDB::bind_method(D_METHOD("get_max_radius"), &PhysXFlowBlastBridge3D::get_max_radius);
	ClassDB::bind_method(D_METHOD("set_smoke", "smoke"), &PhysXFlowBlastBridge3D::set_smoke);
	ClassDB::bind_method(D_METHOD("get_smoke"), &PhysXFlowBlastBridge3D::get_smoke);
	ClassDB::bind_method(D_METHOD("set_temperature", "temperature"), &PhysXFlowBlastBridge3D::set_temperature);
	ClassDB::bind_method(D_METHOD("get_temperature"), &PhysXFlowBlastBridge3D::get_temperature);
	ClassDB::bind_method(D_METHOD("set_fuel", "fuel"), &PhysXFlowBlastBridge3D::set_fuel);
	ClassDB::bind_method(D_METHOD("get_fuel"), &PhysXFlowBlastBridge3D::get_fuel);
	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &PhysXFlowBlastBridge3D::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &PhysXFlowBlastBridge3D::get_velocity);
	ClassDB::bind_method(D_METHOD("set_lifetime", "lifetime"), &PhysXFlowBlastBridge3D::set_lifetime);
	ClassDB::bind_method(D_METHOD("get_lifetime"), &PhysXFlowBlastBridge3D::get_lifetime);

	ADD_GROUP("Bridge", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "get_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "destructible_path", PROPERTY_HINT_NODE_TYPE, "PhysXDestructible3D"), "set_destructible_path", "get_destructible_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "simulation_path", PROPERTY_HINT_NODE_TYPE, "PhysXFlowSimulation3D"), "set_simulation_path", "get_simulation_path");
	ADD_GROUP("Effect", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius_scale", PROPERTY_HINT_RANGE, "0.01,8.0,0.01"), "set_radius_scale", "get_radius_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_radius", PROPERTY_HINT_RANGE, "0.05,16.0,0.05,suffix:m"), "set_max_radius", "get_max_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "smoke", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_smoke", "get_smoke");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "temperature", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_temperature", "get_temperature");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fuel", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_fuel", "get_fuel");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "velocity", PROPERTY_HINT_RANGE, "-4.0,8.0,0.05,suffix:m/s"), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lifetime", PROPERTY_HINT_RANGE, "0.05,8.0,0.05,suffix:s"), "set_lifetime", "get_lifetime");
}

#endif // GODOT_PHYSX_FLOW && GODOT_PHYSX_BLAST

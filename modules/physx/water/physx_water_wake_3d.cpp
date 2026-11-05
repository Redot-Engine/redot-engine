/**************************************************************************/
/*  physx_water_wake_3d.cpp                                               */
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

#include "physx_water_wake_3d.h"

#include "physx_boat_3d.h"
#include "physx_buoyancy_3d.h"
#include "physx_water_surface_3d.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "scene/3d/physics/rigid_body_3d.h"
#include "scene/resources/texture_rd.h"

void PhysXWaterWake3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_water_surface_path", "path"), &PhysXWaterWake3D::set_water_surface_path);
	ClassDB::bind_method(D_METHOD("get_water_surface_path"), &PhysXWaterWake3D::get_water_surface_path);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &PhysXWaterWake3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &PhysXWaterWake3D::get_size);
	ClassDB::bind_method(D_METHOD("set_cells", "cells"), &PhysXWaterWake3D::set_cells);
	ClassDB::bind_method(D_METHOD("get_cells"), &PhysXWaterWake3D::get_cells);
	ClassDB::bind_method(D_METHOD("set_wave_speed", "speed"), &PhysXWaterWake3D::set_wave_speed);
	ClassDB::bind_method(D_METHOD("get_wave_speed"), &PhysXWaterWake3D::get_wave_speed);
	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &PhysXWaterWake3D::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &PhysXWaterWake3D::get_damping);
	ClassDB::bind_method(D_METHOD("set_border_damping", "damping"), &PhysXWaterWake3D::set_border_damping);
	ClassDB::bind_method(D_METHOD("get_border_damping"), &PhysXWaterWake3D::get_border_damping);
	ClassDB::bind_method(D_METHOD("set_hull_length", "length"), &PhysXWaterWake3D::set_hull_length);
	ClassDB::bind_method(D_METHOD("get_hull_length"), &PhysXWaterWake3D::get_hull_length);
	ClassDB::bind_method(D_METHOD("set_hull_beam", "beam"), &PhysXWaterWake3D::set_hull_beam);
	ClassDB::bind_method(D_METHOD("get_hull_beam"), &PhysXWaterWake3D::get_hull_beam);
	ClassDB::bind_method(D_METHOD("set_source_count", "count"), &PhysXWaterWake3D::set_source_count);
	ClassDB::bind_method(D_METHOD("get_source_count"), &PhysXWaterWake3D::get_source_count);
	ClassDB::bind_method(D_METHOD("set_rest_depth", "depth"), &PhysXWaterWake3D::set_rest_depth);
	ClassDB::bind_method(D_METHOD("get_rest_depth"), &PhysXWaterWake3D::get_rest_depth);
	ClassDB::bind_method(D_METHOD("set_wake_depth", "depth"), &PhysXWaterWake3D::set_wake_depth);
	ClassDB::bind_method(D_METHOD("get_wake_depth"), &PhysXWaterWake3D::get_wake_depth);
	ClassDB::bind_method(D_METHOD("set_reference_speed", "speed"), &PhysXWaterWake3D::set_reference_speed);
	ClassDB::bind_method(D_METHOD("get_reference_speed"), &PhysXWaterWake3D::get_reference_speed);
	ClassDB::bind_method(D_METHOD("set_shore_grid_fade", "fade"), &PhysXWaterWake3D::set_shore_grid_fade);
	ClassDB::bind_method(D_METHOD("get_shore_grid_fade"), &PhysXWaterWake3D::get_shore_grid_fade);
	ClassDB::bind_method(D_METHOD("set_bow_foam", "foam"), &PhysXWaterWake3D::set_bow_foam);
	ClassDB::bind_method(D_METHOD("get_bow_foam"), &PhysXWaterWake3D::get_bow_foam);
	ClassDB::bind_method(D_METHOD("set_propeller_foam", "foam"), &PhysXWaterWake3D::set_propeller_foam);
	ClassDB::bind_method(D_METHOD("get_propeller_foam"), &PhysXWaterWake3D::get_propeller_foam);
	ClassDB::bind_method(D_METHOD("set_foam_persistence", "seconds"), &PhysXWaterWake3D::set_foam_persistence);
	ClassDB::bind_method(D_METHOD("get_foam_persistence"), &PhysXWaterWake3D::get_foam_persistence);
	ClassDB::bind_method(D_METHOD("set_foam_slope", "slope"), &PhysXWaterWake3D::set_foam_slope);
	ClassDB::bind_method(D_METHOD("get_foam_slope"), &PhysXWaterWake3D::get_foam_slope);
	ClassDB::bind_method(D_METHOD("set_foam_slope_gain", "gain"), &PhysXWaterWake3D::set_foam_slope_gain);
	ClassDB::bind_method(D_METHOD("get_foam_slope_gain"), &PhysXWaterWake3D::get_foam_slope_gain);
	ClassDB::bind_method(D_METHOD("set_foam_spread", "speed"), &PhysXWaterWake3D::set_foam_spread);
	ClassDB::bind_method(D_METHOD("get_foam_spread"), &PhysXWaterWake3D::get_foam_spread);
	ClassDB::bind_method(D_METHOD("add_foam", "world_position", "radius", "amount_per_second", "drift"), &PhysXWaterWake3D::add_foam, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("sample_height", "world_pos"), &PhysXWaterWake3D::sample_height);
	ClassDB::bind_method(D_METHOD("get_grid_origin"), &PhysXWaterWake3D::get_grid_origin);
	ClassDB::bind_method(D_METHOD("get_height_texture"), &PhysXWaterWake3D::get_height_texture);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "water_surface_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "PhysXWaterSurface3D"), "set_water_surface_path", "get_water_surface_path");
	ADD_GROUP("Grid", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "size", PROPERTY_HINT_RANGE, "4,200,0.1,or_greater,suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cells", PROPERTY_HINT_RANGE, "16,512,1"), "set_cells", "get_cells");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wave_speed", PROPERTY_HINT_RANGE, "0.1,20,0.01,suffix:m/s"), "set_wave_speed", "get_wave_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping", PROPERTY_HINT_RANGE, "0,10,0.01"), "set_damping", "get_damping");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "border_damping", PROPERTY_HINT_RANGE, "0,60,0.1"), "set_border_damping", "get_border_damping");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shore_grid_fade", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_shore_grid_fade", "get_shore_grid_fade");
	ADD_GROUP("Foam", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bow_foam", PROPERTY_HINT_RANGE, "0,20,0.01,or_greater"), "set_bow_foam", "get_bow_foam");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "propeller_foam", PROPERTY_HINT_RANGE, "0,5,0.01"), "set_propeller_foam", "get_propeller_foam");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "foam_persistence", PROPERTY_HINT_RANGE, "0.05,30,0.05,suffix:s"), "set_foam_persistence", "get_foam_persistence");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "foam_slope", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_foam_slope", "get_foam_slope");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "foam_slope_gain", PROPERTY_HINT_RANGE, "0,20,0.01"), "set_foam_slope_gain", "get_foam_slope_gain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "foam_spread", PROPERTY_HINT_RANGE, "0,10,0.01,suffix:m/s"), "set_foam_spread", "get_foam_spread");
	ADD_GROUP("Hull", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hull_length", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater,suffix:m"), "set_hull_length", "get_hull_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hull_beam", PROPERTY_HINT_RANGE, "0,30,0.01,or_greater,suffix:m"), "set_hull_beam", "get_hull_beam");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "source_count", PROPERTY_HINT_RANGE, vformat("1,%d,1", MAX_KEEL_SOURCES)), "set_source_count", "get_source_count");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rest_depth", PROPERTY_HINT_RANGE, "0,1,0.001,suffix:m"), "set_rest_depth", "get_rest_depth");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wake_depth", PROPERTY_HINT_RANGE, "0,3,0.001,suffix:m"), "set_wake_depth", "get_wake_depth");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reference_speed", PROPERTY_HINT_RANGE, "0.1,50,0.1,suffix:m/s"), "set_reference_speed", "get_reference_speed");
}

PhysXWaterWake3D::~PhysXWaterWake3D() {
	_clear();
}

RigidBody3D *PhysXWaterWake3D::get_body() const {
	return Object::cast_to<RigidBody3D>(get_parent());
}

PhysXWaterSurface3D *PhysXWaterWake3D::get_water_surface() const {
	return ObjectDB::get_instance<PhysXWaterSurface3D>(water_id);
}

void PhysXWaterWake3D::_clear() {
	PhysXWaterSurface3D *water = get_water_surface();
	if (water != nullptr && slot >= 0) {
		water->unregister_wake(slot);
	}
	slot = -1;
	pending_foam.clear();
	if (height_texture.is_valid()) {
		height_texture->set_texture_rd_rid(RID());
		height_texture.unref();
	}
	if (solver != nullptr) {
		memdelete(solver);
		solver = nullptr;
	}
}

void PhysXWaterWake3D::_build() {
	_clear();
	// The water: our own path, else the sibling buoyancy's.
	PhysXWaterSurface3D *water = Object::cast_to<PhysXWaterSurface3D>(get_node_or_null(water_surface_path));
	PhysXBuoyancy3D *buoyancy = nullptr;
	if (get_parent() != nullptr) {
		for (int i = 0; i < get_parent()->get_child_count(); i++) {
			buoyancy = Object::cast_to<PhysXBuoyancy3D>(get_parent()->get_child(i));
			if (buoyancy != nullptr) {
				break;
			}
		}
	}
	if (water == nullptr && buoyancy != nullptr) {
		water = Object::cast_to<PhysXWaterSurface3D>(buoyancy->get_node_or_null(buoyancy->get_water_surface_path()));
	}
	water_id = water != nullptr ? water->get_instance_id() : ObjectID();
	if (water == nullptr) {
		return;
	}

	// Hull extent: the buoyancy's sample points (along local Z), unless set.
	float zmin = -2.0f, zmax = 2.0f, xmax = 0.6f;
	if (buoyancy != nullptr && !buoyancy->get_effective_sample_points().is_empty()) {
		zmin = 1e30f;
		zmax = -1e30f;
		xmax = 0.0f;
		for (const Vector3 &p : buoyancy->get_effective_sample_points()) {
			zmin = MIN(zmin, p.z);
			zmax = MAX(zmax, p.z);
			xmax = MAX(xmax, Math::abs(p.x));
		}
	}
	length = hull_length > 0.0f ? hull_length : MAX(zmax - zmin, 0.5f);
	beam = hull_beam > 0.0f ? hull_beam : MAX(xmax * 2.0f, 0.3f);
	bow_z = hull_length > 0.0f ? length * 0.5f : zmax;

	solver = memnew(WaterWakeSolver);
	if (!solver->is_available()) {
		memdelete(solver);
		solver = nullptr;
		return;
	}
	solver->build(cells, size);
	height_texture.instantiate();
	height_texture->set_texture_rd_rid(solver->get_height_texture_rd_rid());
	slot = water->register_wake(get_instance_id());
	if (slot < 0) {
		WARN_PRINT("PhysXWaterWake3D: the water surface already draws its maximum of 4 wakes; this one is simulated but not shown.");
	}
}

void PhysXWaterWake3D::_physics_step(double p_delta) {
	RigidBody3D *body = get_body();
	PhysXWaterSurface3D *water = get_water_surface();
	if (body == nullptr || water == nullptr || solver == nullptr || !solver->is_built()) {
		return;
	}
	const Transform3D xf = body->get_global_transform();
	const Vector3 fwd = xf.basis.get_column(2).normalized();
	const Vector3 vel = body->get_linear_velocity();
	const float speed = Vector2(vel.x, vel.z).length();
	// Only while the hull sits in the water.
	const float above = xf.origin.y - water->sample_height_excluding_wake(xf.origin, get_instance_id());
	const float in_water = CLAMP(1.0f - above / MAX(beam, 0.5f), 0.0f, 1.0f);
	const float speed_frac = MIN(speed / reference_speed, 1.5f);
	const float depth = (rest_depth + (wake_depth - rest_depth) * speed_frac) * in_water;

	// A row of dips along the keel, deepest at the bow (it pushes the water
	// aside), shallower astern.
	Vector<WaterWakeSolver::Source> sources;
	const Vector3 bow = xf.origin + fwd * bow_z;
	for (int i = 0; i < source_count; i++) {
		const float bow_stern_fraction = source_count > 1 ? (float)i / (source_count - 1) : 0.5f;
		const Vector3 source_position = bow - fwd * (length * bow_stern_fraction);
		WaterWakeSolver::Source source;
		source.world_xz = Vector2(source_position.x, source_position.z);
		source.radius = beam * Math::lerp(0.45f, 0.6f, bow_stern_fraction);
		source.depth = depth * Math::lerp(1.0f, 0.55f, bow_stern_fraction);
		sources.push_back(source);
	}
	// Propeller wash: foam only, behind the propeller, with the thrust.
	PhysXBoat3D *boat = nullptr;
	for (int i = 0; i < body->get_child_count() && boat == nullptr; i++) {
		boat = Object::cast_to<PhysXBoat3D>(body->get_child(i));
	}
	if (boat != nullptr && boat->get_max_thrust() > 0.0f && propeller_foam > 0.0f) {
		const float churn = Math::abs(boat->get_applied_thrust()) / boat->get_max_thrust();
		if (churn > 0.0f) {
			const Vector3 prop = xf.xform(boat->get_propeller_position()) - fwd * 0.6f;
			WaterWakeSolver::Source source;
			source.world_xz = Vector2(prop.x, prop.z);
			source.radius = MAX(beam * 0.35f, 0.25f);
			source.depth = 0.0f;
			source.foam = propeller_foam * churn;
			sources.push_back(source);
		}
	}
	// Foam other nodes added (landing spray, splashes).
	for (const WaterWakeSolver::Source &f : pending_foam) {
		if (sources.size() >= WaterWakeSolverGPU::MAX_SOURCES) {
			break;
		}
		sources.push_back(f);
	}
	pending_foam.clear();
	const float border_cells = MAX(cells * 0.1f, 4.0f);
	// Most of the grid trails behind: the wake is there, not ahead.
	Vector2 heading(vel.x, vel.z);
	heading = speed > 0.5f ? heading / speed : Vector2(fwd.x, fwd.z).normalized();
	const Vector2 center = Vector2(xf.origin.x, xf.origin.z) - heading * (size * 0.25f);
	const Vector3 stern = bow - fwd * length;
	solver->step(p_delta, center, sources, wave_speed, damping, border_damping, border_cells, foam_persistence, foam_slope, foam_slope_gain,
			Vector2(bow.x, bow.z), Vector2(stern.x, stern.z), foam_spread * MIN(speed_frac, 1.0f), MAX(beam * 3.0f, 2.0f), beam * 0.5f, bow_foam * speed_frac * in_water);
	if (slot >= 0) {
		const Vector2 o = solver->get_origin();
		water->set_wake_slot(slot, height_texture, Vector4(o.x, o.y, solver->get_size(), shore_grid_fade));
	}
}

void PhysXWaterWake3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(true);
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			// Built on the first tick, once the water and buoyancy are ready.
			if (solver == nullptr && water_id.is_null()) {
				_build();
			}
			_physics_step(get_physics_process_delta_time());
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_clear();
			water_id = ObjectID();
		} break;
	}
}

void PhysXWaterWake3D::add_foam(const Vector3 &p_world_pos, float p_radius, float p_amount_per_second, bool p_drift) {
	if (p_amount_per_second <= 0.0f || pending_foam.size() >= WaterWakeSolverGPU::MAX_SOURCES) {
		return;
	}
	WaterWakeSolver::Source source;
	source.world_xz = Vector2(p_world_pos.x, p_world_pos.z);
	source.radius = MAX(p_radius, 0.05f);
	source.depth = 0.0f; // foam only, no dip
	source.foam = p_amount_per_second;
	source.drift = p_drift;
	pending_foam.push_back(source);
}

float PhysXWaterWake3D::sample_height(const Vector3 &p_world_pos) const {
	return solver != nullptr ? solver->sample_height(Vector2(p_world_pos.x, p_world_pos.z)) : 0.0f;
}

Vector2 PhysXWaterWake3D::get_grid_origin() const {
	return solver != nullptr ? solver->get_origin() : Vector2();
}

void PhysXWaterWake3D::set_water_surface_path(const NodePath &p_path) {
	water_surface_path = p_path;
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		_clear();
		water_id = ObjectID();
	}
	update_configuration_warnings();
}

void PhysXWaterWake3D::set_size(float p_size) {
	size = MAX(p_size, 4.0f);
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		_clear();
		water_id = ObjectID();
	}
}

void PhysXWaterWake3D::set_cells(int p_cells) {
	cells = CLAMP(p_cells, 16, 512);
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		_clear();
		water_id = ObjectID();
	}
}

PackedStringArray PhysXWaterWake3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	if (Object::cast_to<RigidBody3D>(get_parent()) == nullptr) {
		warnings.push_back(RTR("PhysXWaterWake3D follows a RigidBody3D: make it a child of one (a boat or floater)."));
	}
	return warnings;
}

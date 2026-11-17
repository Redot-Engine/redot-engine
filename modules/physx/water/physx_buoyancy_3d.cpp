/**************************************************************************/
/*  physx_buoyancy_3d.cpp                                                 */
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

#include "physx_buoyancy_3d.h"

#include "physx_water_surface_3d.h"
#include "physx_water_wake_3d.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/rigid_body_3d.h"
#include "scene/resources/3d/shape_3d.h"
#include "scene/resources/mesh.h"

void PhysXBuoyancy3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_water_surface_path", "path"), &PhysXBuoyancy3D::set_water_surface_path);
	ClassDB::bind_method(D_METHOD("get_water_surface_path"), &PhysXBuoyancy3D::get_water_surface_path);
	ClassDB::bind_method(D_METHOD("set_sample_points", "points"), &PhysXBuoyancy3D::set_sample_points);
	ClassDB::bind_method(D_METHOD("get_sample_points"), &PhysXBuoyancy3D::get_sample_points);
	ClassDB::bind_method(D_METHOD("set_hull_area", "area"), &PhysXBuoyancy3D::set_hull_area);
	ClassDB::bind_method(D_METHOD("get_hull_area"), &PhysXBuoyancy3D::get_hull_area);
	ClassDB::bind_method(D_METHOD("set_hull_height", "height"), &PhysXBuoyancy3D::set_hull_height);
	ClassDB::bind_method(D_METHOD("get_hull_height"), &PhysXBuoyancy3D::get_hull_height);
	ClassDB::bind_method(D_METHOD("set_water_density", "density"), &PhysXBuoyancy3D::set_water_density);
	ClassDB::bind_method(D_METHOD("get_water_density"), &PhysXBuoyancy3D::get_water_density);
	ClassDB::bind_method(D_METHOD("set_linear_drag", "drag"), &PhysXBuoyancy3D::set_linear_drag);
	ClassDB::bind_method(D_METHOD("get_linear_drag"), &PhysXBuoyancy3D::get_linear_drag);
	ClassDB::bind_method(D_METHOD("set_angular_drag", "drag"), &PhysXBuoyancy3D::set_angular_drag);
	ClassDB::bind_method(D_METHOD("get_angular_drag"), &PhysXBuoyancy3D::get_angular_drag);
	ClassDB::bind_method(D_METHOD("set_disturb_water", "enabled"), &PhysXBuoyancy3D::set_disturb_water);
	ClassDB::bind_method(D_METHOD("get_disturb_water"), &PhysXBuoyancy3D::get_disturb_water);
	ClassDB::bind_method(D_METHOD("set_ripple_radius", "radius"), &PhysXBuoyancy3D::set_ripple_radius);
	ClassDB::bind_method(D_METHOD("get_ripple_radius"), &PhysXBuoyancy3D::get_ripple_radius);
	ClassDB::bind_method(D_METHOD("set_ripple_rest_strength", "strength"), &PhysXBuoyancy3D::set_ripple_rest_strength);
	ClassDB::bind_method(D_METHOD("get_ripple_rest_strength"), &PhysXBuoyancy3D::get_ripple_rest_strength);
	ClassDB::bind_method(D_METHOD("set_ripple_reference_speed", "speed"), &PhysXBuoyancy3D::set_ripple_reference_speed);
	ClassDB::bind_method(D_METHOD("get_ripple_reference_speed"), &PhysXBuoyancy3D::get_ripple_reference_speed);
	ClassDB::bind_method(D_METHOD("get_submerged_fraction"), &PhysXBuoyancy3D::get_submerged_fraction);
	ClassDB::bind_method(D_METHOD("get_point_depths"), &PhysXBuoyancy3D::get_point_depths);
	ClassDB::bind_method(D_METHOD("get_effective_sample_points"), &PhysXBuoyancy3D::get_effective_sample_points);
	ClassDB::bind_method(D_METHOD("get_water_height", "world_pos"), &PhysXBuoyancy3D::get_water_height);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "water_surface_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "PhysXWaterSurface3D"), "set_water_surface_path", "get_water_surface_path");
	ADD_GROUP("Hull", "");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "sample_points"), "set_sample_points", "get_sample_points");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hull_area", PROPERTY_HINT_RANGE, "0,100,0.01,or_greater,suffix:m²"), "set_hull_area", "get_hull_area");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hull_height", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater,suffix:m"), "set_hull_height", "get_hull_height");
	ADD_GROUP("Water", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "water_density", PROPERTY_HINT_RANGE, "1,2000,1,or_greater,suffix:kg/m³"), "set_water_density", "get_water_density");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "linear_drag", PROPERTY_HINT_RANGE, "0,1000,0.1,or_greater"), "set_linear_drag", "get_linear_drag");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angular_drag", PROPERTY_HINT_RANGE, "0,1000,0.1,or_greater"), "set_angular_drag", "get_angular_drag");
	ADD_GROUP("Ripples", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disturb_water"), "set_disturb_water", "get_disturb_water");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ripple_radius", PROPERTY_HINT_RANGE, "0,20,0.01,or_greater,suffix:m"), "set_ripple_radius", "get_ripple_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ripple_rest_strength", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_ripple_rest_strength", "get_ripple_rest_strength");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ripple_reference_speed", PROPERTY_HINT_RANGE, "0.01,50,0.01,or_greater,suffix:m/s"), "set_ripple_reference_speed", "get_ripple_reference_speed");
}

RigidBody3D *PhysXBuoyancy3D::get_body() const {
	return Object::cast_to<RigidBody3D>(get_parent());
}

PhysXWaterSurface3D *PhysXBuoyancy3D::get_water_surface() const {
	return ObjectDB::get_instance<PhysXWaterSurface3D>(water_id);
}

float PhysXBuoyancy3D::get_water_height(const Vector3 &p_world_pos) const {
	PhysXWaterSurface3D *water = get_water_surface();
	return water != nullptr ? water->sample_height_excluding_wake(p_world_pos, wake_id) : -Math::INF;
}

void PhysXBuoyancy3D::_resolve() {
	PhysXWaterSurface3D *water = Object::cast_to<PhysXWaterSurface3D>(get_node_or_null(water_surface_path));
	water_id = water != nullptr ? water->get_instance_id() : ObjectID();
	wake_id = ObjectID();
	if (get_parent() != nullptr) {
		for (int i = 0; i < get_parent()->get_child_count(); i++) {
			if (Object::cast_to<PhysXWaterWake3D>(get_parent()->get_child(i)) != nullptr) {
				wake_id = get_parent()->get_child(i)->get_instance_id();
			}
		}
	}

	// Hull bounds from the body's collision shapes, in the body's space.
	AABB bounds;
	bool have_bounds = false;
	RigidBody3D *body = get_body();
	if (body != nullptr) {
		for (int i = 0; i < body->get_child_count(); i++) {
			CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(body->get_child(i));
			if (cs == nullptr || cs->is_disabled() || cs->get_shape().is_null()) {
				continue;
			}
			Ref<ArrayMesh> debug = cs->get_shape()->get_debug_mesh();
			if (debug.is_null()) {
				continue;
			}
			const AABB box = cs->get_transform().xform(debug->get_aabb());
			bounds = have_bounds ? bounds.merge(box) : box;
			have_bounds = true;
		}
	}
	if (!have_bounds) {
		bounds = AABB(Vector3(-0.5, -0.5, -0.5), Vector3(1, 1, 1));
	}

	// Default points: the bottom of the hull bounds, inset a little, three
	// along the long axis and two across -- enough to pitch and roll.
	points = sample_points;
	if (points.is_empty()) {
		const bool long_z = bounds.size.z >= bounds.size.x;
		const Vector3 inset = bounds.size * 0.15f;
		const float sample_bottom_y = bounds.position.y;
		for (int l = 0; l < 3; l++) {
			for (int a = 0; a < 2; a++) {
				const float tl = l / 2.0f;
				const float ta = (float)a;
				const float sample_x = long_z ? Math::lerp(bounds.position.x + inset.x, bounds.get_end().x - inset.x, ta) : Math::lerp(bounds.position.x + inset.x, bounds.get_end().x - inset.x, tl);
				const float sample_z = long_z ? Math::lerp(bounds.position.z + inset.z, bounds.get_end().z - inset.z, tl) : Math::lerp(bounds.position.z + inset.z, bounds.get_end().z - inset.z, ta);
				points.push_back(Vector3(sample_x, sample_bottom_y, sample_z));
			}
		}
	}
	// A hull isn't a box: about three quarters of its bounding footprint.
	area = hull_area > 0.0f ? hull_area : MAX(bounds.size.x * bounds.size.z * 0.75f, 0.01f);
	height = hull_height > 0.0f ? hull_height : MAX(bounds.size.y, 0.01f);
	radius = ripple_radius > 0.0f ? ripple_radius : MAX(Math::sqrt(bounds.size.x * bounds.size.z) * 0.5f, 0.1f);
	point_depths.resize(points.size());
	point_depths.fill(-Math::INF);
}

void PhysXBuoyancy3D::_physics_step() {
	RigidBody3D *body = get_body();
	PhysXWaterSurface3D *water = get_water_surface();
	if (body == nullptr || water == nullptr || points.is_empty()) {
		submerged_fraction = 0.0f;
		return;
	}
	const Transform3D xf = body->get_global_transform();
	const float gravity_magnitude = body->get_gravity().length();
	const Vector3 up = body->get_gravity().length_squared() > 0.0f ? -body->get_gravity().normalized() : Vector3(0, 1, 0);
	const Vector3 lin = body->get_linear_velocity();
	const Vector3 ang = body->get_angular_velocity();
	const float area_per_sample = area / points.size();
	float used = 0.0f;
	for (int i = 0; i < points.size(); i++) {
		const Vector3 wp = xf.xform(points[i]);
		const float depth = water->sample_height_excluding_wake(wp, wake_id) - wp.y;
		point_depths.set(i, depth);
		if (!(depth > 0.0f)) {
			continue;
		}
		const float submerged_depth = MIN(depth, height);
		used += submerged_depth;
		const Vector3 offset = wp - xf.origin;
		const Vector3 sample_velocity = lin + ang.cross(offset);
		// Archimedes for this point's share of the footprint, plus drag on
		// the part that's in the water.
		Vector3 force = up * (water_density * gravity_magnitude * area_per_sample * submerged_depth);
		force -= sample_velocity * (linear_drag * area_per_sample * (submerged_depth / height));
		body->apply_force(force, offset);
	}
	submerged_fraction = used / (points.size() * height);
	if (submerged_fraction > 0.0f) {
		body->apply_torque(-ang * (angular_drag * submerged_fraction));
	}

	if (disturb_water) {
		// Only near the surface, harder the faster it moves -- area_per_sample still
		// floater only dimples the water.
		const float above = xf.origin.y - water->sample_height_excluding_wake(xf.origin, wake_id);
		const float proximity = CLAMP(1.0f - above / radius, 0.0f, 1.0f);
		if (proximity > 0.0f) {
			const float speed_frac = lin.length() / ripple_reference_speed;
			const float strength = CLAMP(ripple_rest_strength + (1.0f - ripple_rest_strength) * speed_frac, ripple_rest_strength, 1.0f) * proximity;
			water->submit_sphere((int)(uint64_t)get_instance_id(), xf.origin, radius, strength);
		} else {
			water->clear_sphere((int)(uint64_t)get_instance_id());
		}
	}
}

void PhysXBuoyancy3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_resolve();
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(true);
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			_physics_step();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			PhysXWaterSurface3D *water = get_water_surface();
			if (water != nullptr) {
				water->clear_sphere((int)(uint64_t)get_instance_id());
			}
		} break;
	}
}

void PhysXBuoyancy3D::set_water_surface_path(const NodePath &p_path) {
	water_surface_path = p_path;
	if (is_inside_tree()) {
		_resolve();
	}
	update_configuration_warnings();
}

void PhysXBuoyancy3D::set_sample_points(const PackedVector3Array &p_points) {
	sample_points = p_points;
	if (is_inside_tree()) {
		_resolve();
	}
}

void PhysXBuoyancy3D::set_hull_area(float p_area) {
	hull_area = MAX(p_area, 0.0f);
	if (is_inside_tree()) {
		_resolve();
	}
}

void PhysXBuoyancy3D::set_hull_height(float p_height) {
	hull_height = MAX(p_height, 0.0f);
	if (is_inside_tree()) {
		_resolve();
	}
}

void PhysXBuoyancy3D::set_disturb_water(bool p_enabled) {
	disturb_water = p_enabled;
	PhysXWaterSurface3D *water = get_water_surface();
	if (!disturb_water && water != nullptr) {
		water->clear_sphere((int)(uint64_t)get_instance_id());
	}
}

void PhysXBuoyancy3D::set_ripple_radius(float p_radius) {
	ripple_radius = MAX(p_radius, 0.0f);
	if (is_inside_tree()) {
		_resolve();
	}
}

PackedStringArray PhysXBuoyancy3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	if (Object::cast_to<RigidBody3D>(get_parent()) == nullptr) {
		warnings.push_back(RTR("PhysXBuoyancy3D only floats a RigidBody3D: make it a child of one."));
	}
	if (water_surface_path.is_empty()) {
		warnings.push_back(RTR("Set water_surface_path to the PhysXWaterSurface3D to float on."));
	}
	return warnings;
}

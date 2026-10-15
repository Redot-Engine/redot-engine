/**************************************************************************/
/*  physx_water_spray_3d.cpp                                              */
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

#include "physx_water_spray_3d.h"

#include "physx_boat_3d.h"
#include "physx_buoyancy_3d.h"
#include "physx_water_surface_3d.h"
#include "physx_water_wake_3d.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/3d/gpu_particles_collision_3d.h"
#include "scene/3d/physics/rigid_body_3d.h"
#include "scene/resources/particle_process_material.h"

void PhysXWaterSpray3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_water_surface_path", "path"), &PhysXWaterSpray3D::set_water_surface_path);
	ClassDB::bind_method(D_METHOD("get_water_surface_path"), &PhysXWaterSpray3D::get_water_surface_path);
	ClassDB::bind_method(D_METHOD("set_bow_emitters", "paths"), &PhysXWaterSpray3D::set_bow_emitters);
	ClassDB::bind_method(D_METHOD("get_bow_emitters"), &PhysXWaterSpray3D::get_bow_emitters);
	ClassDB::bind_method(D_METHOD("set_stern_emitters", "paths"), &PhysXWaterSpray3D::set_stern_emitters);
	ClassDB::bind_method(D_METHOD("get_stern_emitters"), &PhysXWaterSpray3D::get_stern_emitters);
	ClassDB::bind_method(D_METHOD("set_slam_emitters", "paths"), &PhysXWaterSpray3D::set_slam_emitters);
	ClassDB::bind_method(D_METHOD("get_slam_emitters"), &PhysXWaterSpray3D::get_slam_emitters);
	ClassDB::bind_method(D_METHOD("set_start_speed", "speed"), &PhysXWaterSpray3D::set_start_speed);
	ClassDB::bind_method(D_METHOD("get_start_speed"), &PhysXWaterSpray3D::get_start_speed);
	ClassDB::bind_method(D_METHOD("set_reference_speed", "speed"), &PhysXWaterSpray3D::set_reference_speed);
	ClassDB::bind_method(D_METHOD("get_reference_speed"), &PhysXWaterSpray3D::get_reference_speed);
	ClassDB::bind_method(D_METHOD("set_slow_velocity_scale", "scale"), &PhysXWaterSpray3D::set_slow_velocity_scale);
	ClassDB::bind_method(D_METHOD("get_slow_velocity_scale"), &PhysXWaterSpray3D::get_slow_velocity_scale);
	ClassDB::bind_method(D_METHOD("set_submerge_depth", "depth"), &PhysXWaterSpray3D::set_submerge_depth);
	ClassDB::bind_method(D_METHOD("get_submerge_depth"), &PhysXWaterSpray3D::get_submerge_depth);
	ClassDB::bind_method(D_METHOD("set_slam_speed", "speed"), &PhysXWaterSpray3D::set_slam_speed);
	ClassDB::bind_method(D_METHOD("get_slam_speed"), &PhysXWaterSpray3D::get_slam_speed);
	ClassDB::bind_method(D_METHOD("set_slam_full_speed", "speed"), &PhysXWaterSpray3D::set_slam_full_speed);
	ClassDB::bind_method(D_METHOD("get_slam_full_speed"), &PhysXWaterSpray3D::get_slam_full_speed);
	ClassDB::bind_method(D_METHOD("set_slam_cooldown", "seconds"), &PhysXWaterSpray3D::set_slam_cooldown);
	ClassDB::bind_method(D_METHOD("get_slam_cooldown"), &PhysXWaterSpray3D::get_slam_cooldown);
	ClassDB::bind_method(D_METHOD("set_water_collision", "enabled"), &PhysXWaterSpray3D::set_water_collision);
	ClassDB::bind_method(D_METHOD("get_water_collision"), &PhysXWaterSpray3D::get_water_collision);
	ClassDB::bind_method(D_METHOD("set_collision_size", "size"), &PhysXWaterSpray3D::set_collision_size);
	ClassDB::bind_method(D_METHOD("get_collision_size"), &PhysXWaterSpray3D::get_collision_size);
	ClassDB::bind_method(D_METHOD("set_collision_fit_radius", "radius"), &PhysXWaterSpray3D::set_collision_fit_radius);
	ClassDB::bind_method(D_METHOD("get_collision_fit_radius"), &PhysXWaterSpray3D::get_collision_fit_radius);
	ClassDB::bind_method(D_METHOD("set_collision_margin", "margin"), &PhysXWaterSpray3D::set_collision_margin);
	ClassDB::bind_method(D_METHOD("get_collision_margin"), &PhysXWaterSpray3D::get_collision_margin);
	ClassDB::bind_method(D_METHOD("set_collision_smoothing", "seconds"), &PhysXWaterSpray3D::set_collision_smoothing);
	ClassDB::bind_method(D_METHOD("get_collision_smoothing"), &PhysXWaterSpray3D::get_collision_smoothing);
	ClassDB::bind_method(D_METHOD("set_spray_foam", "foam"), &PhysXWaterSpray3D::set_spray_foam);
	ClassDB::bind_method(D_METHOD("get_spray_foam"), &PhysXWaterSpray3D::get_spray_foam);
	ClassDB::bind_method(D_METHOD("set_spray_foam_density", "density"), &PhysXWaterSpray3D::set_spray_foam_density);
	ClassDB::bind_method(D_METHOD("get_spray_foam_density"), &PhysXWaterSpray3D::get_spray_foam_density);
	ClassDB::bind_method(D_METHOD("set_spray_foam_offset", "offset"), &PhysXWaterSpray3D::set_spray_foam_offset);
	ClassDB::bind_method(D_METHOD("get_spray_foam_offset"), &PhysXWaterSpray3D::get_spray_foam_offset);

	const String emitter_list_hint = vformat("%d/%d:%s", Variant::NODE_PATH, PROPERTY_HINT_NODE_PATH_VALID_TYPES, "GPUParticles3D");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "water_surface_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "PhysXWaterSurface3D"), "set_water_surface_path", "get_water_surface_path");
	ADD_GROUP("Emitters", "");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "bow_emitters", PROPERTY_HINT_TYPE_STRING, emitter_list_hint), "set_bow_emitters", "get_bow_emitters");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "stern_emitters", PROPERTY_HINT_TYPE_STRING, emitter_list_hint), "set_stern_emitters", "get_stern_emitters");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "slam_emitters", PROPERTY_HINT_TYPE_STRING, emitter_list_hint), "set_slam_emitters", "get_slam_emitters");
	ADD_GROUP("Speed", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "start_speed", PROPERTY_HINT_RANGE, "0,20,0.01,or_greater,suffix:m/s"), "set_start_speed", "get_start_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reference_speed", PROPERTY_HINT_RANGE, "0.1,50,0.1,or_greater,suffix:m/s"), "set_reference_speed", "get_reference_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slow_velocity_scale", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_slow_velocity_scale", "get_slow_velocity_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "submerge_depth", PROPERTY_HINT_RANGE, "0.01,2,0.01,suffix:m"), "set_submerge_depth", "get_submerge_depth");
	ADD_GROUP("Slam", "slam_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slam_speed", PROPERTY_HINT_RANGE, "0,20,0.01,suffix:m/s"), "set_slam_speed", "get_slam_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slam_full_speed", PROPERTY_HINT_RANGE, "0.1,30,0.01,suffix:m/s"), "set_slam_full_speed", "get_slam_full_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "slam_cooldown", PROPERTY_HINT_RANGE, "0,5,0.01,suffix:s"), "set_slam_cooldown", "get_slam_cooldown");
	ADD_GROUP("Water Collision", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "water_collision"), "set_water_collision", "get_water_collision");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_size", PROPERTY_HINT_RANGE, "0.5,100,0.1,or_greater,suffix:m"), "set_collision_size", "get_collision_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_fit_radius", PROPERTY_HINT_RANGE, "0.1,20,0.01,suffix:m"), "set_collision_fit_radius", "get_collision_fit_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_margin", PROPERTY_HINT_RANGE, "0,1,0.001,suffix:m"), "set_collision_margin", "get_collision_margin");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_smoothing", PROPERTY_HINT_RANGE, "0,1,0.01,suffix:s"), "set_collision_smoothing", "get_collision_smoothing");
	ADD_GROUP("Foam", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spray_foam", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_spray_foam", "get_spray_foam");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spray_foam_density", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_spray_foam_density", "get_spray_foam_density");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spray_foam_offset", PROPERTY_HINT_RANGE, "-2,4,0.01,suffix:m"), "set_spray_foam_offset", "get_spray_foam_offset");

	ADD_SIGNAL(MethodInfo("slammed", PropertyInfo(Variant::VECTOR3, "position"), PropertyInfo(Variant::FLOAT, "speed")));
}

RigidBody3D *PhysXWaterSpray3D::get_body() const {
	return Object::cast_to<RigidBody3D>(get_parent());
}

PhysXWaterSurface3D *PhysXWaterSpray3D::get_water_surface() const {
	return ObjectDB::get_instance<PhysXWaterSurface3D>(water_id);
}

void PhysXWaterSpray3D::_clear() {
	emitters.clear();
	water_id = ObjectID();
	wake_id = ObjectID();
	built = false;
}

void PhysXWaterSpray3D::_add_emitters(const TypedArray<NodePath> &p_paths, Role p_role) {
	const Transform3D to_body = get_body()->get_global_transform().affine_inverse();
	for (int i = 0; i < p_paths.size(); i++) {
		GPUParticles3D *particles = Object::cast_to<GPUParticles3D>(get_node_or_null(p_paths[i]));
		if (particles == nullptr) {
			continue;
		}
		Emitter emitter;
		emitter.id = particles->get_instance_id();
		emitter.role = p_role;
		emitter.rest = to_body * particles->get_global_transform();
		emitter.base_ratio = particles->get_amount_ratio();
		// Our own copy of its process material: its launch speed is scaled
		// per emitter, and the resource may be shared.
		Ref<ParticleProcessMaterial> material = particles->get_process_material();
		if (material.is_valid()) {
			material = material->duplicate();
			particles->set_process_material(material);
			emitter.material = material;
			emitter.base_velocity_min = material->get_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY);
			emitter.base_velocity_max = material->get_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY);
			emitter.inherit_velocity = material->get_inherit_velocity_ratio();
			emitter.gravity = MAX(-material->get_gravity().y, 0.1f);
			emitter.friction = material->get_collision_mode() == ParticleProcessMaterial::COLLISION_RIGID ? material->get_collision_friction() : 0.0f;
			// Where it launches from: a few spots along its emission points
			// (averaged in groups), or the emitter itself.
			const Ref<Texture2D> points_tex = material->get_emission_point_texture();
			const Ref<Texture2D> normals_tex = material->get_emission_normal_texture();
			const Ref<Image> points = points_tex.is_valid() ? points_tex->get_image() : Ref<Image>();
			const Ref<Image> normals = normals_tex.is_valid() ? normals_tex->get_image() : Ref<Image>();
			const int count = points.is_valid() ? MIN(material->get_emission_point_count(), points->get_width() * points->get_height()) : 0;
			const bool directed = material->get_emission_shape() == ParticleProcessMaterial::EMISSION_SHAPE_DIRECTED_POINTS && normals.is_valid() && normals->get_width() * normals->get_height() >= count;
			if (count > 0 && material->get_emission_shape() >= ParticleProcessMaterial::EMISSION_SHAPE_POINTS) {
				const int groups = MIN(count, 6);
				for (int g = 0; g < groups; g++) {
					Vector3 splash_position;
					Vector3 splash_direction;
					const int from = g * count / groups;
					const int to = (g + 1) * count / groups;
					for (int pi = from; pi < to; pi++) {
						const Color position_pixel = points->get_pixel(pi % points->get_width(), pi / points->get_width());
						splash_position += Vector3(position_pixel.r, position_pixel.g, position_pixel.b);
						if (directed) {
							const Color normal_pixel = normals->get_pixel(pi % normals->get_width(), pi / normals->get_width());
							splash_direction += Vector3(normal_pixel.r, normal_pixel.g, normal_pixel.b);
						}
					}
					emitter.launch_points.push_back(splash_position / MAX(to - from, 1));
					// Directed points turn the material's direction (+Z) onto
					// each point's normal: the spray there goes along it.
					emitter.launch_dirs.push_back(directed && splash_direction.length_squared() > 1e-8f ? splash_direction.normalized() : material->get_direction().normalized());
				}
			} else {
				emitter.launch_points.push_back(Vector3());
				emitter.launch_dirs.push_back(material->get_direction().normalized());
			}
		}
		if (p_role == ROLE_SLAM) {
			particles->set_emitting(false);
		} else {
			particles->set_amount_ratio(0.0f);
			particles->set_emitting(true);
		}
		emitters.push_back(emitter);
	}
}

void PhysXWaterSpray3D::_build() {
	_clear();
	built = true; // tried: not again every tick when there's no water
	RigidBody3D *body = get_body();
	if (body == nullptr) {
		return;
	}
	// The water: our own path, else the sibling buoyancy's.
	PhysXWaterSurface3D *water = Object::cast_to<PhysXWaterSurface3D>(get_node_or_null(water_surface_path));
	for (int i = 0; i < body->get_child_count(); i++) {
		Node *sibling = body->get_child(i);
		PhysXBuoyancy3D *buoyancy = Object::cast_to<PhysXBuoyancy3D>(sibling);
		if (water == nullptr && buoyancy != nullptr) {
			water = Object::cast_to<PhysXWaterSurface3D>(buoyancy->get_node_or_null(buoyancy->get_water_surface_path()));
		}
		if (Object::cast_to<PhysXWaterWake3D>(sibling) != nullptr) {
			wake_id = sibling->get_instance_id();
		}
	}
	if (water == nullptr) {
		return;
	}
	water_id = water->get_instance_id();
	_add_emitters(bow_emitters, ROLE_BOW);
	_add_emitters(stern_emitters, ROLE_STERN);
	_add_emitters(slam_emitters, ROLE_SLAM);
}

void PhysXWaterSpray3D::_update_collider(const PhysXWaterSurface3D *p_water, const Vector3 &p_center, const Basis &p_level, double p_delta) {
	have_plane = false;
	if (collider == nullptr || !water_collision) {
		return;
	}
	// A plane fitted to the water around the hull: its height under the
	// hull and its slope across collision_fit_radius each way (the waves,
	// without the hull's own wake).
	const Vector3 ax = p_level.get_column(0) * collision_fit_radius;
	const Vector3 az = p_level.get_column(2) * collision_fit_radius;
	const float water_height = p_water->sample_height_excluding_wake(p_center, wake_id);
	const float hxp = p_water->sample_height_excluding_wake(p_center + ax, wake_id);
	const float hxn = p_water->sample_height_excluding_wake(p_center - ax, wake_id);
	const float hzp = p_water->sample_height_excluding_wake(p_center + az, wake_id);
	const float hzn = p_water->sample_height_excluding_wake(p_center - az, wake_id);
	if (!Math::is_finite(water_height) || !Math::is_finite(hxp) || !Math::is_finite(hxn) || !Math::is_finite(hzp) || !Math::is_finite(hzn)) {
		// Near dry land: no plane to fit.
		collider->set_visible(false);
		have_smooth = false;
		have_collider_xform = false;
		return;
	}
	collider->set_visible(true);
	const Vector3 up(0, 1, 0);
	const Vector3 tx = (p_level.get_column(0) + up * ((hxp - hxn) / (2.0f * collision_fit_radius))).normalized();
	const Vector3 tz = p_level.get_column(2) + up * ((hzp - hzn) / (2.0f * collision_fit_radius));
	// Eased: the water's heights here update in steps (each readback), and
	// spray resting on the plane was jolted by every one.
	const Vector3 fitted = tz.cross(tx).normalized();
	const float ease = collision_smoothing > 0.0f ? 1.0f - Math::exp(-(float)p_delta / collision_smoothing) : 1.0f;
	if (!have_smooth) {
		smooth_height = water_height;
		smooth_normal = fitted;
		have_smooth = true;
	} else {
		smooth_height = Math::lerp(smooth_height, water_height, ease);
		smooth_normal = smooth_normal.lerp(fitted, ease).normalized();
	}
	const Vector3 normal = smooth_normal;
	const Vector3 lx = p_level.get_column(0);
	const Vector3 surface_tangent = (lx - normal * lx.dot(normal)).normalized();
	const Vector3 surface_bitangent = surface_tangent.cross(normal).normalized();
	const float depth = 2.0f;
	const Vector3 box_size(collision_size, depth, collision_size);
	if (!collider->get_size().is_equal_approx(box_size)) {
		collider->set_size(box_size); // only on a change: each set reaches the renderer
	}
	// The box's top face on the plane.
	plane_point = Vector3(p_center.x, smooth_height, p_center.z);
	plane_normal = normal;
	have_plane = true;
	const Transform3D placed(Basis(normal.cross(surface_bitangent), normal, surface_bitangent), plane_point - normal * (depth * 0.5f));
	collider_from = have_collider_xform ? collider_to : placed;
	collider_to = placed;
	if (!have_collider_xform) {
		collider->set_global_transform(placed);
	}
	have_collider_xform = true;
}

void PhysXWaterSpray3D::_deposit_foam(const Emitter &p_emitter, const GPUParticles3D *p_particles, const Vector3 &p_body_velocity, float p_ratio, float p_velocity_scale, double p_delta) {
	PhysXWaterWake3D *wake = ObjectDB::get_instance<PhysXWaterWake3D>(wake_id);
	if (wake == nullptr || spray_foam <= 0.0f || p_ratio <= 0.001f || p_emitter.launch_points.is_empty()) {
		return;
	}
	// Each launch spot's spray, flown out: launched at the mean speed along
	// its direction plus the share of the hull's velocity it inherits, it
	// comes back down to the water (collision_margin below where it left)
	// after airborne_time, then skids on: the material's friction takes that share of its
	// speed off each particle step (fixed_fps, else 60/s assumed) until its
	// lifetime runs out.
	const Transform3D xf = p_particles->get_global_transform();
	const float speed = 0.5f * (p_emitter.base_velocity_min + p_emitter.base_velocity_max) * p_velocity_scale;
	const float step = 1.0f / (p_particles->get_fixed_fps() > 0 ? p_particles->get_fixed_fps() : 60);
	const float slow = p_emitter.friction > 0.0f ? step / p_emitter.friction : 0.0f; // e-folding time of the skid
	for (int i = 0; i < p_emitter.launch_points.size(); i++) {
		// Specks, not a solid band: droplets stir the water here and there.
		if (rng.randf() >= spray_foam_density * p_ratio) {
			continue;
		}
		const Vector3 drop_velocity = xf.basis.xform(p_emitter.launch_dirs[i]).normalized() * speed + p_body_velocity * p_emitter.inherit_velocity;
		const float drop_gravity = p_emitter.gravity;
		const float airborne_time = (drop_velocity.y + Math::sqrt(MAX(drop_velocity.y * drop_velocity.y + 2.0f * drop_gravity * collision_margin, 0.0f))) / drop_gravity;
		// Somewhere along the landing and skid of spray landing now: launched
		// airborne_time (+ ts into the skid) ago, from where the launch point was then --
		// the hull has moved on since. (Placing it where this tick's spray will
		// land put the foam ahead of the spray.)
		const float scatter_fraction = rng.randf();
		float ts = 0.0f;
		Vector3 skid;
		if (slow > 0.0f) {
			const float left = MAX((float)p_particles->get_lifetime() - airborne_time, 0.0f);
			const float reach = scatter_fraction * (1.0f - Math::exp(-left / slow));
			ts = -slow * Math::log(1.0f - reach);
			skid = Vector3(drop_velocity.x, 0.0f, drop_velocity.z) * slow * reach;
		}
		Vector3 out(drop_velocity.x, 0.0f, drop_velocity.z);
		out = out.length_squared() > 1e-6f ? out.normalized() : Vector3();
		const Vector3 across(out.z, 0.0f, -out.x);
		const Vector3 at = xf.xform(p_emitter.launch_points[i]) + drop_velocity * airborne_time + skid - p_body_velocity * (airborne_time + ts) + out * (spray_foam_offset + rng.random(-0.15f, 0.15f)) + across * rng.random(-0.25f, 0.25f);
		// A one-off speck: its whole amount this step.
		wake->add_foam(at, rng.random(0.12f, 0.25f), spray_foam / (float)p_delta);
	}
}

void PhysXWaterSpray3D::_physics_step(double p_delta) {
	RigidBody3D *body = get_body();
	PhysXWaterSurface3D *water = get_water_surface();
	if (body == nullptr || water == nullptr || emitters.is_empty() || p_delta <= 0.0) {
		return;
	}
	const Transform3D xf = body->get_global_transform();
	const Vector3 lin = body->get_linear_velocity();
	const Vector3 ang = body->get_angular_velocity();
	// Level, turned with the hull's heading: spray leaves the water upward
	// whatever the hull's pitch and roll.
	Vector3 fwd = xf.basis.get_column(2);
	fwd.y = 0.0f;
	fwd = fwd.length_squared() > 1e-6f ? fwd.normalized() : Vector3(0, 0, 1);
	const Vector3 up(0, 1, 0);
	const Basis level(up.cross(fwd).normalized(), up, fwd);
	const float forward_speed = MAX(lin.dot(fwd), 0.0f);
	const float bow_rate = Math::smoothstep(start_speed, reference_speed, forward_speed);
	const float bow_velocity = Math::lerp(slow_velocity_scale, 1.0f, MIN(forward_speed / reference_speed, 1.0f));
	_update_collider(water, xf.origin, level, p_delta);

	// Propeller churn, from a sibling PhysXBoat3D.
	float churn = 0.0f;
	for (int i = 0; i < body->get_child_count(); i++) {
		const PhysXBoat3D *boat = Object::cast_to<PhysXBoat3D>(body->get_child(i));
		if (boat != nullptr && boat->get_max_thrust() > 0.0f) {
			churn = MIN(Math::abs(boat->get_applied_thrust()) / boat->get_max_thrust(), 1.0f);
			break;
		}
	}
	const float stern_velocity = Math::lerp(slow_velocity_scale, 1.0f, churn);

	for (Emitter &e : emitters) {
		GPUParticles3D *particles = ObjectDB::get_instance<GPUParticles3D>(e.id);
		if (particles == nullptr) {
			continue;
		}
		e.cooldown = MAX(e.cooldown - (float)p_delta, 0.0f);
		const Vector3 hull = xf.xform(e.rest.origin);
		// How far that hull point sits above the water (as the buoyancy sees
		// it, without the hull's own wake), and the drawn surface it sprays
		// from.
		const float water_level = water->sample_height_excluding_wake(hull, wake_id);
		if (!Math::is_finite(water_level)) {
			// Over dry land: nothing to spray.
			if (e.role != ROLE_SLAM) {
				particles->set_amount_ratio(0.0f);
			}
			e.have_prev = false;
			continue;
		}
		const float surface = water->sample_height(hull);
		const float gap = hull.y - water_level;
		const float wet = CLAMP(1.0f - gap / submerge_depth, 0.0f, 1.0f);
		float spawn_y = surface;
		if (have_plane && plane_normal.y > 0.1f) {
			// Clear of the collider's top, which can sit above the drawn
			// surface in the hull's own wake.
			const float plane_y = plane_point.y - (plane_normal.x * (hull.x - plane_point.x) + plane_normal.z * (hull.z - plane_point.z)) / plane_normal.y;
			spawn_y = MAX(surface, plane_y) + collision_margin;
		}
		particles->set_global_transform(Transform3D(level * e.rest.basis, Vector3(hull.x, spawn_y, hull.z)));

		float velocity_scale = 1.0f;
		switch (e.role) {
			case ROLE_BOW: {
				particles->set_amount_ratio(e.base_ratio * wet * bow_rate);
				velocity_scale = bow_velocity;
				_deposit_foam(e, particles, lin, e.base_ratio * wet * bow_rate, velocity_scale, p_delta);
			} break;
			case ROLE_STERN: {
				particles->set_amount_ratio(e.base_ratio * wet * churn);
				velocity_scale = stern_velocity;
				_deposit_foam(e, particles, lin, e.base_ratio * wet * churn, velocity_scale, p_delta);
			} break;
			case ROLE_SLAM: {
				// Dropped back onto the water: the hull point was clear of it
				// last tick and is in it now, closing faster than slam_speed.
				const float hull_vy = (lin + ang.cross(hull - xf.origin)).y;
				const float water_vy = e.have_prev ? (water_level - e.prev_water) / (float)p_delta : 0.0f;
				const float closing = water_vy - hull_vy;
				if (e.have_prev && e.prev_gap > 0.0f && gap <= 0.0f && closing > slam_speed && e.cooldown <= 0.0f) {
					const float strength = CLAMP(closing / slam_full_speed, 0.2f, 1.0f);
					particles->set_amount_ratio(e.base_ratio * strength);
					if (e.material.is_valid()) {
						const float scale = Math::lerp(slow_velocity_scale, 1.0f, strength);
						e.material->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, e.base_velocity_min * scale);
						e.material->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, e.base_velocity_max * scale);
					}
					particles->restart();
					e.cooldown = slam_cooldown;
					emit_signal(SNAME("slammed"), Vector3(hull.x, surface, hull.z), closing);
					// The splash leaves a patch of foam.
					PhysXWaterWake3D *wake = ObjectDB::get_instance<PhysXWaterWake3D>(wake_id);
					if (wake != nullptr && spray_foam > 0.0f) {
						wake->add_foam(Vector3(hull.x, surface, hull.z), 1.2f, strength / (float)p_delta);
					}
				}
				e.prev_gap = gap;
				e.prev_water = water_level;
				e.have_prev = true;
				continue;
			}
		}
		if (e.material.is_valid()) {
			const float vmax = e.base_velocity_max * velocity_scale;
			// Only on a real change: each set reaches the material's shader.
			if (Math::abs(e.material->get_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY) - vmax) > 0.01f) {
				e.material->set_param_min(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, e.base_velocity_min * velocity_scale);
				e.material->set_param_max(ParticleProcessMaterial::PARAM_INITIAL_LINEAR_VELOCITY, vmax);
			}
		}
	}
}

void PhysXWaterSpray3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(true);
				set_process_internal(true);
				// The water for the spray to land on; fitted every tick, moved
				// every frame (NOTIFICATION_INTERNAL_PROCESS).
				collider = memnew(GPUParticlesCollisionBox3D);
				collider->set_as_top_level(true);
				collider->set_physics_interpolation_mode(PHYSICS_INTERPOLATION_MODE_OFF);
				collider->set_visible(false);
				add_child(collider, false, INTERNAL_MODE_BACK);
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			// Set up on the first tick, once the water and emitters are ready.
			if (!built) {
				_build();
			}
			_physics_step(get_physics_process_delta_time());
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			// Between the last two ticks' planes: spray steps every frame, and
			// a collider that only moved each tick stair-stepped under it.
			if (collider != nullptr && have_collider_xform && collider->is_visible()) {
				const real_t physics_interp_fraction = Engine::get_singleton()->get_physics_interpolation_fraction();
				collider->set_global_transform(collider_from.interpolate_with(collider_to, physics_interp_fraction));
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_clear();
		} break;
	}
}

void PhysXWaterSpray3D::set_water_surface_path(const NodePath &p_path) {
	water_surface_path = p_path;
	built = false;
	update_configuration_warnings();
}

void PhysXWaterSpray3D::set_water_collision(bool p_enabled) {
	water_collision = p_enabled;
	if (collider != nullptr && !water_collision) {
		collider->set_visible(false);
	}
}

void PhysXWaterSpray3D::set_bow_emitters(const TypedArray<NodePath> &p_paths) {
	bow_emitters = p_paths;
	built = false;
	update_configuration_warnings();
}

void PhysXWaterSpray3D::set_stern_emitters(const TypedArray<NodePath> &p_paths) {
	stern_emitters = p_paths;
	built = false;
	update_configuration_warnings();
}

void PhysXWaterSpray3D::set_slam_emitters(const TypedArray<NodePath> &p_paths) {
	slam_emitters = p_paths;
	built = false;
	update_configuration_warnings();
}

PackedStringArray PhysXWaterSpray3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	if (Object::cast_to<RigidBody3D>(get_parent()) == nullptr) {
		warnings.push_back(RTR("PhysXWaterSpray3D follows a RigidBody3D: make it a child of one (a boat)."));
	}
	if (bow_emitters.is_empty() && stern_emitters.is_empty() && slam_emitters.is_empty()) {
		warnings.push_back(RTR("No emitters: add GPUParticles3D nodes to bow_emitters, stern_emitters or slam_emitters."));
	}
	for (int i = 0; i < slam_emitters.size(); i++) {
		const GPUParticles3D *particles = Object::cast_to<GPUParticles3D>(get_node_or_null(slam_emitters[i]));
		if (particles != nullptr && !particles->get_one_shot()) {
			warnings.push_back(RTR("Slam emitters fire a burst on each slam: turn on one_shot on them."));
			break;
		}
	}
	return warnings;
}

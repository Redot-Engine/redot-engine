/**************************************************************************/
/*  physx_chunk_emitter_3d.cpp                                            */
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

#include "physx_chunk_emitter_3d.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/time.h"
#include "scene/resources/3d/world_3d.h"
#include "servers/physics_3d/physics_server_3d.h"
#include "servers/rendering/rendering_server.h"

void PhysXChunkEmitter3D::_apply_chunk_mesh() {
	if (!multimesh.is_valid()) {
		return;
	}
	Ref<Mesh> m = chunk_mesh;
	if (m.is_null()) {
		if (default_chunk_mesh.is_null() || default_chunk_mesh_shape != chunk_shape) {
			if (chunk_shape == SHAPE_SPHERE) {
				Ref<SphereMesh> sm;
				sm.instantiate();
				sm->set_radius(0.5);
				sm->set_height(1.0);
				default_chunk_mesh = sm;
			} else {
				Ref<BoxMesh> bm;
				bm.instantiate();
				bm->set_size(Vector3(1, 1, 1));
				default_chunk_mesh = bm;
			}
			default_chunk_mesh_shape = chunk_shape;
		}
		m = default_chunk_mesh;
	}
	RenderingServer::get_singleton()->multimesh_set_mesh(multimesh, m->get_rid());
}

void PhysXChunkEmitter3D::set_chunk_mesh(const Ref<Mesh> &p_v) {
	chunk_mesh = p_v;
	_apply_chunk_mesh();
}

void PhysXChunkEmitter3D::_ensure_multimesh() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs->multimesh_get_mesh(multimesh).is_null()) {
		_apply_chunk_mesh();
	}
	if (multimesh_capacity != max_active) {
		rs->multimesh_allocate_data(multimesh, max_active, RS::MULTIMESH_TRANSFORM_3D);
		multimesh_capacity = max_active;
	}
}

void PhysXChunkEmitter3D::_free_chunk(uint32_t p_index) {
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	ps->free(chunks[p_index].body);
	ps->free(chunks[p_index].shape);
	chunks.remove_at(p_index);
}

void PhysXChunkEmitter3D::get_active_chunk_bodies(LocalVector<ChunkBody> &r_out) const {
	r_out.clear();
	if (chunks.is_empty()) {
		return;
	}
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	const bool is_sphere = chunk_shape == SHAPE_SPHERE;
	r_out.reserve(chunks.size());
	for (uint32_t i = 0; i < chunks.size(); i++) {
		PhysicsDirectBodyState3D *state = ps->body_get_direct_state(chunks[i].body);
		if (state == nullptr) {
			continue;
		}
		ChunkBody cb;
		cb.xform = state->get_transform();
		cb.velocity = state->get_linear_velocity();
		const float h = chunks[i].size * 0.5f;
		cb.half_extents = Vector3(h, h, h);
		cb.sphere = is_sphere;
		cb.id = chunks[i].id;
		r_out.push_back(cb);
	}
}

void PhysXChunkEmitter3D::apply_chunk_impulse(uint64_t p_chunk_id, const Vector3 &p_impulse) {
	// Stable-id lookup: chunk slots shift when older chunks are recycled, so an
	// array index captured earlier could aim the impulse at the wrong chunk.
	for (uint32_t i = 0; i < chunks.size(); i++) {
		if (chunks[i].id == p_chunk_id) {
			PhysicsServer3D::get_singleton()->body_apply_central_impulse(chunks[i].body, p_impulse);
			return;
		}
	}
}

void PhysXChunkEmitter3D::_sync_transforms() {
	if (multimesh.is_null()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	// Always push the count first: when the last chunk is recycled (lifetime or
	// max_active) this is what actually hides the stale instances -- otherwise
	// they stay frozen on screen until the next spawn.
	rs->multimesh_set_visible_instances(multimesh, chunks.size());
	if (chunks.is_empty()) {
		return;
	}

	const Transform3D to_local = get_global_transform().affine_inverse();
	const double now = Time::get_singleton()->get_ticks_msec() / 1000.0;
	AABB aabb;
	bool first = true;
	for (uint32_t i = 0; i < chunks.size(); i++) {
		PhysicsDirectBodyState3D *state = ps->body_get_direct_state(chunks[i].body);
		Transform3D xform = state ? to_local * state->get_transform() : Transform3D();
		if (first) {
			aabb = AABB(xform.origin, Vector3());
			first = false;
		} else {
			aabb.expand_to(xform.origin);
		}
		// Shrink toward zero over the last shrink_time seconds of natural
		// lifetime, instead of popping when the chunk is recycled. Only the
		// render scale changes — the collision shape stays full size, so a
		// mid-shrink chunk is still solid (fine at the sizes/speeds this is
		// meant for; not worth a per-frame shape resize).
		float scale = chunks[i].size;
		if (shrink_time > 0.0f) {
			const double remain = (double)lifetime - (now - chunks[i].spawn_time);
			if (remain < (double)shrink_time) {
				scale *= (float)CLAMP(remain / (double)shrink_time, 0.0, 1.0);
			}
		}
		xform.basis.scale(Vector3(scale, scale, scale));
		rs->multimesh_instance_set_transform(multimesh, i, xform);
	}
	aabb.grow_by(0.5);
	cached_aabb = aabb;
	update_gizmos();
}

void PhysXChunkEmitter3D::_spawn_one(const Vector3 &p_world_pos, const Vector3 &p_dir, const RID &p_space) {
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();

	const float size = Math::random(chunk_size_min, chunk_size_max);
	const float speed = Math::random(impulse_min, impulse_max);
	Basis rand_basis;
	rand_basis.rotate(Vector3(Math::randf(), Math::randf(), Math::randf()).normalized(), Math::random(0.0f, (float)Math::TAU));

	RID body = ps->body_create();
	ps->body_set_mode(body, PhysicsServer3D::BODY_MODE_RIGID);
	ps->body_set_collision_layer(body, collision_layer);
	ps->body_set_collision_mask(body, collision_mask);

	RID shape;
	real_t volume;
	if (chunk_shape == SHAPE_SPHERE) {
		shape = ps->sphere_shape_create();
		ps->shape_set_data(shape, size * 0.5f);
		volume = (4.0 / 3.0) * Math::PI * Math::pow((real_t)(size * 0.5f), (real_t)3.0);
	} else {
		shape = ps->box_shape_create();
		ps->shape_set_data(shape, Vector3(size, size, size) * 0.5f);
		volume = (real_t)size * (real_t)size * (real_t)size;
	}
	ps->body_add_shape(body, shape);

	const real_t mass = MAX(density * volume, (real_t)0.001);
	ps->body_set_param(body, PhysicsServer3D::BODY_PARAM_MASS, mass);
	ps->body_set_param(body, PhysicsServer3D::BODY_PARAM_FRICTION, friction);
	ps->body_set_param(body, PhysicsServer3D::BODY_PARAM_BOUNCE, bounce);
	ps->body_set_param(body, PhysicsServer3D::BODY_PARAM_LINEAR_DAMP, linear_damp);
	ps->body_set_param(body, PhysicsServer3D::BODY_PARAM_ANGULAR_DAMP, angular_damp);
	ps->body_set_state(body, PhysicsServer3D::BODY_STATE_TRANSFORM, Transform3D(rand_basis, p_world_pos));
	ps->body_set_state(body, PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY, p_dir * speed);
	Vector3 spin = Vector3(Math::random(-1.0f, 1.0f), Math::random(-1.0f, 1.0f), Math::random(-1.0f, 1.0f)) * spin_impulse;
	ps->body_set_state(body, PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY, spin);
	ps->body_set_space(body, p_space);

	Chunk chunk;
	chunk.body = body;
	chunk.shape = shape;
	chunk.spawn_time = Time::get_singleton()->get_ticks_msec() / 1000.0;
	chunk.size = size;
	chunk.id = next_chunk_id++;
	chunks.push_back(chunk);
}

void PhysXChunkEmitter3D::spawn_at(const Vector3 &p_world_pos, const Vector3 &p_direction, int p_count) {
	if (!inside_world || get_world_3d().is_null()) {
		return;
	}
	_ensure_multimesh();

	Vector3 n = p_direction;
	if (n.length_squared() < 0.0001) {
		n = Vector3(0, 1, 0);
	} else {
		n.normalize();
	}
	Vector3 tangent = (Math::abs(n.y) < 0.99 ? n.cross(Vector3(0, 1, 0)) : n.cross(Vector3(1, 0, 0))).normalized();
	Vector3 bitangent = n.cross(tangent);

	const RID space = get_world_3d()->get_space();
	const int count = p_count >= 0 ? p_count : chunk_count;

	for (int i = 0; i < count; i++) {
		const float theta = Math::deg_to_rad(Math::random(0.0f, spread_degrees));
		const float phi = Math::random(0.0f, (float)Math::TAU);
		Vector3 dir = n * Math::cos(theta) + (tangent * Math::cos(phi) + bitangent * Math::sin(phi)) * Math::sin(theta);
		dir.normalize();
		_spawn_one(p_world_pos, dir, space);
	}

	while ((int)chunks.size() > max_active) {
		_free_chunk(0); // oldest first
	}
	_sync_transforms();
}

void PhysXChunkEmitter3D::clear() {
	for (uint32_t i = 0; i < chunks.size(); i++) {
		PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
		ps->free(chunks[i].body);
		ps->free(chunks[i].shape);
	}
	chunks.clear();
	if (multimesh.is_valid()) {
		RenderingServer::get_singleton()->multimesh_set_visible_instances(multimesh, 0);
	}
}

void PhysXChunkEmitter3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			inside_world = true;
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(true);
			}
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			clear();
			inside_world = false;
			set_physics_process_internal(false);
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (emitting && emission_rate > 0.0 && inside_world && get_world_3d().is_valid()) {
				_ensure_multimesh();
				emission_accum += get_physics_process_delta_time() * emission_rate;
				const RID space = get_world_3d()->get_space();
				// Normalize once per tick so the stored direction's LENGTH does
				// not silently scale the launch speed (_spawn_one treats the
				// direction as unit and multiplies by the impulse range) --
				// same semantics as spawn_at().
				Vector3 emit_dir = emission_direction;
				if (emit_dir.length_squared() < 0.0001) {
					emit_dir = Vector3(0, 1, 0);
				} else {
					emit_dir.normalize();
				}
				while (emission_accum >= 1.0) {
					_spawn_one(get_global_position(), emit_dir, space);
					emission_accum -= 1.0;
				}
				while ((int)chunks.size() > max_active) {
					_free_chunk(0);
				}
			}
			if (!chunks.is_empty()) {
				const double now = Time::get_singleton()->get_ticks_msec() / 1000.0;
				for (int i = (int)chunks.size() - 1; i >= 0; i--) {
					if (now - chunks[i].spawn_time > lifetime) {
						_free_chunk(i);
					}
				}
				_sync_transforms();
			}
		} break;
	}
}

void PhysXChunkEmitter3D::set_chunk_count(int p_v) {
	chunk_count = MAX(p_v, 0);
}
void PhysXChunkEmitter3D::set_chunk_size_min(float p_v) {
	chunk_size_min = MAX(p_v, 0.001f);
}
void PhysXChunkEmitter3D::set_chunk_size_max(float p_v) {
	chunk_size_max = MAX(p_v, 0.001f);
}
void PhysXChunkEmitter3D::set_chunk_shape(ChunkShape p_v) {
	chunk_shape = p_v;
	if (chunk_mesh.is_null()) {
		_apply_chunk_mesh();
	}
}
void PhysXChunkEmitter3D::set_impulse_min(float p_v) {
	impulse_min = MAX(p_v, 0.0f);
}
void PhysXChunkEmitter3D::set_impulse_max(float p_v) {
	impulse_max = MAX(p_v, 0.0f);
}
void PhysXChunkEmitter3D::set_spread_degrees(float p_v) {
	spread_degrees = CLAMP(p_v, 0.0f, 180.0f);
}
void PhysXChunkEmitter3D::set_spin_impulse(float p_v) {
	spin_impulse = MAX(p_v, 0.0f);
}
void PhysXChunkEmitter3D::set_lifetime(float p_v) {
	lifetime = MAX(p_v, 0.05f);
}
void PhysXChunkEmitter3D::set_max_active(int p_v) {
	max_active = MAX(p_v, 1);
	while ((int)chunks.size() > max_active) {
		_free_chunk(0);
	}
	if (multimesh.is_valid()) {
		_ensure_multimesh();
	}
}
void PhysXChunkEmitter3D::set_collision_layer(uint32_t p_v) {
	collision_layer = p_v;
}
void PhysXChunkEmitter3D::set_collision_mask(uint32_t p_v) {
	collision_mask = p_v;
}
void PhysXChunkEmitter3D::set_density(real_t p_v) {
	density = MAX(p_v, 1.0);
}
void PhysXChunkEmitter3D::set_shrink_time(float p_v) {
	shrink_time = MAX(p_v, 0.0f);
}
void PhysXChunkEmitter3D::set_friction(real_t p_v) {
	friction = MAX(p_v, 0.0);
}
void PhysXChunkEmitter3D::set_bounce(real_t p_v) {
	bounce = CLAMP(p_v, 0.0, 1.0);
}
void PhysXChunkEmitter3D::set_linear_damp(real_t p_v) {
	linear_damp = MAX(p_v, 0.0);
}
void PhysXChunkEmitter3D::set_angular_damp(real_t p_v) {
	angular_damp = MAX(p_v, 0.0);
}
void PhysXChunkEmitter3D::set_emitting(bool p_v) {
	emitting = p_v;
}
void PhysXChunkEmitter3D::set_emission_rate(float p_v) {
	emission_rate = MAX(p_v, 0.0f);
}
void PhysXChunkEmitter3D::set_emission_direction(const Vector3 &p_v) {
	emission_direction = p_v;
}

AABB PhysXChunkEmitter3D::get_aabb() const {
	return cached_aabb;
}

void PhysXChunkEmitter3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("spawn_at", "world_position", "direction", "count"), &PhysXChunkEmitter3D::spawn_at, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("clear"), &PhysXChunkEmitter3D::clear);
	ClassDB::bind_method(D_METHOD("get_active_chunk_count"), &PhysXChunkEmitter3D::get_active_chunk_count);

	ClassDB::bind_method(D_METHOD("set_chunk_count", "count"), &PhysXChunkEmitter3D::set_chunk_count);
	ClassDB::bind_method(D_METHOD("get_chunk_count"), &PhysXChunkEmitter3D::get_chunk_count);
	ClassDB::bind_method(D_METHOD("set_chunk_size_min", "size"), &PhysXChunkEmitter3D::set_chunk_size_min);
	ClassDB::bind_method(D_METHOD("get_chunk_size_min"), &PhysXChunkEmitter3D::get_chunk_size_min);
	ClassDB::bind_method(D_METHOD("set_chunk_size_max", "size"), &PhysXChunkEmitter3D::set_chunk_size_max);
	ClassDB::bind_method(D_METHOD("get_chunk_size_max"), &PhysXChunkEmitter3D::get_chunk_size_max);
	ClassDB::bind_method(D_METHOD("set_chunk_shape", "shape"), &PhysXChunkEmitter3D::set_chunk_shape);
	ClassDB::bind_method(D_METHOD("get_chunk_shape"), &PhysXChunkEmitter3D::get_chunk_shape);
	ClassDB::bind_method(D_METHOD("set_impulse_min", "speed"), &PhysXChunkEmitter3D::set_impulse_min);
	ClassDB::bind_method(D_METHOD("get_impulse_min"), &PhysXChunkEmitter3D::get_impulse_min);
	ClassDB::bind_method(D_METHOD("set_impulse_max", "speed"), &PhysXChunkEmitter3D::set_impulse_max);
	ClassDB::bind_method(D_METHOD("get_impulse_max"), &PhysXChunkEmitter3D::get_impulse_max);
	ClassDB::bind_method(D_METHOD("set_spread_degrees", "degrees"), &PhysXChunkEmitter3D::set_spread_degrees);
	ClassDB::bind_method(D_METHOD("get_spread_degrees"), &PhysXChunkEmitter3D::get_spread_degrees);
	ClassDB::bind_method(D_METHOD("set_spin_impulse", "value"), &PhysXChunkEmitter3D::set_spin_impulse);
	ClassDB::bind_method(D_METHOD("get_spin_impulse"), &PhysXChunkEmitter3D::get_spin_impulse);
	ClassDB::bind_method(D_METHOD("set_lifetime", "seconds"), &PhysXChunkEmitter3D::set_lifetime);
	ClassDB::bind_method(D_METHOD("get_lifetime"), &PhysXChunkEmitter3D::get_lifetime);
	ClassDB::bind_method(D_METHOD("set_shrink_time", "seconds"), &PhysXChunkEmitter3D::set_shrink_time);
	ClassDB::bind_method(D_METHOD("get_shrink_time"), &PhysXChunkEmitter3D::get_shrink_time);
	ClassDB::bind_method(D_METHOD("set_friction", "value"), &PhysXChunkEmitter3D::set_friction);
	ClassDB::bind_method(D_METHOD("get_friction"), &PhysXChunkEmitter3D::get_friction);
	ClassDB::bind_method(D_METHOD("set_bounce", "value"), &PhysXChunkEmitter3D::set_bounce);
	ClassDB::bind_method(D_METHOD("get_bounce"), &PhysXChunkEmitter3D::get_bounce);
	ClassDB::bind_method(D_METHOD("set_linear_damp", "value"), &PhysXChunkEmitter3D::set_linear_damp);
	ClassDB::bind_method(D_METHOD("get_linear_damp"), &PhysXChunkEmitter3D::get_linear_damp);
	ClassDB::bind_method(D_METHOD("set_angular_damp", "value"), &PhysXChunkEmitter3D::set_angular_damp);
	ClassDB::bind_method(D_METHOD("get_angular_damp"), &PhysXChunkEmitter3D::get_angular_damp);
	ClassDB::bind_method(D_METHOD("set_max_active", "count"), &PhysXChunkEmitter3D::set_max_active);
	ClassDB::bind_method(D_METHOD("get_max_active"), &PhysXChunkEmitter3D::get_max_active);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &PhysXChunkEmitter3D::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &PhysXChunkEmitter3D::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &PhysXChunkEmitter3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysXChunkEmitter3D::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_density", "density"), &PhysXChunkEmitter3D::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &PhysXChunkEmitter3D::get_density);
	ClassDB::bind_method(D_METHOD("set_chunk_mesh", "mesh"), &PhysXChunkEmitter3D::set_chunk_mesh);
	ClassDB::bind_method(D_METHOD("get_chunk_mesh"), &PhysXChunkEmitter3D::get_chunk_mesh);
	ClassDB::bind_method(D_METHOD("set_emitting", "enabled"), &PhysXChunkEmitter3D::set_emitting);
	ClassDB::bind_method(D_METHOD("is_emitting"), &PhysXChunkEmitter3D::is_emitting);
	ClassDB::bind_method(D_METHOD("set_emission_rate", "rate"), &PhysXChunkEmitter3D::set_emission_rate);
	ClassDB::bind_method(D_METHOD("get_emission_rate"), &PhysXChunkEmitter3D::get_emission_rate);
	ClassDB::bind_method(D_METHOD("set_emission_direction", "direction"), &PhysXChunkEmitter3D::set_emission_direction);
	ClassDB::bind_method(D_METHOD("get_emission_direction"), &PhysXChunkEmitter3D::get_emission_direction);

	ADD_GROUP("Chunks", "chunk_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_count", PROPERTY_HINT_RANGE, "1,64,1"), "set_chunk_count", "get_chunk_count");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "chunk_size_min", PROPERTY_HINT_RANGE, "0.01,1,0.01,suffix:m"), "set_chunk_size_min", "get_chunk_size_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "chunk_size_max", PROPERTY_HINT_RANGE, "0.01,1,0.01,suffix:m"), "set_chunk_size_max", "get_chunk_size_max");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_shape", PROPERTY_HINT_ENUM, "Box,Sphere"), "set_chunk_shape", "get_chunk_shape");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "chunk_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_chunk_mesh", "get_chunk_mesh");

	ADD_GROUP("Motion", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "impulse_min", PROPERTY_HINT_RANGE, "0,40,0.1"), "set_impulse_min", "get_impulse_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "impulse_max", PROPERTY_HINT_RANGE, "0,40,0.1"), "set_impulse_max", "get_impulse_max");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spread_degrees", PROPERTY_HINT_RANGE, "0,180,0.5,degrees"), "set_spread_degrees", "get_spread_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spin_impulse", PROPERTY_HINT_RANGE, "0,30,0.1"), "set_spin_impulse", "get_spin_impulse");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density", PROPERTY_HINT_RANGE, "1,20000,1,suffix:kg/m³"), "set_density", "get_density");

	ADD_GROUP("Budget", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lifetime", PROPERTY_HINT_RANGE, "0.1,30,0.1,suffix:s"), "set_lifetime", "get_lifetime");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shrink_time", PROPERTY_HINT_RANGE, "0,5,0.05,suffix:s"), "set_shrink_time", "get_shrink_time");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_active", PROPERTY_HINT_RANGE, "1,2000,1"), "set_max_active", "get_max_active");

	ADD_GROUP("Chunk Body", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "friction", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_friction", "get_friction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bounce", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_bounce", "get_bounce");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "linear_damp", PROPERTY_HINT_RANGE, "0,10,0.01"), "set_linear_damp", "get_linear_damp");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angular_damp", PROPERTY_HINT_RANGE, "0,10,0.01"), "set_angular_damp", "get_angular_damp");

	ADD_GROUP("Continuous Emission", "emission_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emitting"), "set_emitting", "is_emitting");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_rate", PROPERTY_HINT_RANGE, "0,200,0.1,suffix:chunks/s"), "set_emission_rate", "get_emission_rate");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emission_direction"), "set_emission_direction", "get_emission_direction");

	ADD_GROUP("Collision", "collision_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");

	BIND_ENUM_CONSTANT(SHAPE_BOX);
	BIND_ENUM_CONSTANT(SHAPE_SPHERE);
}

PhysXChunkEmitter3D::PhysXChunkEmitter3D() {
	multimesh = RenderingServer::get_singleton()->multimesh_create();
	set_base(multimesh);
}

PhysXChunkEmitter3D::~PhysXChunkEmitter3D() {
	clear();
	set_base(RID());
	if (multimesh.is_valid()) {
		RenderingServer::get_singleton()->free(multimesh);
	}
}

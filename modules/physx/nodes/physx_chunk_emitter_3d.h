/**************************************************************************/
/*  physx_chunk_emitter_3d.h                                              */
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

#include "core/templates/local_vector.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/mesh.h"
#include "scene/resources/multimesh.h"

// A general-purpose burst of small rigid-body chunks, rendered as one
// MultiMesh -- impact debris (a bullet hit, an explosion, a footstep), an
// exploding crate, hail, a rockslide, confetti that actually collides --
// anything that wants many small solid things flying and settling for real.
// call spawn_at() for a one-off burst (bias it toward a surface normal for
// impacts, or use spread_degrees=180 for an omnidirectional explosion), or
// turn on `emitting` for a steady stream from the node's own position.
//
// Real PhysicsServer3D bodies, not a particle effect -- they land on slopes
// and pile up, collide with (and can be occluded/blocked by) the rest of the
// world, and interact through the normal collision_layer/mask rules. No
// per-chunk Node.
//
// The chunks are ordinary dynamic rigid bodies, created through the generic
// PhysicsServer3D RID API -- this needs no PhysX-specific code and works on
// any 3D physics backend. On this module, with a physx_gpu=yes build and a
// CUDA device, they ride the same GPU rigid-body dynamics as everything else
// in the scene automatically (the whole PxScene is GPU-accelerated, not
// individual actors), which is what makes a high chunk_count/max_active
// affordable; on the CPU path (or another backend) lower them, the same way
// NVIDIA-PhysX-era games scaled their debris counts down without a
// supporting GPU.
class PhysXChunkEmitter3D : public GeometryInstance3D {
	GDCLASS(PhysXChunkEmitter3D, GeometryInstance3D);

public:
	enum ChunkShape {
		SHAPE_BOX,
		SHAPE_SPHERE,
	};

private:
	struct Chunk {
		RID body;
		RID shape;
		double spawn_time = 0.0;
		float size = 0.1; // full edge length / diameter, for the render transform's scale
		uint64_t id = 0; // stable id (get_active_chunk_bodies -> apply_chunk_impulse)
	};

	uint64_t next_chunk_id = 1; // 0 is never handed out, so 0 reads as "no chunk"

	int chunk_count = 14; // spawned per spawn_at() call
	float chunk_size_min = 0.06;
	float chunk_size_max = 0.16;
	ChunkShape chunk_shape = SHAPE_BOX;
	float impulse_min = 3.0;
	float impulse_max = 8.0;
	float spread_degrees = 55.0; // cone half-angle around the burst direction; 180 = omnidirectional
	float spin_impulse = 6.0; // max random angular velocity, rad/s
	float lifetime = 5.0; // seconds before a chunk is recycled even under budget
	// Seconds before end-of-lifetime a chunk scales to 0 instead of popping;
	// 0 = pop (default). Render scale only — the collision shape stays full
	// size, so a mid-shrink chunk is still solid.
	float shrink_time = 0.0;
	// Surface/simulation params applied to every spawned chunk body. Crank
	// friction/damp up for something that should pile and stop.
	real_t friction = 0.5;
	real_t bounce = 0.0;
	real_t linear_damp = 0.0;
	real_t angular_damp = 0.0;
	int max_active = 200; // hard cap; oldest chunks are freed to make room
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	real_t density = 1200.0; // kg/m^3, used to derive chunk mass from its volume

	bool emitting = false; // continuous stream from the node's own position
	float emission_rate = 10.0; // chunks/sec while emitting
	Vector3 emission_direction = Vector3(0, 1, 0);
	double emission_accum = 0.0;

	RID multimesh;
	LocalVector<Chunk> chunks;
	AABB cached_aabb = AABB(Vector3(-0.5, -0.5, -0.5), Vector3(1, 1, 1));
	bool inside_world = false;
	int multimesh_capacity = 0;

	Ref<Mesh> chunk_mesh; // if unset, a unit BoxMesh (or sphere, matching chunk_shape) is used
	Ref<Mesh> default_chunk_mesh;
	ChunkShape default_chunk_mesh_shape = SHAPE_BOX;

	void _apply_chunk_mesh();
	void _ensure_multimesh();
	void _free_chunk(uint32_t p_index);
	void _sync_transforms();
	void _spawn_one(const Vector3 &p_world_pos, const Vector3 &p_dir, const RID &p_space);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_chunk_count(int p_v);
	int get_chunk_count() const { return chunk_count; }
	void set_chunk_size_min(float p_v);
	float get_chunk_size_min() const { return chunk_size_min; }
	void set_chunk_size_max(float p_v);
	float get_chunk_size_max() const { return chunk_size_max; }
	void set_chunk_shape(ChunkShape p_v);
	ChunkShape get_chunk_shape() const { return chunk_shape; }
	void set_impulse_min(float p_v);
	float get_impulse_min() const { return impulse_min; }
	void set_impulse_max(float p_v);
	float get_impulse_max() const { return impulse_max; }
	void set_spread_degrees(float p_v);
	float get_spread_degrees() const { return spread_degrees; }
	void set_spin_impulse(float p_v);
	float get_spin_impulse() const { return spin_impulse; }
	void set_lifetime(float p_v);
	float get_lifetime() const { return lifetime; }
	void set_shrink_time(float p_v);
	float get_shrink_time() const { return shrink_time; }
	void set_friction(real_t p_v);
	real_t get_friction() const { return friction; }
	void set_bounce(real_t p_v);
	real_t get_bounce() const { return bounce; }
	void set_linear_damp(real_t p_v);
	real_t get_linear_damp() const { return linear_damp; }
	void set_angular_damp(real_t p_v);
	real_t get_angular_damp() const { return angular_damp; }
	void set_max_active(int p_v);
	int get_max_active() const { return max_active; }
	void set_collision_layer(uint32_t p_v);
	uint32_t get_collision_layer() const { return collision_layer; }
	void set_collision_mask(uint32_t p_v);
	uint32_t get_collision_mask() const { return collision_mask; }
	void set_density(real_t p_v);
	real_t get_density() const { return density; }
	void set_chunk_mesh(const Ref<Mesh> &p_v);
	Ref<Mesh> get_chunk_mesh() const { return chunk_mesh; }

	// Live state of the active chunks, for other systems that want to couple
	// against the debris (e.g. the MPM fluid). All in world space.
	struct ChunkBody {
		Transform3D xform;
		Vector3 velocity;
		Vector3 half_extents; // box: half size; sphere: x = radius
		bool sphere = false;
		// Stable across recycles (unlike the array slot): pass it back to
		// apply_chunk_impulse(). A chunk freed before the call is a no-op,
		// never an impulse aimed at the wrong chunk.
		uint64_t id = 0;
	};
	void get_active_chunk_bodies(LocalVector<ChunkBody> &r_out) const;
	void apply_chunk_impulse(uint64_t p_chunk_id, const Vector3 &p_impulse);

	void set_emitting(bool p_v);
	bool is_emitting() const { return emitting; }
	void set_emission_rate(float p_v);
	float get_emission_rate() const { return emission_rate; }
	void set_emission_direction(const Vector3 &p_v);
	Vector3 get_emission_direction() const { return emission_direction; }

	// Bursts p_count chunks (chunk_count if < 0) from p_world_pos, biased along
	// p_direction within a spread_degrees cone (spread_degrees=180 scatters
	// them in every direction, for a non-directional burst like an explosion).
	void spawn_at(const Vector3 &p_world_pos, const Vector3 &p_direction, int p_count = -1);
	void clear();
	int get_active_chunk_count() const { return chunks.size(); }

	AABB get_aabb() const override;

	PhysXChunkEmitter3D();
	~PhysXChunkEmitter3D();
};

VARIANT_ENUM_CAST(PhysXChunkEmitter3D::ChunkShape);

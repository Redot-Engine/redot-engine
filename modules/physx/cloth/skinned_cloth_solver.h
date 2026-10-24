/**************************************************************************/
/*  skinned_cloth_solver.h                                                */
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

#include "core/math/transform_3d.h"
#include "core/object/ref_counted.h"
#include "core/templates/rid.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/variant.h"

class RenderingDevice;

// Character cloth on the RenderingDevice (any GPU with compute): see
// skinned_cloth.glsl. Built from per-particle data prepared on the CPU by
// PhysXSkinnedCloth3D; stepped once per frame with that frame's bone
// matrices and body capsules. Results land in two RGBA32F textures
// (position, normal) the render material reads directly.
class SkinnedClothSolverGPU : public RefCounted {
	GDSOFTCLASS(SkinnedClothSolverGPU, RefCounted);

public:
	RenderingDevice *rd = nullptr;
	bool local = false;
	SafeFlag built;

	static constexpr int MAX_CAPSULES = 64;

	RID shader, pipeline;
	RID buf_params, buf_rest, buf_skin_idx, buf_skin_w, buf_bones, buf_target, buf_target_prev;
	RID buf_pos, buf_prev, buf_constraints, buf_tethers, buf_capsules, buf_adj_offsets, buf_adj_pairs;
	RID tex_pos, tex_nrm;
	RID buf_rest_normal, buf_target_normal;
	RID buf_cell_head, buf_cell_next, buf_self_delta;
	int hash_size = 1; // self-collision hash table, power of two
	RID uniform_set;

	int particle_count = 0;
	int bone_count = 0;
	int tex_width = 0;
	int tex_height = 0;
	Vector<int> batch_offsets; // constraint colour batches: [start0, start1, ..., total]
	bool needs_reset = true;

	void rt_compile(Ref<SkinnedClothSolverGPU> p_self);
	void rt_build(Ref<SkinnedClothSolverGPU> p_self, PackedFloat32Array p_rest, PackedFloat32Array p_rest_normals, PackedInt32Array p_skin_idx, PackedFloat32Array p_skin_w,
			PackedFloat32Array p_constraints, PackedInt32Array p_batch_offsets, PackedFloat32Array p_tethers,
			PackedInt32Array p_adj_offsets, PackedInt32Array p_adj_pairs, int p_bone_count);
	void rt_step(Ref<SkinnedClothSolverGPU> p_self, PackedFloat32Array p_bones, PackedFloat32Array p_capsules, PackedFloat32Array p_params, int p_substeps);
	void rt_reset(Ref<SkinnedClothSolverGPU> p_self);
	void rt_free_buffers();
	void rt_free(Ref<SkinnedClothSolverGPU> p_self);

	~SkinnedClothSolverGPU();
};

class SkinnedClothSolver {
	Ref<SkinnedClothSolverGPU> gpu;
	void _dispatch(const Callable &p_call) const;

public:
	struct Settings {
		float stiffness = 0.9f; // 0..1, applied per constraint pass on top of each constraint's own stiffness
		float damping = 0.02f; // fraction of velocity lost per second
		Vector3 gravity = Vector3(0, -9.8f, 0);
		float thickness = 0.01f;
		float tether_slack = 1.03f; // a particle may drift this far beyond its rest distance to its anchor
		float tether_stiffness = 1.0f;
		float friction = 0.2f;
		float animation_drive = 0.0f; // fraction of the way back to the animated pose per 1/60 s
		float self_collision_thickness = 0.0f; // > 0: particles keep this far apart (self collision)
		float backstop = 0.02f; // how far (m) the cloth may sink inward past its animated position; < 0 = off
		Transform3D output_xform; // world -> the space the output textures are written in
	};

	bool is_available() const { return gpu.is_valid() && gpu->rd != nullptr; }

	// Particle data, flattened: rest xyz + max distance per particle (4
	// floats), skin indices/weights (4 per particle), constraints (a, b as
	// float bits, rest length, stiffness) sorted into colour batches,
	// tethers (anchor as float bits or ~0u, rest distance), triangle
	// adjacency for normals.
	void build(const PackedFloat32Array &p_rest, const PackedFloat32Array &p_rest_normals, const PackedInt32Array &p_skin_idx, const PackedFloat32Array &p_skin_w,
			const PackedFloat32Array &p_constraints, const PackedInt32Array &p_batch_offsets, const PackedFloat32Array &p_tethers,
			const PackedInt32Array &p_adj_offsets, const PackedInt32Array &p_adj_pairs, int p_bone_count);
	// p_bones: 16 floats (column-major mat4) per skin bind, world space.
	// p_capsules: 16 floats per capsule -- previous frame (a.xyz, ra, b.xyz,
	// rb) then this frame's -- world space, blended across the substeps.
	void step(const PackedFloat32Array &p_bones, const PackedFloat32Array &p_capsules, double p_delta, int p_substeps, const Settings &p_settings);
	void reset();

	RID get_position_texture_rd_rid() const;
	RID get_normal_texture_rd_rid() const;
	int get_texture_width() const;

	SkinnedClothSolver();
	~SkinnedClothSolver();
};

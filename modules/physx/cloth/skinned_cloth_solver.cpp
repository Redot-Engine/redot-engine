/**************************************************************************/
/*  skinned_cloth_solver.cpp                                              */
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

#include "skinned_cloth_solver.h"

#include "skinned_cloth.glsl.gen.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_method_pointer.h"
#include "servers/display/display_server.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_device_binds.h"
#include "servers/rendering/rendering_server.h"

namespace {
constexpr int PARAMS_BYTES = 144; // 9 * vec4, std140 -- see skinned_cloth.glsl's Params
constexpr uint32_t GROUP = 64; // local_size_x

uint32_t groups_for(int p_count) {
	return (uint32_t)((MAX(p_count, 1) + (int)GROUP - 1) / (int)GROUP);
}

enum Mode {
	MODE_SKIN = 0,
	MODE_INTEGRATE = 1,
	MODE_DISTANCE = 2,
	MODE_LIMITS = 3,
	MODE_OUTPUT = 4,
	MODE_RESET = 5,
	MODE_HASH_CLEAR = 6,
	MODE_HASH_INSERT = 7,
	MODE_SELF = 8,
	MODE_SELF_APPLY = 9,
};

struct PassConstants {
	int32_t mode;
	int32_t batch_start;
	int32_t batch_count;
	float frac;
};

RID storage_from(RenderingDevice *p_rd, const uint8_t *p_data, int p_bytes) {
	Vector<uint8_t> bytes;
	bytes.resize(MAX(p_bytes, 16));
	memset(bytes.ptrw(), 0, bytes.size());
	if (p_data != nullptr && p_bytes > 0) {
		memcpy(bytes.ptrw(), p_data, p_bytes);
	}
	return p_rd->storage_buffer_create(bytes.size(), bytes);
}
} // namespace

/* ===================================================================== */
/*  SkinnedClothSolverGPU                                                 */
/* ===================================================================== */

SkinnedClothSolverGPU::~SkinnedClothSolverGPU() {
}

void SkinnedClothSolverGPU::rt_compile(Ref<SkinnedClothSolverGPU> p_self) {
	if (rd == nullptr) {
		return;
	}
	Ref<RDShaderFile> sf;
	sf.instantiate();
	if (sf->parse_versions_from_text(skinned_cloth_shader_glsl) != OK) {
		ERR_PRINT("SkinnedClothSolver: the compute shader failed to compile.");
		return;
	}
	shader = rd->shader_create_from_spirv(sf->get_spirv_stages());
	ERR_FAIL_COND(shader.is_null());
	pipeline = rd->compute_pipeline_create(shader);
}

void SkinnedClothSolverGPU::rt_free_buffers() {
	if (rd == nullptr) {
		return;
	}
	built.clear();
	if (uniform_set.is_valid() && rd->uniform_set_is_valid(uniform_set)) {
		rd->free(uniform_set);
	}
	uniform_set = RID();
	RID *rids[] = { &buf_params, &buf_rest, &buf_skin_idx, &buf_skin_w, &buf_bones, &buf_target, &buf_target_prev,
		&buf_pos, &buf_prev, &buf_constraints, &buf_tethers, &buf_capsules, &buf_adj_offsets, &buf_adj_pairs, &tex_pos, &tex_nrm,
		&buf_rest_normal, &buf_target_normal, &buf_cell_head, &buf_cell_next, &buf_self_delta };
	for (RID *r : rids) {
		if (r->is_valid()) {
			rd->free(*r);
			*r = RID();
		}
	}
}

void SkinnedClothSolverGPU::rt_free(Ref<SkinnedClothSolverGPU> p_self) {
	rt_free_buffers();
	if (rd == nullptr) {
		return;
	}
	if (pipeline.is_valid()) {
		rd->free(pipeline);
		pipeline = RID();
	}
	if (shader.is_valid()) {
		rd->free(shader);
		shader = RID();
	}
}

void SkinnedClothSolverGPU::rt_build(Ref<SkinnedClothSolverGPU> p_self, PackedFloat32Array p_rest, PackedFloat32Array p_rest_normals, PackedInt32Array p_skin_idx, PackedFloat32Array p_skin_w,
		PackedFloat32Array p_constraints, PackedInt32Array p_batch_offsets, PackedFloat32Array p_tethers,
		PackedInt32Array p_adj_offsets, PackedInt32Array p_adj_pairs, int p_bone_count) {
	if (rd == nullptr || shader.is_null()) {
		return;
	}
	rt_free_buffers();
	particle_count = p_rest.size() / 4;
	bone_count = MAX(p_bone_count, 1);
	batch_offsets = p_batch_offsets;
	if (particle_count == 0) {
		return;
	}
	const int n = particle_count;

	buf_params = rd->uniform_buffer_create(PARAMS_BYTES);
	buf_rest = storage_from(rd, (const uint8_t *)p_rest.ptr(), p_rest.size() * 4);
	buf_skin_idx = storage_from(rd, (const uint8_t *)p_skin_idx.ptr(), p_skin_idx.size() * 4);
	buf_skin_w = storage_from(rd, (const uint8_t *)p_skin_w.ptr(), p_skin_w.size() * 4);
	buf_bones = storage_from(rd, nullptr, bone_count * 64);
	buf_target = storage_from(rd, nullptr, n * 16);
	buf_target_prev = storage_from(rd, nullptr, n * 16);
	buf_pos = storage_from(rd, nullptr, n * 16);
	buf_prev = storage_from(rd, nullptr, n * 16);
	buf_constraints = storage_from(rd, (const uint8_t *)p_constraints.ptr(), p_constraints.size() * 4);
	buf_tethers = storage_from(rd, (const uint8_t *)p_tethers.ptr(), p_tethers.size() * 4);
	buf_capsules = storage_from(rd, nullptr, MAX_CAPSULES * 64);
	buf_rest_normal = storage_from(rd, (const uint8_t *)p_rest_normals.ptr(), p_rest_normals.size() * 4);
	buf_target_normal = storage_from(rd, nullptr, n * 16);
	buf_adj_offsets = storage_from(rd, (const uint8_t *)p_adj_offsets.ptr(), p_adj_offsets.size() * 4);
	buf_adj_pairs = storage_from(rd, (const uint8_t *)p_adj_pairs.ptr(), p_adj_pairs.size() * 4);

	hash_size = 1;
	while (hash_size < n * 2) {
		hash_size <<= 1;
	}
	buf_cell_head = storage_from(rd, nullptr, hash_size * 4);
	buf_cell_next = storage_from(rd, nullptr, n * 4);
	buf_self_delta = storage_from(rd, nullptr, n * 16);

	tex_width = MIN(n, 1024);
	tex_height = (n + tex_width - 1) / tex_width;
	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
	tf.width = tex_width;
	tf.height = tex_height;
	tf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	Vector<uint8_t> zeros;
	zeros.resize(tex_width * tex_height * 16);
	memset(zeros.ptrw(), 0, zeros.size());
	Vector<Vector<uint8_t>> data;
	data.push_back(zeros);
	tex_pos = rd->texture_create(tf, RD::TextureView(), data);
	tex_nrm = rd->texture_create(tf, RD::TextureView(), data);

	Vector<RD::Uniform> uniforms;
	{
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u.binding = 0;
		u.append_id(buf_params);
		uniforms.push_back(u);
	}
	const RID storages[] = { buf_rest, buf_skin_idx, buf_skin_w, buf_bones, buf_target, buf_target_prev, buf_pos, buf_prev,
		buf_constraints, buf_tethers, buf_capsules, buf_adj_offsets, buf_adj_pairs };
	for (int i = 0; i < 13; i++) {
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.binding = i + 1;
		u.append_id(storages[i]);
		uniforms.push_back(u);
	}
	const RID images[] = { tex_pos, tex_nrm };
	for (int i = 0; i < 2; i++) {
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_IMAGE;
		u.binding = 14 + i;
		u.append_id(images[i]);
		uniforms.push_back(u);
	}
	const RID normals[] = { buf_rest_normal, buf_target_normal, buf_cell_head, buf_cell_next, buf_self_delta };
	for (int i = 0; i < 5; i++) {
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.binding = 16 + i;
		u.append_id(normals[i]);
		uniforms.push_back(u);
	}
	uniform_set = rd->uniform_set_create(uniforms, shader, 0);
	needs_reset = true;
	built.set();
}

void SkinnedClothSolverGPU::rt_reset(Ref<SkinnedClothSolverGPU> p_self) {
	needs_reset = true;
}

void SkinnedClothSolverGPU::rt_step(Ref<SkinnedClothSolverGPU> p_self, PackedFloat32Array p_bones, PackedFloat32Array p_capsules, PackedFloat32Array p_params, int p_substeps) {
	if (rd == nullptr || !built.is_set() || uniform_set.is_null()) {
		return;
	}
	const int n = particle_count;
	rd->buffer_update(buf_bones, 0, MIN(p_bones.size(), bone_count * 16) * 4, p_bones.ptr());
	if (!p_capsules.is_empty()) {
		rd->buffer_update(buf_capsules, 0, MIN(p_capsules.size(), MAX_CAPSULES * 16) * 4, p_capsules.ptr());
	}
	// Params: counts (particles), counts2 (capsules, tex width, bones), then
	// the float block the caller packed (step, gravity, limits, output
	// transform rows = 24 floats).
	uint8_t params[PARAMS_BYTES];
	memset(params, 0, sizeof(params));
	int32_t counts[8] = { n, hash_size, MAX(p_substeps, 1), 0, MIN((int)p_capsules.size() / 16, MAX_CAPSULES), tex_width, bone_count, 0 };
	memcpy(params, counts, sizeof(counts));
	memcpy(params + 32, p_params.ptr(), MIN(p_params.size(), 28) * 4);
	const bool self_collision = p_params.size() > 24 && p_params[24] > 0.0f;
	rd->buffer_update(buf_params, 0, PARAMS_BYTES, params);

	PassConstants pc = { MODE_SKIN, 0, 0, 0.0f };
	RD::ComputeListID cl = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(cl, pipeline);
	rd->compute_list_bind_uniform_set(cl, uniform_set, 0);
	auto run = [&](int p_mode, int p_start, int p_count, float p_frac, int p_threads) {
		pc.mode = p_mode;
		pc.batch_start = p_start;
		pc.batch_count = p_count;
		pc.frac = p_frac;
		rd->compute_list_set_push_constant(cl, &pc, sizeof(pc));
		rd->compute_list_dispatch(cl, groups_for(p_threads), 1, 1);
		rd->compute_list_add_barrier(cl);
	};
	if (needs_reset) {
		run(MODE_RESET, 0, 0, 1.0f, n);
		needs_reset = false;
	} else {
		run(MODE_SKIN, 0, 0, 1.0f, n);
	}
	const int substeps = MAX(p_substeps, 1);
	const int batches = batch_offsets.size() - 1;
	for (int s = 0; s < substeps; s++) {
		const float frac = (float)(s + 1) / (float)substeps;
		run(MODE_INTEGRATE, 0, 0, frac, n);
		for (int b = 0; b < batches; b++) {
			const int start = batch_offsets[b];
			const int count = batch_offsets[b + 1] - start;
			if (count > 0) {
				run(MODE_DISTANCE, start, count, frac, count);
			}
		}
		if (self_collision) {
			run(MODE_HASH_CLEAR, 0, 0, frac, hash_size);
			run(MODE_HASH_INSERT, 0, 0, frac, n);
			run(MODE_SELF, 0, 0, frac, n);
			run(MODE_SELF_APPLY, 0, 0, frac, n);
		}
		run(MODE_LIMITS, 0, 0, frac, n);
	}
	run(MODE_OUTPUT, 0, 0, 1.0f, n);
	rd->compute_list_end();
	if (local) {
		rd->submit();
		rd->sync();
	}
}

/* ===================================================================== */
/*  SkinnedClothSolver                                                    */
/* ===================================================================== */

SkinnedClothSolver::SkinnedClothSolver() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		return;
	}
	gpu.instantiate();
	gpu->rd = rs->get_rendering_device();
	if (gpu->rd == nullptr) {
		if (!DisplayServer::can_create_rendering_device()) {
			gpu.unref();
			return;
		}
		gpu->rd = rs->create_local_rendering_device();
		if (gpu->rd == nullptr) {
			gpu.unref();
			return;
		}
		gpu->local = true;
	}
	_dispatch(callable_mp(gpu.ptr(), &SkinnedClothSolverGPU::rt_compile).bind(gpu));
}

SkinnedClothSolver::~SkinnedClothSolver() {
	if (gpu.is_valid() && gpu->rd != nullptr) {
		if (gpu->local) {
			gpu->rt_free(gpu);
			memdelete(gpu->rd);
			gpu->rd = nullptr;
		} else {
			RenderingServer *rs = RenderingServer::get_singleton();
			rs->call_on_render_thread(callable_mp(gpu.ptr(), &SkinnedClothSolverGPU::rt_free).bind(gpu));
			rs->sync();
		}
	}
	gpu.unref();
}

void SkinnedClothSolver::_dispatch(const Callable &p_call) const {
	if (gpu.is_null()) {
		return;
	}
	if (gpu->local) {
		p_call.call();
	} else {
		RenderingServer::get_singleton()->call_on_render_thread(p_call);
	}
}

void SkinnedClothSolver::build(const PackedFloat32Array &p_rest, const PackedFloat32Array &p_rest_normals, const PackedInt32Array &p_skin_idx, const PackedFloat32Array &p_skin_w,
		const PackedFloat32Array &p_constraints, const PackedInt32Array &p_batch_offsets, const PackedFloat32Array &p_tethers,
		const PackedInt32Array &p_adj_offsets, const PackedInt32Array &p_adj_pairs, int p_bone_count) {
	_dispatch(callable_mp(gpu.ptr(), &SkinnedClothSolverGPU::rt_build).bind(gpu, p_rest, p_rest_normals, p_skin_idx, p_skin_w, p_constraints, p_batch_offsets, p_tethers, p_adj_offsets, p_adj_pairs, p_bone_count));
	if (gpu.is_valid() && !gpu->local) {
		// Texture RIDs must exist before the caller wraps them.
		RenderingServer::get_singleton()->sync();
	}
}

void SkinnedClothSolver::step(const PackedFloat32Array &p_bones, const PackedFloat32Array &p_capsules, double p_delta, int p_substeps, const Settings &p_settings) {
	const int substeps = MAX(p_substeps, 1);
	const float dt = (float)(MIN(p_delta, 1.0 / 20.0) / substeps);
	// Velocity kept per substep from a per-second damping fraction.
	const float keep = Math::pow(1.0f - CLAMP(p_settings.damping, 0.0f, 0.999f), dt);
	PackedFloat32Array params;
	params.resize(28);
	params.fill(0.0f);
	float *w = params.ptrw();
	w[0] = dt;
	w[1] = keep;
	// Animation drive given per 1/60 s, applied per substep.
	w[2] = 1.0f - Math::pow(1.0f - CLAMP(p_settings.animation_drive, 0.0f, 0.999f), dt * 60.0f);
	w[3] = p_settings.stiffness;
	w[4] = p_settings.gravity.x;
	w[5] = p_settings.gravity.y;
	w[6] = p_settings.gravity.z;
	w[7] = p_settings.thickness;
	w[8] = p_settings.tether_slack;
	w[9] = p_settings.tether_stiffness;
	w[10] = p_settings.friction;
	w[11] = p_settings.backstop;
	w[24] = MAX(p_settings.self_collision_thickness, 0.0f);
	const Transform3D &o = p_settings.output_xform;
	for (int r = 0; r < 3; r++) {
		w[12 + r * 4 + 0] = o.basis.rows[r].x;
		w[12 + r * 4 + 1] = o.basis.rows[r].y;
		w[12 + r * 4 + 2] = o.basis.rows[r].z;
		w[12 + r * 4 + 3] = o.origin[r];
	}
	_dispatch(callable_mp(gpu.ptr(), &SkinnedClothSolverGPU::rt_step).bind(gpu, p_bones, p_capsules, params, substeps));
}

void SkinnedClothSolver::reset() {
	_dispatch(callable_mp(gpu.ptr(), &SkinnedClothSolverGPU::rt_reset).bind(gpu));
}

RID SkinnedClothSolver::get_position_texture_rd_rid() const {
	return gpu.is_valid() ? gpu->tex_pos : RID();
}

RID SkinnedClothSolver::get_normal_texture_rd_rid() const {
	return gpu.is_valid() ? gpu->tex_nrm : RID();
}

int SkinnedClothSolver::get_texture_width() const {
	return gpu.is_valid() ? gpu->tex_width : 0;
}

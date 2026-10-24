/**************************************************************************/
/*  gas_solver.cpp                                                        */
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

#include "gas_solver.h"

#include "shaders/gas_bs_clear.glsl.gen.h"
#include "shaders/gas_bs_clear_range.glsl.gen.h"
#include "shaders/gas_bs_curl.glsl.gen.h"
#include "shaders/gas_bs_divergence.glsl.gen.h"
#include "shaders/gas_bs_jacobi.glsl.gen.h"
#include "shaders/gas_bs_project.glsl.gen.h"
#include "shaders/gas_bs_step.glsl.gen.h"
#include "shaders/gas_bs_touch.glsl.gen.h"

#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "servers/display/display_server.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_device_binds.h"
#include "servers/rendering/rendering_server.h"

namespace {
constexpr int BLK = 4;
constexpr int BCELLS = 64;
constexpr int GROUP = 64;
// 5 * vec4 (dt_buoy_vort_diss/origin_dx_pad/box_blocks_cells/hash_maxb_time_pad/
// turb_strength_scale_pad) + colliders[] + growth_range + 4 emitter arrays,
// std140 -- see gas_block_inc.glsl's Params
constexpr uint32_t PARAMS_BYTES = 80 + GasSolver::MAX_COLLIDERS * 32 + 16 + GasSolver::MAX_EMITTERS * 16 * 4;
constexpr float GROWTH_BLOCKS_Y = 2.0f; // blocks added to the top of the box per growth pass
constexpr int GROWTH_EVERY_STEPS = 90; // ~1.5s at 60Hz -- growth is cheap but not free (extra touch dispatch + readback)
// Fixed (not convergence-checked) Jacobi iteration count for the pressure
// solve -- each iteration is its own submit+sync round trip (see _dispatch),
// so this is a direct per-step GPU-latency cost, not just compute cost. MUST
// be even: GasSolver::step relies on an even count leaving the final pressure
// in buf_pressure_a (see uset_project_a/b's fixed press5 choice in
// _build_buffers). Chosen low to keep this usable at interactive rates;
// revisit (or add a proper multigrid/CG solver) if projection quality needs
// to improve -- this is a real perf/quality knob, not a magic constant.
constexpr int PRESSURE_JACOBI_ITERS = 32;

uint32_t groups_for(int count) {
	return (uint32_t)((count + GROUP - 1) / GROUP);
}
} //namespace

GasSolver::GasSolver() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		return;
	}
	// Always a private local device for v1 -- see the header for why (no
	// render-thread/async-readback machinery yet, unlike the MPM fluid
	// solver). This is the same fallback MPM itself uses when headless.
	if (!DisplayServer::can_create_rendering_device()) {
		return;
	}
	rd = rs->create_local_rendering_device();
	if (rd == nullptr) {
		return;
	}
	_compile_shaders();
}

GasSolver::~GasSolver() {
	if (rd == nullptr) {
		return;
	}
	_free_buffers();
	RID *shaders[] = { &shader_touch, &shader_clear, &shader_clear_range, &shader_curl, &shader_step,
		&shader_divergence, &shader_jacobi, &shader_project };
	for (RID *s : shaders) {
		if (s->is_valid()) {
			rd->free(*s);
		}
	}
	memdelete(rd); // we own this local device
	rd = nullptr;
}

void GasSolver::_compile_shaders() {
	struct Entry {
		const char *src;
		RID *shader;
		RID *pipeline;
	};
	Entry entries[] = {
		{ gas_bs_touch_shader_glsl, &shader_touch, &pipeline_touch },
		{ gas_bs_clear_shader_glsl, &shader_clear, &pipeline_clear },
		{ gas_bs_clear_range_shader_glsl, &shader_clear_range, &pipeline_clear_range },
		{ gas_bs_curl_shader_glsl, &shader_curl, &pipeline_curl },
		{ gas_bs_step_shader_glsl, &shader_step, &pipeline_step },
		{ gas_bs_divergence_shader_glsl, &shader_divergence, &pipeline_divergence },
		{ gas_bs_jacobi_shader_glsl, &shader_jacobi, &pipeline_jacobi },
		{ gas_bs_project_shader_glsl, &shader_project, &pipeline_project },
	};
	for (Entry &e : entries) {
		Ref<RDShaderFile> sf;
		sf.instantiate();
		if (sf->parse_versions_from_text(e.src) != OK) {
			ERR_PRINT("GasSolver: a compute shader failed to compile.");
			return;
		}
		*e.shader = rd->shader_create_from_spirv(sf->get_spirv_stages());
		ERR_FAIL_COND(e.shader->is_null());
		*e.pipeline = rd->compute_pipeline_create(*e.shader);
		ERR_FAIL_COND(e.pipeline->is_null());
	}
	shaders_ok = true;
}

void GasSolver::_free_buffers() {
	RID *usets[] = { &uset_touch, &uset_clear, &uset_curl_a, &uset_curl_b, &uset_step_atob, &uset_step_btoa,
		&uset_divergence_a, &uset_divergence_b, &uset_jacobi_p5top6, &uset_jacobi_p6top5, &uset_project_a, &uset_project_b };
	for (RID *u : usets) {
		if (u->is_valid()) {
			rd->free(*u);
			*u = RID();
		}
	}
	RID *bufs[] = { &buf_params, &buf_bhash, &buf_bhash_val, &buf_bkey, &buf_bcounts, &buf_grid_a, &buf_grid_b, &buf_grid_scratch, &buf_curl,
		&buf_divergence, &buf_pressure_a, &buf_pressure_b };
	for (RID *b : bufs) {
		if (b->is_valid()) {
			rd->free(*b);
			*b = RID();
		}
	}
}

RID GasSolver::_make_uniform_set(RID p_shader, RID p_grid5, RID p_grid6, RID p_press5, RID p_press6) const {
	const RID by_binding[11] = { buf_params, buf_bhash, buf_bhash_val, buf_bkey, buf_bcounts, p_grid5, p_grid6, buf_curl,
		buf_divergence, p_press5, p_press6 };
	Vector<RD::Uniform> uniforms;
	for (int bnd = 0; bnd < 11; bnd++) {
		RD::Uniform u;
		u.uniform_type = (bnd == 0) ? RD::UNIFORM_TYPE_UNIFORM_BUFFER : RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.binding = bnd;
		u.append_id(by_binding[bnd]);
		uniforms.push_back(u);
	}
	RID uset = rd->uniform_set_create(uniforms, p_shader, 0);
	ERR_FAIL_COND_V(uset.is_null(), RID());
	return uset;
}

void GasSolver::_build_buffers() {
	const Vector3i box_cells = settings.box_blocks * BLK;
	block_count = settings.box_blocks.x * settings.box_blocks.y * settings.box_blocks.z;
	cell_count = box_cells.x * box_cells.y * box_cells.z;
	// Reserve headroom for vertical growth (see _grow_if_needed) so it never
	// needs to reallocate a buffer -- just claim more of an already-sized
	// pool. 3x the initial block count comfortably covers tripling the box's
	// height; block_touch degrades gracefully (stops inserting) if a plume
	// somehow needs more than that.
	max_blocks = block_count * 3 + 16;
	hash_slots = 1;
	while (hash_slots < max_blocks * 2) {
		hash_slots *= 2;
	}

	buf_params = rd->uniform_buffer_create(PARAMS_BYTES);

	Vector<uint8_t> hash_init;
	hash_init.resize(hash_slots * (int)sizeof(uint32_t));
	{
		uint32_t *w = (uint32_t *)hash_init.ptrw();
		for (int i = 0; i < hash_slots; i++) {
			w[i] = 0xffffffffu;
		}
	}
	buf_bhash = rd->storage_buffer_create(hash_init.size(), hash_init);
	buf_bhash_val = rd->storage_buffer_create(hash_slots * sizeof(uint32_t));
	buf_bkey = rd->storage_buffer_create(max_blocks * sizeof(uint32_t));
	buf_bcounts = rd->storage_buffer_create(4 * sizeof(uint32_t));
	buf_grid_a = rd->storage_buffer_create(max_blocks * BCELLS * 4 * sizeof(float));
	buf_grid_b = rd->storage_buffer_create(max_blocks * BCELLS * 4 * sizeof(float));
	buf_grid_scratch = rd->storage_buffer_create(max_blocks * BCELLS * 4 * sizeof(float));
	buf_curl = rd->storage_buffer_create(max_blocks * BCELLS * 4 * sizeof(float));
	buf_divergence = rd->storage_buffer_create(max_blocks * BCELLS * sizeof(float));
	buf_pressure_a = rd->storage_buffer_create(max_blocks * BCELLS * sizeof(float));
	buf_pressure_b = rd->storage_buffer_create(max_blocks * BCELLS * sizeof(float));

	uset_touch = _make_uniform_set(shader_touch, buf_grid_a, buf_grid_b, buf_pressure_a, buf_pressure_b);
	uset_clear = _make_uniform_set(shader_clear, buf_grid_a, buf_grid_b, buf_pressure_a, buf_pressure_b);
	uset_curl_a = _make_uniform_set(shader_curl, buf_grid_a, buf_grid_b, buf_pressure_a, buf_pressure_b);
	uset_curl_b = _make_uniform_set(shader_curl, buf_grid_b, buf_grid_a, buf_pressure_a, buf_pressure_b);
	uset_step_atob = _make_uniform_set(shader_step, buf_grid_a, buf_grid_b, buf_pressure_a, buf_pressure_b);
	uset_step_btoa = _make_uniform_set(shader_step, buf_grid_b, buf_grid_a, buf_pressure_a, buf_pressure_b);
	// Divergence pass reads velocity from whichever grid buffer holds this
	// frame's post-step (pre-projection) state -- GasSolver::step picks the
	// variant matching that buffer. grid6 is unused by the shader, but MUST
	// still be bound to a DIFFERENT physical buffer than grid5: binding the
	// same resource to two different slots in one descriptor set is a real
	// hazard even when the shader source never reads/writes through the
	// second slot (reproduced: this exact pattern silently zeroed density
	// after the solver had configure()'d more than once -- see
	// buf_grid_scratch's header comment for the sibling bug this came from).
	uset_divergence_a = _make_uniform_set(shader_divergence, buf_grid_a, buf_grid_b, buf_pressure_a, buf_pressure_b);
	uset_divergence_b = _make_uniform_set(shader_divergence, buf_grid_b, buf_grid_a, buf_pressure_a, buf_pressure_b);
	// Jacobi ping-pongs pressure5/pressure6 across PRESSURE_JACOBI_ITERS
	// iterations; grid args are unused by the shader.
	uset_jacobi_p5top6 = _make_uniform_set(shader_jacobi, buf_grid_a, buf_grid_b, buf_pressure_a, buf_pressure_b);
	uset_jacobi_p6top5 = _make_uniform_set(shader_jacobi, buf_grid_a, buf_grid_b, buf_pressure_b, buf_pressure_a);
	// Project reads NEW_STATE (grid5) and writes the corrected result into
	// buf_grid_scratch (grid6) -- NOT aliased to the same physical buffer
	// (see buf_grid_scratch's header comment for why that broke). step()
	// copies buf_grid_scratch back into NEW_STATE afterward. pressure5 is
	// fixed to buf_pressure_a, which is where PRESSURE_JACOBI_ITERS (even)
	// leaves the final solve -- iteration 0 reads a/writes b, iteration 1
	// reads b/writes a, ... an even count always ends with the latest write
	// in a.
	uset_project_a = _make_uniform_set(shader_project, buf_grid_a, buf_grid_scratch, buf_pressure_a, buf_pressure_b);
	uset_project_b = _make_uniform_set(shader_project, buf_grid_b, buf_grid_scratch, buf_pressure_a, buf_pressure_b);
}

void GasSolver::_dispatch(RID p_pipeline, RID p_uset, int p_thread_count) const {
	RD::ComputeListID cl = rd->compute_list_begin();
	rd->compute_list_bind_compute_pipeline(cl, p_pipeline);
	rd->compute_list_bind_uniform_set(cl, p_uset, 0);
	rd->compute_list_dispatch(cl, groups_for(p_thread_count), 1, 1);
	rd->compute_list_end();
	rd->submit();
	rd->sync();
}

void GasSolver::_upload_params(double p_dt) {
	Vector<uint8_t> b;
	b.resize(PARAMS_BYTES);
	uint8_t *w = b.ptrw();
	auto put_f = [&](int off, float v) { memcpy(w + off, &v, sizeof(float)); };
	auto put_i = [&](int off, int32_t v) { memcpy(w + off, &v, sizeof(int32_t)); };

	put_f(0, (float)p_dt);
	put_f(4, settings.buoyancy);
	put_f(8, settings.vorticity_strength);
	put_f(12, settings.dissipation);
	put_f(16, grid_anchor.x);
	put_f(20, grid_anchor.y);
	put_f(24, grid_anchor.z);
	put_f(28, settings.cell_size);
	put_i(32, settings.box_blocks.x);
	put_i(36, settings.box_blocks.y);
	put_i(40, settings.box_blocks.z);
	put_i(44, 0);
	put_f(48, (float)hash_slots);
	put_f(52, (float)max_blocks);
	put_f(56, (float)time_accum);
	put_f(60, 0.0f);
	put_f(64, settings.turbulence_strength);
	put_f(68, settings.turbulence_scale);
	put_f(72, 0.0f);
	put_f(76, 0.0f);
	// gas_block_inc.glsl declares colliders_c0[MAX_GAS_COLLIDERS] and
	// colliders_c1[MAX_GAS_COLLIDERS] as two SEPARATE contiguous arrays (all
	// c0's back to back, then all c1's), not interleaved per-collider -- an
	// earlier version of this loop wrote c0/c1 interleaved (stride 32,
	// position then extents each iteration), which silently fed the wrong
	// collider's data into colliders_c1 for every slot but the last and made
	// every collider's shape params (radius/box-extents/plane-normal) read as
	// garbage -- diagnosed by printing the C++-side resolved values (correct)
	// against the fact that a plane collider was passing gas straight through
	// (only explained by the GPU seeing a different byte layout).
	for (int i = 0; i < MAX_COLLIDERS; i++) {
		const int off0 = 80 + i * 16;
		const ColliderSlot &slot = collider_slots[i];
		put_f(off0, slot.collider.position.x);
		put_f(off0 + 4, slot.collider.position.y);
		put_f(off0 + 8, slot.collider.position.z);
		put_f(off0 + 12, slot.enabled ? (float)slot.collider.shape : -1.0f);
	}
	for (int i = 0; i < MAX_COLLIDERS; i++) {
		const int off1 = 80 + MAX_COLLIDERS * 16 + i * 16;
		const ColliderSlot &slot = collider_slots[i];
		put_f(off1, slot.collider.extents.x);
		put_f(off1 + 4, slot.collider.extents.y);
		put_f(off1 + 8, slot.collider.extents.z);
		put_f(off1 + 12, 0.0f);
	}
	const int growth_off = 80 + MAX_COLLIDERS * 32;
	put_i(growth_off, pending_growth_range.x);
	put_i(growth_off + 4, pending_growth_range.y);
	put_i(growth_off + 8, 0);
	put_i(growth_off + 12, 0);
	// Three parallel arrays (emitter_pos_shape / emitter_size_density /
	// emitter_velocity), matching gas_block_inc.glsl's Params layout exactly.
	const int emitters_off = growth_off + 16;
	for (int i = 0; i < MAX_EMITTERS; i++) {
		const Emitter &em = emitters[i];
		const int pos_off = emitters_off + i * 16;
		put_f(pos_off, em.world_position.x);
		put_f(pos_off + 4, em.world_position.y);
		put_f(pos_off + 8, em.world_position.z);
		put_f(pos_off + 12, em.enabled ? (float)em.shape : -1.0f);
		const int size_off = emitters_off + MAX_EMITTERS * 16 + i * 16;
		put_f(size_off, em.size.x);
		put_f(size_off + 4, em.size.y);
		put_f(size_off + 8, em.size.z);
		put_f(size_off + 12, em.density);
		const int vel_off = emitters_off + MAX_EMITTERS * 32 + i * 16;
		put_f(vel_off, em.velocity.x);
		put_f(vel_off + 4, em.velocity.y);
		put_f(vel_off + 8, em.velocity.z);
		put_f(vel_off + 12, 0.0f);
		const int extra_off = emitters_off + MAX_EMITTERS * 48 + i * 16;
		put_f(extra_off, em.divergence);
		put_f(extra_off + 4, em.swirl);
		put_f(extra_off + 8, 0.0f);
		put_f(extra_off + 12, 0.0f);
	}

	rd->buffer_update(buf_params, 0, PARAMS_BYTES, b.ptr());
}

void GasSolver::configure(const Settings &p_settings, const Transform3D &p_xform) {
	if (rd == nullptr || !shaders_ok) {
		return;
	}
	_free_buffers();
	settings = p_settings;
	domain_xform = p_xform;
	a_is_current = true;
	time_accum = 0.0;
	steps_since_growth = 0;
	initial_box_blocks_y = settings.box_blocks.y;
	max_box_blocks_y = settings.box_blocks.y * 3; // matches _build_buffers' 3x block-pool headroom
	pending_growth_range = Vector3i();

	const Vector3i box_cells = settings.box_blocks * BLK;
	// Frozen at configure, like the MPM block-sparse fluid's grid_anchor --
	// the box's WORLD FOOTPRINT does not track the transform afterwards (v1
	// scope cut: no lateral reflow, see the header). Only Y grows, and only
	// upward (see _grow_if_needed) -- grid_anchor stays the box's bottom
	// corner regardless, so already-touched cells never change world
	// position when that happens.
	grid_anchor = p_xform.xform(-Vector3(box_cells) * settings.cell_size * 0.5f);

	_build_buffers();
	_upload_params(0.0);
	_dispatch(pipeline_touch, uset_touch, block_count);
	_dispatch(pipeline_clear, uset_clear, cell_count);
	_read_slot_blocks();
}

void GasSolver::reposition(const Transform3D &p_xform) {
	if (rd == nullptr || !shaders_ok || buf_params.is_null()) {
		return; // never configure()'d -- nothing to reposition
	}
	domain_xform = p_xform;
	a_is_current = true;
	time_accum = 0.0;
	steps_since_growth = 0;
	// Undo any accumulated vertical growth back to the originally configured
	// height, same as a full configure() does -- box_blocks/max_blocks/
	// hash_slots below all stay derived from settings as originally sized,
	// so buffers (unchanged, reused) are still big enough.
	settings.box_blocks.y = initial_box_blocks_y;
	max_box_blocks_y = initial_box_blocks_y * 3;
	pending_growth_range = Vector3i();
	block_count = settings.box_blocks.x * settings.box_blocks.y * settings.box_blocks.z;
	cell_count = (settings.box_blocks * BLK).x * (settings.box_blocks * BLK).y * (settings.box_blocks * BLK).z;

	const Vector3i box_cells = settings.box_blocks * BLK;
	grid_anchor = p_xform.xform(-Vector3(box_cells) * settings.cell_size * 0.5f);

	// Reset the hash table to empty -- the OLD position's block keys are now
	// meaningless (wrong world position), and unlike configure() we're NOT
	// recreating buf_bhash/buf_bcounts fresh, so their old contents would
	// otherwise persist.
	Vector<uint8_t> hash_init;
	hash_init.resize(hash_slots * (int)sizeof(uint32_t));
	{
		uint32_t *w = (uint32_t *)hash_init.ptrw();
		for (int i = 0; i < hash_slots; i++) {
			w[i] = 0xffffffffu;
		}
	}
	rd->buffer_update(buf_bhash, 0, hash_init.size(), hash_init.ptr());
	Vector<uint8_t> counts_init;
	counts_init.resize(4 * sizeof(uint32_t));
	memset(counts_init.ptrw(), 0, counts_init.size());
	rd->buffer_update(buf_bcounts, 0, counts_init.size(), counts_init.ptr());

	_upload_params(0.0);
	_dispatch(pipeline_touch, uset_touch, block_count);
	_dispatch(pipeline_clear, uset_clear, cell_count);
	_read_slot_blocks();
}

void GasSolver::_grow_if_needed() {
	if (settings.box_blocks.y >= max_box_blocks_y) {
		return; // reserved pool headroom exhausted -- stop trying
	}
	// Read the current active-block count (bcounts[0]) before growing: any
	// slot claimed by the touch dispatch below is one atomicAdd(bcounts[0])
	// past whatever this was, so [old_count, new_count) is exactly "every
	// block gas_bs_touch just inserted this pass" -- see gas_bs_clear_range.
	Vector<uint8_t> raw = rd->buffer_get_data(buf_bcounts, 0, sizeof(uint32_t));
	const uint32_t old_count = *(const uint32_t *)raw.ptr();

	settings.box_blocks.y += (int)GROWTH_BLOCKS_Y;
	block_count = settings.box_blocks.x * settings.box_blocks.y * settings.box_blocks.z;
	cell_count = (settings.box_blocks * BLK).x * (settings.box_blocks * BLK).y * (settings.box_blocks * BLK).z;
	_upload_params(0.0); // pending_growth_range is (0,0,0) here -- this upload is just the new box size
	_dispatch(pipeline_touch, uset_touch, block_count);

	raw = rd->buffer_get_data(buf_bcounts, 0, sizeof(uint32_t));
	const uint32_t new_count = *(const uint32_t *)raw.ptr();
	if (new_count > old_count) {
		pending_growth_range = Vector3i((int)old_count, (int)new_count, 0);
		_upload_params(0.0); // re-upload with the real range so gas_bs_clear_range sees it
		_dispatch(pipeline_clear_range, uset_clear, ((int)(new_count - old_count)) * BCELLS);
		pending_growth_range = Vector3i();
	}
	_read_slot_blocks();
}

void GasSolver::_read_slot_blocks() {
	Vector<uint8_t> raw = rd->buffer_get_data(buf_bkey, 0, block_count * sizeof(uint32_t));
	slot_block.resize(block_count);
	const uint32_t *keys = (const uint32_t *)raw.ptr();
	for (int slot = 0; slot < block_count; slot++) {
		const uint32_t key = keys[slot];
		slot_block.write[slot] = Vector3i(
				int(key & 1023u) - 512,
				int((key >> 10) & 1023u) - 512,
				int((key >> 20) & 1023u) - 512);
	}
}

void GasSolver::step(double p_delta, const Transform3D &p_xform) {
	if (rd == nullptr || !shaders_ok || buf_params.is_null()) {
		return;
	}
	domain_xform = p_xform;
	time_accum += p_delta;

	steps_since_growth++;
	if (steps_since_growth >= GROWTH_EVERY_STEPS) {
		steps_since_growth = 0;
		_grow_if_needed(); // uploads its own params; harmless that step()'s own upload below follows right after
	}
	_upload_params(p_delta);

	// Single-submit step (PART-1): the old path ran every pass through
	// _dispatch's own submit+sync -- 36 full CPU<->GPU round trips per tick
	// (curl, step, divergence, 32 Jacobi iterations, project). Record ALL
	// passes into ONE compute list with barriers between them (same pattern
	// as the MPM solver) and submit once. Godot RD records buffer commands
	// into the submission queue in call order, so the grid copy below still
	// executes after the passes without a second submit.
	const bool was_a = a_is_current;
	step_submit_count = 0;

	RD::ComputeListID cl = rd->compute_list_begin();

	RID curl_uset = was_a ? uset_curl_a : uset_curl_b;
	rd->compute_list_bind_compute_pipeline(cl, pipeline_curl);
	rd->compute_list_bind_uniform_set(cl, curl_uset, 0);
	rd->compute_list_dispatch(cl, groups_for(cell_count), 1, 1);
	rd->compute_list_add_barrier(cl);

	RID step_uset = was_a ? uset_step_atob : uset_step_btoa;
	rd->compute_list_bind_compute_pipeline(cl, pipeline_step);
	rd->compute_list_bind_uniform_set(cl, step_uset, 0);
	rd->compute_list_dispatch(cl, groups_for(cell_count), 1, 1);
	rd->compute_list_add_barrier(cl);

	RID divergence_uset = was_a ? uset_divergence_b : uset_divergence_a;
	rd->compute_list_bind_compute_pipeline(cl, pipeline_divergence);
	rd->compute_list_bind_uniform_set(cl, divergence_uset, 0);
	rd->compute_list_dispatch(cl, groups_for(cell_count), 1, 1);
	rd->compute_list_add_barrier(cl);

	for (int it = 0; it < PRESSURE_JACOBI_ITERS; it++) {
		RID jacobi_uset = (it % 2 == 0) ? uset_jacobi_p5top6 : uset_jacobi_p6top5;
		rd->compute_list_bind_compute_pipeline(cl, pipeline_jacobi);
		rd->compute_list_bind_uniform_set(cl, jacobi_uset, 0);
		rd->compute_list_dispatch(cl, groups_for(cell_count), 1, 1);
		rd->compute_list_add_barrier(cl);
	}

	RID project_uset = was_a ? uset_project_b : uset_project_a;
	rd->compute_list_bind_compute_pipeline(cl, pipeline_project);
	rd->compute_list_bind_uniform_set(cl, project_uset, 0);
	rd->compute_list_dispatch(cl, groups_for(cell_count), 1, 1);
	rd->compute_list_end();

	// Copy scratch -> NEW_STATE, recorded after the passes in the same
	// submission (ordered on the device timeline), then ONE sync.
	RID new_state_grid = was_a ? buf_grid_b : buf_grid_a;
	rd->buffer_copy(buf_grid_scratch, new_state_grid, 0, 0, max_blocks * BCELLS * 4 * sizeof(float));
	rd->submit();
	rd->sync();
	step_submit_count = 1;

	a_is_current = !was_a;
}

void GasSolver::set_colliders(const Vector<Collider> &p_colliders) {
	for (int i = 0; i < MAX_COLLIDERS; i++) {
		collider_slots[i].enabled = i < p_colliders.size();
		if (collider_slots[i].enabled) {
			collider_slots[i].collider = p_colliders[i];
		}
	}
}

void GasSolver::set_emitters(const Vector<Emitter> &p_emitters) {
	for (int i = 0; i < MAX_EMITTERS; i++) {
		emitters[i].enabled = i < p_emitters.size();
		if (emitters[i].enabled) {
			emitters[i] = p_emitters[i];
			emitters[i].enabled = true; // in case the caller left it default-false
		}
	}
}

void GasSolver::get_render_cells(float p_density_threshold, Vector<Vector3> &r_positions, Vector<float> &r_density) const {
	r_positions.clear();
	r_density.clear();
	if (rd == nullptr || buf_grid_a.is_null()) {
		return;
	}
	RID cur = a_is_current ? buf_grid_a : buf_grid_b;
	Vector<uint8_t> raw = rd->buffer_get_data(cur, 0, max_blocks * BCELLS * 4 * sizeof(float));
	if (raw.size() < max_blocks * BCELLS * 4 * (int)sizeof(float)) {
		return;
	}
	const float *data = (const float *)raw.ptr();

	for (int slot = 0; slot < slot_block.size(); slot++) {
		const Vector3i bc = slot_block[slot];
		for (int lz = 0; lz < BLK; lz++) {
			for (int ly = 0; ly < BLK; ly++) {
				for (int lx = 0; lx < BLK; lx++) {
					const int lin = (lz * BLK + ly) * BLK + lx;
					const int idx = (slot * BCELLS + lin) * 4;
					const float dens = data[idx + 3];
					if (dens < p_density_threshold) {
						continue;
					}
					const Vector3i c(bc.x * BLK + lx, bc.y * BLK + ly, bc.z * BLK + lz);
					const Vector3 wpos = grid_anchor + Vector3(c) * settings.cell_size;
					r_positions.push_back(wpos);
					r_density.push_back(dens);
				}
			}
		}
	}
}

int GasSolver::get_domain_cells() const {
	const Vector3i dims = settings.box_blocks * BLK;
	return dims.x * dims.y * dims.z;
}

void GasSolver::get_density_grid(Vector<float> &r_density, Vector3i &r_dims, Vector3 &r_anchor, float &r_cell_size) const {
	r_density.clear();
	r_dims = Vector3i();
	r_anchor = grid_anchor;
	r_cell_size = settings.cell_size;
	if (rd == nullptr || buf_grid_a.is_null()) {
		return;
	}
	const Vector3i dims = settings.box_blocks * BLK;
	r_density.resize(dims.x * dims.y * dims.z); // zero-initialized: any cell whose block hasn't landed in slot_block yet (shouldn't happen -- box is fully pre-touched) reads as empty
	float *w = r_density.ptrw();
	memset(w, 0, r_density.size() * sizeof(float));

	RID cur = a_is_current ? buf_grid_a : buf_grid_b;
	Vector<uint8_t> raw = rd->buffer_get_data(cur, 0, max_blocks * BCELLS * 4 * sizeof(float));
	if (raw.size() < max_blocks * BCELLS * 4 * (int)sizeof(float)) {
		return;
	}
	const float *data = (const float *)raw.ptr();

	for (int slot = 0; slot < slot_block.size(); slot++) {
		const Vector3i bc = slot_block[slot];
		for (int lz = 0; lz < BLK; lz++) {
			for (int ly = 0; ly < BLK; ly++) {
				for (int lx = 0; lx < BLK; lx++) {
					const int lin = (lz * BLK + ly) * BLK + lx;
					const int idx = (slot * BCELLS + lin) * 4;
					const Vector3i c(bc.x * BLK + lx, bc.y * BLK + ly, bc.z * BLK + lz);
					if (c.x < 0 || c.y < 0 || c.z < 0 || c.x >= dims.x || c.y >= dims.y || c.z >= dims.z) {
						continue; // a block beyond the current (possibly since-shrunk-in-code, never in practice) box -- defensive only
					}
					w[c.x + c.y * dims.x + c.z * dims.x * dims.y] = data[idx + 3];
				}
			}
		}
	}
	r_dims = dims;
}

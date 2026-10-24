/**************************************************************************/
/*  water_solver.cpp                                                      */
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

#include "water_solver.h"

#include "water_blit_ocean.glsl.gen.h"
#include "water_blit_ripple.glsl.gen.h"
#include "water_caustics_map.glsl.gen.h"
#include "water_fft.glsl.gen.h"
#include "water_foam.glsl.gen.h"
#include "water_ripple.glsl.gen.h"
#include "water_shore_foam.glsl.gen.h"
#include "water_spectrum_evolve.glsl.gen.h"
#include "water_spectrum_init.glsl.gen.h"
#include "water_swash.glsl.gen.h"

#include "core/math/math_funcs.h"
#include "core/object/callable_method_pointer.h"
#include "core/os/memory.h"
#include "servers/display/display_server.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_device_binds.h"
#include "servers/rendering/rendering_server.h"

namespace {
constexpr int PARAMS_BYTES = 64; // 4 * vec4, std140 -- see water_inc.glsl's Params
constexpr int OCEAN_INIT_PARAMS_BYTES = 64; // 4 * vec4 -- see water_spectrum_init.glsl's Params
constexpr int OCEAN_EVOLVE_PARAMS_BYTES = 32; // 2 * vec4 -- see water_spectrum_evolve.glsl's Params
constexpr int FFT_PARAMS_BYTES = 16; // 1 * ivec4 -- see water_fft.glsl's Params
constexpr int BLIT_PARAMS_BYTES = 16; // 1 * ivec4 -- see water_blit_*.glsl's Params
constexpr int FOAM_PARAMS_BYTES = 32; // vec4 + ivec4 -- see water_foam.glsl's Params
constexpr int SHORE_FOAM_PARAMS_BYTES = 64; // 3 * vec4 + ivec4 -- see water_shore_foam.glsl's Params
constexpr int SWASH_PARAMS_BYTES = 64; // 3 * vec4 + ivec4 -- see water_swash.glsl's Params
constexpr int CAUSTICS_PARAMS_BYTES = 112; // 7 * vec4, std140 -- see water_caustics_map.glsl's Params
constexpr uint32_t GROUP = 8; // matches every water_*.glsl's local_size_x/y

uint32_t groups_for(int p_count) {
	return (uint32_t)((p_count + (int)GROUP - 1) / (int)GROUP);
}

int log2i(int p_v) {
	int log2_result = 0;
	while ((1 << log2_result) < p_v) {
		log2_result++;
	}
	return log2_result;
}
} // namespace

/* ===================================================================== */
/*  WaterSolverGPU                                                        */
/* ===================================================================== */

WaterSolverGPU::~WaterSolverGPU() {
#ifdef DEV_ENABLED
	// rt_free() should have run first (via WaterSolver's destructor). If a
	// stale async callback kept us alive past it, everything is already
	// null and there is nothing to do -- same discipline as
	// MPMFluidSolverGPU's own destructor.
	DEV_ASSERT(shader_ripple.is_null() && pipeline_ripple.is_null());
#endif
}

void WaterSolverGPU::rt_compile(Ref<WaterSolverGPU> p_self) {
	if (rd == nullptr) {
		return;
	}
	struct Entry {
		const char *src;
		RID *shader;
		RID *pipeline;
	};
	Entry entries[] = {
		{ water_ripple_shader_glsl, &shader_ripple, &pipeline_ripple },
		{ water_spectrum_init_shader_glsl, &shader_spectrum_init, &pipeline_spectrum_init },
		{ water_spectrum_evolve_shader_glsl, &shader_spectrum_evolve, &pipeline_spectrum_evolve },
		{ water_fft_shader_glsl, &shader_fft, &pipeline_fft },
		{ water_blit_ripple_shader_glsl, &shader_blit_ripple, &pipeline_blit_ripple },
		{ water_blit_ocean_shader_glsl, &shader_blit_ocean, &pipeline_blit_ocean },
		{ water_foam_shader_glsl, &shader_foam, &pipeline_foam },
		{ water_shore_foam_shader_glsl, &shader_shore_foam, &pipeline_shore_foam },
		{ water_swash_shader_glsl, &shader_swash, &pipeline_swash },
	};
	for (Entry &e : entries) {
		Ref<RDShaderFile> sf;
		sf.instantiate();
		if (sf->parse_versions_from_text(e.src) != OK) {
			ERR_PRINT("WaterSolver: a compute shader failed to compile.");
			return;
		}
		*e.shader = rd->shader_create_from_spirv(sf->get_spirv_stages());
		ERR_FAIL_COND(e.shader->is_null());
		*e.pipeline = rd->compute_pipeline_create(*e.shader);
		ERR_FAIL_COND(e.pipeline->is_null());
	}

	// Light-space caustic map: a real RD graphics (vertex+fragment) pipeline,
	// not a compute one -- built here alongside the compute shaders above
	// (compiled unconditionally; the grid/buffers it actually draws into are
	// only built in _rt_build_buffers() if caustics_enabled was requested, so
	// an unused pipeline costs nothing beyond this one-time compile).
	{
		Ref<RDShaderFile> sf;
		sf.instantiate();
		if (sf->parse_versions_from_text(water_caustics_map_shader_glsl) != OK) {
			ERR_PRINT("WaterSolver: the caustics map shader failed to compile.");
			return;
		}
		shader_caustics = rd->shader_create_from_spirv(sf->get_spirv_stages());
		ERR_FAIL_COND(shader_caustics.is_null());

		Vector<RD::VertexAttribute> attrs;
		RD::VertexAttribute attr;
		attr.location = 0;
		attr.format = RD::DATA_FORMAT_R32G32_SFLOAT;
		attr.stride = sizeof(float) * 2;
		attrs.push_back(attr);
		caustics_vertex_format = rd->vertex_format_create(attrs);

		Vector<RD::AttachmentFormat> afs;
		RD::AttachmentFormat af;
		af.format = RD::DATA_FORMAT_R8G8B8A8_UNORM; // R caustics, G water mask (4 channels: a 2-channel texture got sampled with a luminance-alpha swizzle)
		af.usage_flags = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
		afs.push_back(af);
		RD::FramebufferFormatID fb_format = rd->framebuffer_format_create(afs);

		RD::PipelineColorBlendState blend_state;
		RD::PipelineColorBlendState::Attachment blend_attachment;
		blend_attachment.enable_blend = true;
		blend_attachment.src_color_blend_factor = RD::BLEND_FACTOR_ONE;
		blend_attachment.dst_color_blend_factor = RD::BLEND_FACTOR_ONE;
		blend_attachment.color_blend_op = RD::BLEND_OP_ADD;
		blend_attachment.src_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
		blend_attachment.dst_alpha_blend_factor = RD::BLEND_FACTOR_ONE;
		blend_attachment.alpha_blend_op = RD::BLEND_OP_ADD;
		blend_state.attachments.push_back(blend_attachment);
		pipeline_caustics = rd->render_pipeline_create(shader_caustics, fb_format, caustics_vertex_format, RD::RENDER_PRIMITIVE_TRIANGLES, RD::PipelineRasterizationState(), RD::PipelineMultisampleState(), RD::PipelineDepthStencilState(), blend_state, 0);
		ERR_FAIL_COND(pipeline_caustics.is_null());

		RD::SamplerState ss;
		ss.min_filter = RD::SAMPLER_FILTER_LINEAR;
		ss.mag_filter = RD::SAMPLER_FILTER_LINEAR;
		ss.repeat_u = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		ss.repeat_v = RD::SAMPLER_REPEAT_MODE_CLAMP_TO_EDGE;
		sampler_linear = rd->sampler_create(ss);
		ERR_FAIL_COND(sampler_linear.is_null());
		ss.repeat_u = RD::SAMPLER_REPEAT_MODE_REPEAT;
		ss.repeat_v = RD::SAMPLER_REPEAT_MODE_REPEAT;
		sampler_linear_repeat = rd->sampler_create(ss);
		ERR_FAIL_COND(sampler_linear_repeat.is_null());
	}
	shaders_ok = true;
}

void WaterSolverGPU::_rt_free_buffers() {
	RID *usets[] = { &uset_atob, &uset_btoa, &uset_spectrum_init, &uset_spectrum_evolve,
		&uset_fft_spec_to_a, &uset_fft_atob, &uset_fft_btoa, &uset_fft_dz_to_c, &uset_fft_ctod, &uset_fft_dtoc,
		&uset_fft_c_to_e, &uset_fft_etof, &uset_fft_ftoe, &uset_fft_d_to_g, &uset_fft_gtoh, &uset_fft_htog,
		&uset_blit_ripple, &uset_blit_ocean, &uset_caustics, &uset_foam, &uset_shore_foam, &uset_shore_foam_copy, &uset_swash, &uset_swash_copy };
	for (RID *uniform : usets) {
		if (uniform->is_valid()) {
			rd->free(*uniform);
			*uniform = RID();
		}
	}
	RID *framebuffers[] = { &caustics_framebuffer };
	for (RID *framebuffer : framebuffers) {
		if (framebuffer->is_valid()) {
			rd->free(*framebuffer);
			*framebuffer = RID();
		}
	}
	RID *arrays[] = { &caustics_vertex_array, &caustics_index_array };
	for (RID *array : arrays) {
		if (array->is_valid()) {
			rd->free(*array);
			*array = RID();
		}
	}
	RID *vertex_index_buffers[] = { &caustics_vertex_buffer, &caustics_index_buffer };
	for (RID *buffer : vertex_index_buffers) {
		if (buffer->is_valid()) {
			rd->free(*buffer);
			*buffer = RID();
		}
	}
	RID *bufs[] = { &buf_params, &buf_state_a, &buf_state_b, &buf_height, &buf_spheres, &buf_impulses,
		&buf_ocean_init_params, &buf_ocean_evolve_params, &buf_fft_params, &buf_h0, &buf_ocean_spec, &buf_fft_a, &buf_fft_b,
		&buf_ocean_spec_dz, &buf_fft_c, &buf_fft_d, &buf_ocean_spec_c, &buf_fft_e, &buf_fft_f, &buf_ocean_spec_d, &buf_fft_g, &buf_fft_h,
		&buf_blit_ripple_params, &buf_blit_ocean_params, &buf_caustics_params, &buf_foam_params, &buf_shore_foam_params, &buf_shore_foam_copy_params, &buf_swash_params, &buf_swash_copy_params };
	for (RID *b : bufs) {
		if (b->is_valid()) {
			rd->free(*b);
			*b = RID();
		}
	}
	RID *texs[] = { &tex_ripple_height, &tex_ocean_height, &tex_ocean_disp, &tex_ocean_deriv, &tex_ocean_foam, &tex_shore_foam, &tex_shore_foam_tmp, &tex_caustics, &tex_cell_depth, &tex_ocean_fade, &tex_shore_depth, &tex_swash, &tex_swash_tmp };
	for (RID *t : texs) {
		if (t->is_valid()) {
			rd->free(*t);
			*t = RID();
		}
	}
}

void WaterSolverGPU::_rt_build_buffers() {
	const int grid_size = grid_resolution;
	const int cells = grid_size * grid_size;
	const int on = ocean_grid_resolution;
	const int ocells = on * on;
	ocean_log2n = log2i(on);
	ocean_time_accum = 0.0;
	a_is_current = true;

	buf_params = rd->uniform_buffer_create(PARAMS_BYTES);
	// Zero-filled: storage_buffer_create() without data leaves the memory
	// uninitialized, and the wave equation reads its previous state -- any
	// NaN left in recycled GPU memory spreads across the whole surface.
	{
		Vector<uint8_t> zeros;
		zeros.resize(cells * 4 * sizeof(float));
		memset(zeros.ptrw(), 0, zeros.size());
		buf_state_a = rd->storage_buffer_create(zeros.size(), zeros);
		buf_state_b = rd->storage_buffer_create(zeros.size(), zeros);
		zeros.resize(cells * sizeof(float));
		buf_height = rd->storage_buffer_create(zeros.size(), zeros);
	}
	buf_spheres = rd->storage_buffer_create(MAX_SPHERES * 4 * sizeof(float));
	buf_impulses = rd->storage_buffer_create(MAX_IMPULSES * 4 * sizeof(float));

	// Per-cell still-water depth (R16F) and the ocean chop fade derived from
	// it (R8), uploaded once. No per-cell depth = the constant depth, and full
	// chop, everywhere.
	{
		has_cell_depth = _init_cell_depth.size() == cells;
		Vector<uint8_t> depth_bytes;
		depth_bytes.resize(cells * sizeof(uint16_t));
		uint16_t *dw = (uint16_t *)depth_bytes.ptrw();
		Vector<uint8_t> fade_bytes;
		fade_bytes.resize(cells);
		uint8_t *fw = fade_bytes.ptrw();
		Vector<uint8_t> shore_bytes;
		shore_bytes.resize(cells * sizeof(uint16_t));
		uint16_t *sw = (uint16_t *)shore_bytes.ptrw();
		for (int i = 0; i < cells; i++) {
			const float column_depth = has_cell_depth ? _init_cell_depth[i] : _init_depth;
			dw[i] = Math::make_half_float(column_depth);
			fw[i] = (uint8_t)Math::round(WaterSolver::shallow_fade(column_depth, shallow_fade_depth) * 255.0f);
			sw[i] = Math::make_half_float(column_depth <= WaterSolver::WALL_DEPTH * 0.5f ? 100.0f : column_depth);
		}
		RD::TextureFormat depth_tf;
		depth_tf.format = RD::DATA_FORMAT_R16_SFLOAT;
		depth_tf.width = grid_size;
		depth_tf.height = grid_size;
		depth_tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
		Vector<Vector<uint8_t>> depth_data;
		depth_data.push_back(depth_bytes);
		tex_cell_depth = rd->texture_create(depth_tf, RD::TextureView(), depth_data);

		RD::TextureFormat fade_tf = depth_tf;
		fade_tf.format = RD::DATA_FORMAT_R8_UNORM;
		Vector<Vector<uint8_t>> fade_data;
		fade_data.push_back(fade_bytes);
		tex_ocean_fade = rd->texture_create(fade_tf, RD::TextureView(), fade_data);

		Vector<Vector<uint8_t>> shore_data;
		shore_data.push_back(shore_bytes);
		tex_shore_depth = rd->texture_create(depth_tf, RD::TextureView(), shore_data);
	}

	auto make_uset = [&](RID p_state_in, RID p_state_out) {
		const RID by_binding[6] = { buf_params, p_state_in, p_state_out, buf_height, buf_spheres, buf_impulses };
		Vector<RD::Uniform> uniforms;
		for (int bnd = 0; bnd < 6; bnd++) {
			RD::Uniform uniform;
			uniform.uniform_type = (bnd == 0) ? RD::UNIFORM_TYPE_UNIFORM_BUFFER : RD::UNIFORM_TYPE_STORAGE_BUFFER;
			uniform.binding = bnd;
			uniform.append_id(by_binding[bnd]);
			uniforms.push_back(uniform);
		}
		RD::Uniform depth_uniform;
		depth_uniform.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
		depth_uniform.binding = 6;
		depth_uniform.append_id(sampler_linear);
		depth_uniform.append_id(tex_cell_depth);
		uniforms.push_back(depth_uniform);
		return rd->uniform_set_create(uniforms, shader_ripple, 0);
	};
	uset_atob = make_uset(buf_state_a, buf_state_b);
	uset_btoa = make_uset(buf_state_b, buf_state_a);

	// FFT ocean layer.
	buf_ocean_init_params = rd->uniform_buffer_create(OCEAN_INIT_PARAMS_BYTES);
	buf_ocean_evolve_params = rd->uniform_buffer_create(OCEAN_EVOLVE_PARAMS_BYTES);
	buf_fft_params = rd->uniform_buffer_create(FFT_PARAMS_BYTES);
	buf_h0 = rd->storage_buffer_create(ocells * 4 * sizeof(float));
	buf_ocean_spec = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_fft_a = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_fft_b = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_ocean_spec_dz = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_fft_c = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_fft_d = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_ocean_spec_c = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_fft_e = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_fft_f = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_ocean_spec_d = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_fft_g = rd->storage_buffer_create(ocells * 2 * sizeof(float));
	buf_fft_h = rd->storage_buffer_create(ocells * 2 * sizeof(float));

	{
		Vector<RD::Uniform> uniforms;
		const RID by_binding[2] = { buf_ocean_init_params, buf_h0 };
		for (int bnd = 0; bnd < 2; bnd++) {
			RD::Uniform uniform;
			uniform.uniform_type = (bnd == 0) ? RD::UNIFORM_TYPE_UNIFORM_BUFFER : RD::UNIFORM_TYPE_STORAGE_BUFFER;
			uniform.binding = bnd;
			uniform.append_id(by_binding[bnd]);
			uniforms.push_back(uniform);
		}
		uset_spectrum_init = rd->uniform_set_create(uniforms, shader_spectrum_init, 0);
	}
	auto make_uset3 = [&](RID p_shader, RID p_params, RID p_in, RID p_out) {
		Vector<RD::Uniform> uniforms;
		const RID by_binding[3] = { p_params, p_in, p_out };
		for (int bnd = 0; bnd < 3; bnd++) {
			RD::Uniform uniform;
			uniform.uniform_type = (bnd == 0) ? RD::UNIFORM_TYPE_UNIFORM_BUFFER : RD::UNIFORM_TYPE_STORAGE_BUFFER;
			uniform.binding = bnd;
			uniform.append_id(by_binding[bnd]);
			uniforms.push_back(uniform);
		}
		return rd->uniform_set_create(uniforms, p_shader, 0);
	};
	{
		Vector<RD::Uniform> uniforms;
		const RID by_binding[6] = { buf_ocean_evolve_params, buf_h0, buf_ocean_spec, buf_ocean_spec_dz, buf_ocean_spec_c, buf_ocean_spec_d };
		for (int bnd = 0; bnd < 6; bnd++) {
			RD::Uniform uniform;
			uniform.uniform_type = (bnd == 0) ? RD::UNIFORM_TYPE_UNIFORM_BUFFER : RD::UNIFORM_TYPE_STORAGE_BUFFER;
			uniform.binding = bnd;
			uniform.append_id(by_binding[bnd]);
			uniforms.push_back(uniform);
		}
		uset_spectrum_evolve = rd->uniform_set_create(uniforms, shader_spectrum_evolve, 0);
	}
	uset_fft_spec_to_a = make_uset3(shader_fft, buf_fft_params, buf_ocean_spec, buf_fft_a);
	uset_fft_atob = make_uset3(shader_fft, buf_fft_params, buf_fft_a, buf_fft_b);
	uset_fft_btoa = make_uset3(shader_fft, buf_fft_params, buf_fft_b, buf_fft_a);
	uset_fft_dz_to_c = make_uset3(shader_fft, buf_fft_params, buf_ocean_spec_dz, buf_fft_c);
	uset_fft_ctod = make_uset3(shader_fft, buf_fft_params, buf_fft_c, buf_fft_d);
	uset_fft_dtoc = make_uset3(shader_fft, buf_fft_params, buf_fft_d, buf_fft_c);
	uset_fft_c_to_e = make_uset3(shader_fft, buf_fft_params, buf_ocean_spec_c, buf_fft_e);
	uset_fft_etof = make_uset3(shader_fft, buf_fft_params, buf_fft_e, buf_fft_f);
	uset_fft_ftoe = make_uset3(shader_fft, buf_fft_params, buf_fft_f, buf_fft_e);
	uset_fft_d_to_g = make_uset3(shader_fft, buf_fft_params, buf_ocean_spec_d, buf_fft_g);
	uset_fft_gtoh = make_uset3(shader_fft, buf_fft_params, buf_fft_g, buf_fft_h);
	uset_fft_htog = make_uset3(shader_fft, buf_fft_params, buf_fft_h, buf_fft_g);

	// Blit-to-texture: real RD textures, sampled directly by the scene
	// renderer via a caller-side Texture2DRD -- see the header for why.
	RD::TextureFormat ripple_tf;
	ripple_tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	ripple_tf.width = grid_size;
	ripple_tf.height = grid_size;
	ripple_tf.usage_bits = RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT;
	tex_ripple_height = rd->texture_create(ripple_tf, RD::TextureView());

	RD::TextureFormat ocean_tf = ripple_tf;
	ocean_tf.width = on;
	ocean_tf.height = on;
	tex_ocean_height = rd->texture_create(ocean_tf, RD::TextureView());
	RD::TextureFormat disp_tf = ocean_tf;
	disp_tf.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
	tex_ocean_disp = rd->texture_create(disp_tf, RD::TextureView());
	tex_ocean_deriv = rd->texture_create(disp_tf, RD::TextureView());
	{
		RD::TextureFormat foam_tf = ocean_tf;
		foam_tf.format = RD::DATA_FORMAT_R32_SFLOAT;
		Vector<uint8_t> zeros;
		zeros.resize(ocells * sizeof(float));
		memset(zeros.ptrw(), 0, zeros.size());
		Vector<Vector<uint8_t>> foam_data;
		foam_data.push_back(zeros);
		tex_ocean_foam = rd->texture_create(foam_tf, RD::TextureView(), foam_data);
	}

	buf_blit_ripple_params = rd->uniform_buffer_create(BLIT_PARAMS_BYTES);
	buf_blit_ocean_params = rd->uniform_buffer_create(BLIT_PARAMS_BYTES);
	{
		int32_t v[4] = { grid_size, 0, 0, 0 };
		PackedByteArray params_bytes;
		params_bytes.resize(BLIT_PARAMS_BYTES);
		memcpy(params_bytes.ptrw(), v, sizeof(v));
		rd->buffer_update(buf_blit_ripple_params, 0, BLIT_PARAMS_BYTES, params_bytes.ptr());
	}
	{
		int32_t v[4] = { on, 0, 0, 0 };
		PackedByteArray params_bytes;
		params_bytes.resize(BLIT_PARAMS_BYTES);
		memcpy(params_bytes.ptrw(), v, sizeof(v));
		rd->buffer_update(buf_blit_ocean_params, 0, BLIT_PARAMS_BYTES, params_bytes.ptr());
	}
	auto make_blit_uset = [&](RID p_shader, RID p_params, RID p_in_buf, RID p_out_tex) {
		Vector<RD::Uniform> uniforms;
		RD::Uniform u0;
		u0.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u0.binding = 0;
		u0.append_id(p_params);
		uniforms.push_back(u0);
		RD::Uniform u1;
		u1.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u1.binding = 1;
		u1.append_id(p_in_buf);
		uniforms.push_back(u1);
		RD::Uniform u2;
		u2.uniform_type = RD::UNIFORM_TYPE_IMAGE;
		u2.binding = 2;
		u2.append_id(p_out_tex);
		uniforms.push_back(u2);
		return rd->uniform_set_create(uniforms, p_shader, 0);
	};
	uset_blit_ripple = make_blit_uset(shader_blit_ripple, buf_blit_ripple_params, buf_height, tex_ripple_height);
	{
		Vector<RD::Uniform> uniforms;
		const RID ids[8] = { buf_blit_ocean_params, buf_fft_b, tex_ocean_height, buf_fft_d, tex_ocean_disp, buf_fft_f, buf_fft_h, tex_ocean_deriv };
		const RD::UniformType types[8] = { RD::UNIFORM_TYPE_UNIFORM_BUFFER, RD::UNIFORM_TYPE_STORAGE_BUFFER, RD::UNIFORM_TYPE_IMAGE,
			RD::UNIFORM_TYPE_STORAGE_BUFFER, RD::UNIFORM_TYPE_IMAGE, RD::UNIFORM_TYPE_STORAGE_BUFFER, RD::UNIFORM_TYPE_STORAGE_BUFFER,
			RD::UNIFORM_TYPE_IMAGE };
		for (int bnd = 0; bnd < 8; bnd++) {
			RD::Uniform uniform;
			uniform.uniform_type = types[bnd];
			uniform.binding = bnd;
			uniform.append_id(ids[bnd]);
			uniforms.push_back(uniform);
		}
		uset_blit_ocean = rd->uniform_set_create(uniforms, shader_blit_ocean, 0);
	}
	buf_foam_params = rd->uniform_buffer_create(FOAM_PARAMS_BYTES);
	{
		Vector<RD::Uniform> uniforms;
		const RID ids[4] = { buf_foam_params, tex_ocean_deriv, tex_ocean_disp, tex_ocean_foam };
		for (int bnd = 0; bnd < 4; bnd++) {
			RD::Uniform uniform;
			uniform.uniform_type = bnd == 0 ? RD::UNIFORM_TYPE_UNIFORM_BUFFER : RD::UNIFORM_TYPE_IMAGE;
			uniform.binding = bnd;
			uniform.append_id(ids[bnd]);
			uniforms.push_back(uniform);
		}
		uset_foam = rd->uniform_set_create(uniforms, shader_foam, 0);
	}
	{
		RD::TextureFormat shore_tf = ripple_tf;
		Vector<uint8_t> zeros;
		zeros.resize(cells * sizeof(float));
		memset(zeros.ptrw(), 0, zeros.size());
		Vector<Vector<uint8_t>> shore_data;
		shore_data.push_back(zeros);
		tex_shore_foam = rd->texture_create(shore_tf, RD::TextureView(), shore_data);
		tex_shore_foam_tmp = rd->texture_create(shore_tf, RD::TextureView(), shore_data);
	}
	{
		RD::TextureFormat swash_tf = ripple_tf;
		swash_tf.format = RD::DATA_FORMAT_R32G32B32A32_SFLOAT;
		Vector<uint8_t> zeros;
		zeros.resize(cells * 4 * sizeof(float));
		memset(zeros.ptrw(), 0, zeros.size());
		Vector<Vector<uint8_t>> swash_data;
		swash_data.push_back(zeros);
		tex_swash = rd->texture_create(swash_tf, RD::TextureView(), swash_data);
		tex_swash_tmp = rd->texture_create(swash_tf, RD::TextureView(), swash_data);
	}
	buf_shore_foam_params = rd->uniform_buffer_create(SHORE_FOAM_PARAMS_BYTES);
	buf_shore_foam_copy_params = rd->uniform_buffer_create(SHORE_FOAM_PARAMS_BYTES);
	{
		// Copy-mode params never change: just grid_size and the copy flag.
		int32_t copy_res[4] = { grid_size, 1, 1, 0 };
		Vector<uint8_t> cb;
		cb.resize(SHORE_FOAM_PARAMS_BYTES);
		memset(cb.ptrw(), 0, cb.size());
		memcpy(cb.ptrw() + 48, copy_res, sizeof(copy_res));
		rd->buffer_update(buf_shore_foam_copy_params, 0, SHORE_FOAM_PARAMS_BYTES, cb.ptr());
	}
	// Advect + inject reads tex_shore_foam, writes the scratch; the copy
	// pass writes it back so materials always sample tex_shore_foam.
	auto make_shore_uset = [&](RID p_params, RID p_src, RID p_dst) {
		Vector<RD::Uniform> uniforms;
		RD::Uniform u0;
		u0.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u0.binding = 0;
		u0.append_id(p_params);
		uniforms.push_back(u0);
		const RID sampled[3] = { tex_cell_depth, tex_ripple_height, tex_ocean_height };
		for (int i = 0; i < 3; i++) {
			RD::Uniform uniform;
			uniform.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
			uniform.binding = i + 1;
			uniform.append_id(sampler_linear);
			uniform.append_id(sampled[i]);
			uniforms.push_back(uniform);
		}
		const RID images[2] = { p_src, p_dst };
		for (int i = 0; i < 2; i++) {
			RD::Uniform uniform;
			uniform.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			uniform.binding = 4 + i;
			uniform.append_id(images[i]);
			uniforms.push_back(uniform);
		}
		RD::Uniform us;
		us.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
		us.binding = 6;
		us.append_id(sampler_linear);
		us.append_id(tex_swash);
		uniforms.push_back(us);
		return rd->uniform_set_create(uniforms, shader_shore_foam, 0);
	};
	uset_shore_foam = make_shore_uset(buf_shore_foam_params, tex_shore_foam, tex_shore_foam_tmp);
	uset_shore_foam_copy = make_shore_uset(buf_shore_foam_copy_params, tex_shore_foam_tmp, tex_shore_foam);

	buf_swash_params = rd->uniform_buffer_create(SWASH_PARAMS_BYTES);
	buf_swash_copy_params = rd->uniform_buffer_create(SWASH_PARAMS_BYTES);
	{
		int32_t copy_res[4] = { grid_size, 1, 0, 0 };
		Vector<uint8_t> cb;
		cb.resize(SWASH_PARAMS_BYTES);
		memset(cb.ptrw(), 0, cb.size());
		memcpy(cb.ptrw() + 48, copy_res, sizeof(copy_res));
		rd->buffer_update(buf_swash_copy_params, 0, SWASH_PARAMS_BYTES, cb.ptr());
	}
	auto make_swash_uset = [&](RID p_params, RID p_src, RID p_dst) {
		Vector<RD::Uniform> uniforms;
		RD::Uniform u0;
		u0.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
		u0.binding = 0;
		u0.append_id(p_params);
		uniforms.push_back(u0);
		const RID sampled[3] = { tex_cell_depth, tex_ripple_height, tex_ocean_height };
		for (int i = 0; i < 3; i++) {
			RD::Uniform uniform;
			uniform.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
			uniform.binding = i + 1;
			uniform.append_id(sampler_linear);
			uniform.append_id(sampled[i]);
			uniforms.push_back(uniform);
		}
		const RID images[2] = { p_src, p_dst };
		for (int i = 0; i < 2; i++) {
			RD::Uniform uniform;
			uniform.uniform_type = RD::UNIFORM_TYPE_IMAGE;
			uniform.binding = 4 + i;
			uniform.append_id(images[i]);
			uniforms.push_back(uniform);
		}
		return rd->uniform_set_create(uniforms, shader_swash, 0);
	};
	uset_swash = make_swash_uset(buf_swash_params, tex_swash, tex_swash_tmp);
	uset_swash_copy = make_swash_uset(buf_swash_copy_params, tex_swash_tmp, tex_swash);

	// h0 depends only on static settings (wind/domain/amplitude) -- generate
	// it once here, not every step.
	{
		Vector<uint8_t> ib;
		ib.resize(OCEAN_INIT_PARAMS_BYTES);
		uint8_t *iw = ib.ptrw();
		auto put_f = [&](int off, float v) { memcpy(iw + off, &v, sizeof(float)); };
		auto put_i = [&](int off, int32_t v) { memcpy(iw + off, &v, sizeof(int32_t)); };
		put_f(0, ocean_domain_size.x);
		put_f(4, ocean_domain_size.y);
		put_f(8, _init_wind_speed);
		put_f(12, _init_wind_direction.x);
		put_f(16, _init_wind_direction.y);
		put_f(20, _init_gravity);
		put_f(24, _init_wave_amplitude * _init_wave_amplitude);
		put_f(28, 0.0f);
		put_i(32, on);
		put_i(36, 0);
		put_i(40, 0);
		put_i(44, 0);
		put_f(48, _init_fetch);
		put_f(52, _init_depth);
		put_f(56, 0.0f);
		put_f(60, 0.0f);
		rd->buffer_update(buf_ocean_init_params, 0, OCEAN_INIT_PARAMS_BYTES, ib.ptr());

		RD::ComputeListID cl = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(cl, pipeline_spectrum_init);
		rd->compute_list_bind_uniform_set(cl, uset_spectrum_init, 0);
		rd->compute_list_dispatch(cl, groups_for(on), groups_for(on), 1);
		rd->compute_list_end();
		if (local) {
			rd->submit();
			rd->sync();
		}
		// Shared device: this one-time dispatch just gets recorded and rides
		// along with whatever the render thread submits next (the engine's
		// own per-frame flush) -- h0 only needs to be ready before the first
		// rt_step() runs, and rt_build()/rt_step() are always posted to the
		// render thread in that order, so ordering is already guaranteed.
	}

	if (_init_caustics_enabled) {
		_rt_build_caustics_grid();
	}
}

void WaterSolverGPU::_rt_build_caustics_grid() {
	const int caustics_grid_size = MAX(ocean_grid_resolution * 4, CAUSTICS_GRID_MIN_RESOLUTION);
	const int vertex_count = caustics_grid_size * caustics_grid_size;
	Vector<float> vertices;
	vertices.resize(vertex_count * 2);
	for (int z = 0; z < caustics_grid_size; z++) {
		for (int x = 0; x < caustics_grid_size; x++) {
			const int i = (z * caustics_grid_size + x) * 2;
			vertices.write[i] = ((float)x / (caustics_grid_size - 1) - 0.5f) * ocean_domain_size.x * CAUSTICS_TILE_MARGIN;
			vertices.write[i + 1] = ((float)z / (caustics_grid_size - 1) - 0.5f) * ocean_domain_size.y * CAUSTICS_TILE_MARGIN;
		}
	}
	caustics_vertex_buffer = rd->vertex_buffer_create(vertices.size() * sizeof(float), vertices.span().reinterpret<uint8_t>());

	caustics_index_count = (caustics_grid_size - 1) * (caustics_grid_size - 1) * 6;
	Vector<uint32_t> indices;
	indices.resize(caustics_index_count);
	int index = 0;
	for (int z = 0; z < caustics_grid_size - 1; z++) {
		for (int x = 0; x < caustics_grid_size - 1; x++) {
			const uint32_t idx_top_left = z * caustics_grid_size + x;
			const uint32_t idx_top_right = idx_top_left + 1;
			const uint32_t idx_bottom_left = idx_top_left + caustics_grid_size;
			const uint32_t idx_bottom_right = idx_bottom_left + 1;
			indices.write[index++] = idx_top_left;
			indices.write[index++] = idx_bottom_left;
			indices.write[index++] = idx_top_right;
			indices.write[index++] = idx_top_right;
			indices.write[index++] = idx_bottom_left;
			indices.write[index++] = idx_bottom_right;
		}
	}
	caustics_index_buffer = rd->index_buffer_create(caustics_index_count, RD::INDEX_BUFFER_FORMAT_UINT32, indices.span().reinterpret<uint8_t>());
	caustics_index_array = rd->index_array_create(caustics_index_buffer, 0, caustics_index_count);
	Vector<RID> vertex_buffers;
	vertex_buffers.push_back(caustics_vertex_buffer);
	caustics_vertex_array = rd->vertex_array_create(vertex_count, caustics_vertex_format, vertex_buffers);

	RD::TextureFormat texture_format;
	texture_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	texture_format.width = CAUSTICS_MAP_SIZE;
	texture_format.height = CAUSTICS_MAP_SIZE;
	texture_format.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
	tex_caustics = rd->texture_create(texture_format, RD::TextureView());

	Vector<RD::AttachmentFormat> attachment_formats;
	RD::AttachmentFormat attachment_format;
	attachment_format.format = RD::DATA_FORMAT_R8G8B8A8_UNORM;
	attachment_format.usage_flags = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
	attachment_formats.push_back(attachment_format);
	const RD::FramebufferFormatID framebuffer_format = rd->framebuffer_format_create(attachment_formats);
	Vector<RID> attachments;
	attachments.push_back(tex_caustics);
	caustics_framebuffer = rd->framebuffer_create(attachments, framebuffer_format);

	buf_caustics_params = rd->uniform_buffer_create(CAUSTICS_PARAMS_BYTES);
	Vector<RD::Uniform> uniforms;
	RD::Uniform params_uniform;
	params_uniform.uniform_type = RD::UNIFORM_TYPE_UNIFORM_BUFFER;
	params_uniform.binding = 0;
	params_uniform.append_id(buf_caustics_params);
	uniforms.push_back(params_uniform);
	for (int i = 0; i < 2; i++) {
		RD::Uniform height_uniform;
		height_uniform.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
		height_uniform.binding = i + 1;
		// The ocean wraps across its edge (the map spans more than one tile):
		// a clamping sampler put a seam of bright caustics there.
		height_uniform.append_id(i == 0 ? sampler_linear : sampler_linear_repeat);
		height_uniform.append_id(i == 0 ? tex_ripple_height : tex_ocean_height);
		uniforms.push_back(height_uniform);
	}
	RD::Uniform depth_uniform;
	depth_uniform.uniform_type = RD::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
	depth_uniform.binding = 3;
	depth_uniform.append_id(sampler_linear);
	depth_uniform.append_id(tex_cell_depth);
	uniforms.push_back(depth_uniform);
	uset_caustics = rd->uniform_set_create(uniforms, shader_caustics, 0);
}

void WaterSolverGPU::rt_build(Ref<WaterSolverGPU> p_self, int p_grid_resolution, Vector2 p_domain_size, int p_ocean_grid_resolution, Vector2 p_ocean_domain_size, float p_wind_speed, Vector2 p_wind_direction, float p_wave_amplitude, float p_gravity, bool p_caustics_enabled, PackedFloat32Array p_cell_depth, float p_depth, float p_shallow_fade_depth, float p_fetch) {
	if (rd == nullptr || !shaders_ok) {
		return;
	}
	built.set_to(false);
	_rt_free_buffers();
	grid_resolution = p_grid_resolution;
	domain_size = p_domain_size;
	ocean_grid_resolution = p_ocean_grid_resolution;
	ocean_domain_size = p_ocean_domain_size;
	_init_wind_speed = p_wind_speed;
	_init_wind_direction = p_wind_direction;
	_init_wave_amplitude = p_wave_amplitude;
	_init_gravity = p_gravity;
	_init_caustics_enabled = p_caustics_enabled;
	_init_cell_depth = p_cell_depth;
	_init_depth = p_depth;
	_init_fetch = MAX(p_fetch, 1.0f);
	shallow_fade_depth = p_shallow_fade_depth;
	_rt_build_buffers();
	built.set_to(true);
}

void WaterSolverGPU::rt_step(Ref<WaterSolverGPU> p_self, double p_delta, float p_depth, float p_damping, float p_gravity, float p_water_level, float p_ripple_amplitude, PackedFloat32Array p_spheres, PackedFloat32Array p_impulses) {
	if (rd == nullptr || !shaders_ok || buf_params.is_null()) {
		return;
	}
	const int grid_size = grid_resolution;
	const int on = ocean_grid_resolution;

	// Ripple layer params + body data.
	{
		Vector<uint8_t> params_payload;
		params_payload.resize(PARAMS_BYTES);
		uint8_t *w = params_payload.ptrw();
		auto put_f = [&](int off, float v) { memcpy(w + off, &v, sizeof(float)); };
		auto put_i = [&](int off, int32_t v) { memcpy(w + off, &v, sizeof(int32_t)); };
		put_f(0, domain_size.x * 0.5f);
		put_f(4, domain_size.y * 0.5f);
		put_f(8, p_depth);
		put_f(12, (float)p_delta);
		put_f(16, p_damping);
		put_f(20, p_gravity);
		put_f(24, domain_size.x / (float)grid_size);
		put_f(28, p_water_level);
		put_i(32, grid_size);
		put_i(36, p_spheres.size() / 4);
		put_i(40, p_impulses.size() / 4);
		put_i(44, 0);
		put_f(48, p_ripple_amplitude);
		put_f(52, 0.0f);
		put_f(56, 0.0f);
		put_f(60, 0.0f);
		rd->buffer_update(buf_params, 0, PARAMS_BYTES, params_payload.ptr());
	}
	{
		Vector<uint8_t> spheres_payload;
		spheres_payload.resize(MAX_SPHERES * 4 * sizeof(float));
		memset(spheres_payload.ptrw(), 0, spheres_payload.size());
		memcpy(spheres_payload.ptrw(), p_spheres.ptr(), MIN((size_t)p_spheres.size() * sizeof(float), (size_t)spheres_payload.size()));
		rd->buffer_update(buf_spheres, 0, spheres_payload.size(), spheres_payload.ptr());
	}
	{
		Vector<uint8_t> ib;
		ib.resize(MAX_IMPULSES * 4 * sizeof(float));
		memset(ib.ptrw(), 0, ib.size());
		memcpy(ib.ptrw(), p_impulses.ptr(), MIN((size_t)p_impulses.size() * sizeof(float), (size_t)ib.size()));
		rd->buffer_update(buf_impulses, 0, ib.size(), ib.ptr());
	}
	{
		RD::ComputeListID cl = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(cl, pipeline_ripple);
		rd->compute_list_bind_uniform_set(cl, a_is_current ? uset_atob : uset_btoa, 0);
		rd->compute_list_dispatch(cl, groups_for(grid_size), groups_for(grid_size), 1);
		rd->compute_list_end();
	}
	a_is_current = !a_is_current;

	// FFT ocean layer: evolve, then the Stockham butterfly passes.
	ocean_time_accum += p_delta;
	{
		Vector<uint8_t> eb;
		eb.resize(OCEAN_EVOLVE_PARAMS_BYTES);
		uint8_t *w = eb.ptrw();
		auto put_f = [&](int off, float v) { memcpy(w + off, &v, sizeof(float)); };
		auto put_i = [&](int off, int32_t v) { memcpy(w + off, &v, sizeof(int32_t)); };
		put_f(0, ocean_domain_size.x);
		put_f(4, ocean_domain_size.y);
		put_f(8, p_depth);
		put_f(12, (float)ocean_time_accum);
		put_i(16, on);
		put_i(20, 0);
		put_i(24, 0);
		put_i(28, 0);
		rd->buffer_update(buf_ocean_evolve_params, 0, OCEAN_EVOLVE_PARAMS_BYTES, eb.ptr());
	}
	{
		RD::ComputeListID cl = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(cl, pipeline_spectrum_evolve);
		rd->compute_list_bind_uniform_set(cl, uset_spectrum_evolve, 0);
		rd->compute_list_dispatch(cl, groups_for(on), groups_for(on), 1);
		rd->compute_list_end();
	}

	bool first = true;
	bool cur_is_a = false;
	for (int dir = 0; dir < 2; dir++) {
		for (int p = 0; p < ocean_log2n; p++) {
			const int ns = 1 << (p + 1);
			Vector<uint8_t> fb;
			fb.resize(FFT_PARAMS_BYTES);
			int32_t vals[4] = { on, ns, dir == 0 ? 1 : 0, 0 };
			memcpy(fb.ptrw(), vals, sizeof(vals));
			rd->buffer_update(buf_fft_params, 0, FFT_PARAMS_BYTES, fb.ptr());

			// Both chains (height + x displacement, z displacement) share each
			// pass's params and ping-pong in lockstep.
			RID usets_pass[4];
			if (first) {
				usets_pass[0] = uset_fft_spec_to_a;
				usets_pass[1] = uset_fft_dz_to_c;
				usets_pass[2] = uset_fft_c_to_e;
				usets_pass[3] = uset_fft_d_to_g;
				first = false;
				cur_is_a = true;
			} else {
				usets_pass[0] = cur_is_a ? uset_fft_atob : uset_fft_btoa;
				usets_pass[1] = cur_is_a ? uset_fft_ctod : uset_fft_dtoc;
				usets_pass[2] = cur_is_a ? uset_fft_etof : uset_fft_ftoe;
				usets_pass[3] = cur_is_a ? uset_fft_gtoh : uset_fft_htog;
				cur_is_a = !cur_is_a;
			}
			RD::ComputeListID cl = rd->compute_list_begin();
			rd->compute_list_bind_compute_pipeline(cl, pipeline_fft);
			for (const RID &pass_uset : usets_pass) {
				rd->compute_list_bind_uniform_set(cl, pass_uset, 0);
				rd->compute_list_dispatch(cl, groups_for(on), groups_for(on), 1);
			}
			rd->compute_list_end();
		}
	}
	// Always lands in fft_b (and fft_d, fft_f, fft_h) -- 2*log2(N) passes is
	// always even (see the header's note).

	// Blit both layers into their real RD textures for zero-copy rendering.
	{
		RD::ComputeListID cl = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(cl, pipeline_blit_ripple);
		rd->compute_list_bind_uniform_set(cl, uset_blit_ripple, 0);
		rd->compute_list_dispatch(cl, groups_for(grid_size), groups_for(grid_size), 1);
		rd->compute_list_end();
	}
	{
		RD::ComputeListID cl = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(cl, pipeline_blit_ocean);
		rd->compute_list_bind_uniform_set(cl, uset_blit_ocean, 0);
		rd->compute_list_dispatch(cl, groups_for(on), groups_for(on), 1);
		rd->compute_list_end();
	}
	// Persistent foam: inject where the surface folds, fade the rest.
	{
		float fp[4] = { Math::exp(-(float)p_delta / MAX(foam_persistence, 0.01f)), foam_choppiness, foam_threshold, 0.25f };
		int32_t fr[4] = { on, foam_enabled ? 1 : 0, 0, 0 };
		Vector<uint8_t> fb;
		fb.resize(FOAM_PARAMS_BYTES);
		memcpy(fb.ptrw(), fp, sizeof(fp));
		memcpy(fb.ptrw() + sizeof(fp), fr, sizeof(fr));
		rd->buffer_update(buf_foam_params, 0, FOAM_PARAMS_BYTES, fb.ptr());
		RD::ComputeListID cl = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(cl, pipeline_foam);
		rd->compute_list_bind_uniform_set(cl, uset_foam, 0);
		rd->compute_list_dispatch(cl, groups_for(on), groups_for(on), 1);
		rd->compute_list_end();
	}
	// Shore foam over the shallows, on the ripple grid. Lingers a bit longer
	// than the whitecaps.
	{
		float sp[12] = {
			Math::exp(-(float)p_delta / MAX(foam_persistence * 1.5f, 0.01f)), shore_foam_band, 0.2f, p_water_level,
			domain_size.x, domain_size.y, ocean_domain_size.x, ocean_domain_size.y,
			(float)p_delta, domain_size.x / (float)grid_size, shore_undertow, p_gravity
		};
		int32_t sr[4] = { grid_size, (foam_enabled && shore_foam_band > 0.0f) ? 1 : 0, 0, 0 };
		Vector<uint8_t> spheres_payload;
		spheres_payload.resize(SHORE_FOAM_PARAMS_BYTES);
		memcpy(spheres_payload.ptrw(), sp, sizeof(sp));
		memcpy(spheres_payload.ptrw() + sizeof(sp), sr, sizeof(sr));
		rd->buffer_update(buf_shore_foam_params, 0, SHORE_FOAM_PARAMS_BYTES, spheres_payload.ptr());
		RD::ComputeListID cl = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(cl, pipeline_shore_foam);
		rd->compute_list_bind_uniform_set(cl, uset_shore_foam, 0);
		rd->compute_list_dispatch(cl, groups_for(grid_size), groups_for(grid_size), 1);
		rd->compute_list_add_barrier(cl);
		rd->compute_list_bind_uniform_set(cl, uset_shore_foam_copy, 0);
		rd->compute_list_dispatch(cl, groups_for(grid_size), groups_for(grid_size), 1);
		rd->compute_list_end();
	}
	// Swash run-up and wet sand, on the ripple grid.
	{
		const float dt = (float)p_delta;
		float wp[12] = {
			dt, swash_run_up, swash_drain_speed, Math::exp(-dt / MAX(wet_sand_dry_time, 0.05f)),
			domain_size.x, domain_size.y, ocean_domain_size.x, ocean_domain_size.y,
			p_water_level, (float)Math::fmod(ocean_time_accum, 10000.0), 0.25f, 1.0f - Math::exp(-dt / 6.0f)
		};
		int32_t wr[4] = { grid_size, 0, 0, 0 };
		Vector<uint8_t> wb;
		wb.resize(SWASH_PARAMS_BYTES);
		memcpy(wb.ptrw(), wp, sizeof(wp));
		memcpy(wb.ptrw() + sizeof(wp), wr, sizeof(wr));
		rd->buffer_update(buf_swash_params, 0, SWASH_PARAMS_BYTES, wb.ptr());
		RD::ComputeListID cl = rd->compute_list_begin();
		rd->compute_list_bind_compute_pipeline(cl, pipeline_swash);
		rd->compute_list_bind_uniform_set(cl, uset_swash, 0);
		rd->compute_list_dispatch(cl, groups_for(grid_size), groups_for(grid_size), 1);
		rd->compute_list_add_barrier(cl);
		rd->compute_list_bind_uniform_set(cl, uset_swash_copy, 0);
		rd->compute_list_dispatch(cl, groups_for(grid_size), groups_for(grid_size), 1);
		rd->compute_list_end();
	}

	if (local) {
		rd->submit();
		rd->sync();
		MutexLock lock(cache_mtx);
		Vector<uint8_t> rraw = rd->buffer_get_data(buf_height, 0, grid_size * grid_size * sizeof(float));
		ripple_height_cache.resize(grid_size * grid_size);
		if (rraw.size() > 0) {
			memcpy(ripple_height_cache.ptrw(), rraw.ptr(), MIN((size_t)rraw.size(), (size_t)(grid_size * grid_size * sizeof(float))));
		}
		Vector<uint8_t> oraw = rd->buffer_get_data(buf_fft_b, 0, on * on * 2 * sizeof(float));
		ocean_height_cache.resize(on * on);
		ocean_imag_cache.resize(on * on);
		if (oraw.size() > 0) {
			const float *od = (const float *)oraw.ptr();
			float *hw = ocean_height_cache.ptrw();
			float *iw = ocean_imag_cache.ptrw();
			for (int i = 0; i < on * on; i++) {
				hw[i] = od[i * 2 + 0];
				iw[i] = od[i * 2 + 1];
			}
		}
		Vector<uint8_t> dzraw = rd->buffer_get_data(buf_fft_d, 0, on * on * 2 * sizeof(float));
		ocean_dz_cache.resize(on * on);
		ocean_slope_x_cache.resize(on * on);
		if (dzraw.size() > 0) {
			const float *dd = (const float *)dzraw.ptr();
			float *zw = ocean_dz_cache.ptrw();
			float *sw = ocean_slope_x_cache.ptrw();
			for (int i = 0; i < on * on; i++) {
				zw[i] = dd[i * 2 + 0];
				sw[i] = dd[i * 2 + 1];
			}
		}
		height_ready.set();
		return;
	}

	// Shared device: async readback, two independent reads (ripple, ocean) --
	// lands a few frames later on the render thread and refreshes the
	// caches; each callback binds a Ref to us so we survive the wait, same
	// convention as every other async readback in this module.
	rd->buffer_get_data_async(buf_height, callable_mp(this, &WaterSolverGPU::rt_on_ripple_height).bind(p_self), 0, grid_size * grid_size * sizeof(float));
	rd->buffer_get_data_async(buf_fft_b, callable_mp(this, &WaterSolverGPU::rt_on_ocean_height).bind(p_self), 0, on * on * 2 * sizeof(float));
	rd->buffer_get_data_async(buf_fft_d, callable_mp(this, &WaterSolverGPU::rt_on_ocean_dz).bind(p_self), 0, on * on * 2 * sizeof(float));
}

void WaterSolverGPU::rt_on_ocean_dz(const PackedByteArray &p_data, Ref<WaterSolverGPU> p_self) {
	MutexLock lock(cache_mtx);
	const int on = ocean_grid_resolution;
	ocean_dz_cache.resize(on * on);
	ocean_slope_x_cache.resize(on * on);
	if (p_data.size() > 0) {
		const float *dd = (const float *)p_data.ptr();
		const int count = MIN((int)(p_data.size() / (2 * (int)sizeof(float))), on * on);
		float *zw = ocean_dz_cache.ptrw();
		float *sw = ocean_slope_x_cache.ptrw();
		for (int i = 0; i < count; i++) {
			zw[i] = dd[i * 2 + 0];
			sw[i] = dd[i * 2 + 1];
		}
	}
}

void WaterSolverGPU::rt_render_caustics(Ref<WaterSolverGPU> p_self, Vector3 p_sun_direction, Vector3 p_light_right, Vector3 p_light_up, Vector3 p_origin, float p_half_extent, float p_reference_depth, float p_ior, bool p_open_beyond) {
	if (rd == nullptr || !shaders_ok || uset_caustics.is_null() || caustics_framebuffer.is_null()) {
		return;
	}

	Vector<uint8_t> params;
	params.resize(CAUSTICS_PARAMS_BYTES);
	memset(params.ptrw(), 0, params.size());
	uint8_t *w = params.ptrw();
	auto put_f = [&](int p_offset, float p_value) { memcpy(w + p_offset, &p_value, sizeof(float)); };
	auto put_v3 = [&](int p_offset, const Vector3 &p_value, float p_w) {
		put_f(p_offset, p_value.x);
		put_f(p_offset + 4, p_value.y);
		put_f(p_offset + 8, p_value.z);
		put_f(p_offset + 12, p_w);
	};
	put_v3(0, p_sun_direction, p_ior);
	put_v3(16, p_light_right, p_half_extent);
	put_v3(32, p_light_up, p_reference_depth);
	put_f(48, domain_size.x);
	put_f(52, domain_size.y);
	put_f(56, (has_cell_depth ? 1.0f : 0.0f) + (p_open_beyond ? 2.0f : 0.0f));
	put_f(60, shallow_fade_depth);
	put_f(64, ocean_domain_size.x);
	put_f(68, ocean_domain_size.y);
	put_f(72, ocean_grid_resolution);
	put_f(76, ocean_grid_resolution);
	put_f(80, CAUSTICS_MAP_SIZE);
	put_f(84, CAUSTICS_MAP_SIZE);
	put_v3(96, p_origin, 0.0f);
	rd->buffer_update(buf_caustics_params, 0, CAUSTICS_PARAMS_BYTES, params.ptr());

	Vector<Color> clear_colors;
	clear_colors.push_back(Color(0, 0, 0, 1));
	RD::DrawListID draw_list = rd->draw_list_begin(caustics_framebuffer, RD::DRAW_CLEAR_COLOR_0, clear_colors);
	rd->draw_list_set_viewport(draw_list, Rect2(0, 0, CAUSTICS_MAP_SIZE, CAUSTICS_MAP_SIZE));
	rd->draw_list_bind_render_pipeline(draw_list, pipeline_caustics);
	rd->draw_list_bind_uniform_set(draw_list, uset_caustics, 0);
	rd->draw_list_bind_vertex_array(draw_list, caustics_vertex_array);
	rd->draw_list_bind_index_array(draw_list, caustics_index_array);
	// Instance 0: the refracted caustics (R); instance 1: the water mask (G).
	rd->draw_list_draw(draw_list, true, 2);
	rd->draw_list_end();
}

void WaterSolverGPU::rt_on_ripple_height(const PackedByteArray &p_data, Ref<WaterSolverGPU> p_self) {
	MutexLock lock(cache_mtx);
	const int grid_size = grid_resolution;
	ripple_height_cache.resize(grid_size * grid_size);
	if (p_data.size() > 0) {
		memcpy(ripple_height_cache.ptrw(), p_data.ptr(), MIN((size_t)p_data.size(), (size_t)(grid_size * grid_size * sizeof(float))));
	}
	height_ready.set();
}

void WaterSolverGPU::rt_on_ocean_height(const PackedByteArray &p_data, Ref<WaterSolverGPU> p_self) {
	MutexLock lock(cache_mtx);
	const int on = ocean_grid_resolution;
	ocean_height_cache.resize(on * on);
	ocean_imag_cache.resize(on * on);
	if (p_data.size() > 0) {
		const float *od = (const float *)p_data.ptr();
		const int count = MIN((int)(p_data.size() / (2 * (int)sizeof(float))), on * on);
		float *hw = ocean_height_cache.ptrw();
		float *iw = ocean_imag_cache.ptrw();
		for (int i = 0; i < count; i++) {
			hw[i] = od[i * 2 + 0];
			iw[i] = od[i * 2 + 1];
		}
	}
}

void WaterSolverGPU::rt_set_foam(Ref<WaterSolverGPU> p_self, bool p_enabled, float p_choppiness, float p_threshold, float p_persistence, float p_shore_band, float p_shore_undertow) {
	foam_enabled = p_enabled;
	shore_foam_band = p_shore_band;
	shore_undertow = p_shore_undertow;
	foam_choppiness = p_choppiness;
	foam_threshold = p_threshold;
	foam_persistence = p_persistence;
}

void WaterSolverGPU::rt_set_swash(Ref<WaterSolverGPU> p_self, float p_run_up, float p_drain_speed, float p_dry_time) {
	swash_run_up = p_run_up;
	swash_drain_speed = p_drain_speed;
	wet_sand_dry_time = p_dry_time;
}

void WaterSolverGPU::rt_free(Ref<WaterSolverGPU> p_self) {
	if (rd == nullptr) {
		return;
	}
	if (local) {
		rd->sync();
	}
	_rt_free_buffers();
	RID *pipelines[] = { &pipeline_ripple, &pipeline_spectrum_init, &pipeline_spectrum_evolve, &pipeline_fft, &pipeline_blit_ripple, &pipeline_blit_ocean, &pipeline_foam, &pipeline_shore_foam, &pipeline_swash, &pipeline_caustics };
	for (RID *pipeline : pipelines) {
		if (pipeline->is_valid()) {
			rd->free(*pipeline);
			*pipeline = RID();
		}
	}
	RID *shaders[] = { &shader_ripple, &shader_spectrum_init, &shader_spectrum_evolve, &shader_fft, &shader_blit_ripple, &shader_blit_ocean, &shader_foam, &shader_shore_foam, &shader_swash, &shader_caustics };
	for (RID *s : shaders) {
		if (s->is_valid()) {
			rd->free(*s);
			*s = RID();
		}
	}
	if (sampler_linear.is_valid()) {
		rd->free(sampler_linear);
		sampler_linear = RID();
	}
	if (sampler_linear_repeat.is_valid()) {
		rd->free(sampler_linear_repeat);
		sampler_linear_repeat = RID();
	}
	shaders_ok = false;
}

/* ===================================================================== */
/*  WaterSolver -- CPU-side front. Preps payloads, posts render work.     */
/* ===================================================================== */

WaterSolver::WaterSolver() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		return;
	}
	gpu.instantiate();
	gpu->rd = rs->get_rendering_device();
	if (gpu->rd == nullptr) {
		// Headless: no main device. Spin up a private local one and drive it
		// synchronously on this thread -- confirmed empirically this session
		// that this ALSO returns null under --headless, so this path only
		// ever actually engages for a real windowed run without a main
		// device for some other reason; matches MPM's own fallback shape.
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
	if (!gpu.is_null()) {
		_dispatch(callable_mp(gpu.ptr(), &WaterSolverGPU::rt_compile).bind(gpu));
	}
}

WaterSolver::~WaterSolver() {
	if (gpu.is_valid() && gpu->rd != nullptr) {
		if (gpu->local) {
			gpu->rt_free(gpu);
			memdelete(gpu->rd); // we own the local device
			gpu->rd = nullptr;
		} else {
			// WATER-2: mirror the constructor's RS null check -- a Ref held by
			// a script can outlive RenderingServer finalization, and this used
			// to be a null deref instead of a graceful no-op.
			RenderingServer *rs = RenderingServer::get_singleton();
			if (rs != nullptr) {
				rs->call_on_render_thread(callable_mp(gpu.ptr(), &WaterSolverGPU::rt_free).bind(gpu));
				rs->sync(); // wait for the render thread to run rt_free before we drop our Ref
			}
		}
	}
	gpu.unref();
}

void WaterSolver::_dispatch(const Callable &p_call) const {
	if (gpu.is_null()) {
		return;
	}
	if (gpu->local) {
		p_call.call();
	} else {
		RenderingServer::get_singleton()->call_on_render_thread(p_call);
	}
}

void WaterSolver::configure(const Settings &p_settings) {
	settings = p_settings;
	for (SphereSlot &s : sphere_slots) {
		s = SphereSlot();
	}
	pending_impulses.clear();
	if (!gpu.is_null()) {
		_dispatch(callable_mp(gpu.ptr(), &WaterSolverGPU::rt_build).bind(gpu, settings.grid_resolution, settings.domain_size, settings.ocean_grid_resolution, settings.ocean_domain_size, settings.wind_speed, settings.wind_direction, settings.wave_amplitude, settings.gravity, settings.caustics_enabled, settings.cell_depth, settings.depth, settings.shallow_fade_depth, settings.fetch));
	}
}

PackedFloat32Array WaterSolver::_pack_spheres() const {
	PackedFloat32Array out;
	for (const SphereSlot &s : sphere_slots) {
		if (!s.enabled) {
			continue;
		}
		out.push_back(s.world_pos.x - settings.grid_center.x);
		out.push_back(s.world_pos.z - settings.grid_center.y);
		out.push_back(s.radius);
		out.push_back(s.strength);
	}
	return out;
}

PackedFloat32Array WaterSolver::_pack_impulses() const {
	PackedFloat32Array out;
	for (const ImpulseSlot &imp : pending_impulses) {
		out.push_back(imp.world_pos.x - settings.grid_center.x);
		out.push_back(imp.world_pos.z - settings.grid_center.y);
		out.push_back(imp.radius);
		out.push_back(imp.strength);
	}
	return out;
}

void WaterSolver::step(double p_delta) {
	if (!is_available()) {
		return;
	}
	if (!gpu.is_null()) {
		_dispatch(callable_mp(gpu.ptr(), &WaterSolverGPU::rt_step).bind(gpu, p_delta, settings.depth, settings.damping, settings.gravity, settings.water_level, settings.ripple_amplitude, _pack_spheres(), _pack_impulses()));
	}
	// One-shot: consumed by exactly one step, then cleared. Persistent
	// sphere proxies stay until explicitly cleared/overwritten by the caller.
	pending_impulses.clear();
}

void WaterSolver::submit_sphere(uint64_t p_owner, const Vector3 &p_world_pos, float p_radius, float p_strength) {
	int free_slot = -1;
	for (int i = 0; i < MAX_SPHERES; i++) {
		if (sphere_slots[i].enabled && sphere_slots[i].owner == p_owner) {
			sphere_slots[i].world_pos = p_world_pos;
			sphere_slots[i].radius = p_radius;
			sphere_slots[i].strength = p_strength;
			return;
		}
		if (free_slot < 0 && !sphere_slots[i].enabled) {
			free_slot = i;
		}
	}
	if (free_slot < 0) {
		return; // MAX_SPHERES exceeded -- silently drop, matches the module's other MAX_* slot conventions
	}
	sphere_slots[free_slot].owner = p_owner;
	sphere_slots[free_slot].world_pos = p_world_pos;
	sphere_slots[free_slot].radius = p_radius;
	sphere_slots[free_slot].strength = p_strength;
	sphere_slots[free_slot].enabled = true;
}

void WaterSolver::clear_sphere(uint64_t p_owner) {
	for (SphereSlot &s : sphere_slots) {
		if (s.enabled && s.owner == p_owner) {
			s = SphereSlot();
			return;
		}
	}
}

void WaterSolver::submit_impulse(const Vector3 &p_world_pos, float p_radius, float p_strength) {
	if (pending_impulses.size() >= MAX_IMPULSES) {
		return;
	}
	ImpulseSlot imp;
	imp.world_pos = p_world_pos;
	imp.radius = p_radius;
	imp.strength = p_strength;
	pending_impulses.push_back(imp);
}

void WaterSolver::render_caustics(const Vector3 &p_origin, const Vector3 &p_sun_direction, float p_reference_depth, float p_ior, bool p_open_beyond) {
	if (!settings.caustics_enabled || !is_available()) {
		return;
	}
	Vector3 sun_direction = p_sun_direction.normalized();
	if (sun_direction.is_zero_approx()) {
		sun_direction = Vector3(0, -1, 0);
	}
	const Vector3 reference_axis = Math::abs(sun_direction.dot(Vector3(0, 1, 0))) < 0.95f ? Vector3(0, 1, 0) : Vector3(0, 0, 1);
	caustics_light_right = sun_direction.cross(reference_axis).normalized();
	caustics_light_up = caustics_light_right.cross(sun_direction).normalized();
	caustics_origin = p_origin;
	p_reference_depth = MAX(p_reference_depth, 0.1f);
	p_ior = MAX(p_ior, 1.001f);
	caustics_half_extent = MAX(settings.ocean_domain_size.x, settings.ocean_domain_size.y) * 0.5f * WaterSolverGPU::CAUSTICS_TILE_MARGIN + p_reference_depth * 0.75f;
	if (!gpu.is_null()) {
		_dispatch(callable_mp(gpu.ptr(), &WaterSolverGPU::rt_render_caustics).bind(gpu, sun_direction, caustics_light_right, caustics_light_up, caustics_origin, caustics_half_extent, p_reference_depth, p_ior, p_open_beyond));
	}
}

RID WaterSolver::get_ripple_height_texture_rd_rid() const {
	if (gpu.is_null()) {
		return RID();
	}
	return gpu->tex_ripple_height;
}

RID WaterSolver::get_ocean_height_texture_rd_rid() const {
	if (gpu.is_null()) {
		return RID();
	}
	return gpu->tex_ocean_height;
}

RID WaterSolver::get_ocean_foam_texture_rd_rid() const {
	if (gpu.is_null()) {
		return RID();
	}
	return gpu->tex_ocean_foam;
}

// WATER-3: live step-constant update — these four are rt_step ARGUMENTS
// (uploaded every tick from these very fields), so a CPU settings write makes
// them take effect on the next step without tearing down the GPU solver.
void WaterSolver::set_step_constants(float p_depth, float p_damping, float p_water_level, float p_ripple_amplitude) {
	settings.depth = p_depth;
	settings.damping = p_damping;
	settings.water_level = p_water_level;
	settings.ripple_amplitude = p_ripple_amplitude;
}

void WaterSolver::set_foam_settings(bool p_enabled, float p_choppiness, float p_threshold, float p_persistence, float p_shore_band, float p_shore_undertow) {
	if (!gpu.is_null()) {
		_dispatch(callable_mp(gpu.ptr(), &WaterSolverGPU::rt_set_foam).bind(gpu, p_enabled, p_choppiness, p_threshold, p_persistence, p_shore_band, p_shore_undertow));
	}
}

void WaterSolver::set_swash_settings(float p_run_up, float p_drain_speed, float p_dry_time) {
	if (!gpu.is_null()) {
		_dispatch(callable_mp(gpu.ptr(), &WaterSolverGPU::rt_set_swash).bind(gpu, p_run_up, p_drain_speed, p_dry_time));
	}
}

RID WaterSolver::get_swash_texture_rd_rid() const {
	if (gpu.is_null()) {
		return RID();
	}
	return gpu->tex_swash;
}

RID WaterSolver::get_shore_foam_texture_rd_rid() const {
	if (gpu.is_null()) {
		return RID();
	}
	return gpu->tex_shore_foam;
}

RID WaterSolver::get_ocean_derivative_texture_rd_rid() const {
	if (gpu.is_null()) {
		return RID();
	}
	return gpu->tex_ocean_deriv;
}

void WaterSolver::get_ocean_slope_x_grid(Vector<float> &r_slope_x) const {
	r_slope_x.clear();
	if (gpu.is_null()) {
		return;
	}
	MutexLock lock(gpu->cache_mtx);
	r_slope_x = gpu->ocean_slope_x_cache;
}

RID WaterSolver::get_ocean_displacement_texture_rd_rid() const {
	if (gpu.is_null()) {
		return RID();
	}
	return gpu->tex_ocean_disp;
}

void WaterSolver::get_ocean_dz_grid(Vector<float> &r_dz) const {
	r_dz.clear();
	if (gpu.is_null()) {
		return;
	}
	MutexLock lock(gpu->cache_mtx);
	r_dz = gpu->ocean_dz_cache;
}

RID WaterSolver::get_shore_depth_texture_rd_rid() const {
	if (gpu.is_null()) {
		return RID();
	}
	return gpu->tex_shore_depth;
}

RID WaterSolver::get_ocean_fade_texture_rd_rid() const {
	if (gpu.is_null()) {
		return RID();
	}
	return gpu->tex_ocean_fade;
}

float WaterSolver::shallow_fade(float p_depth, float p_fade_depth) {
	if (p_depth <= WALL_DEPTH * 0.5f) {
		return 1.0f;
	}
	// smoothstep(0, fade_depth, depth) -- matches the caustics pass.
	const float fade_fraction = CLAMP(p_depth / MAX(p_fade_depth, 1e-3f), 0.0f, 1.0f);
	return fade_fraction * fade_fraction * (3.0f - 2.0f * fade_fraction);
}

RID WaterSolver::get_caustics_texture_rd_rid() const {
	if (gpu.is_null() || !settings.caustics_enabled) {
		return RID();
	}
	return gpu->tex_caustics;
}

void WaterSolver::get_height_grid(Vector<float> &r_height, int &r_n, Vector2 &r_domain_size) const {
	r_height.clear();
	r_n = settings.grid_resolution;
	r_domain_size = settings.domain_size;
	if (gpu.is_null()) {
		return;
	}
	MutexLock lock(gpu->cache_mtx);
	r_height = gpu->ripple_height_cache;
	if (r_height.size() != r_n * r_n) {
		// grid_resolution just changed: the cache is still the old size until
		// the rebuilt solver reads back. Report "not ready" rather than let
		// callers index the old grid with the new size.
		r_height.clear();
	}
}

void WaterSolver::get_ocean_height_grid(Vector<float> &r_height, Vector<float> &r_imag, int &r_n, Vector2 &r_domain_size) const {
	r_height.clear();
	r_imag.clear();
	r_n = settings.ocean_grid_resolution;
	r_domain_size = settings.ocean_domain_size;
	if (gpu.is_null()) {
		return;
	}
	MutexLock lock(gpu->cache_mtx);
	r_height = gpu->ocean_height_cache;
	r_imag = gpu->ocean_imag_cache;
	if (r_height.size() != r_n * r_n || r_imag.size() != r_n * r_n) {
		// ocean_grid_resolution just changed (see get_height_grid()).
		r_height.clear();
		r_imag.clear();
	}
}

void WaterSolver::sync_now() const {
	if (gpu.is_valid() && !gpu->local) {
		RenderingServer::get_singleton()->sync();
	}
}

/**************************************************************************/
/*  mpm_fluid_solver.cpp                                                  */
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

#include "mpm_fluid_solver.h"

#include "shaders/mpm_clear.glsl.gen.h"
#include "shaders/mpm_couple.glsl.gen.h"
#include "shaders/mpm_foam_advect.glsl.gen.h"
#include "shaders/mpm_foam_spawn.glsl.gen.h"
#include "shaders/mpm_g2p.glsl.gen.h"
#include "shaders/mpm_grid.glsl.gen.h"
#include "shaders/mpm_march.glsl.gen.h"
#include "shaders/mpm_p2g_mass.glsl.gen.h"
#include "shaders/mpm_p2g_mom.glsl.gen.h"
#include "shaders/mpm_render.glsl.gen.h"
#include "shaders/mpm_surface.glsl.gen.h"

#include "core/io/marshalls.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "servers/display/display_server.h"
#include "servers/rendering/rendering_device_binds.h"
#include "servers/rendering/rendering_server.h"

namespace {
constexpr int FLOATS_PER_PARTICLE = 32; // 8 * vec4 (x/v/C x3/F x3)
constexpr int FLOATS_PER_COLLIDER = 16; // 4 * vec4 (c0..c3)
constexpr int MAX_COLLIDERS = 32; // analytic colliders per step (rigid bodies + expanded debris chunks)
constexpr uint32_t PARAMS_BYTES = 176; // 11 * vec4, std140
constexpr float IMP_FIXED = 1024.0f;
constexpr int FLOATS_PER_FOAM = 8; // 2 * vec4 (pos/life, vel/kind)
constexpr uint32_t GROUP = 64;
constexpr uint32_t TRI_BUDGET = 200000; // isosurface triangle cap

uint32_t groups_for(int p_count) {
	return (uint32_t)((p_count + (int)GROUP - 1) / (int)GROUP);
}
} // namespace

MPMFluidSolver::MPMFluidSolver() {
	if (!DisplayServer::can_create_rendering_device()) {
		return;
	}
	rd = RenderingServer::get_singleton()->create_local_rendering_device();
	if (rd == nullptr) {
		return;
	}
	if (!_compile_shaders()) {
		memdelete(rd);
		rd = nullptr;
	}
}

MPMFluidSolver::~MPMFluidSolver() {
	if (rd == nullptr) {
		return;
	}
	_free_buffers(); // uniform sets + buffers
	for (int i = 0; i < PASS_MAX; i++) {
		if (pipeline[i].is_valid()) {
			rd->free(pipeline[i]);
			pipeline[i] = RID();
		}
	}
	for (int i = 0; i < PASS_MAX; i++) {
		if (shader[i].is_valid()) {
			rd->free(shader[i]);
			shader[i] = RID();
		}
	}
	memdelete(rd);
	rd = nullptr;
}

bool MPMFluidSolver::_compile_shaders() {
	const char *src[PASS_MAX] = {
		mpm_clear_shader_glsl,
		mpm_p2g_mass_shader_glsl,
		mpm_p2g_mom_shader_glsl,
		mpm_grid_shader_glsl,
		mpm_couple_shader_glsl,
		mpm_g2p_shader_glsl,
		mpm_foam_spawn_shader_glsl,
		mpm_foam_advect_shader_glsl,
		mpm_surface_shader_glsl,
		mpm_march_shader_glsl,
		mpm_render_shader_glsl,
	};
	for (int i = 0; i < PASS_MAX; i++) {
		Ref<RDShaderFile> sf;
		sf.instantiate();
		Error err = sf->parse_versions_from_text(src[i]);
		if (err != OK) {
			ERR_PRINT(vformat("MPMFluidSolver: compute shader %d failed to compile.", i));
			return false;
		}
		shader[i] = rd->shader_create_from_spirv(sf->get_spirv_stages());
		ERR_FAIL_COND_V(shader[i].is_null(), false);
		pipeline[i] = rd->compute_pipeline_create(shader[i]);
		ERR_FAIL_COND_V(pipeline[i].is_null(), false);
	}
	return true;
}

void MPMFluidSolver::_free_buffers() {
	if (_submitted && rd != nullptr) {
		rd->sync(); // don't free buffers with GPU work still in flight
		_submitted = false;
	}
	_mm_cache = PackedFloat32Array();
	_imp_cache.clear();
	_foam_cache = PackedFloat32Array();
	_foam_alive = 0;
	// Uniform sets first: RD auto-frees a uniform set when a buffer it references
	// is freed, so freeing the buffers first leaves these RIDs dangling.
	for (int i = 0; i < PASS_MAX; i++) {
		if (uset[i].is_valid()) {
			rd->free(uset[i]);
			uset[i] = RID();
		}
	}
	if (uset_mesh.is_valid()) {
		rd->free(uset_mesh);
		uset_mesh = RID();
	}
	RID *bufs[] = { &buf_params, &buf_particles, &buf_grid_i, &buf_grid_v, &buf_colliders, &buf_cimp, &buf_mm, &buf_surf, &buf_mverts, &buf_mnorms, &buf_mcount, &buf_foam, &buf_foam_meta, &buf_foam_norm };
	for (RID *b : bufs) {
		if (b->is_valid()) {
			rd->free(*b);
			*b = RID();
		}
	}
}

void MPMFluidSolver::_compute_scales() {
	dx = settings.domain.x / (float)settings.grid_res;
	// Uniform cell size; the grid gets as many cells per axis as the domain needs
	// so a non-cube domain (e.g. a tall tank) is covered.
	grid_dims = Vector3i(
			settings.grid_res,
			CLAMP((int)Math::round(settings.domain.y / dx), 4, 256),
			CLAMP((int)Math::round(settings.domain.z / dx), 4, 256));
	node_count = grid_dims.x * grid_dims.y * grid_dims.z;

	// Hard budget on grid size. An over-ambitious domain (e.g. the flamethrower
	// demo's 5x20x12 m box at 2 cm particles) resolves to millions of nodes:
	// hundreds of MB of grid buffers, tens of thousands of dispatch groups per
	// grid-wide pass (over Vulkan's base 65535-group guarantee -- a GPU hang on
	// drivers that enforce it), and multi-second sync stalls that TDR the
	// display driver. Coarsen dx (re-resolve y/z, growing x coverage for free)
	// until the node count fits; the domain stays covered, cells just get
	// fatter, and the user gets told.
	const int64_t MAX_NODES = 4000000; // 62.5k groups of 64 -- under the 65535 guarantee
	int64_t nodes = (int64_t)grid_dims.x * grid_dims.y * grid_dims.z;
	if (nodes > MAX_NODES) {
		const int64_t nodes_requested = nodes;
		const float requested_dx = dx;
		int guard = 0;
		while (nodes > MAX_NODES && guard++ < 64) {
			dx *= 1.1f;
			grid_dims = Vector3i(
					settings.grid_res,
					CLAMP((int)Math::round(settings.domain.y / dx), 4, 256),
					CLAMP((int)Math::round(settings.domain.z / dx), 4, 256));
			nodes = (int64_t)grid_dims.x * grid_dims.y * grid_dims.z;
		}
		WARN_PRINT(vformat(
				"MPMFluidSolver: domain %s at grid resolution %d resolves to a %.1fM-node grid (over the %dm-node budget); dx coarsened from %.3f m to %.3f m, grid now %dx%dx%d. Narrow the domain or raise particle_size for finer cells.",
				settings.domain, settings.grid_res, (float)nodes_requested / 1000000.0f,
				(int)(MAX_NODES / 1000000), requested_dx, dx,
				grid_dims.x, grid_dims.y, grid_dims.z));
	}

	node_count = (int)nodes;
	const float spacing = dx * 0.5f;
	// Particle mass from the medium density. For granular this is the "weight"
	// knob: the collider coupling exchanges momentum in proportion to it (light
	// snow shoves aside far easier than heavy sand), while the Drucker-Prager
	// stress stays calibrated by hardness/friction independent of it.
	pmass = MAX(settings.rest_density, 1.0f) * spacing * spacing * spacing;
	_recompute_surface_iso();
}

// The poly6 kernel is heavily h-dependent (norm ~ 1/h^9), so the absolute
// density a well-packed fluid reads with a given kernel radius varies a lot.
// Evaluate that packed reference here and take the iso as a fraction of it, so
// the surface stays at a consistent "fullness" as particle_size changes.
void MPMFluidSolver::_recompute_surface_iso() {
	const float sp = MAX(dx * 0.5f, 1e-4f); // particle spacing
	const float h = MAX(settings.surface_kernel, dx);
	const float h2 = h * h;
	const float norm = (315.0f / (64.0f * 3.14159265f * Math::pow(h, 9.0f))) * pmass;
	const int R = (int)Math::ceil(h / sp) + 1;
	float ref = 0.0f;
	for (int z = -R; z <= R; z++) {
		for (int y = -R; y <= R; y++) {
			for (int x = -R; x <= R; x++) {
				const float d2 = (float)(x * x + y * y + z * z) * sp * sp;
				if (d2 < h2) {
					const float t = h2 - d2;
					ref += norm * t * t * t;
				}
			}
		}
	}
	surf_iso_density = CLAMP(settings.surface_iso, 0.05f, 0.95f) * MAX(ref, 1.0f);
}

LocalVector<float> MPMFluidSolver::_seed_block(int &r_count) const {
	const float spacing = dx * 0.5f;

	// Fill spawn_region, centered on the node, clamped to fit inside the domain
	// (leave a 2-cell margin off the boundary), capped at particle_target.
	const Vector3 margin = Vector3(dx, dx, dx) * 2.0f;
	const Vector3 region = settings.spawn_region.clamp(Vector3(spacing, spacing, spacing), settings.domain - margin * 2.0f);
	const int sx = MAX(1, (int)(region.x / spacing));
	const int sy = MAX(1, (int)(region.y / spacing));
	const int sz = MAX(1, (int)(region.z / spacing));

	// The grid is built in the node's LOCAL frame and placed by the full
	// transform, so the seeded slab pitches and steers with the fluid node --
	// a spawn region on a pitched channel stays on the channel. (The PBD
	// path's spawn() transforms its seed grid the same way; without this the
	// MPM seed is world-axis-aligned and half of it lands inside the terrain
	// on any pitched node.)
	const Vector3 start = -Vector3(sx, sy, sz) * (spacing * 0.5f);

	LocalVector<float> data;
	data.resize((int64_t)MIN((int64_t)sx * sy * sz, (int64_t)settings.particle_target) * FLOATS_PER_PARTICLE);
	memset(data.ptr(), 0, data.size() * sizeof(float)); // velocity / C / F start clean
	int idx = 0;
	const int cap = (int)(data.size() / FLOATS_PER_PARTICLE);
	// y outermost: a fill that runs out of particles before the region is full
	// then covers the whole footprint at a shallower depth (a low pool / a thin
	// sand bed), instead of a partial slab banked against one side.
	for (int yi = 0; yi < sy && idx < cap; yi++) {
		for (int zi = 0; zi < sz && idx < cap; zi++) {
			for (int xi = 0; xi < sx && idx < cap; xi++) {
				Vector3 p = start + Vector3(xi, yi, zi) * spacing;
				if (settings.granular) {
					// Break the seed lattice: brick-stagger alternate rows/layers
					// and jitter hard, so a granular pile has no regular planes to
					// shear along (they read as visible stripes otherwise). The
					// stagger is centered (+-) so the block's center of mass does
					// not drift to one corner.
					p.x += (float((yi + zi) & 1) - 0.5f) * spacing * 0.5f;
					p.z += (float(yi & 1) - 0.5f) * spacing * 0.5f;
					p += Vector3(Math::randf() - 0.5f, Math::randf() - 0.5f, Math::randf() - 0.5f) * spacing;
				} else {
					p += Vector3(Math::randf(), Math::randf(), Math::randf()) * spacing * 0.3f;
				}
				p = domain_xform.xform(p);
				float *o = &data[idx * FLOATS_PER_PARTICLE];
				o[0] = p.x;
				o[1] = p.y;
				o[2] = p.z;
				o[3] = settings.rest_density;
				o[20] = o[25] = o[30] = 1.0f; // deformation gradient F = identity
				idx++;
			}
		}
	}
	r_count = idx;
	return data;
}

void MPMFluidSolver::_pack_params(double p_dt, int p_ncol, PackedByteArray &r_bytes) const {
	r_bytes.resize(PARAMS_BYTES);
	uint8_t *b = r_bytes.ptrw();
	const Vector3 center = domain_xform.origin;
	const Vector3 bmin = center - settings.domain * 0.5f;
	const Vector3 bmax = center + settings.domain * 0.5f;
	auto put_f = [&](uint32_t off, float v) { encode_float(v, b + off); };
	auto put_i = [&](uint32_t off, int32_t v) { encode_uint32((uint32_t)v, b + off); };

	put_f(0, settings.gravity.x);
	put_f(4, settings.gravity.y);
	put_f(8, settings.gravity.z);
	put_f(12, (float)p_dt);
	put_f(16, bmin.x);
	put_f(20, bmin.y);
	put_f(24, bmin.z);
	put_f(28, dx);
	put_i(32, grid_dims.x);
	put_i(36, grid_dims.y);
	put_i(40, grid_dims.z);
	put_i(44, pcount);
	put_f(48, settings.rest_density);
	put_f(52, settings.stiffness);
	put_f(56, settings.viscosity);
	put_f(60, pmass);
	put_f(64, bmin.x);
	put_f(68, bmin.y);
	put_f(72, bmin.z);
	put_f(76, settings.surface_boost); // per-particle mass multiplier for the isosurface scatter
	put_f(80, bmax.x);
	put_f(84, bmax.y);
	put_f(88, bmax.z);
	put_f(92, settings.granular ? 1.0f : 0.0f);
	put_f(96, (float)p_ncol);
	put_f(100, settings.collider_friction);
	put_f(104, surf_iso_density);
	put_f(108, settings.surface_kernel);

	// gran: Drucker-Prager alpha (from the internal friction angle), cohesion,
	// and the Lame parameters (from Young's modulus at a fixed Poisson 0.3).
	const float sinp = Math::sin(Math::deg_to_rad(CLAMP(settings.granular_friction_deg, 1.0f, 55.0f)));
	const float alpha = Math::sqrt(2.0f / 3.0f) * (2.0f * sinp) / (3.0f - sinp);
	const float E = MAX(settings.granular_hardness, 1.0f);
	const float nu = 0.3f;
	const float mu = E / (2.0f * (1.0f + nu));
	const float lambda = E * nu / ((1.0f + nu) * (1.0f - 2.0f * nu));
	put_f(112, alpha);
	put_f(116, MAX(settings.granular_cohesion, 0.0f));
	put_f(120, mu);
	put_f(124, lambda);

	// Foam (Vulkan diffuse layer). Frame dt, not substep dt: the foam passes run
	// once per frame after the substep loop. Capacity as float -- the shader
	// reads it for the ring modulo.
	put_f(128, settings.foam_threshold);
	put_f(132, settings.foam_lifetime);
	put_f(136, CLAMP(settings.foam_buoyancy, 0.0f, 1.0f));
	put_f(140, MAX(settings.foam_drag, 0.01f));
	put_f(144, (float)foam_capacity_resolved);
	put_f(148, (float)(p_dt * settings.substeps));
	put_f(152, CLAMP(settings.foam_spray_ratio, 0.0f, 1.0f));
	put_f(156, CLAMP(settings.foam_bubble_ratio, 0.0f, 1.0f));

	// foam_c: Ihmsen et al. 2012 channel rates (trapped air / wave crest /
	// vorticity) plus the kinetic-energy multiplier.
	put_f(160, MAX(settings.foam_rate_ta, 0.0f));
	put_f(164, MAX(settings.foam_rate_wc, 0.0f));
	put_f(168, MAX(settings.foam_rate_vo, 0.0f));
	put_f(172, MAX(settings.foam_rate_ke, 0.0f));
}

void MPMFluidSolver::_pack_colliders(const LocalVector<SphereCollider> &p_colliders, PackedByteArray &r_bytes) const {
	r_bytes.resize(MAX_COLLIDERS * FLOATS_PER_COLLIDER * 4);
	memset(r_bytes.ptrw(), 0, r_bytes.size());
	uint8_t *b = r_bytes.ptrw();
	const int n = MIN((int)p_colliders.size(), MAX_COLLIDERS);
	for (int i = 0; i < n; i++) {
		const Collider &c = p_colliders[i];
		uint8_t *o = b + i * FLOATS_PER_COLLIDER * 4;
		// c0: center, shape
		encode_float(c.position.x, o + 0);
		encode_float(c.position.y, o + 4);
		encode_float(c.position.z, o + 8);
		encode_float((float)c.shape, o + 12);
		// c1: shape params
		Vector3 ext = c.extents;
		if (c.shape == COLLIDER_PLANE) {
			ext = ext.normalized();
		}
		encode_float(ext.x, o + 16);
		encode_float(ext.y, o + 20);
		encode_float(ext.z, o + 24);
		encode_float(0.0f, o + 28);
		// c2: velocity
		encode_float(c.velocity.x, o + 32);
		encode_float(c.velocity.y, o + 36);
		encode_float(c.velocity.z, o + 40);
		encode_float(0.0f, o + 44);
		// c3: orientation quaternion (identity for sphere / plane)
		const Quaternion q = c.rotation.normalized();
		encode_float(q.x, o + 48);
		encode_float(q.y, o + 52);
		encode_float(q.z, o + 56);
		encode_float(q.w, o + 60);
	}
}

bool MPMFluidSolver::_rebuild_uniform_sets() {
	RID by_binding[11] = { buf_params, buf_particles, buf_grid_i, buf_grid_v, buf_colliders, buf_cimp, buf_mm, buf_surf, buf_foam, buf_foam_meta, buf_foam_norm };
	for (int p = 0; p < PASS_MAX; p++) {
		if (uset[p].is_valid()) {
			rd->free(uset[p]);
			uset[p] = RID();
		}
		// Every pass includes mpm_fluid_inc.glsl, which declares all eleven
		// bindings -- shader reflection puts them all in the set layout whether
		// or not a given pass references them, so provide all eleven.
		Vector<RD::Uniform> uniforms;
		for (int bnd = 0; bnd < 11; bnd++) {
			RD::Uniform u;
			u.uniform_type = (bnd == 0) ? RD::UNIFORM_TYPE_UNIFORM_BUFFER : RD::UNIFORM_TYPE_STORAGE_BUFFER;
			u.binding = bnd;
			u.append_id(by_binding[bnd]);
			uniforms.push_back(u);
		}
		uset[p] = rd->uniform_set_create(uniforms, shader[p], 0);
		if (uset[p].is_null()) {
			return false;
		}
	}

	// The march pass's set 1 (mesh output buffers) is bound lazily by
	// _ensure_mesh_buffers() on the first want_surface step (PART-4).
	return true;
}

void MPMFluidSolver::configure(const Settings &p_settings, const Transform3D &p_xform, bool p_prefill) {
	if (rd == nullptr) {
		return;
	}
	settings = p_settings;
	settings.grid_res = CLAMP(settings.grid_res, 8, 128);
	settings.substeps = CLAMP(settings.substeps, 1, 40);
	domain_xform = p_xform;

	// Stiff granular material needs a tighter substep to stay under CFL; and it
	// should grip a collider (near-zero tangential slip) so a resting object
	// does not just shear the grains out from under itself.
	if (settings.granular) {
		settings.substeps = MAX(settings.substeps, 12);
		settings.collider_friction = MAX(settings.collider_friction, 0.85f);
	}

	_free_buffers();
	_compute_scales(); // sets grid_dims + node_count
	write_head = 0;

	LocalVector<float> pdata;
	if (p_prefill) {
		int seeded = 0;
		pdata = _seed_block(seeded);
		capacity = seeded;
		pcount = seeded;
	} else {
		capacity = MAX(settings.particle_target, 1);
		pdata.resize((int64_t)capacity * FLOATS_PER_PARTICLE);
		memset(pdata.ptr(), 0, pdata.size() * sizeof(float)); // parked at the origin, PCOUNT keeps them out of the sim
		pcount = 0;
	}

	PackedByteArray params;
	_pack_params(1.0 / 60.0 / settings.substeps, 0, params);
	buf_params = rd->uniform_buffer_create(PARAMS_BYTES, params);

	buf_particles = rd->storage_buffer_create(pdata.size() * sizeof(float), Span<uint8_t>((const uint8_t *)pdata.ptr(), pdata.size() * sizeof(float)));
	buf_grid_i = rd->storage_buffer_create(node_count * 4 * sizeof(int32_t));
	buf_grid_v = rd->storage_buffer_create(node_count * 4 * sizeof(float));

	PackedByteArray cdata;
	_pack_colliders(LocalVector<SphereCollider>(), cdata);
	buf_colliders = rd->storage_buffer_create(cdata.size(), cdata);
	buf_cimp = rd->storage_buffer_create(MAX_COLLIDERS * 4 * sizeof(int32_t));
	{
		PackedByteArray mm_zero;
		mm_zero.resize(capacity * 12 * sizeof(float));
		memset(mm_zero.ptrw(), 0, mm_zero.size());
		buf_mm = rd->storage_buffer_create(mm_zero.size(), mm_zero);
	}
	buf_surf = rd->storage_buffer_create(node_count * sizeof(float));

	// PART-4: the isosurface mesh output (2 x 9.6 MB + counter) is allocated
	// LAZILY on the first want_surface step -- fluids without a surface mesh
	// (and every granular bed) never pay for it. See _ensure_mesh_buffers().

	// Vulkan diffuse (foam/spray) layer. Slots always exist so the shared
	// uniform-set bindings stay valid; the passes only dispatch when enabled.
	foam_capacity_resolved = settings.foam_enabled ? CLAMP(settings.foam_capacity, 1, 65536) : 1;
	{
		PackedByteArray fzero;
		fzero.resize(foam_capacity_resolved * FLOATS_PER_FOAM * sizeof(float));
		memset(fzero.ptrw(), 0, fzero.size()); // life = 0 everywhere: all slots parked
		buf_foam = rd->storage_buffer_create(fzero.size(), fzero);
	}
	{
		PackedByteArray mzero;
		mzero.resize(4 * sizeof(uint32_t));
		memset(mzero.ptrw(), 0, mzero.size()); // spawn counter + alive count = 0
		buf_foam_meta = rd->storage_buffer_create(mzero.size(), mzero);
	}
	{
		// Decaying running maxima of the four foam potentials: zeroed once at
		// configure and NEVER per frame -- they persist so the normalization
		// carries scene history.
		PackedByteArray nzero;
		nzero.resize(4 * sizeof(uint32_t));
		memset(nzero.ptrw(), 0, nzero.size());
		buf_foam_norm = rd->storage_buffer_create(nzero.size(), nzero);
	}

	if (!_rebuild_uniform_sets()) {
		_free_buffers();
		return;
	}
	built = true;
}

void MPMFluidSolver::emit(const LocalVector<EmittedParticle> &p_new) {
	if (!is_available() || p_new.is_empty() || capacity == 0) {
		return;
	}
	// Don't buffer_update the particle store while a step is reading it on the GPU.
	_reap_submitted();
	const int n = MIN((int)p_new.size(), capacity);

	// Build the packed particle records (only position + velocity; C and the
	// density slot start at zero / rest).
	LocalVector<float> blob;
	blob.resize((int64_t)n * FLOATS_PER_PARTICLE);
	memset(blob.ptr(), 0, blob.size() * sizeof(float));
	for (int i = 0; i < n; i++) {
		float *o = &blob[i * FLOATS_PER_PARTICLE];
		o[0] = p_new[i].position.x;
		o[1] = p_new[i].position.y;
		o[2] = p_new[i].position.z;
		o[3] = settings.rest_density;
		o[4] = p_new[i].velocity.x;
		o[5] = p_new[i].velocity.y;
		o[6] = p_new[i].velocity.z;
		o[20] = o[25] = o[30] = 1.0f; // deformation gradient F = identity
	}

	const uint32_t stride = FLOATS_PER_PARTICLE * sizeof(float);
	const int first = MIN(n, capacity - write_head); // fits before wrapping
	rd->buffer_update(buf_particles, write_head * stride, first * stride, blob.ptr());
	if (first < n) {
		rd->buffer_update(buf_particles, 0, (n - first) * stride, blob.ptr() + first * FLOATS_PER_PARTICLE);
	}
	write_head = (write_head + n) % capacity;
	pcount = MIN(pcount + n, capacity);
}

void MPMFluidSolver::set_domain_transform(const Transform3D &p_xform) {
	domain_xform = p_xform;
}

// PART-4: create the isosurface mesh output buffers + the march pass's set 1
// on first use. 2 x TRI_BUDGET * 3 * vec4 = ~19 MB that fluids without a
// surface mesh (and every granular bed) used to allocate unconditionally.
void MPMFluidSolver::_ensure_mesh_buffers() {
	if (_mesh_buffers_ready) {
		return;
	}
	_mesh_buffers_ready = true;
	buf_mverts = rd->storage_buffer_create(TRI_BUDGET * 3 * 4 * sizeof(float)); // vec4 / vertex
	buf_mnorms = rd->storage_buffer_create(TRI_BUDGET * 3 * 4 * sizeof(float));
	{
		PackedByteArray mc;
		mc.resize(2 * sizeof(uint32_t));
		encode_uint32(0, mc.ptrw()); // tri_count
		encode_uint32(TRI_BUDGET, mc.ptrw() + 4); // tri_budget
		buf_mcount = rd->storage_buffer_create(mc.size(), mc);
	}
	Vector<RD::Uniform> m;
	const RID mesh_bufs[3] = { buf_mverts, buf_mnorms, buf_mcount };
	for (int b = 0; b < 3; b++) {
		RD::Uniform u;
		u.uniform_type = RD::UNIFORM_TYPE_STORAGE_BUFFER;
		u.binding = b;
		u.append_id(mesh_bufs[b]);
		m.push_back(u);
	}
	uset_mesh = rd->uniform_set_create(m, shader[PASS_MARCH], 1);
	if (uset_mesh.is_null()) {
		ERR_PRINT("MPMFluidSolver: isosurface mesh uniform set failed; surface disabled.");
	}
}

void MPMFluidSolver::step(double p_delta, const LocalVector<SphereCollider> &p_colliders, LocalVector<ImpulseHit> *r_impulses, bool p_want_surface, bool p_async) {
	if (!is_available() || pcount == 0 || p_delta <= 0.0) {
		return;
	}

	// Reap the previous frame's GPU work before starting this one.
	_reap_submitted();

	// MPM_BENCH forces the sync path so get_last_step_msec() reports the real GPU
	// cost of this step instead of ~0 (async overlap).
	if (unlikely(OS::get_singleton()->has_environment("MPM_BENCH"))) {
		p_async = false;
	}

	const int ncol = MIN((int)p_colliders.size(), MAX_COLLIDERS);
	const double frame_dt = p_delta;
	const double dt = frame_dt / (double)settings.substeps;

	PackedByteArray params;
	_pack_params(dt, ncol, params);
	rd->buffer_update(buf_params, 0, PARAMS_BYTES, params.ptr());

	if (ncol > 0) {
		PackedByteArray cdata;
		_pack_colliders(p_colliders, cdata);
		rd->buffer_update(buf_colliders, 0, cdata.size(), cdata.ptr());
	}
	rd->buffer_clear(buf_cimp, 0, MAX_COLLIDERS * 4 * sizeof(int32_t));
	const bool foam_run = settings.foam_enabled && foam_capacity_resolved > 1;
	if (foam_run) {
		// Reset only the alive count; the foam passes re-accumulate it inside
		// this frame's compute list. The spawn counter is deliberately NOT
		// reset: it stays monotonic across frames so the ring genuinely
		// round-robins (a frame's spawns continue where the last frame left
		// off) instead of every frame overwriting the same first slots.
		rd->buffer_clear(buf_foam_meta, sizeof(uint32_t), sizeof(uint32_t));
	}
	if (p_want_surface) {
		_ensure_mesh_buffers();
		if (uset_mesh.is_null()) {
			p_want_surface = false; // lazy alloc failed: drop the surface this frame
		} else {
			rd->buffer_clear(buf_mcount, 0, sizeof(uint32_t)); // reset tri_count, keep tri_budget
			rd->buffer_clear(buf_surf, 0, node_count * sizeof(int32_t)); // reset the density scatter
		}
	}

	const uint32_t ng = groups_for(node_count);
	const uint32_t pg = groups_for(pcount);
	const int cells = (grid_dims.x - 1) * (grid_dims.y - 1) * (grid_dims.z - 1);

	// MPM_PROFILE: time each pass in isolation (inflated by per-rep submit
	// overhead -- read the relative costs, not the absolutes), then skip the
	// real step this frame.
	if (unlikely(OS::get_singleton()->has_environment("MPM_PROFILE"))) {
		struct P {
			const char *name;
			int pass;
			uint32_t groups;
		};
		const P plist[] = {
			{ "clear ", PASS_CLEAR, ng }, { "p2g_mass", PASS_P2G_MASS, pg }, { "p2g_mom ", PASS_P2G_MOM, pg },
			{ "grid  ", PASS_GRID, ng }, { "couple", PASS_COUPLE, ng }, { "g2p   ", PASS_G2P, pg }
		};
		const int REP = 40;
		for (const P &pp : plist) {
			const uint64_t t0 = OS::get_singleton()->get_ticks_usec();
			for (int r = 0; r < REP; r++) {
				RD::ComputeListID pcl = rd->compute_list_begin();
				rd->compute_list_bind_compute_pipeline(pcl, pipeline[pp.pass]);
				rd->compute_list_bind_uniform_set(pcl, uset[pp.pass], 0);
				rd->compute_list_dispatch(pcl, pp.groups, 1, 1);
				rd->compute_list_end();
				rd->submit();
				rd->sync();
			}
			const double us = double(OS::get_singleton()->get_ticks_usec() - t0) / REP;
			print_line(vformat("[mpm-prof] %s  %.3f ms  (x%d subs -> %.3f ms/step)", pp.name, us / 1000.0, settings.substeps, us * settings.substeps / 1000.0));
		}
		_submitted = false;
		return;
	}

	RD::ComputeListID cl = rd->compute_list_begin();
	for (int s = 0; s < settings.substeps; s++) {
		rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_CLEAR]);
		rd->compute_list_bind_uniform_set(cl, uset[PASS_CLEAR], 0);
		rd->compute_list_dispatch(cl, ng, 1, 1);
		rd->compute_list_add_barrier(cl);

		rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_P2G_MASS]);
		rd->compute_list_bind_uniform_set(cl, uset[PASS_P2G_MASS], 0);
		rd->compute_list_dispatch(cl, pg, 1, 1);
		rd->compute_list_add_barrier(cl);

		rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_P2G_MOM]);
		rd->compute_list_bind_uniform_set(cl, uset[PASS_P2G_MOM], 0);
		rd->compute_list_dispatch(cl, pg, 1, 1);
		rd->compute_list_add_barrier(cl);

		rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_GRID]);
		rd->compute_list_bind_uniform_set(cl, uset[PASS_GRID], 0);
		rd->compute_list_dispatch(cl, ng, 1, 1);
		rd->compute_list_add_barrier(cl);

		if (ncol > 0) {
			rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_COUPLE]);
			rd->compute_list_bind_uniform_set(cl, uset[PASS_COUPLE], 0);
			rd->compute_list_dispatch(cl, ng, 1, 1);
			rd->compute_list_add_barrier(cl);
		}

		rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_G2P]);
		rd->compute_list_bind_uniform_set(cl, uset[PASS_G2P], 0);
		rd->compute_list_dispatch(cl, pg, 1, 1);
		rd->compute_list_add_barrier(cl);
	}
	if (foam_run) {
		// Diffuse (foam/spray) layer, once per frame after the substeps: spawn
		// off the final particle/grid state, then advect. Never touches the
		// sim's own buffers. The spawn dispatch is gated near ring saturation
		// (alive is the last reaped count): recycling live foam round-robin
		// reads as popping, so past ~80% the layer only animates what exists.
		const bool can_spawn = _foam_alive < (int)(foam_capacity_resolved * 0.8);
		if (can_spawn) {
			rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_FOAM_SPAWN]);
			rd->compute_list_bind_uniform_set(cl, uset[PASS_FOAM_SPAWN], 0);
			rd->compute_list_dispatch(cl, pg, 1, 1); // one thread per fluid particle
			rd->compute_list_add_barrier(cl);
		}
		rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_FOAM_ADVECT]);
		rd->compute_list_bind_uniform_set(cl, uset[PASS_FOAM_ADVECT], 0);
		rd->compute_list_dispatch(cl, groups_for(foam_capacity_resolved), 1, 1);
		rd->compute_list_add_barrier(cl);
	}
	if (p_want_surface) {
		rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_SURFACE]);
		rd->compute_list_bind_uniform_set(cl, uset[PASS_SURFACE], 0);
		rd->compute_list_dispatch(cl, pg, 1, 1); // one thread per particle (SPH scatter)
		rd->compute_list_add_barrier(cl);

		rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_MARCH]);
		rd->compute_list_bind_uniform_set(cl, uset[PASS_MARCH], 0);
		rd->compute_list_bind_uniform_set(cl, uset_mesh, 1);
		rd->compute_list_dispatch(cl, groups_for(cells), 1, 1);
		rd->compute_list_add_barrier(cl);
	}
	rd->compute_list_bind_compute_pipeline(cl, pipeline[PASS_RENDER]);
	rd->compute_list_bind_uniform_set(cl, uset[PASS_RENDER], 0);
	rd->compute_list_dispatch(cl, pg, 1, 1);
	rd->compute_list_end();

	rd->submit();
	_submitted = true;
	_submitted_ncol = ncol;

	if (!p_async) {
		// Sync now: the caller needs this step's reaction impulse in phase with
		// the physics tick (fluid coupling). Costs the stall.
		_reap_submitted();
	}

	// Hand back the reaped impulses -- this step's (sync) or the previous
	// step's (async, still in flight) per-collider reaction, each tagged with
	// the collider id that earned it (PART-5).
	if (r_impulses != nullptr) {
		*r_impulses = _imp_cache;
	}
}

// Zero a range of a PackedFloat32Array (helper for the prefix-readback pad).
static void zeroffset(PackedFloat32Array &r_arr, int p_from_float, int p_to_float) {
	const int n = MIN(p_to_float, (int)r_arr.size());
	if (p_from_float < n) {
		memset(r_arr.ptrw() + p_from_float, 0, (n - p_from_float) * sizeof(float));
	}
}

// Wait on the previously-submitted compute work (done by now -- a full frame has
// passed) and cache the render buffer + collider impulses.
void MPMFluidSolver::_reap_submitted() const {
	if (!_submitted) {
		return;
	}
	const uint64_t t0 = OS::get_singleton()->get_ticks_usec();
	rd->sync();
	last_step_usec = OS::get_singleton()->get_ticks_usec() - t0; // GPU wait for the last step's compute
	_submitted = false;

	// PART-7: only the first pcount rows are live (PASS_RENDER dispatches
	// groups_for(pcount)) -- read back the LIVE PREFIX, not the full-capacity
	// buffer (a 2.9 MB PCIe transfer at 60k capacity vs. the live row count),
	// then zero-pad the cache on the CPU so get_multimesh_buffer() still
	// hands RS a full-capacity buffer. The old readback shipped
	// (capacity - pcount) dead rows across the bus every frame.
	const int live_rows = MIN(pcount, capacity);
	Vector<uint8_t> mm;
	if (live_rows > 0) {
		mm = rd->buffer_get_data(buf_mm, 0, live_rows * 12 * sizeof(float));
	}
	_mm_cache.resize(capacity * 12);
	if (live_rows > 0) {
		memcpy(_mm_cache.ptrw(), mm.ptr(), MIN((int)mm.size(), (int)(live_rows * 12 * sizeof(float))));
	}
	if (live_rows < capacity) {
		zeroffset(_mm_cache, live_rows * 12, capacity * 12);
	}

	_imp_cache.clear();
	if (_submitted_ncol > 0) {
		Vector<uint8_t> raw = rd->buffer_get_data(buf_cimp, 0, _submitted_ncol * 4 * sizeof(int32_t));
		const int32_t *ci = (const int32_t *)raw.ptr();
		for (int i = 0; i < _submitted_ncol; i++) {
			ImpulseHit hit;
			hit.collider_id = i < (int)_submitted_ids.size() ? _submitted_ids[i] : 0;
			hit.impulse = Vector3(ci[i * 4 + 0], ci[i * 4 + 1], ci[i * 4 + 2]) / IMP_FIXED;
			_imp_cache.push_back(hit);
		}
	}

	// Foam readback: alive count from the meta buffer, then the alive slots'
	// records out of the foam ring (life <= 0 slots are parked), packed as
	// x,y,z,kind for kind-aware rendering.
	if (settings.foam_enabled && buf_foam.is_valid()) {
		Vector<uint8_t> meta = rd->buffer_get_data(buf_foam_meta, 0, 2 * sizeof(uint32_t));
		const uint32_t alive = meta.size() >= 8 ? MIN(decode_uint32(meta.ptr() + 4), (uint32_t)foam_capacity_resolved) : 0;
		PackedFloat32Array data;
		if (alive == 0) {
			data.clear(); // skip the full-buffer readback entirely
		} else {
			Vector<uint8_t> raw = rd->buffer_get_data(buf_foam);
			const int slots = MIN((int)(raw.size() / (FLOATS_PER_FOAM * sizeof(float))), foam_capacity_resolved);
			const float *f = (const float *)raw.ptr();
			data.resize(alive * 4);
			int written = 0;
			for (int i = 0; i < slots && written < (int)alive; i++) {
				const float *o = f + i * FLOATS_PER_FOAM;
				if (o[3] > 0.0f) { // lifetime remaining
					float *d = data.ptrw() + written * 4;
					d[0] = o[0];
					d[1] = o[1];
					d[2] = o[2];
					d[3] = o[7]; // kind
					written++;
				}
			}
			data.resize(written * 4);
		}
		_foam_cache = data;
		_foam_alive = (int)alive;
	} else {
		_foam_cache = PackedFloat32Array();
		_foam_alive = 0;
	}
}

PackedFloat32Array MPMFluidSolver::get_multimesh_buffer() const {
	// Cached from the last reaped step -- no readback here. Full-capacity buffer
	// (MultiMesh::set_buffer wants every slot); the caller only shows `pcount`.
	return _mm_cache;
}

PackedVector3Array MPMFluidSolver::get_foam_positions() const {
	// Unpacks the kind-aware cache (x,y,z,kind) into the legacy positions-only
	// contract. Cached from the last reaped step -- no readback here, same
	// contract as get_multimesh_buffer().
	PackedVector3Array out;
	const int n = _foam_cache.size() / 4;
	out.resize(n);
	for (int i = 0; i < n; i++) {
		out.write[i] = Vector3(_foam_cache[i * 4 + 0], _foam_cache[i * 4 + 1], _foam_cache[i * 4 + 2]);
	}
	return out;
}

float MPMFluidSolver::get_submersion(const AABB &p_world_aabb) const {
	if (!is_available() || p_world_aabb.get_volume() <= 0.0f || _mm_cache.is_empty()) {
		return 0.0f;
	}
	// The cached MultiMesh rows carry the world-space particle origins (floats
	// 3/7/11), so this is a pure CPU scan over the last reaped frame -- no
	// readback, no GPU work.
	int inside = 0;
	const int n = MIN(pcount, (int)(_mm_cache.size() / 12));
	const float *b = _mm_cache.ptr();
	for (int i = 0; i < n; i++) {
		const float *t = b + i * 12;
		if (p_world_aabb.has_point(Vector3(t[3], t[7], t[11]))) {
			inside++;
		}
	}
	const float spacing = dx * 0.5f; // each particle stands in for a spacing^3 cell
	const float filled = (float)inside * spacing * spacing * spacing;
	return CLAMP(filled / p_world_aabb.get_volume(), 0.0f, 1.0f);
}

PackedVector3Array MPMFluidSolver::get_positions() const {
	PackedVector3Array out;
	if (!is_available() || pcount == 0) {
		return out;
	}
	_reap_submitted(); // no reading a buffer with a step still on the GPU
	Vector<uint8_t> raw = rd->buffer_get_data(buf_particles);
	const float *f = (const float *)raw.ptr();
	out.resize(pcount);
	for (int i = 0; i < pcount; i++) {
		const float *o = f + i * FLOATS_PER_PARTICLE;
		out.write[i] = Vector3(o[0], o[1], o[2]);
	}
	return out;
}

int MPMFluidSolver::get_surface_mesh(PackedVector3Array &r_vertices, PackedVector3Array &r_normals) const {
	r_vertices.clear();
	r_normals.clear();
	if (!is_available() || buf_mcount.is_null()) {
		return 0;
	}
	_reap_submitted();
	Vector<uint8_t> cnt = rd->buffer_get_data(buf_mcount, 0, sizeof(uint32_t));
	if (cnt.size() < (int)sizeof(uint32_t)) {
		return 0;
	}
	const uint32_t tris = MIN(decode_uint32(cnt.ptr()), TRI_BUDGET);
	if (tris == 0) {
		return 0;
	}
	const uint32_t verts = tris * 3;
	Vector<uint8_t> vraw = rd->buffer_get_data(buf_mverts, 0, verts * 4 * sizeof(float));
	Vector<uint8_t> nraw = rd->buffer_get_data(buf_mnorms, 0, verts * 4 * sizeof(float));
	const float *vf = (const float *)vraw.ptr();
	const float *nf = (const float *)nraw.ptr();
	r_vertices.resize(verts);
	r_normals.resize(verts);
	for (uint32_t i = 0; i < verts; i++) {
		r_vertices.write[i] = Vector3(vf[i * 4 + 0], vf[i * 4 + 1], vf[i * 4 + 2]);
		r_normals.write[i] = Vector3(nf[i * 4 + 0], nf[i * 4 + 1], nf[i * 4 + 2]);
	}
	return (int)tris;
}

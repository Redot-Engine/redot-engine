/**************************************************************************/
/*  water_solver.h                                                        */
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

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/rid.h"
#include "core/templates/safe_refcount.h"
#include "core/templates/vector.h"
#include "core/variant/variant.h"

class RenderingDevice;

/* ===================================================================== */
/*  WaterSolverGPU -- everything that touches the RenderingDevice. Every   */
/*  method here runs on the render thread (posted via                      */
/*  RenderingServer::call_on_render_thread) except the destructor, which   */
/*  runs wherever the last Ref is dropped. Mirrors MPMFluidSolverGPU's     */
/*  exact shape (see mpm_fluid_solver.h) -- shared device when windowed    */
/*  (async, dispatched via the render thread), a private local device when */
/*  headless (synchronous, run on the calling thread directly).            */
/* ===================================================================== */
class WaterSolverGPU : public RefCounted {
	GDSOFTCLASS(WaterSolverGPU, RefCounted);

public:
	RenderingDevice *rd = nullptr;
	bool local = false; // headless fallback: a private device, driven synchronously
	SafeFlag built;

	static constexpr int MAX_SPHERES = 48;
	static constexpr int MAX_IMPULSES = 16;

	RID shader_ripple, pipeline_ripple;
	RID shader_spectrum_init, pipeline_spectrum_init;
	RID shader_spectrum_evolve, pipeline_spectrum_evolve;
	RID shader_fft, pipeline_fft;
	// Blit-to-texture: the only new shader shape in this module -- writes a
	// storage buffer's contents into a real RD image2D (R32_SFLOAT), which
	// Texture2DRD then wraps for direct, zero-copy sampling by the scene
	// renderer. Every earlier solver in this module only ever produced
	// buffers for more compute or a packed MultiMesh; this is genuinely new
	// territory, flagged as such in the project plan.
	RID shader_blit_ripple, pipeline_blit_ripple;
	RID shader_blit_ocean, pipeline_blit_ocean;
	// Persistent whitecap foam (water_foam.glsl), run after the ocean blit.
	RID shader_foam, pipeline_foam;
	// Shore foam (water_shore_foam.glsl), on the ripple grid, after the foam pass.
	RID shader_shore_foam, pipeline_shore_foam;
	// Swash run-up and wet sand (water_swash.glsl), on the ripple grid.
	RID shader_swash, pipeline_swash;

	RID buf_params;
	RID buf_state_a, buf_state_b; // ping-pong ripple state (h, h_prev, foam, _)
	RID buf_height; // composed ripple height, full grid resolution
	RID buf_spheres, buf_impulses;
	RID uset_atob, uset_btoa;

	RID buf_ocean_init_params, buf_ocean_evolve_params, buf_fft_params;
	RID buf_h0, buf_ocean_spec, buf_fft_a, buf_fft_b;
	RID uset_spectrum_init, uset_spectrum_evolve, uset_fft_spec_to_a, uset_fft_atob, uset_fft_btoa;
	// Three more FFT chains for the choppy displacement, the surface slopes
	// and the displacement derivatives (see water_spectrum_evolve.glsl for
	// how they're packed). Same passes, same params; they land in fft_d,
	// fft_f and fft_h.
	RID buf_ocean_spec_dz, buf_fft_c, buf_fft_d;
	RID uset_fft_dz_to_c, uset_fft_ctod, uset_fft_dtoc;
	RID buf_ocean_spec_c, buf_fft_e, buf_fft_f;
	RID uset_fft_c_to_e, uset_fft_etof, uset_fft_ftoe;
	RID buf_ocean_spec_d, buf_fft_g, buf_fft_h;
	RID uset_fft_d_to_g, uset_fft_gtoh, uset_fft_htog;

	RID tex_ripple_height, tex_ocean_height; // real RD textures, wrapped by Texture2DRD on the caller side
	RID tex_ocean_disp; // RGBA32F (Dx, Dz, dDx/dz, 0), same resolution as tex_ocean_height
	RID tex_ocean_deriv; // RGBA32F (dh/dx, dh/dz, dDx/dx, dDz/dz)
	RID tex_ocean_foam; // R32F foam amount 0..1, rest coordinates, persists across steps
	RID buf_foam_params, uset_foam;
	float foam_choppiness = 1.0f;
	float foam_threshold = 0.7f;
	float foam_persistence = 2.5f;
	bool foam_enabled = true;
	float shore_foam_band = 0.5f;
	float shore_undertow = 0.3f;
	RID tex_shore_foam; // R32F shore foam 0..1 on the ripple grid, persists across steps
	RID tex_shore_foam_tmp; // scratch for the foam's advection (see water_shore_foam.glsl)
	RID buf_shore_foam_params, buf_shore_foam_copy_params, uset_shore_foam, uset_shore_foam_copy;
	float swash_run_up = 0.3f;
	float swash_drain_speed = 0.12f;
	float wet_sand_dry_time = 12.0f;
	RID tex_swash; // RGBA32F (run-up m, wetness 0..1, mean-square rise, 0) on the ripple grid
	RID tex_swash_tmp; // scratch: the run-up climbs from neighbors (see water_swash.glsl)
	RID buf_swash_params, buf_swash_copy_params, uset_swash, uset_swash_copy;
	// Still-water depth per ripple cell (R16F meters, <= 0 dry -- see
	// water_inc.glsl), bound to the ripple pass and the caustics pass, and
	// the ocean chop's shallow-water fade derived from it (R8, sampled by the
	// surface material). Both uploaded once per build.
	RID tex_cell_depth;
	RID tex_ocean_fade;
	// R16F still-water depth for materials' waterline: like tex_cell_depth,
	// but a wall cell reads as deep water so nothing shore-like happens at a
	// pool's wall.
	RID tex_shore_depth;
	bool has_cell_depth = false;
	float shallow_fade_depth = 1.0f;
	RID buf_blit_ripple_params, buf_blit_ocean_params; // just {n}, uploaded once at build (grid resolution is fixed per configure())
	RID uset_blit_ripple, uset_blit_ocean;

	// Light-space caustic map -- a real RD graphics (vertex+fragment) pipeline,
	// not a SubViewport (see water_caustics_map.glsl's own header comment for
	// why: a second live SubViewport was confirmed this session to corrupt the
	// MAIN viewport's transparent-object rendering in this engine fork).
	// Vertex/index buffers are built once per configure() (fixed topology --
	// a flat grid over ocean_domain_size, denser than the ocean layer to
	// reduce faceting in refracted-ray projection),
	// not regenerated per frame; only the uniform buffer's light-space basis/
	// reference-depth/sun direction change per step.
	RID shader_caustics, pipeline_caustics;
	RID sampler_linear;
	RID sampler_linear_repeat; // the periodic ocean textures, where they're tiled (caustics)
	RID buf_caustics_params;
	RID caustics_vertex_buffer, caustics_index_buffer;
	int64_t caustics_vertex_format = -1;
	RID caustics_vertex_array, caustics_index_array;
	RID tex_caustics; // R8_UNORM, wrapped by Texture2DRD on the caller side, same as tex_ripple_height/tex_ocean_height
	RID caustics_framebuffer;
	RID uset_caustics;
	int caustics_index_count = 0;
	static constexpr int CAUSTICS_MAP_SIZE = 2048;
	static constexpr int CAUSTICS_GRID_MIN_RESOLUTION = 256;
	// The caustics grid spans this much more than one ocean tile, so light
	// refracted in from beyond the tile's edge is in the map too and a
	// receiver folding its position into the tile (get_caustics_tile_size())
	// sees no seam.
	static constexpr float CAUSTICS_TILE_MARGIN = 1.25f;

	int ocean_log2n = 0;
	double ocean_time_accum = 0.0;
	bool a_is_current = true;

	// CPU readback caches -- see rt_read_height()/rt_on_*_height(). Guarded
	// by cache_mtx, same convention as mpm_fluid_solver.h's mm_cache et al.
	Mutex cache_mtx;
	Vector<float> ripple_height_cache;
	Vector<float> ocean_height_cache;
	Vector<float> ocean_imag_cache; // x displacement (see water_spectrum_evolve.glsl)
	Vector<float> ocean_dz_cache; // z displacement
	Vector<float> ocean_slope_x_cache; // dh/dx (for tests)
	SafeFlag height_ready;

	// All posted to the render thread via RenderingServer::call_on_render_thread
	// (or run synchronously now, on the calling thread, for the local/headless
	// path -- see WaterSolver::_dispatch()). Every call binds a Ref to this
	// object (p_self) so it outlives work in flight.
	void rt_compile(Ref<WaterSolverGPU> p_self);
	// Wind/amplitude only matter here -- they feed the ONE-TIME h0 spectrum
	// generation (see rt_build()'s body), not the per-step evolution.
	// p_cell_depth: grid_resolution^2 still-water depths in meters (<= 0 =
	// dry), row-major by Z then X like the ripple grid; empty = p_depth
	// everywhere.
	void rt_build(Ref<WaterSolverGPU> p_self, int p_grid_resolution, Vector2 p_domain_size, int p_ocean_grid_resolution, Vector2 p_ocean_domain_size, float p_wind_speed, Vector2 p_wind_direction, float p_wave_amplitude, float p_gravity, bool p_caustics_enabled, PackedFloat32Array p_cell_depth, float p_depth, float p_shallow_fade_depth, float p_fetch);
	// The sea state changed: regenerates the h0 spectrum IN PLACE -- the
	// buffers, pipelines and every texture stay bound (a full rt_build()
	// left the sea undrawn for a frame); the waves keep their phases.
	// No-op before the first rt_build().
	void rt_reinit_spectrum(Ref<WaterSolverGPU> p_self, float p_wind_speed, Vector2 p_wind_direction, float p_wave_amplitude, float p_gravity, float p_fetch, float p_depth);
	// p_spheres/p_impulses: flat, 4 floats/entry (world_x, world_z, radius,
	// strength), packed on the calling thread into an immutable value-copy
	// snapshot before dispatch -- same thread-safety pattern
	// MPMFluidSolver::step()'s p_colliders argument already uses, not a
	// pointer/reference into WaterSolver's own live sphere_slots (which live
	// on the calling thread, not the render thread). Also kicks the height
	// readback unconditionally every call (matching MPMFluidSolverGPU::
	// rt_step()'s own precedent -- no separate throttle needed, the async
	// callback landing "a few frames later" already provides natural,
	// implicit throttling).
	void rt_step(Ref<WaterSolverGPU> p_self, double p_delta, float p_depth, float p_damping, float p_gravity, float p_water_level, float p_ripple_amplitude, PackedFloat32Array p_spheres, PackedFloat32Array p_impulses);
	// Light-space caustic map render -- a real RD draw pass (see this class's
	// own field comments for why not a SubViewport). p_sun_direction need not
	// be normalized (normalized on upload); p_light_right/p_light_up are the
	// light-space projection basis (both unit, perpendicular to the sun
	// direction and each other -- computed on the calling thread, not here,
	// since building an arbitrary-direction orthonormal basis is a plain, pure
	// CPU computation with no GPU dependency). Called once per step, separate
	// from rt_step() itself so caustics-specific state doesn't have to route
	// through that function's already-large signature; no-ops if the caustics
	// pipeline was never built (caustics_enabled false at configure() time).
	void rt_render_caustics(Ref<WaterSolverGPU> p_self, Vector3 p_sun_direction, Vector3 p_light_right, Vector3 p_light_up, Vector3 p_origin, float p_half_extent, float p_reference_depth, float p_ior, bool p_open_beyond);
	void rt_free(Ref<WaterSolverGPU> p_self);
	void rt_set_foam(Ref<WaterSolverGPU> p_self, bool p_enabled, float p_choppiness, float p_threshold, float p_persistence, float p_shore_band, float p_shore_undertow);
	void rt_set_swash(Ref<WaterSolverGPU> p_self, float p_run_up, float p_drain_speed, float p_dry_time);

	// Async readback callbacks: RenderingDevice invokes them with the data as
	// the runtime arg; Callable::bind APPENDS the bound args, so the data
	// comes first. Two separate reads/callbacks (ripple, ocean), same
	// pattern as MPMFluidSolverGPU's rt_on_mm/rt_on_impulses/rt_on_surf_* --
	// no ordering dependency between them, whichever lands first updates the
	// cache immediately.
	void rt_on_ripple_height(const PackedByteArray &p_data, Ref<WaterSolverGPU> p_self);
	void rt_on_ocean_height(const PackedByteArray &p_data, Ref<WaterSolverGPU> p_self);
	void rt_on_ocean_dz(const PackedByteArray &p_data, Ref<WaterSolverGPU> p_self);

	~WaterSolverGPU();

private:
	bool shaders_ok = false;
	int grid_resolution = 0;
	Vector2 domain_size;
	int ocean_grid_resolution = 0;
	Vector2 ocean_domain_size;

	// Static settings the one-time h0 spectrum init needs, stashed here by
	// rt_build() between being called and _rt_build_buffers() running.
	float _init_wind_speed = 8.0f;
	Vector2 _init_wind_direction = Vector2(1, 0);
	float _init_wave_amplitude = 1.0f;
	float _init_gravity = 9.81f;
	float _init_fetch = 100000.0f;
	bool _init_caustics_enabled = false;
	PackedFloat32Array _init_cell_depth;
	float _init_depth = 3.0f;

	void _rt_free_buffers();
	void _rt_dispatch_spectrum_init();
	void _rt_build_buffers();
	void _rt_build_caustics_grid();
};

/* ===================================================================== */
/*  WaterSolver -- CPU-side front. Preps payloads, posts render work.      */
/*  Same public shape as the original single-class WaterSolver,           */
/*  so WaterRippleProbe/PhysXWaterSurface3D don't need to change how they */
/*  call it -- only the internals moved to the shared-device/async model. */
/* ===================================================================== */
class WaterSolver {
public:
	struct Settings {
		int grid_resolution = 128; // N, ripple grid cells per side (power of two not required for the ripple layer)
		Vector2 domain_size = Vector2(20, 20); // world-space extents (full width/depth, not half)
		float depth = 3.0f; // used by the FFT layer's dispersion relation
		// ALPHA in water_ripple.glsl. Originally tuned to 0.5 purely so the
		// probe test's impulse would settle within ~90 steps -- that
		// was optimizing for a fast, bounded test, not for how real water
		// actually looks. At 0.5, ripples die out almost as fast as they're
		// created and never travel far, reading as thick/viscous ("jello or
		// syrup", a real user observation) rather than water. Lowered so
		// ripples persist and travel a meaningfully longer distance before
		// dissipating -- see the probe test for the real before/after decay
		// numbers this was checked against, including over a longer horizon
		// than the probe's original ~90-step window to confirm it stays
		// bounded/stable, not just slower to settle.
		float damping = 0.08f;
		float gravity = 9.81f;
		float water_level = 0.0f; // mean surface world Y
		float ripple_amplitude = 1.0f; // multiplier on body-driven ripple displacement

		// FFT ocean spectrum layer. Independent grid/resolution from
		// the ripple layer above -- see water_solver.h's own note on why these
		// stay architecturally separate. MUST be a power of two -- the
		// Stockham FFT passes require it.
		int ocean_grid_resolution = 64;
		Vector2 ocean_domain_size = Vector2(40, 40);
		float wind_speed = 8.0f;
		Vector2 wind_direction = Vector2(1, 0); // need not be normalized, normalized on upload
		float wave_amplitude = 1.0f; // linear height multiplier on the physical spectrum (1 = JONSWAP heights in meters)
		// Distance in meters the wind has blown over open water: sets how
		// tall and how long the wind waves get (JONSWAP). 100 km is a fully
		// developed open sea; a pool's size gives tiny, short ripples.
		float fetch = 100000.0f;

		// Light-space caustic map (see water_caustics_map.glsl). The grid/
		// buffers are only built if this is true at configure() time -- keep
		// it false (the default) for a water surface that never wants
		// caustics to skip that cost entirely.
		bool caustics_enabled = false;

		// World XZ the ripple and ocean grids are centered on. Body positions
		// passed to submit_sphere()/submit_impulse() are world space and made
		// relative to this before upload.
		Vector2 grid_center;
		// Optional still-water depth per ripple cell, grid_resolution^2 meters
		// (<= 0 = dry land), row-major by Z then X. It sets each cell's wave
		// speed and the shoreline. Empty = `depth` everywhere. WALL_DEPTH
		// marks a dry cell that's a wall (outside a surface_mesh outline)
		// rather than a beach: the water beside it stays deep.
		PackedFloat32Array cell_depth;
		// Water shallower than this fades the FFT ocean chop out, down to
		// none at the waterline.
		float shallow_fade_depth = 1.0f;
	};

	static constexpr int MAX_SPHERES = WaterSolverGPU::MAX_SPHERES;
	static constexpr int MAX_IMPULSES = WaterSolverGPU::MAX_IMPULSES;

	WaterSolver();
	~WaterSolver();

	// A RenderingDevice is present (independent of whether configure() has
	// finished building on the render thread yet).
	bool has_device() const { return gpu.is_valid() && gpu->rd != nullptr; }
	// Ready to step: device up and the render thread has finished building
	// buffers/textures for the current configure().
	bool is_available() const { return has_device() && gpu->built.is_set(); }

	void configure(const Settings &p_settings);
	// Moves the ripple/ocean grids to p_grid_center with NO rebuild: the
	// mesh, FFT ocean and every texture stay bound; the ripples are cleared
	// (the old square's heights are not the new one's).
	void recenter(const Vector2 &p_grid_center);
	// The sea state changed: regenerates the ocean spectrum IN PLACE on a
	// built solver (no rebuild -- the waves keep their phases); before a
	// build the values are simply picked up by the next configure().
	void reinit_spectrum(float p_wind_speed, Vector2 p_wind_direction, float p_wave_amplitude, float p_fetch, float p_depth);
	void step(double p_delta);

	// Persistent body proxy, world space, keyed by an arbitrary caller-chosen
	// id (ObjectID in practice) -- call every physics tick while the body is
	// in the water; entries not resubmitted this step stay at their last
	// value (no implicit aging -- callers should call clear_sphere() in
	// _exit_tree() for the clean-despawn case).
	void submit_sphere(uint64_t p_owner, const Vector3 &p_world_pos, float p_radius, float p_strength = 1.0f);
	void clear_sphere(uint64_t p_owner);

	// One-shot, event-driven (a splash) -- queued, consumed by the next
	// step(), then cleared. Not persistent like spheres.
	void submit_impulse(const Vector3 &p_world_pos, float p_radius, float p_strength);

	// Zero-copy RD texture RIDs (R32_SFLOAT height, meters) for a caller-side
	// Texture2DRD to wrap directly -- valid once is_available(). This is the
	// real payoff of the shared-device design: the scene renderer samples
	// these with no CPU round trip at all.
	RID get_ripple_height_texture_rd_rid() const;
	RID get_ocean_height_texture_rd_rid() const;
	// R8 shallow-water fade for the ocean layer over the ripple domain (1 =
	// full chop, 0 = none); valid once is_available().
	RID get_ocean_fade_texture_rd_rid() const;
	// R16F still-water depth (m) over the ripple domain for materials (dry
	// shore negative, walls deep); valid once is_available().
	RID get_shore_depth_texture_rd_rid() const;
	// Ocean layer derivatives for materials, same grid as its height; valid
	// once is_available(). RGBA32F (Dx, Dz, dDx/dz, 0) and
	// (dh/dx, dh/dz, dDx/dx, dDz/dz).
	RID get_ocean_displacement_texture_rd_rid() const;
	RID get_ocean_derivative_texture_rd_rid() const;
	// R32F persistent whitecap foam (0..1) on the ocean grid; valid once
	// is_available().
	RID get_ocean_foam_texture_rd_rid() const;
	// Foam on/off, injection (fold Jacobian below p_threshold, with the
	// choppiness the materials displace by) and fade time; applies live, no
	// rebuild. Off clears the foam texture.
	// p_shore_band: still-water depth (m) within which waves arriving over the
	// shallows leave shore foam (0 = none). p_shore_undertow: steady seaward
	// drift (m/s) the backwash drags that foam back out with.
	void set_foam_settings(bool p_enabled, float p_choppiness, float p_threshold, float p_persistence, float p_shore_band, float p_shore_undertow);
	void set_step_constants(float p_depth, float p_damping, float p_water_level, float p_ripple_amplitude); // WATER-3 live update
	// R32F shore foam (0..1) on the ripple grid; valid once is_available().
	RID get_shore_foam_texture_rd_rid() const;
	// Swash: the highest run-up (m above still water) of the biggest waves,
	// the vertical speed (m/s) the backwash drains back down at, and how
	// long (s) uncovered sand takes to dry. Applies live.
	void set_swash_settings(float p_run_up, float p_drain_speed, float p_dry_time);
	// RGBA32F on the ripple grid: r run-up level (m above still water), g sand
	// wetness 0..1 (see water_swash.glsl); valid once is_available().
	RID get_swash_texture_rd_rid() const;
	// Dry cell that's a wall, not a shore (see Settings::cell_depth).
	static constexpr float WALL_DEPTH = -10000.0f;
	// The ocean layer's fade factor for a still-water depth (1 for a wall
	// cell, so nothing shallow-water blends in along a wall).
	static float shallow_fade(float p_depth, float p_fade_depth);
	Vector2 get_domain_size() const { return settings.domain_size; }
	Vector2 get_grid_center() const { return settings.grid_center; }
	Vector2 get_ocean_domain_size() const { return settings.ocean_domain_size; }

	// Light-space caustic map (see water_caustics_map.glsl's own header for
	// the real design: a shadow-map-like technique so ANY receiving surface
	// -- a flat floor, a wall, an uneven terrain, a submerged prop -- can
	// sample the same map, not just a fixed flat plane). p_sun_direction
	// need not be normalized. Call once per step, alongside step() itself;
	// no-ops if caustics_enabled was false at configure() time.
	// p_open_beyond: the water carries on past the simulated grid (it's
	// rendered out there), so light past the grid still enters water.
	void render_caustics(const Vector3 &p_origin, const Vector3 &p_sun_direction, float p_reference_depth = 2.5f, float p_ior = 1.333f, bool p_open_beyond = false);
	// Zero-copy RD texture RID (R8_UNORM intensity), same convention as
	// get_ripple_height_texture_rd_rid(). The light-space basis/extent used
	// to produce it, needed by any receiver's own lookup shader code.
	RID get_caustics_texture_rd_rid() const;
	Vector3 get_caustics_light_right() const { return caustics_light_right; }
	Vector3 get_caustics_light_up() const { return caustics_light_up; }
	Vector3 get_caustics_origin() const { return caustics_origin; }
	float get_caustics_half_extent() const { return caustics_half_extent; }
	// The ocean waves repeat every ocean_domain_size, so the caustics do too:
	// a receiver can fold its world XZ into the tile around the origin
	// (p - tile * round((p - origin) / tile)) before projecting, and get
	// caustics all along a beach instead of only over the simulated square.
	Vector2 get_caustics_tile_size() const { return settings.ocean_domain_size; }

	// CPU-side cached readback, for buoyancy sampling (a consumer with very
	// different freshness/precision needs than rendering -- see the project
	// plan's note on why these stay separate paths from the zero-copy
	// texture above, not a redundant "same data twice"). Returns whatever
	// the last async callback landed (a few frames stale, same tradeoff
	// MPM's own mm_cache/pos_cache accept) -- cheap (a mutex lock + copy),
	// no dispatch, no sync, safe to call every physics tick from any number
	// of callers.
	void get_height_grid(Vector<float> &r_height, int &r_n, Vector2 &r_domain_size) const;
	// r_imag carries the ocean's choppy x displacement (see
	// water_spectrum_evolve.glsl); get_ocean_dz_grid() the z displacement.
	void get_ocean_height_grid(Vector<float> &r_height, Vector<float> &r_imag, int &r_n, Vector2 &r_domain_size) const;
	void get_ocean_dz_grid(Vector<float> &r_dz) const;
	// dh/dx of the ocean layer from the FFT (readback, for tests).
	void get_ocean_slope_x_grid(Vector<float> &r_slope_x) const;

	// Forces the render thread to catch up right now (matches
	// MPMFluidSolver::get_positions()'s own "blocking, test/query helper"
	// precedent) -- for tests wanting a deterministic "definitely landed by
	// now" read after step(); real per-tick callers should NOT call this
	// (defeats the whole point of the async design) and should just call
	// get_height_grid()/get_ocean_height_grid() directly.
	void sync_now() const;

private:
	Ref<WaterSolverGPU> gpu;
	Settings settings;
	// CPU-side light-space basis, recomputed each render_caustics() call and
	// cached here for get_caustics_light_right()/get_caustics_light_up()/
	// get_caustics_half_extent() -- a receiver's own lookup shader needs
	// these to match whatever the map was actually rendered with.
	Vector3 caustics_light_right = Vector3(1, 0, 0);
	Vector3 caustics_light_up = Vector3(0, 0, 1);
	Vector3 caustics_origin;
	float caustics_half_extent = 1.0f;

	struct SphereSlot {
		uint64_t owner = 0;
		Vector3 world_pos;
		float radius = 0.0f;
		float strength = 1.0f;
		bool enabled = false;
	};
	SphereSlot sphere_slots[MAX_SPHERES];

	struct ImpulseSlot {
		Vector3 world_pos;
		float radius = 0.0f;
		float strength = 0.0f;
	};
	Vector<ImpulseSlot> pending_impulses;

	PackedFloat32Array _pack_spheres() const;
	PackedFloat32Array _pack_impulses() const;

	// Run render work now (local device) or hand it to the render thread
	// (shared device).
	void _dispatch(const Callable &p_call) const;
};

/**************************************************************************/
/*  gas_solver.h                                                          */
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
#include "core/math/vector3.h"
#include "core/math/vector3i.h"
#include "core/templates/rid.h"
#include "core/templates/vector.h"

class RenderingDevice;

// A sparse-grid Eulerian gas/smoke solver (velocity + density field, block-
// hash sparse grid), on plain compute shaders. Shares its block-hash lookup
// technique with the MPM block-sparse fluid grid (see gas_block_inc.glsl for
// the full design writeup) but the field is persistent -- advected and
// diffused in place across steps -- rather than rebuilt from particles every
// substep. v1 scope (see gas_block_inc.glsl header): the box's lateral (X/Z)
// footprint is fixed at configure() -- only its height grows automatically
// (see _grow_if_needed), up to 3x the configured height -- no pressure
// projection, up to MAX_COLLIDERS sphere colliders.
//
// Owns a private local RenderingDevice and runs fully synchronously (submit +
// sync every dispatch) -- simpler than the MPM fluid solver's shared-device /
// render-thread / async-readback machinery, at the cost of not overlapping
// with the main render thread's own GPU work. A reasonable place to start;
// revisit if this solver needs the same treatment MPM's Stage A/B did.
class GasSolver {
public:
	struct Settings {
		Vector3i box_blocks = Vector3i(6, 10, 6); // grid size in 4^3 blocks
		float cell_size = 0.08f;
		float buoyancy = 9.0f;
		float vorticity_strength = 8.0f;
		float dissipation = 0.996f;
		// Extra curl-noise perturbation velocity (m/s) added only to the
		// backtrace sample, not the real solved field -- fakes fine "billowing
		// cauliflower" turbulent detail a coarse real-time grid can't resolve
		// on its own. 0 = off (matches pre-turbulence behavior exactly).
		// scale is the base octave's spatial frequency (1/meters); higher =
		// finer wrinkles. See gas_block_inc.glsl's turbulence_velocity().
		float turbulence_strength = 0.0f;
		float turbulence_scale = 2.5f;
	};

	enum EmitterShape { EMITTER_SPHERE,
		EMITTER_BOX };
	// A single injection point, independently shaped/sized/aimed -- unlike
	// colliders (voxelized, shared radius fallback), every emitter carries its
	// own full parameter set. World space; set fresh each step (like
	// colliders), not frozen at configure().
	struct Emitter {
		bool enabled = false;
		EmitterShape shape = EMITTER_SPHERE;
		Vector3 world_position;
		Vector3 size = Vector3(0.22f, 0.22f, 0.22f); // sphere: .x is the radius; box: half-extents
		Vector3 velocity = Vector3(0, 1.4f, 0); // world space
		float density = 1.0f;
		// Outward-radial speed from world_position, added on top of velocity --
		// an explosion/burst emitter (a "nuke plume" base) wants every cell
		// pushed away from center, not all pushed the same direction.
		float divergence = 0.0f;
		// Tangential speed around world +Y through world_position -- directly
		// authors rotation (vorticity confinement alone only amplifies
		// whatever incidental curl the jitter happens to seed).
		float swirl = 0.0f;
	};

	static constexpr int MAX_COLLIDERS = 4;
	// 8, not 4 -- see gas_block_inc.glsl's MAX_GAS_EMITTERS for why (three
	// simultaneous emitter groups for a full mushroom-cloud anatomy).
	static constexpr int MAX_EMITTERS = 8;

	GasSolver();
	~GasSolver();

	bool is_available() const { return rd != nullptr && shaders_ok; }
	bool is_configured() const { return !buf_grid_a.is_null(); }
	// World position of cell (0,0,0)'s min corner, frozen at configure() --
	// see configure()'s own note on why this never re-tracks the node's
	// transform. Cheap (no GPU readback), safe to call every frame (e.g. from
	// an editor gizmo wanting to show where the domain ACTUALLY is instead of
	// naively following the node).
	Vector3 get_grid_anchor() const { return grid_anchor; }
	// Current world-space size of the box (grows with _grow_if_needed, so this
	// can change frame to frame even though grid_anchor doesn't). 4 = cells
	// per block (BLK in gas_solver.cpp/gas_block_inc.glsl -- not visible here,
	// this header has no GLSL/.cpp-local includes).
	Vector3 get_world_size() const { return Vector3(settings.box_blocks * 4) * settings.cell_size; }

	// (Re)builds the grid for these settings and touches every block in the
	// fixed box. p_xform places the box in world space (its -half-extent
	// corner sits at the box's minimum corner); called again to resize.
	void configure(const Settings &p_settings, const Transform3D &p_xform);

	// Lightweight re-lock of the domain at a NEW world position, same
	// settings (box_blocks/cell_size unchanged) -- for when the NODE moves
	// but nothing about its size changed. Reuses the EXISTING buffers/
	// uniform sets (just re-touches + re-clears them, resets the hash table)
	// instead of configure()'s full free+rebuild. Cheaper (skips
	// reallocating every buffer on every move), AND sidesteps a real bug
	// found the hard way: repeatedly free()-ing and recreating the grid/
	// divergence/pressure buffers via configure() left density silently
	// stuck at zero after more than one such cycle -- root cause not fully
	// pinned down (looked like a GPU/driver-level issue with reused memory,
	// not a logic bug in any one shader -- an empty no-op shader dispatched
	// in divergence's exact place was fine, any real one wasn't), but
	// avoiding the repeated alloc/free cycle entirely sidesteps it. Requires
	// an existing configure() to have already run (buffers must exist).
	void reposition(const Transform3D &p_xform);

	// Advance one step. p_xform is kept for a future domain-following feature
	// (see configure()'s note on the frozen box) -- colliders and emitters are
	// already world-space, set fresh each step via set_colliders/set_emitters.
	void step(double p_delta, const Transform3D &p_xform);

	// An analytic collider coupled to the gas domain -- mirrors
	// MPMFluidSolver::Collider's shape catalog minus rotation (axis-aligned
	// box only, v1; a roof/floor/wall is exactly the case a rotation-free
	// box/plane already covers).
	enum ColliderShape { COLLIDER_SPHERE = 0,
		COLLIDER_BOX = 1,
		COLLIDER_PLANE = 2 };
	struct Collider {
		ColliderShape shape = COLLIDER_SPHERE;
		Vector3 position; // world; sphere/box: center | plane: any point on it
		Vector3 extents; // sphere: x=radius | box: half-extents | plane: unit outward normal
	};

	// Up to MAX_COLLIDERS voxelized-solid obstacles, world space, set fresh
	// each step. Entries beyond p_colliders' size disable those slots.
	void set_colliders(const Vector<Collider> &p_colliders);

	// Up to MAX_EMITTERS independent sphere/box injection points, world space.
	// Entries beyond p_emitters' size are disabled.
	void set_emitters(const Vector<Emitter> &p_emitters);

	// Synchronous readback: cell world positions + density for every active
	// cell above p_density_threshold. Used by the node to build a debug-view
	// MultiMesh. Blocking -- fine at this solver's cell counts (tens of
	// thousands), not meant for a tight per-frame budget the way the async
	// MPM path is.
	void get_render_cells(float p_density_threshold, Vector<Vector3> &r_positions, Vector<float> &r_density) const;

	// Synchronous readback: every cell's density (no threshold), densely
	// packed in x + y*dims.x + z*dims.x*dims.y order -- ready to slice
	// straight into an ImageTexture3D (r_dims.z slices of r_dims.x*r_dims.y).
	// r_dims tracks the box's CURRENT size (grows with _grow_if_needed), so
	// the caller must recreate its texture whenever r_dims changes. r_anchor
	// is the world position of cell (0,0,0)'s min corner (matches
	// grid_anchor) and r_cell_size the world size of one cell -- together
	// they place the dense box in world space for a FogVolume.
	void get_density_grid(Vector<float> &r_density, Vector3i &r_dims, Vector3 &r_anchor, float &r_cell_size) const;

private:
	RenderingDevice *rd = nullptr;
	bool shaders_ok = false;

	RID shader_touch, shader_clear, shader_clear_range, shader_curl, shader_step;
	RID shader_divergence, shader_jacobi, shader_project;
	RID pipeline_touch, pipeline_clear, pipeline_clear_range, pipeline_curl, pipeline_step;
	RID pipeline_divergence, pipeline_jacobi, pipeline_project;

	RID buf_params;
	RID buf_bhash, buf_bhash_val, buf_bkey, buf_bcounts;
	RID buf_grid_a, buf_grid_b;
	// gas_bs_project needs to both read (this frame's post-step, pre-
	// projection state) and write (the corrected result) -- doing that by
	// aliasing the SAME physical buffer to both binding 5 and 6 (the original
	// approach) turned out to be a real hazard, not just a GLSL semantics
	// technicality: reproduced a case where density silently went to zero
	// and stayed there after the solver had been configure()'d (freed +
	// rebuilt) more than once, root-caused to this aliasing via a disable/
	// re-enable isolation test. Project now reads NEW_STATE and writes into
	// this dedicated scratch buffer instead, and GasSolver::step() copies the
	// result back into NEW_STATE (a cheap RenderingDevice::buffer_copy, not a
	// compute dispatch) so the existing grid_a/grid_b ping-pong scheme is
	// otherwise untouched.
	RID buf_grid_scratch;
	RID buf_curl;
	// Pressure-projection scratch -- see gas_block_inc.glsl's header for the
	// solve this feeds (gas_bs_divergence -> gas_bs_jacobi x N -> gas_bs_project).
	RID buf_divergence, buf_pressure_a, buf_pressure_b;

	RID uset_touch, uset_clear;
	RID uset_curl_a, uset_curl_b; // read grid A (resp. B) as "current", write curl
	RID uset_step_atob, uset_step_btoa; // ping-pong: read/write swap each step
	RID uset_divergence_a, uset_divergence_b; // read grid A (resp. B) as the post-step field to compute div from
	RID uset_jacobi_p5top6, uset_jacobi_p6top5; // ping-pong across PRESSURE_JACOBI_ITERS iterations
	RID uset_project_a, uset_project_b; // in-place correct grid A (resp. B); pressure5 fixed by iteration-count parity

	Settings settings;
	Transform3D domain_xform;
	Vector3 grid_anchor; // world position of cell (0,0,0)'s min corner, frozen at configure()

	// World-space, set fresh each step by the caller (not part of Settings,
	// not frozen at configure()). enabled=false slots upload shape=-1 (see
	// _upload_params), which collider_sdf() in gas_block_inc.glsl treats as
	// "always outside" -- no separate enabled flag needed on the GPU side.
	struct ColliderSlot {
		Collider collider;
		bool enabled = false;
	};
	ColliderSlot collider_slots[MAX_COLLIDERS];

	Emitter emitters[MAX_EMITTERS];

	// Dynamic vertical growth: only ever adds blocks at the TOP of the box
	// (settings.box_blocks.y increases; grid_anchor and every already-touched
	// cell's coordinate stay valid, so this never disturbs existing smoke --
	// see gas_bs_clear_range.glsl for why the clear step is safe). Reserved
	// pool capacity is sized generously at configure() so growth never needs
	// to reallocate a buffer, only claim more of what's already allocated.
	int initial_box_blocks_y = 0;
	int max_box_blocks_y = 0; // cap so it stops trying once the reserved pool would be exhausted
	int steps_since_growth = 0;
	Vector3i pending_growth_range; // x = first newly-touched slot this pass, y = one past the last, z unused; (0,0,0) = no growth this upload
	void _grow_if_needed();

	int block_count = 0; // current active block count (grows over time)
	int cell_count = 0; // current active cell count (grows over time)
	int hash_slots = 0;
	int max_blocks = 0;
	bool a_is_current = true;
	double time_accum = 0.0;

	Vector<Vector3i> slot_block; // slot -> block coord, read back once after configure()

	void _compile_shaders();
	void _free_buffers();
	void _build_buffers();
	void _upload_params(double p_dt);
	void _read_slot_blocks();
	RID _make_uniform_set(RID p_shader, RID p_grid5, RID p_grid6, RID p_press5, RID p_press6) const;
	void _dispatch(RID p_pipeline, RID p_uset, int p_thread_count) const;

	// Diagnostics (PART-1 validation): submits issued by the last step().
	// The single-submit step sets this to exactly 1.
	int step_submit_count = 0;

public:
	int get_last_step_submits() const { return step_submit_count; }

	// Total CELLS in the currently configured domain (PART-3): bounds the
	// debug point cloud's MultiMesh to the real domain instead of a fixed
	// worst-case pool.
	int get_domain_cells() const;
};

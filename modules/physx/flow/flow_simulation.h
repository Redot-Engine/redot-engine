/**************************************************************************/
/*  flow_simulation.h                                                     */
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

#ifdef GODOT_PHYSX_FLOW

#include "flow_context_rd.h"
#include "flow_runtime.h"

#include "core/math/basis.h"
#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/math/vector3i.h"
#include "core/templates/local_vector.h"

// One NVIDIA Flow sparse grid ("simulation"): owns an NvFlowGrid on the
// shared FlowRuntime plus the CPU-side parameter assembly and readback
// pipeline. All Flow SDK calls happen on the main thread.
//
// Parameter model: each step() receives plain Godot-space records (emitters,
// colliders, settings); they are translated into NvFlowGridSimulateLayerParams
// / emitter param arrays and packed into a hand-built NvFlowDatabaseSnapshot
// (the SDK's simulate() consumes typed instance arrays keyed by reflect
// datatype -- NvFlowDatabaseSnapshot_findTypeArray -- so the versioned
// NvFlowGridParams transaction database is not needed and is deliberately
// not used).
//
// Readback: after simulate()+offscreen(), the grid's sparse density texture
// and sparse table buffer are copied into persistent readback buffers on the
// Flow context (public copy passes). Once the device queue reports the frame
// complete, the sparse layout is decoded on the CPU -- exactly the mapping
// the SDK's own raymarch performs on the GPU (NvFlowTableValueToIndexRead in
// NvFlowShader.hlsli): per blockIdx, a packed table value yields the block's
// real texel base; virtual voxel = real voxel - offset; world position =
// location * blockSizeWorld + localVoxel * cellSize. The result is a dense
// RGBA-half volume (R=smoke, G=temperature, B=fuel, A=burn) ready for an
// ImageTexture3D. This costs one sparse-sized GPU->CPU copy per frame; a
// future direct GPU interop path is documented as a non-goal for v1.

// Grid capacity / quality knobs (recreating the grid on change).
struct FlowSimGridConfig {
	uint32_t max_blocks = 4096; // NvFlowGridDesc.maxLocations
};

// Per-layer simulation settings (subset of NvFlowGridSimulateLayerParams the
// node exposes; everything else keeps SDK defaults).
struct FlowSimSettings {
	float cell_size = 0.0f; // 0 = auto (derive from emitter radius)
	float steps_per_second = 60.0f;
	float time_scale = 1.0f;
	uint32_t max_steps_per_simulate = 4;
	uint32_t velocity_substeps = 1;
	uint32_t block_min_lifetime = 4;
	bool simulate_when_paused = false;

	// Advection / combustion (Godot world-space; Flow itself is axis-agnostic,
	// the SDK sample defaults like gravity=(0,0,-100) are USD Z-up conventions
	// we do NOT inherit -- all vectors below are authored in Godot's Y-up).
	Vector3 gravity = Vector3(0, -100, 0); // Flow's buoyancy works against this magnitude
	float buoyancy_per_temp = 2.0f;
	float buoyancy_per_smoke = 0.0f;
	float cooling_rate = 1.5f;
	float smoke_fade = 0.65f; // advection fade, ~dissipation
	float temperature_fade = 0.0f;
	float fuel_fade = 0.0f;
	bool combustion_enabled = true;

	float vorticity_force_scale = 0.6f;
	bool pressure_enabled = true;

	// A FLOOR: with floor_enabled, gas never goes below floor_height -- a
	// wide solid under it (a solid reserves no blocks, so this costs
	// nothing). Colliders cannot describe terrain (heightmaps/trimeshes are
	// not gathered), so without one a blast's or a fire's gas sank into the
	// ground and spent the grid's block budget there.
	bool floor_enabled = false;
	float floor_height = 0.0f;
};

// Render-path settings: Flow's own ray marcher draws the grid with a
// colormap (temperature normalized over temperature_range -> linear RGB,
// opacity from smoke) and self-shadows the gas toward light_direction.
// Filled by the node every step; consumed by step() when it commits the
// offscreen/render layer params.
struct FlowRenderSettings {
	static constexpr int COLORMAP_POINTS = 32;
	float attenuation = 0.05f; // per unit of smoke, per unit distance
	float temperature_range = 2.0f;
	float shadow_min_intensity = 0.125f;
	int shadow_steps = 48; // 0 disables shadowing
	Vector3 light_direction = Vector3(0.3, 1.0, 0.2).normalized();
	float colormap_rgb[COLORMAP_POINTS][3] = {};
};

// What the light scattering pass (flow_scatter_rd) needs to read the gas Flow
// just drew: the density atlas (rgba: temperature, -, sun light, smoke) and its
// sparse table, both RIDs valid until render_finish(), and the table's level
// parameters (Flow's NvFlowSparseLevelParams, 40 words). Meters.
struct FlowScatterSource {
	bool valid = false;
	uint64_t density_rid = 0;
	uint64_t table_rid = 0;
	uint32_t level[40] = {};
	float block_size_world[3] = {};
	int layer_and_level = 0;
	float world_min[3] = {};
	float world_max[3] = {};
};

// An injection source (sphere or box) in Godot world space.
struct FlowEmitterData {
	uint64_t luid = 0;
	bool enabled = true;
	bool is_box = false; // sphere otherwise
	Transform3D world_xform;
	Vector3 velocity; // emission velocity, node-local, rotated by world_xform
	float radius = 0.5f; // sphere radius (local units of world_xform)
	Vector3 half_size = Vector3(0.5, 0.5, 0.5); // box half extents (local)
	float divergence = 0.0f;
	float temperature = 0.5f;
	float fuel = 0.8f;
	float smoke = 0.0f;
	float burn = 0.0f;
	float couple_rate = 2.0f; // master couple-rate for all channels
	uint32_t sub_steps = 1;
};

// Collision geometry (box or triangle mesh) driven by the physics bridge.
// Flow expresses collision as emitters with isPhysicsCollision=true.
struct FlowColliderData {
	uint64_t luid = 0;
	bool enabled = true;
	bool is_mesh = false; // box otherwise
	Transform3D world_xform;
	Vector3 linear_velocity; // world-space, at xform.origin
	Vector3 angular_velocity; // world-space rad/s
	Vector3 half_size = Vector3(0.5, 0.5, 0.5);
	// Mesh collider data (local space of world_xform, non-indexed triangles).
	LocalVector<float> mesh_positions; // xyz triples
	void *mesh_owner = nullptr; // opaque cache key of the source shape
};

// One frame of decoded sparse data (dense volume), GPU-completed.
struct FlowReadbackFrame {
	uint64_t frame_index = 0;
	double absolute_sim_time = 0.0;
	bool valid = false;
	Vector3 world_min;
	Vector3 world_size;
	float cell_size = 0.0f;
	// Dense RGBA half-float voxel volume, X + Y*dim.x + Z*dim.x*dim.y,
	// packed as 4 uint16 half components per voxel (Image::FORMAT_RGBAH).
	Vector<uint16_t> voxels_rgbaH;
	Vector3i dims = Vector3i();
	uint32_t active_blocks = 0;
	float max_smoke = 0.0f;
	float max_temperature = 0.0f;
};

class FlowSimulation {
public:
	enum DiagFlags {
		DIAG_NONE = 0,
		DIAG_SPARSE_LAYOUT = 1, // log texture dims/block layout once per configure
		DIAG_READBACK_STATS = 2, // log per-readback decode stats
	};

	~FlowSimulation();

	bool is_available() const { return runtime != nullptr && runtime->is_available(); }
	String get_unavailable_reason() const { return runtime ? runtime->get_unavailable_reason() : String(); }
	// "rd" on the RenderingDevice path, or the fallback device's API name.
	String get_backend_name() const;

	void set_diag_flags(uint32_t p_flags) { diag_flags = p_flags; }

	// Grid lifecycle. Recreates the NvFlowGrid when max_blocks changes.
	void configure(const FlowSimGridConfig &p_config);
	FlowSimGridConfig get_config() const { return grid_config; }

	// Advances the simulation. delta/absolute time in seconds; emitters and
	// colliders are read for the duration of the call only (copied into SDK
	// parameter storage). force_clear resets the sparse grids.
	void step(double p_absolute_time, float p_delta, const FlowEmitterData *p_emitters, int p_emitter_count,
			const FlowColliderData *p_colliders, int p_collider_count, const FlowSimSettings &p_settings,
			const FlowRenderSettings &p_render, bool p_force_clear, bool p_paused, bool p_want_readback);

	void reset();

	// Consumes the newest completed readback frame. Returns true when
	// out_frame was filled with data newer than the last consume.
	bool poll_readback(FlowReadbackFrame &r_frame);

	// Renders the grid with Flow's own GPU ray marcher (RenderingDevice path
	// only) into the scene color image (RID id p_color_rid), occluded by the
	// scene depth (RID id p_depth_rid at its own size). view/projection are
	// row-vector 4x4 world->clip matrices in Flow units (Godot's units, so
	// no scaling). Returns the RID id of the composited color texture -- copy
	// it over the scene color before render_finish() -- or 0.
	uint64_t render(const float p_view[16], const float p_projection[16], uint32_t p_width, uint32_t p_height,
			uint64_t p_depth_rid, uint32_t p_depth_width, uint32_t p_depth_height, uint64_t p_color_rid, FlowScatterSource *r_scatter = nullptr);
	// Ends the render's Flow frame; call after the result has been copied.
	void render_finish();
	// The volume/table the last render() read, for the light scattering pass
	// (valid until render_finish()).
	FlowScatterSource scatter_source;

	bool is_using_rd() const { return using_rd && rd_context != nullptr; }

	uint32_t get_active_block_count() const;
	uint64_t get_simulated_frame_count() const { return frames_simulated; }
	double get_sim_time() const { return sim_time; }

	struct TimingStats {
		uint64_t last_submit_usec = 0; // simulate + offscreen + record copies
		uint64_t last_decode_usec = 0; // CPU sparse->dense decode
		uint64_t last_flush_usec = 0;
	};
	TimingStats get_timing_stats() const { return timing; }

private:
	FlowRuntime *runtime = nullptr; // acquired in configure() (loader always; device only on the fallback path)
	FlowContextRD::Context *rd_context = nullptr; // RenderingDevice-backed Flow context, null on the fallback path
	bool using_rd = false;
	NvFlowGrid *grid = nullptr;
	FlowSimGridConfig grid_config;

	// The SDK's parameter transaction database: our hand-built per-frame
	// snapshot is committed INTO it, and simulate() consumes the processed
	// snapshot it hands back (it assigns valid layer luids / versions; a
	// desc built straight from our arrays produced a live context but zero
	// block allocation -- the grid ignored params it did not get through
	// this database, which is also what the SDK sample path uses).
	NvFlowGridParams *grid_params = nullptr;
	NvFlowGridParamsSnapshot *mapped_snapshot = nullptr;
	// The desc mapped over the CURRENT snapshot, kept alive across the frame:
	// render() composites with the SAME committed params step() simulated
	// with (offscreen colormap / raymarch tuning). Re-pulling by time in
	// render() handed the SDK a stale snapshot and crashed.
	NvFlowGridParamsDesc mapped_desc = {};
	bool desc_active = false;
	uint64_t pull_id = 1;

	// SDK parameter storage (stable for the duration of a recorded frame).
	NvFlowGridSimulateLayerParams layer_params = {};
	LocalVector<NvFlowGridEmitterSphereParams> sphere_params;
	LocalVector<NvFlowGridEmitterSphereParams *> sphere_ptrs;
	LocalVector<NvFlowGridEmitterBoxParams> box_params;
	LocalVector<NvFlowGridEmitterBoxParams *> box_ptrs;
	LocalVector<NvFlowGridEmitterMeshParams> mesh_params;
	LocalVector<NvFlowGridEmitterMeshParams *> mesh_ptrs;
	// Mesh collider CPU arrays referenced by mesh_params (parallel to
	// mesh_params; FlowEmitterMeshParams points into these).
	struct MeshArrays {
		LocalVector<NvFlowFloat3> positions;
		LocalVector<int> indices; // flattened face vertex indices
		LocalVector<int> face_counts; // vertices per face (3)
	};
	LocalVector<MeshArrays> mesh_arrays;

	// Render-path parameters, kept alive across one step()'s commit ->
	// offscreen/render: the snapshot database points into these.
	NvFlowGridOffscreenLayerParams offscreen_params = NvFlowGridOffscreenLayerParams_default;
	NvFlowGridRenderLayerParams render_layer_params = NvFlowGridRenderLayerParams_default;
	NvFlowUint8 *layer_ptr = nullptr;
	NvFlowUint8 *offscreen_ptr = nullptr;
	NvFlowUint8 *render_ptr = nullptr;
	float colormap_x[FlowRenderSettings::COLORMAP_POINTS] = {};
	NvFlowFloat4 colormap_rgba[FlowRenderSettings::COLORMAP_POINTS] = {};
	float colormap_scale[FlowRenderSettings::COLORMAP_POINTS] = {};

	// The params database shallow-copies the committed desc (GridParams.cpp
	// commitParams keeps the typeSnapshots/instanceDatas POINTERS), and the
	// draw-time offscreen() re-reads them after step() has returned. The whole
	// commit chain must therefore live in stable storage, never on step()'s
	// stack (stack copies made every offscreen lookup fall back to the SDK's
	// default white colormap).
	NvFlowDatabaseTypeSnapshot commit_type_snapshots[6] = {};
	NvFlowDatabaseSnapshot commit_db_snapshot = {};
	NvFlowGridParamsDescSnapshot commit_snapshot = {};

	void _apply_render_params(const FlowRenderSettings &p_render);

	// Readback pipeline: TWO buffer slots, recorded in alternation, so a
	// frame's copies stay intact on the GPU until its completion id is
	// observed (single-buffering races: the next record overwrites the
	// buffer one tick before the previous frame's completion is visible).
	struct InFlightReadback {
		uint64_t frame_index = 0; // device frame this copy was submitted in (0 = idle)
		double absolute_sim_time = 0.0;
		// Sparse layout snapshot matching the copied frame.
		NvFlowSparseLayerParams layer = {};
		NvFlowSparseLevelParams level = {};
		LocalVector<NvFlowInt4> locations;
		NvFlowFormat texture_format = eNvFlowFormat_unknown;
		uint32_t texture_width = 0;
		uint32_t texture_height = 0;
		uint32_t texture_depth = 0;
		uint64_t table_bytes = 0;
	};
	struct ReadbackSlot {
		NvFlowBuffer *texture_buffer = nullptr;
		NvFlowBuffer *table_buffer = nullptr;
		uint64_t texture_buffer_bytes = 0;
		uint64_t table_buffer_bytes = 0;
		InFlightReadback inflight;
		bool consumed = true;
	};
	ReadbackSlot slots[2];
	uint32_t record_slot = 0;
	bool readback_limit_warned = false;
	bool cmap_read_done = false;
	mutable bool render_diag_done = false; // TEMP diagnostics // one warn per grid life

	FlowReadbackFrame last_readback;
	uint64_t frames_simulated = 0;
	double sim_time = 0.0;
	double step_entry_time = 0.0; // FLOW-8: time the CURRENT step started
	uint32_t diag_flags = DIAG_NONE;
	TimingStats timing;

	void _destroy_grid();
	void _ensure_readback_buffers(ReadbackSlot &r_slot, uint64_t p_texture_bytes, uint64_t p_table_bytes);
	void _release_readback_buffers();
	void _record_readback_copies(ReadbackSlot &r_slot, NvFlowContextInterface *p_ci, NvFlowGridInterface *p_gi);
	bool _decode_slot(ReadbackSlot &r_slot, FlowReadbackFrame &r_frame);

	NvFlowContextInterface *_context_iface() const; // valid while a grid is alive
	NvFlowContext *_context() const;
	// Closes a Flow frame: schedules this frame's readbacks and a fence that
	// marks it complete; r_frame_id receives the id completion is gated on.
	void _flush_frame(uint64_t *r_frame_id);
	uint64_t _last_frame_completed() const;

	static NvFlowFloat4x4 _to_flow_matrix(const Transform3D &p_xform);
	static NvFlowFloat4x4 _to_flow_velocity_matrix(const Transform3D &p_xform, const Vector3 &p_linear, const Vector3 &p_angular);
};

#endif // GODOT_PHYSX_FLOW

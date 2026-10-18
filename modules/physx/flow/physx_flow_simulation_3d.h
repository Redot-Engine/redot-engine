/**************************************************************************/
/*  physx_flow_simulation_3d.h                                            */
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

#include "flow_scatter_rd.h"
#include "flow_simulation.h"
#include "core/math/projection.h"
#include "core/templates/paged_array.h"
#include "core/os/mutex.h"
#include "core/object/object_id.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"
#include "scene/3d/node_3d.h"

class PhysXFlowRenderEffect;
class PhysXFlowEmitter3D;
class PhysXFlowCollider3D;
class CollisionObject3D;
class PhysicsBody3D;
class RenderingDevice;

// A NVIDIA Flow sparse-grid fluid/fire/smoke simulation (Flow 2.2, loaded at
// runtime from nvflow.dll/nvflowext.dll -- see flow/docs/IMPLEMENTATION_
// NOTES.md for the full design). Unlike PhysXGas3D's fixed-domain solver, a
// Flow grid is unbounded and sparse: blocks allocate anywhere in world space
// around whatever the emitters touch, bounded by max_blocks. There is no
// domain to freeze or reposition -- emitter/collider world transforms are
// the whole authoring surface.
//
// Simulated on Godot's own RenderingDevice whenever one exists (Forward+ and
// Mobile): the gas never leaves VRAM, and PhysXFlowRenderEffect draws it
// into the frame with Flow's own ray marcher (self-shadowed by the scene's
// first DirectionalLight3D). Nothing crosses to the CPU except the opt-in
// cpu_readback query copy. On headless / Compatibility there is no
// RenderingDevice: the grid falls back to the SDK's own Vulkan/CPU device
// (the v1 path the headless test suite still drives), with no drawing.
class PhysXFlowSimulation3D : public Node3D {
	GDCLASS(PhysXFlowSimulation3D, Node3D);

	FlowSimulation *simulation = nullptr;

	// --- Simulation settings (FlowSimSettings mirror) ---------------------
	bool enabled = true;
	bool paused = false;
	int max_blocks = 1024; // upstream PhysXFlow3D parity; 4096 x several sims exhausts VRAM
	// Voxel size. 0 would mean the SDK's auto-cellsize, but that resolves no
	// allocation in 2.2.0 without a seeded densityCellSize (measured), so the
	// default is a real 0.25 m voxel -- keep 0 only for experiments.
	float cell_size = 0.25f;
	float steps_per_second = 60.0f;
	float time_scale = 1.0f;
	int max_steps_per_simulate = 4;
	int velocity_substeps = 1;
	int block_min_lifetime = 4;
	bool simulate_when_paused = false;
	bool combustion_enabled = true;
	Vector3 gravity = Vector3(0, -100, 0); // Flow-authored buoyancy reference (see FlowSimSettings)
	float buoyancy_per_temp = 2.0f;
	float buoyancy_per_smoke = 0.0f;
	float cooling_rate = 1.5f;
	float smoke_fade = 0.65f;
	float temperature_fade = 0.0f;
	float fuel_fade = 0.0f;
	float vorticity = 0.6f;
	bool pressure_projection = true;

	bool sim_settings_dirty = true; // max_blocks changes recreate the grid
	double sim_time_accum = 0.0; // monotonic clock fed to Flow's absoluteSimTime

	// --- Emitters / colliders ----------------------------------------------
	TypedArray<NodePath> emitters;
	TypedArray<NodePath> colliders;
	float collider_fallback_size = 0.5f; // half-extent for unresolvable nodes

	// Stable luids across frames per resolved node (Flow matches params by luid).
	uint64_t next_luid = 2; // 1 is the sim layer
	HashMap<ObjectID, uint64_t> node_luids;

	// Transient event emitters (see add_transient_emitter).
	struct TransientEmitter {
		Vector3 position;
		float radius = 0.5f;
		float smoke = 1.0f;
		float temperature = 0.0f;
		float fuel = 0.0f;
		float velocity = 1.0f;
		float lifetime = 1.5f;
		float age = 0.0f;
		uint64_t luid = 0;
	};
	LocalVector<TransientEmitter> transient_emitters;
	uint64_t next_transient_luid = 0xF000000000000000ull; // dedicated range, never collides with node luids

	// Physics-server collider bridge cache: cooked triangle soups keyed by
	// shape RID (built once per shape, reused while the RID lives).
	struct CachedShapeMesh {
		LocalVector<float> positions; // local-space xyz triples
		bool analytic_box = false;
		Vector3 box_half_extents;
	};
	HashMap<RID, CachedShapeMesh> shape_mesh_cache;
	HashSet<ObjectID> active_collider_ids; // current-pass collider nodes (FLOW-4 pruning)
	int cache_validate_countdown = 60; // 1/sec at 60 Hz
	uint64_t frame_count = 0;

	// Body transforms of kinematic bodies that report no velocity, for
	// finite-difference estimates (keyed by node instance id).
	HashMap<ObjectID, Transform3D> kinematic_last_transforms;

	// --- Rendering (Flow's own GPU ray marcher) ------------------------------
	bool volumetric_render = true;
	// Smoke opacity in the ray march, per unit of smoke per meter. The old
	// FogVolume's density scale was linear (24 read well there); the ray
	// marcher's attenuation is exponential, so the default maps 1/10th.
	float fog_density = 4.0f; // 1:1 ray-march attenuation (upstream smoke_density)
	Color smoke_albedo = Color(0.9f, 0.9f, 0.9f);
	bool fire_emission = true;
	float fire_emission_strength = 6.0f;
	// Temperature the colormap spans (the colormap's x axis: 0..range).
	float temperature_range = 2.0f;
	// Self-shadowing reach of the gas, in steps of 3/4 of a cell: 48 covers
	// ~1.8 m at the default 25 cm cells -- fewer leaves wide gas lit almost
	// evenly. 0 disables shadowing.
	int shadow_steps = 48;
	// Light scattering: the gas is lit and self-shadows toward the scene's
	// light (scattering_receive_shadows nonzero = shadows sample). Maps the
	// reference Flow node's light_scattering / scattering_receive_shadows.
	enum ScatteringShadowFlags {
		SCATTERING_SHADOW_OMNI = 1,
		SCATTERING_SHADOW_SPOT = 2,
	};
	bool light_scattering = true;
	int scattering_receive_shadows = 3;
	float scattering_strength = 1.0f;
	float scattering_anisotropy = 0.4f; // Henyey-Greenstein g
	int scattering_resolution = 4; // frame pixels per scattering texel, each way
	int scattering_max_lights = 4;
	int scattering_shadow_steps = 4; // toward each point/spot light
	float scattering_shadow_reach = 4.0f; // meters
	// The volume/table render() read this frame, for the scattering pass.
	FlowScatterSource scatter_source;
	// Point/spot lights gathered for the scattering pass (see _gather_scatter_lights).
	Mutex scatter_mutex;
	LocalVector<FlowScatterRD::Light> scatter_lights;
	LocalVector<ObjectID> scatter_light_ids;
	int scatter_search_countdown = 0;
	FlowScatterRD *scatter = nullptr;
	int stat_scatter_lights = 0;
	int stat_scatter_passes = 0;
	int stat_scatter_no_source = 0;
	bool debug_stats = false;

	// Optional CPU copy of the gas for the query API (get_last_max_smoke,
	// bounds): one full decode every READBACK_INTERVAL steps. The GPU render
	// path never touches it. On the fallback path (no RenderingDevice) there
	// is no render to show and readback is the only data channel, so it runs
	// regardless of this flag.
	bool cpu_readback = false; // opt-in: serves only the query API now (upstream default)
	static constexpr int READBACK_INTERVAL = 4;

	FlowReadbackFrame last_frame;
	uint64_t texture_update_count = 0; // decoded readback frames (FLOW-1 diagnostic)

	// The render effect calls this per view, on the render thread.
	friend class PhysXFlowRenderEffect;
	void render_rd(PhysXFlowRenderEffect *p_effect, RenderingDevice *p_rd, const float p_view[16], const float p_projection[16],
			const Size2i &p_size, const RID &p_depth, const RID &p_color, const Projection &p_inv_view_projection,
			const Vector3 &p_camera, const PagedArray<RID> *p_lights, const RID &p_shadow_atlas);
	void _gather_scatter_lights();
	void _fill_scatter_shadows(LocalVector<FlowScatterRD::Light> &r_lights, const PagedArray<RID> *p_instances, const RID &p_shadow_atlas, uint32_t p_flags) const;
	FlowScatterRD &get_scatter();

	// Where the gas roughly is (its emitters' middle), for drawing flows
	// back to front. Written by _step, read on the render thread.
	Vector3 draw_center;

	RenderingDevice *rendering_device = nullptr; // captured at start; null on the fallback path
	ObjectID attached_world;

	// Render-side counters (diagnostics).
	uint64_t render_calls = 0;
	uint64_t render_outputs = 0;

	// First visible DirectionalLight3D, looked up every so often.
	ObjectID light_id;
	int light_search_countdown = 0;

	// Editor-friendly diagnostics (also the test hooks).
	bool available = false;

	void _ensure_simulation();
	void _step(double p_delta);
	FlowSimSettings _build_settings() const;
	bool _shape_cache_matches(const RID &p_shape, const CachedShapeMesh &p_cache) const;
	void _resolve_emitters(LocalVector<FlowEmitterData> &r_emitters);
	void _resolve_transients(double p_delta, LocalVector<FlowEmitterData> &r_emitters);
	void _resolve_colliders(LocalVector<FlowColliderData> &r_colliders, double p_delta);
	void _update_render();
	void _gather_light(Vector3 &r_direction, Color &r_color, Color &r_ambient);
	void _fill_render_settings(FlowRenderSettings &r_settings);
	uint64_t _luid_for(ObjectID p_id);

	bool _build_server_shape_mesh(const RID &p_shape_rid, CachedShapeMesh &r_cache);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	PackedStringArray get_configuration_warnings() const override;

	// Runtime/gameplay control.
	void set_enabled(bool p_enabled);
	bool get_enabled() const { return enabled; }
	void set_paused(bool p_paused);
	bool get_paused() const { return paused; }
	void reset_simulation();
	void step_once(); // advance a single step while paused (editor stepping)

	// Transient event emitters: one-shot smoke/fire bursts in world space
	// (destruction dust, impact puffs, explosion fireballs). They live for
	// `lifetime` seconds of simulated time, decaying their injected values
	// linearly to zero. This is the generic event-adapter target: the Blast
	// bridge forwards fracture events here, and scripts may call it directly
	// for any event source -- the simulation knows nothing about Blast.
	void add_transient_emitter(const Vector3 &p_world_position, float p_radius,
			float p_smoke, float p_temperature, float p_fuel, float p_velocity, float p_lifetime = 1.5f);

	// Diagnostics (all cheap; some return stale values until the first step).
	bool is_flow_available() const;
	int get_frame_count() const;
	int get_texture_update_count() const;
	Dictionary get_diagnostics() const;
	int get_active_block_count() const;
	Vector3 get_active_bounds_min() const { return last_frame.world_min; }
	Vector3 get_active_bounds_size() const { return last_frame.world_size; }
	float get_last_max_smoke() const { return last_frame.max_smoke; }
	float get_last_max_temperature() const { return last_frame.max_temperature; }

	void set_max_blocks(int p_blocks);
	int get_max_blocks() const { return max_blocks; }
	void set_cell_size(float p_size);
	float get_cell_size() const { return cell_size; }
	void set_steps_per_second(float p_sps);
	float get_steps_per_second() const { return steps_per_second; }
	void set_time_scale(float p_scale);
	float get_time_scale() const { return time_scale; }
	void set_max_steps_per_simulate(int p_steps);
	int get_max_steps_per_simulate() const { return max_steps_per_simulate; }
	void set_velocity_substeps(int p_steps);
	int get_velocity_substeps() const { return velocity_substeps; }
	void set_block_min_lifetime(int p_seconds);
	int get_block_min_lifetime() const { return block_min_lifetime; }
	void set_simulate_when_paused(bool p_enabled);
	bool get_simulate_when_paused() const { return simulate_when_paused; }
	void set_combustion_enabled(bool p_enabled);
	bool get_combustion_enabled() const { return combustion_enabled; }
	void set_gravity(const Vector3 &p_gravity);
	Vector3 get_gravity() const { return gravity; }
	void set_buoyancy_per_temp(float p_b);
	float get_buoyancy_per_temp() const { return buoyancy_per_temp; }
	void set_buoyancy_per_smoke(float p_b);
	float get_buoyancy_per_smoke() const { return buoyancy_per_smoke; }
	void set_cooling_rate(float p_rate);
	float get_cooling_rate() const { return cooling_rate; }
	void set_smoke_fade(float p_fade);
	float get_smoke_fade() const { return smoke_fade; }
	void set_temperature_fade(float p_fade);
	float get_temperature_fade() const { return temperature_fade; }
	void set_fuel_fade(float p_fade);
	float get_fuel_fade() const { return fuel_fade; }
	void set_vorticity(float p_strength);
	float get_vorticity() const { return vorticity; }
	void set_pressure_projection(bool p_enabled);
	bool get_pressure_projection() const { return pressure_projection; }

	void set_emitters(const TypedArray<NodePath> &p_emitters) { emitters = p_emitters; }
	TypedArray<NodePath> get_emitters() const { return emitters; }
	void set_colliders(const TypedArray<NodePath> &p_colliders) { colliders = p_colliders; }
	TypedArray<NodePath> get_colliders() const { return colliders; }
	void set_collider_fallback_size(float p_size) { collider_fallback_size = MAX(p_size, 0.001f); }
	float get_collider_fallback_size() const { return collider_fallback_size; }

	void set_volumetric_render(bool p_enabled);
	bool get_volumetric_render() const { return volumetric_render; }
	void set_fog_density(float p_density);
	float get_fog_density() const { return fog_density; }
	void set_smoke_albedo(const Color &p_color);
	Color get_smoke_albedo() const { return smoke_albedo; }
	void set_fire_emission(bool p_enabled);
	bool get_fire_emission() const { return fire_emission; }
	void set_fire_emission_strength(float p_strength);
	float get_fire_emission_strength() const { return fire_emission_strength; }
	void set_temperature_range(float p_range);
	float get_temperature_range() const { return temperature_range; }
	void set_shadow_steps(int p_steps);
	int get_shadow_steps() const { return shadow_steps; }
	void set_light_scattering(bool p_enabled) { light_scattering = p_enabled; }
	bool is_light_scattering() const { return light_scattering; }
	void set_scattering_receive_shadows(int p_flags) { scattering_receive_shadows = p_flags; }
	int get_scattering_receive_shadows() const { return scattering_receive_shadows; }
	void set_cpu_readback(bool p_enabled);
	bool get_cpu_readback() const { return cpu_readback; }
	void set_debug_stats(bool p_enabled);
	bool get_debug_stats() const { return debug_stats; }

	// For the render effect's back-to-front sort (render thread).
	Vector3 get_draw_center() const { return draw_center; }

	PhysXFlowSimulation3D() {}
	~PhysXFlowSimulation3D();
};

#endif // GODOT_PHYSX_FLOW

/**************************************************************************/
/*  physx_particle_fluid_3d.h                                             */
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

#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/mesh.h"

class MPMFluidSolver;
class PhysXChunkEmitter3D;

// A GPU fluid volume. Two backends:
//   * PBD  -- PhysX 5 PxPBDParticleSystem, GPU (CUDA) only. Foam and a GPU
//             isosurface mesh are available on this path. Inert unless the 3D
//             physics engine is "PhysX", the build has GPU support
//             (physx_gpu=yes) and a CUDA device is present.
//   * MPM  -- a weakly-compressible MLS-MPM solver on plain compute shaders
//             (RenderingDevice, no CUDA), so it runs on any Vulkan/Metal/D3D12
//             device and needs no physics engine. Emission, analytic colliders
//             (mpm_colliders) and a GPU-marched isosurface; no foam yet.
// "Solver" picks one; Auto uses PBD when a CUDA device is available, else MPM.
class PhysXParticleFluid3D : public GeometryInstance3D {
	GDCLASS(PhysXParticleFluid3D, GeometryInstance3D);

public:
	enum SolverBackend {
		SOLVER_AUTO,
		SOLVER_PBD,
		SOLVER_MPM,
	};

protected:
	// PhysXGranular3D overrides these to run the material as Drucker-Prager
	// grains instead of a liquid. Everything else -- emission, colliders,
	// domain, rendering, the RID lifecycle -- is shared.
	virtual bool _is_granular() const { return false; }
	virtual float _granular_friction_deg() const { return 35.0f; }
	virtual float _granular_hardness() const { return 150000.0f; }
	virtual float _granular_cohesion() const { return 0.0f; }
	virtual float _granular_density() const { return 1000.0f; }

private:
	RID fluid; // PhysXServer3D particle-fluid RID
	RID multimesh;

	SolverBackend solver = SOLVER_AUTO;
	MPMFluidSolver *mpm = nullptr;
	// PART-5: impulse routing registry -- maps collider ids to the sources
	// that submitted them; async-reaped impulses (one frame late) route
	// through here so a reshuffled collider list cannot misroute reactions.
	struct MPMSourceEntry {
		Node3D *node = nullptr;
		PhysXChunkEmitter3D *chunk_emitter = nullptr;
		uint64_t chunk_id = 0;
		Vector3 extents;
		uint64_t last_seen = 0;
	};
	HashMap<uint64_t, MPMSourceEntry> _mpm_source_registry;
	uint64_t _mpm_frame = 0;

	// MPM-only tuning (ignored on the PBD path).
	Vector3 mpm_domain_size = Vector3(3, 3, 3);
	int mpm_grid_resolution = 0; // 0 = auto from particle_size (~2 cells per particle)
	int mpm_substeps = 5;
	float mpm_stiffness = 6000.0;
	// PhysicsBody3D nodes coupled to the MPM fluid as analytic shapes: they push
	// the fluid, and RigidBody3D entries get the reaction impulse back. Up to 32
	// total (see MAX_COLLIDERS), shared with auto-collected bodies and debris.
	TypedArray<NodePath> mpm_colliders;
	// When set, also collides against every non-static PhysicsBody3D overlapping
	// the domain each step -- so the fluid reacts to the player, thrown objects
	// etc. without listing them, the way the CUDA path does automatically.
	bool mpm_auto_colliders = false;
	RID _mpm_query_shape; // box shape reused for the auto-collider overlap query
	HashMap<ObjectID, Vector3> _mpm_prev_pos; // last frame's position, for velocity of non-rigid colliders

	uint32_t _mpm_surface_tick = 0; // isosurface is re-marched + read back every Nth step, not every step
	double _mpm_accum = 0.0; // frame-delta accumulator so the MPM solve is capped at ~60 Hz

	SolverBackend _resolved_solver() const;

protected:
	bool _mpm_configured = false;
	bool _mpm_emit_mode = false;
	bool spawned = false;
	// Backend frozen at fluid creation (_make_fluid): runtime property changes
	// (notably toggling foam_enabled, which the Auto rule now keys on) never
	// flip a live backend -- the next spawn()/free re-resolves instead.
	SolverBackend resolved = SOLVER_AUTO;
	bool _mpm_path() const { return resolved == SOLVER_MPM; }
	void _mpm_configure(bool p_prefill = true);

private:
	void _mpm_step(double p_delta);
	void _mpm_emit_step(double p_delta);
	void _update_mpm_foam();
	int _mpm_resolved_grid_res() const;
	void _mpm_surface_params(float &r_iso, float &r_kernel, float &r_boost) const;
	void _mpm_apply_surface_params();
	Ref<Mesh> particle_mesh;

	// Foam/spray/bubble particles, drawn in their own MultiMesh + RS instance.
	RID foam_multimesh;
	PackedFloat32Array foam_buffer_scratch; // reused; avoids a per-frame realloc
	// (MPM foam records arrive as x,y,z,kind; the kind scales the sprite)
	RID foam_instance;
	Ref<Mesh> foam_mesh;

	int particle_count = 4096;
	float particle_size = 0.1;
	float viscosity = 0.01;
	float surface_tension = 0.006;
	float cohesion = 0.02;
	float vorticity = 8.0;
	Vector3 spawn_region_size = Vector3(1, 1, 1);
	bool spawn_on_ready = true;

	bool emitting = false;
	float emission_rate = 2000.0; // particles per second
	float emission_radius = 0.1; // spawn disc/sphere radius at the node origin
	Vector3 emission_velocity = Vector3(0, -3, 0); // local space; length is speed

	// When true, PhysX marching-cubes a smooth triangle mesh from the particles
	// (PxIsosurfaceExtractor) and this node draws it as an ArrayMesh.
	bool surface_mesh = false;
	// Opt-in: feed PhysX per-particle anisotropy to the isosurface extractor.
	// Crisper crests, but it needles fast particles, so keep it off while emitting.
	bool surface_anisotropy = false;
	RID array_mesh;
	Ref<Material> water_material;
	uint32_t surface_mesh_version = 0;

	// Foam isosurface layer: a coarser second mesh over the diffuse particles,
	// drawn only alongside surface_mesh. Its own world-space instance.
	RID foam_array_mesh;
	RID foam_mesh_instance;
	Ref<Material> foam_water_material;
	uint32_t foam_surface_mesh_version = 0;

	bool foam_enabled = false;
	int foam_particle_count = 16384;
	float foam_lifetime = 1.5;
	float foam_threshold = 300.0;
	float foam_buoyancy = 0.9;
	float foam_size = 0.0; // froth clump scale; 0 = follow particle_size
	float _effective_foam_size() const { return foam_size > 0.0f ? foam_size : particle_size; }

	double emit_accum = 0.0;

	// Editor-only: a cheap CPU particle animation (spawn -> gravity -> recycle)
	// so the emitter's motion reads in the viewport. Never runs at game time.
	RID preview_multimesh;
	Ref<Mesh> preview_mesh;
	LocalVector<Vector3> preview_pos;
	LocalVector<Vector3> preview_vel;
	LocalVector<float> preview_age;
	double preview_accum = 0.0;
	double preview_throttle = 0.0;
	void _editor_preview_enter();
	void _editor_preview_exit();
	void _editor_preview_step(double p_delta);

	void _make_fluid();
	void _free_fluid();
	void _apply_params();
	void _apply_foam();
	void _update_render();
	void _update_surface_mesh();
	void _commit_iso_mesh(RID p_mesh, PackedVector3Array &verts, PackedVector3Array &normals, PackedInt32Array &indices, const Ref<Material> &p_material, bool p_to_local, bool p_keep_largest_component, float p_feature_size);
	void _emit_step(double p_delta);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_solver(SolverBackend p_solver);
	SolverBackend get_solver() const { return solver; }
	void set_mpm_domain_size(const Vector3 &p_size);
	Vector3 get_mpm_domain_size() const { return mpm_domain_size; }
	void set_mpm_grid_resolution(int p_res);
	int get_mpm_grid_resolution() const { return mpm_grid_resolution; }
	void set_mpm_substeps(int p_substeps);
	int get_mpm_substeps() const { return mpm_substeps; }
	void set_mpm_stiffness(float p_stiffness);
	float get_mpm_stiffness() const { return mpm_stiffness; }
	void set_mpm_colliders(const TypedArray<NodePath> &p_colliders);
	TypedArray<NodePath> get_mpm_colliders() const { return mpm_colliders; }
	void set_mpm_auto_colliders(bool p_enabled) { mpm_auto_colliders = p_enabled; }
	bool is_mpm_auto_colliders() const { return mpm_auto_colliders; }

	void set_particle_count(int p_count);
	int get_particle_count() const { return particle_count; }
	void set_particle_size(float p_size);
	float get_particle_size() const { return particle_size; }
	void set_viscosity(float p_v);
	float get_viscosity() const { return viscosity; }
	void set_surface_tension(float p_v);
	float get_surface_tension() const { return surface_tension; }
	void set_cohesion(float p_v);
	float get_cohesion() const { return cohesion; }
	void set_vorticity(float p_v);
	float get_vorticity() const { return vorticity; }
	void set_spawn_region_size(const Vector3 &p_size);
	Vector3 get_spawn_region_size() const { return spawn_region_size; }
	void set_spawn_on_ready(bool p_enable);
	bool get_spawn_on_ready() const { return spawn_on_ready; }

	void set_emitting(bool p_emitting);
	bool is_emitting() const { return emitting; }
	void set_emission_rate(float p_rate);
	float get_emission_rate() const { return emission_rate; }
	void set_emission_radius(float p_radius);
	float get_emission_radius() const { return emission_radius; }
	void set_emission_velocity(const Vector3 &p_velocity);
	Vector3 get_emission_velocity() const { return emission_velocity; }

	void set_surface_mesh(bool p_enabled);
	bool is_surface_mesh() const { return surface_mesh; }
	void set_surface_anisotropy(bool p_enabled);
	bool is_surface_anisotropy() const { return surface_anisotropy; }

	void set_foam_enabled(bool p_enabled);
	bool is_foam_enabled() const { return foam_enabled; }
	void set_foam_particle_count(int p_count);
	int get_foam_particle_count() const { return foam_particle_count; }
	void set_foam_lifetime(float p_v);
	float get_foam_lifetime() const { return foam_lifetime; }
	void set_foam_threshold(float p_v);
	float get_foam_threshold() const { return foam_threshold; }
	void set_foam_buoyancy(float p_v);
	float get_foam_buoyancy() const { return foam_buoyancy; }
	void set_foam_size(float p_v);
	float get_foam_size() const { return foam_size; }
	int get_live_foam_count() const;

	// Fill the region (centered on this node) with a jittered grid of particles.
	void spawn();
	// Remove all particles.
	bool is_mpm_available() const;
	void clear();
	int get_live_particle_count() const;
	// GPU cost of the last MPM solver step (submit+sync), ms. 0 on the PBD path.
	double get_mpm_step_msec() const;

	PackedVector3Array get_particle_positions() const;
	// World-space positions of the live diffuse (foam/spray) particles. On the
	// MPM backend this reads the Vulkan diffuse layer's cache; on the CUDA/PBD
	// backend it forwards to the server's diffuse readback.
	PackedVector3Array get_foam_positions() const;

	// 0..1 fraction of the world-space box currently filled with fluid.
	float get_submersion(const AABB &p_world_aabb) const;

	AABB get_aabb() const override;
	PackedStringArray get_configuration_warnings() const override;

	PhysXParticleFluid3D();
	~PhysXParticleFluid3D();
};

VARIANT_ENUM_CAST(PhysXParticleFluid3D::SolverBackend);

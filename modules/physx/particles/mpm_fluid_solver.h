/**************************************************************************/
/*  mpm_fluid_solver.h                                                    */
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

#include "core/math/aabb.h"
#include "core/math/quaternion.h"
#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/math/vector3i.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"
#include "core/variant/variant.h"

class RenderingDevice;

// A cross-vendor MLS-MPM fluid on plain compute shaders (RenderingDevice, no
// CUDA). This is the fallback backend for PhysXParticleFluid3D when the PhysX GPU
// (PBD / CUDA) path is unavailable -- it runs on any Vulkan/Metal/D3D12 device.
//
// Weakly-compressible APIC / MLS-MPM: five compute passes per substep
// (clear grid, P2G mass, P2G momentum, grid update, G2P) plus an optional rigid
// coupling pass (analytic sphere colliders as moving velocity boundary
// conditions) and a render pass that packs positions into a MultiMesh instance
// buffer on the GPU. The whole substep loop stays on the device; the CPU reads
// back only the small collider-impulse buffer each frame, plus the particle
// buffer on demand.
class MPMFluidSolver {
public:
	struct Settings {
		int particle_target = 60000;
		int substeps = 5;
		int grid_res = 48;
		float stiffness = 6000.0f;
		float viscosity = 0.65f;
		float rest_density = 1000.0f;
		float collider_friction = 0.25f;
		float surface_iso = 0.5f; // isosurface level as a fraction of the native packed kernel density
		float surface_kernel = 0.06f; // isosurface SPH kernel radius, world meters (from particle_size)
		float surface_boost = 1.0f; // per-particle mass multiplier in the surface scatter -- inflates the mesh
		// Granular (Drucker-Prager sand / snow) instead of a fluid EOS.
		bool granular = false;
		float granular_hardness = 1.5e5f; // Young's modulus (Pa); softer = more stable, mushier
		float granular_friction_deg = 35.0f; // internal friction angle -> angle of repose
		float granular_cohesion = 0.0f; // 0 = dry sand; small values -> wet sand / packing snow
		Vector3 gravity = Vector3(0, -9.8f, 0);
		Vector3 domain = Vector3(3, 3, 3); // MPM sim box / boundary, centered on the solver transform
		Vector3 spawn_region = Vector3(1, 1, 1); // prefill fills this, centered on the transform (clamped to the domain)

		// Vulkan diffuse (foam/spray/bubble) layer, per Ihmsen et al. 2012 with
		// the SPH neighbor sums translated into MPM measures (strain/curl off
		// the affine matrix, mass-field gradient for the wave crest). Potentials
		// are auto-normalized against a decaying running max, so foam_threshold
		// acts as the master SPAWN SCALE (expected foam per particle-second at
		// full intensity), not a raw unit gate -- it holds across scenes. The
		// layer never joins the solve; it adds two small passes once per frame.
		bool foam_enabled = false;
		int foam_capacity = 8192; // diffuse-particle slots
		float foam_threshold = 3.0f; // master spawn scale
		float foam_lifetime = 1.5f; // seconds before a diffuse particle fades (foam; spray halves it, bubbles double it)
		float foam_buoyancy = 0.9f; // bubble rise, as a fraction of gravity (capped)
		float foam_drag = 0.9f; // bubble drag toward the fluid velocity
		// Per-channel spawn rates (Ihmsen's k_ta / k_wc / k_vo, RikkaBunny's
		// multiplicative kinetic energy). Not node-exposed yet; C++-tunable.
		float foam_rate_ta = 2.0f; // trapped air (shear + compression)
		float foam_rate_wc = 2.0f; // wave crest (outward motion through the surface band)
		float foam_rate_vo = 1.0f; // vorticity
		float foam_rate_ke = 1.0f; // kinetic-energy multiplier
		// Kind-classification thresholds, as fluid coverage fractions of the
		// containing cell's 8 corner nodes: at or below spray_ratio -> spray,
		// at or above bubble_ratio -> bubble, between -> foam.
		float foam_spray_ratio = 0.25f;
		float foam_bubble_ratio = 0.75f;
	};

	// An analytic collider coupled to the fluid.
	enum ColliderShape { COLLIDER_SPHERE,
		COLLIDER_BOX,
		COLLIDER_PLANE,
		COLLIDER_CAPSULE };
	struct Collider {
		ColliderShape shape = COLLIDER_SPHERE;
		Vector3 position; // world center (or a point on the plane)
		Vector3 extents; // sphere: x=radius | box: half-extents | plane: unit normal | capsule: (radius, half-height, -)
		Quaternion rotation; // box / capsule orientation
		Vector3 velocity;
		// PART-5: caller-defined stable identity. Async reaps return impulses
		// one frame late, tagged with the id of the collider slot that earned
		// them AT SUBMIT TIME -- the node routes them by this, not by slot
		// index, so a reshuffled collider list between frames can't misroute.
		uint64_t id = 0;
	};
	using SphereCollider = Collider; // transitional alias

	// PART-5: an async-reaped reaction impulse tagged with the collider id
	// that earned it (see Collider::id).
	struct ImpulseHit {
		uint64_t collider_id = 0;
		Vector3 impulse;
	};

	MPMFluidSolver();
	~MPMFluidSolver();

	// A local RenderingDevice and the pipelines came up (independent of whether
	// a particle block has been seeded yet).
	bool has_device() const { return rd != nullptr; }
	// Ready to step: device up and a particle block configured.
	bool is_available() const { return rd != nullptr && built; }

	// (Re)build buffers. `p_prefill` seeds a full jittered block at rest (drop-in-
	// a-tank); otherwise the buffer starts empty and fills via emit() (a faucet).
	// `p_xform` places the domain center in world.
	void configure(const Settings &p_settings, const Transform3D &p_xform, bool p_prefill = true);

	// Move the domain center without reseeding.
	void set_domain_transform(const Transform3D &p_xform);

	// Live isosurface tweak -- no reseed. `p_iso` is a fraction (0..1) of the
	// packed kernel density, `p_kernel` the SPH scatter radius in meters.
	void set_surface_params(float p_iso, float p_kernel, float p_boost) {
		settings.surface_iso = p_iso;
		settings.surface_kernel = p_kernel;
		settings.surface_boost = p_boost;
		_recompute_surface_iso();
	}

	// Runtime foam toggle. The slot allocation was sized at configure() -- a
	// solver configured with foam off has a single parked slot, so toggling on
	// stays inert until the next configure().
	void set_foam_active(bool p_enabled) { settings.foam_enabled = p_enabled; }
	bool is_foam_active() const { return settings.foam_enabled; }

	struct EmittedParticle {
		Vector3 position; // world
		Vector3 velocity;
	};
	// Insert new particles at the ring write head, overwriting the oldest once the
	// buffer is full. No-op in prefill mode's spare capacity is fine too.
	void emit(const LocalVector<EmittedParticle> &p_new);

	// Advance one rendered frame. `r_impulses`, when non-null, is filled with the
	// per-collider reaction impulse accumulated over the frame's substeps (world
	// space, N*s) so the caller can push its rigid bodies.
	// `p_want_surface` also runs the density-smoothing + GPU-march passes so
	// get_surface_mesh() has fresh geometry this frame.
	// `p_async`: submit the compute work and return, reaping it next step (visuals
	// + reaction one frame late, GPU overlapped). Set false to sync before
	// returning -- the fluid coupling needs the reaction impulse in phase.
	void step(double p_delta, const LocalVector<SphereCollider> &p_colliders, LocalVector<ImpulseHit> *r_impulses, bool p_want_surface = false, bool p_async = true);
	// In-place mutable access to the reaped MultiMesh cache (PART-7): the node
	// rewrites origins without a copy-on-write detach.
	PackedFloat32Array &get_multimesh_buffer_mut() { return _mm_cache; }

	int get_particle_count() const { return pcount; } // live particles
	int get_capacity() const { return capacity; } // buffer / MultiMesh size
	double get_last_step_msec() const { return last_step_usec / 1000.0; } // GPU wait for the last step (async: usually near 0)

	// 12 floats / particle: MultiMesh TRANSFORM_3D rows, ready for
	// RenderingServer::multimesh_set_buffer(). Reads back the GPU render buffer.
	PackedFloat32Array get_multimesh_buffer() const;
	PackedVector3Array get_positions() const;

	// GPU-marched isosurface geometry from the last step run with
	// p_want_surface = true. Non-indexed triangle soup, world space; returns the
	// triangle count. Ready for RenderingServer::mesh_add_surface_from_arrays.
	int get_surface_mesh(PackedVector3Array &r_vertices, PackedVector3Array &r_normals) const;

	// Diffuse (foam/spray/bubble) layer, cached from the last reaped step.
	// get_foam_positions(): world-space, alive-only positions (the legacy
	// contract). get_foam_data(): 4 floats per alive particle -- xyz + kind
	// (0 = spray, 1 = foam, 2 = bubble) -- for kind-aware rendering.
	// get_foam_count() is the matching instance count.
	PackedVector3Array get_foam_positions() const;
	PackedFloat32Array get_foam_data() const { return _foam_cache; }
	int get_foam_count() const { return _foam_alive; }

	// Fraction of the fluid's cached particle positions inside `p_world_aabb`,
	// weighted by particle cell volume -- the MPM counterpart of the PBD
	// server's particle_fluid_get_submersion (script-side buoyancy probes).
	float get_submersion(const AABB &p_world_aabb) const;

private:
	enum Pass { PASS_CLEAR,
		PASS_P2G_MASS,
		PASS_P2G_MOM,
		PASS_GRID,
		PASS_COUPLE,
		PASS_G2P,
		PASS_FOAM_SPAWN,
		PASS_FOAM_ADVECT,
		PASS_SURFACE,
		PASS_MARCH,
		PASS_RENDER,
		PASS_MAX };

	RenderingDevice *rd = nullptr;
	bool built = false;

	Settings settings;
	Transform3D domain_xform;

	RID shader[PASS_MAX];
	RID pipeline[PASS_MAX];
	RID uset[PASS_MAX];
	RID uset_mesh; // set 1 for the march pass: vertex / normal / counter buffers

	RID buf_params;
	RID buf_particles;
	RID buf_grid_i;
	RID buf_grid_v;
	RID buf_colliders;
	RID buf_cimp;
	RID buf_mm;
	RID buf_surf;
	RID buf_mverts;
	RID buf_mnorms;
	RID buf_mcount;
	RID buf_foam;
	RID buf_foam_meta;
	RID buf_foam_norm; // decaying running maxima of the four foam potentials (uint-packed floats)

	mutable uint64_t last_step_usec = 0;
	// Async render readback: a step submits its compute work and returns; the
	// next step reaps it (a full frame later, so the sync never stalls) and
	// caches the MultiMesh buffer + collider impulses. Visuals + reaction run one
	// frame behind, which is imperceptible and keeps the GPU overlapped.
	mutable bool _submitted = false;
	mutable int _submitted_ncol = 0;
	mutable PackedFloat32Array _mm_cache;
	mutable LocalVector<ImpulseHit> _imp_cache;
	mutable LocalVector<uint64_t> _submitted_ids; // collider id per submitted slot (PART-5)
	mutable bool _mesh_buffers_ready = false; // PART-4: lazy isosurface buffers
	// Foam readback, same async reap as the render buffer above: 4 floats per
	// alive particle (xyz + kind).
	mutable PackedFloat32Array _foam_cache;
	mutable int _foam_alive = 0;
	int foam_capacity_resolved = 1; // actual foam slot count (>= 1 keeps the buffer bindings valid)
	void _reap_submitted() const;
	void _ensure_mesh_buffers(); // PART-4: lazy isosurface buffer creation
	int pcount = 0; // live particles (== capacity when prefilled)
	int capacity = 0; // particle-buffer slots
	int write_head = 0; // next slot emit() overwrites once full
	int node_count = 0;
	Vector3i grid_dims; // per-axis cell count (domain can be non-cube; dx is uniform)
	float dx = 0.0f;
	float pmass = 0.0f;
	float surf_iso_density = 500.0f; // settings.surface_iso resolved to a kg/m^3 threshold

	bool _compile_shaders();
	void _free_buffers();
	void _compute_scales();
	void _recompute_surface_iso();
	LocalVector<float> _seed_block(int &r_count) const;
	void _pack_params(double p_dt, int p_ncol, PackedByteArray &r_bytes) const;
	void _pack_colliders(const LocalVector<SphereCollider> &p_colliders, PackedByteArray &r_bytes) const;
	bool _rebuild_uniform_sets();
};

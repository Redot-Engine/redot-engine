/**************************************************************************/
/*  physx_gas_3d.h                                                        */
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

#include "core/variant/typed_array.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/gradient.h"
#include "scene/resources/gradient_texture.h"

class GasSolver;
class Mesh;
class FogVolume;
class FogMaterial;
class ImageTexture3D;
class ShaderMaterial;
class Shader;

// A sparse-grid Eulerian smoke/gas volume (velocity + density field on a
// block-hash sparse grid -- see particles/gas_block_inc.glsl for the full
// design writeup and particles/gas_solver.h for the solver). A validated
// proof of concept (see the PhysX module backlog memory), ported into the
// module as a real node for the first time -- still v1 scope: the domain's
// lateral (X/Z) footprint is fixed at first configure (grows automatically
// upward as a plume rises, see GasSolver::_grow_if_needed, but not
// sideways), no pressure projection, and up to 4 sphere colliders. Up to 4
// independent sphere/box emitters via PhysXGasEmitter3D (see emitters).
//
// Renders via a stock Godot FogVolume + FogMaterial: the solver's density
// grid is baked into an ImageTexture3D each step and handed to
// FogMaterial.density_texture, so the actual raymarching, camera-frustum
// froxel integration, temporal reprojection and light/shadow scattering are
// all engine code -- nothing custom here beyond producing the texture. A
// legacy debug point-cloud (MultiMesh, one small cube per grid cell above a
// density threshold) is still available behind debug_point_cloud for
// comparing against the raw grid data.
class PhysXGas3D : public Node3D {
	GDCLASS(PhysXGas3D, Node3D);

	GasSolver *solver = nullptr;

	Vector3 domain_size = Vector3(1.92f, 3.2f, 1.92f); // world size of the fixed box (rounded to whole 4-cell blocks)
	float cell_size = 0.08f;

	// Up to GasSolver::MAX_EMITTERS Node3D paths, each an injection point. A
	// PhysXGasEmitter3D gives its own independent shape/size/velocity/density
	// (see _resolve_emitter); any other Node3D falls back to a sphere using
	// emitter_radius/emitter_velocity/emitter_density -- same fallback
	// pattern as colliders' collider_radius.
	TypedArray<NodePath> emitters;
	float emitter_radius = 0.22f;
	Vector3 emitter_velocity = Vector3(0, 1.4f, 0);
	float emitter_density = 1.0f;
	// Reads a PhysXGasEmitter3D's own properties, or falls back to a sphere
	// using emitter_radius/emitter_velocity/emitter_density (r_divergence/
	// r_swirl always 0 for the fallback -- use a real PhysXGasEmitter3D for
	// burst/swirl authoring). r_shape: 0 = sphere (r_size.x is the radius),
	// 1 = box (r_size is half-extents). Plain out-params, not a
	// GasSolver::Emitter&, so this header doesn't need gas_solver.h.
	void _resolve_emitter(Node3D *p_node, int &r_shape, Vector3 &r_size, Vector3 &r_velocity, float &r_density, float &r_divergence, float &r_swirl) const;

	float buoyancy = 9.0f;
	float vorticity_strength = 8.0f;
	float dissipation = 0.996f;
	// Extra curl-noise perturbation velocity -- see GasSolver::Settings for
	// the full explanation. 0 = off, matches pre-turbulence behavior exactly.
	float turbulence_strength = 0.0f;
	float turbulence_scale = 2.5f;

	// Up to GasSolver::MAX_COLLIDERS Node3D paths; each one resolves to an
	// analytic collider (sphere/box/plane, see _resolve_collider). A
	// CollisionShape3D (or CollisionObject3D) carrying a Sphere/Box/
	// WorldBoundary shape gets its own world-scaled shape; anything else
	// falls back to a sphere using collider_radius.
	TypedArray<NodePath> colliders;
	float collider_radius = 0.0f;
	// Out-params instead of returning GasSolver::Collider directly -- GasSolver
	// is only forward-declared above, so this header doesn't need gas_solver.h
	// (mirrors _resolve_emitter's same reasoning). r_shape: 0=sphere, 1=box,
	// 2=plane.
	void _resolve_collider(Node3D *p_node, int &r_shape, Vector3 &r_position, Vector3 &r_extents) const;

	// Volumetric (FogVolume) render -- the default and primary render path.
	bool volumetric_render = true;
	float fog_density = 24.0f; // FogMaterial.density; our raw density values are roughly 0..1, so this is the visible multiplier -- tuned against a real scene (6.0 read as barely-there haze)
	Color fog_albedo = Color(1, 1, 1);
	FogVolume *fog_volume = nullptr;
	Ref<FogMaterial> fog_material;
	Ref<ImageTexture3D> density_texture;
	Vector<Ref<Image>> slice_images; // persistent per-Z slices (PART-2)
	Vector3i slice_dims;
	uint64_t density_readback_tick = 0;
	int multimesh_cap = 0; // debug cloud instance cap (PART-3)
	Vector3i density_texture_dims; // last size the texture was created at; a growth-driven resize needs a fresh create(), not just update()

	// Fire look: swaps the FogVolume's material for a custom shader that maps
	// the SAME density field through a hot-core-to-smoke colour ramp with
	// emission, instead of stock FogMaterial's flat uniform albedo/no-
	// emission. No solver changes -- density is already highest right at an
	// emitter and falls off outward/upward, so this reads as a cheap "hot
	// core cooling into smoke" hint, good enough for a short-lived
	// explosion/fireball. NOT real combustion (no independent temperature
	// channel, no persistent cooling once density itself has dispersed) --
	// see the backlog memory for that bigger-lift alternative.
	bool fire_look = false;
	// Multi-stop colour ramp sampled by density (see the shader source for
	// the exact mapping) -- alpha doubles as the emission-intensity control
	// at that stop (0 = pure albedo/no glow, 1 = full EMISSION), the same
	// convention GPUParticles3D's own colour ramps use for fade. Sharing
	// this SAME Gradient resource with a GPUParticles3D layer's
	// ParticleProcessMaterial.color_ramp keeps a volumetric fire layer and a
	// particle spark layer visually consistent -- see PhysXGas3D.xml.
	Ref<Gradient> fire_color_ramp;
	Ref<GradientTexture1D> fire_color_ramp_tex; // derived from fire_color_ramp, not exposed
	float fire_emission_strength = 6.0f;
	Ref<ShaderMaterial> fire_material;
	Ref<Shader> fire_shader; // built once, shared by fire_material
	void _ensure_fire_material();
	void _init_default_fire_color_ramp();

	// Legacy debug point cloud -- off by default now that the FogVolume path
	// exists; still useful to sanity-check the raw grid against the shaded
	// volumetric result.
	bool debug_point_cloud = false;
	bool debug_diagnostics = false; // opt-in NaN/rate diagnostics (PART-2)
	float render_threshold = 0.01f;
	Ref<Mesh> cell_mesh;
	RID multimesh;
	RID mm_instance;

	bool configured = false;
	float last_max_density = 0.0f;
	// A live drag in the editor fires NOTIFICATION_TRANSFORM_CHANGED every
	// tick the mouse moves -- reconfiguring (wipes + rebuilds the grid)
	// immediately on each one meant the grid never got to accumulate any
	// density for as long as you were actively dragging (looked completely
	// broken/empty the whole time, not just wrong afterward). Debounced
	// instead: a move only actually reconfigures once it's been RECONFIGURE_
	// SETTLE_SECONDS since the last transform change.
	static constexpr double RECONFIGURE_SETTLE_SECONDS = 0.2;
	bool pending_reconfigure = false;
	double reconfigure_settle_timer = 0.0;

	void _ensure_configured();
	void _step(double p_delta);
	void _update_render();
	void _update_volumetric_render();
	void _update_point_cloud_render();
	void _ensure_fog_volume();
	void _ensure_point_cloud();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	// FogVolume rendering (see _update_volumetric_render) is entirely gated
	// scene-wide by Environment.volumetric_fog_enabled -- if that's off, this
	// node (and every other FogVolume in the scene) renders nothing, with no
	// error. Same check FogVolume itself uses (see fog_volume.cpp).
	PackedStringArray get_configuration_warnings() const override;

	void set_domain_size(const Vector3 &p_size);
	Vector3 get_domain_size() const { return domain_size; }
	// True once the solver has actually configured (locked its world-space
	// domain) -- before that, get_domain_size() at this node's current
	// transform IS where it'll end up, so a gizmo has nothing better to show.
	// After, the domain no longer tracks the node (see GasSolver::configure's
	// note) and a gizmo should show the REAL frozen box instead.
	bool is_domain_configured() const;
	Vector3 get_configured_domain_anchor() const; // world position of the frozen box's min corner
	Vector3 get_configured_domain_size() const; // current world-space size (grows with the plume)
	// Highest density value seen in the last render-update's readback (free --
	// already reading the whole grid back for the FogVolume texture/point
	// cloud every frame). 0 after a reconfigure until the next render update;
	// stays 0 forever if nothing is actually injecting/left over, which is
	// the single most direct way to tell "genuinely empty" apart from "a
	// rendering problem showing nothing despite real density existing."
	float get_last_max_density() const { return last_max_density; }
	void set_cell_size(float p_size);
	float get_cell_size() const { return cell_size; }

	void set_emitters(const TypedArray<NodePath> &p_emitters) { emitters = p_emitters; }
	TypedArray<NodePath> get_emitters() const { return emitters; }
	void set_emitter_radius(float p_r) { emitter_radius = MAX(p_r, 0.001f); }
	float get_emitter_radius() const { return emitter_radius; }
	void set_emitter_velocity(const Vector3 &p_v) { emitter_velocity = p_v; }
	Vector3 get_emitter_velocity() const { return emitter_velocity; }
	void set_emitter_density(float p_d) { emitter_density = MAX(p_d, 0.0f); }
	float get_emitter_density() const { return emitter_density; }

	void set_buoyancy(float p_b) { buoyancy = p_b; }
	float get_buoyancy() const { return buoyancy; }
	void set_vorticity_strength(float p_v) { vorticity_strength = MAX(p_v, 0.0f); }
	float get_vorticity_strength() const { return vorticity_strength; }
	void set_dissipation(float p_d) { dissipation = CLAMP(p_d, 0.0f, 1.0f); }
	float get_dissipation() const { return dissipation; }
	void set_turbulence_strength(float p_s) { turbulence_strength = MAX(p_s, 0.0f); }
	float get_turbulence_strength() const { return turbulence_strength; }
	void set_turbulence_scale(float p_s) { turbulence_scale = MAX(p_s, 0.01f); }
	float get_turbulence_scale() const { return turbulence_scale; }

	void set_colliders(const TypedArray<NodePath> &p_colliders) { colliders = p_colliders; }
	TypedArray<NodePath> get_colliders() const { return colliders; }
	void set_collider_radius(float p_r) { collider_radius = MAX(p_r, 0.0f); }
	float get_collider_radius() const { return collider_radius; }

	void set_volumetric_render(bool p_enabled);
	bool get_volumetric_render() const { return volumetric_render; }
	void set_fog_density(float p_density);
	float get_fog_density() const { return fog_density; }
	void set_fog_albedo(const Color &p_color);
	Color get_fog_albedo() const { return fog_albedo; }
	void set_fire_look(bool p_enabled);
	bool get_fire_look() const { return fire_look; }
	void set_fire_color_ramp(const Ref<Gradient> &p_ramp);
	Ref<Gradient> get_fire_color_ramp() const { return fire_color_ramp; }
	void set_fire_emission_strength(float p_s);
	float get_fire_emission_strength() const { return fire_emission_strength; }

	void set_debug_point_cloud(bool p_enabled);
	void set_debug_diagnostics(bool p_enabled);
	bool get_debug_diagnostics() const;
	// Diagnostics: GPU submits the gas solver issued on its last step
	// (PART-1 validation; 1 = single-submit). 0 without a device.
	int get_solver_step_submits() const;
	bool get_debug_point_cloud() const { return debug_point_cloud; }
	void set_render_threshold(float p_t) { render_threshold = MAX(p_t, 0.0f); }
	float get_render_threshold() const { return render_threshold; }

	PhysXGas3D() {}
	~PhysXGas3D();
};

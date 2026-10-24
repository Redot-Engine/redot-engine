/**************************************************************************/
/*  physx_water_surface_3d.h                                              */
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

#include "water_solver.h"

#include "scene/3d/node_3d.h"

class Mesh;
class MeshInstance3D;
class ShaderMaterial;
class Texture2DRD;
class Texture2D;

// The real water surface node: owns a WaterSolver (ripple + FFT ocean
// layers), renders it via a plain child MeshInstance3D (a subdivided
// PlaneMesh + a spatial ShaderMaterial that samples the solver's height
// fields), and exposes the sample_height()/submit_sphere()/submit_impulse()
// API buoyant_body.gd (or anything else) talks to.
//
// WaterSolver runs on the SHARED main RenderingDevice (dispatched via the
// render thread, same as MPMFluidSolverGPU) and blits its height fields into
// real RD textures every step. Those textures are wrapped here with
// Texture2DRD (a real, already-public Godot class -- see
// scene/resources/texture_rd.h -- no vanilla-engine changes needed) and fed
// straight to the ShaderMaterial: zero-copy, no CPU round trip, the data
// never leaves the GPU. Set once after the solver becomes available, not
// re-created every frame -- Texture2DRD just wraps an RID by reference, so
// whatever the blit shaders write each step is what the material samples
// automatically.
//
// Buoyancy sampling (sample_height()) is a separate, second consumer of the
// same underlying data with very different needs (CPU-side, a few frames of
// staleness is fine, precision doesn't need to match the rendered mesh
// exactly) -- it still goes through the solver's CPU-cached
// get_height_grid()/get_ocean_height_grid() readback, refreshed periodically
// into a local array this node bilinearly samples. Kept deliberately
// separate from the zero-copy texture path above per the project plan's own
// note: two consumers, two different freshness/precision needs, one
// underlying simulation.
class PhysXWaterSurface3D : public Node3D {
	GDCLASS(PhysXWaterSurface3D, Node3D);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	enum NormalMode {
		// Exact normals and foam from the FFT's slopes, per pixel: smooth
		// crests at any mesh density (realistic).
		NORMAL_MODE_PER_PIXEL,
		// Finite differences of the displaced mesh, per vertex: shading
		// follows the mesh (faceted / stylized, and cheaper).
		NORMAL_MODE_PER_VERTEX,
	};

	void set_domain_size(Vector2 p_size);
	Vector2 get_domain_size() const { return domain_size; }
	void set_grid_resolution(int p_n);
	int get_grid_resolution() const { return grid_resolution; }
	void set_depth(float p_depth);
	float get_depth() const { return depth; }
	void set_water_level(float p_level);
	float get_water_level() const { return water_level; }
	void set_damping(float p_damping);
	float get_damping() const { return damping; }
	void set_ripple_amplitude(float p_amplitude);
	float get_ripple_amplitude() const { return ripple_amplitude; }

	void set_ocean_grid_resolution(int p_n);
	int get_ocean_grid_resolution() const { return ocean_grid_resolution; }
	void set_ocean_domain_size(Vector2 p_size);
	Vector2 get_ocean_domain_size() const { return ocean_domain_size; }
	void set_wind_speed(float p_speed);
	float get_wind_speed() const { return wind_speed; }
	void set_wind_direction(Vector2 p_dir);
	Vector2 get_wind_direction() const { return wind_direction; }
	void set_wave_amplitude(float p_amp);
	float get_wave_amplitude() const { return wave_amplitude; }
	// How far the wind has blown over open water, in meters (JONSWAP): sets
	// how tall and long the wind waves get. 0 = auto: the surface_mesh
	// footprint's longest side for enclosed water, a fully developed open sea
	// otherwise.
	void set_fetch(float p_fetch);
	float get_fetch() const { return fetch; }
	// The fetch actually in use (resolves the 0 = auto case).
	float get_effective_fetch() const;
	// Tessendorf horizontal displacement: pulls the surface toward the
	// ocean waves' crests so they sharpen and the troughs flatten. 0 = pure
	// height field. Where it folds the surface (its Jacobian drops toward
	// zero) the default material draws whitecap foam.
	void set_choppiness(float p_choppiness);
	float get_choppiness() const { return choppiness; }
	// Which the built-in (and a compatible custom) material uses; sets its
	// per_pixel_normals uniform.
	void set_normal_mode(NormalMode p_mode);
	NormalMode get_normal_mode() const { return normal_mode; }
	// Whitecap foam: injected where the choppy displacement folds the surface
	// (area Jacobian below foam_threshold -- 1 = unsqueezed, 0 = folding over)
	// and fading with a time constant of foam_persistence seconds, so crests
	// leave streaks behind. Lower threshold = fewer whitecaps.
	// Off = no whitecap foam at all (a pool, a sheltered pond).
	void set_foam_enabled(bool p_enabled);
	bool get_foam_enabled() const { return foam_enabled; }
	void set_foam_threshold(float p_threshold);
	float get_foam_threshold() const { return foam_threshold; }
	void set_foam_persistence(float p_seconds);
	float get_foam_persistence() const { return foam_persistence; }
	// Shore foam: waves arriving over water shallower than this (meters,
	// from the seabed) leave a foamy sheet at the waterline. 0 = none.
	void set_shore_foam_band(float p_depth);
	float get_shore_foam_band() const { return shore_foam_band; }
	// Shore foam moves with the water: each crest pushes it up the beach and
	// the backwash drags it back out, plus this steady seaward undertow (m/s)
	// that slides it back into the water over time.
	void set_shore_undertow(float p_speed);
	float get_shore_undertow() const { return shore_undertow; }
	// Swash: the biggest arriving waves run up the dry sand as a thin film,
	// at most this high (m above still water; 0 = none), then slide back
	// down at swash_drain_speed (m/s, vertical). Sand the water has covered
	// stays dark and glossy, drying over wet_sand_dry_time seconds. Only
	// shows where the depth varies (seabed_from_floor).
	void set_swash_run_up(float p_height);
	float get_swash_run_up() const { return swash_run_up; }
	void set_swash_drain_speed(float p_speed);
	float get_swash_drain_speed() const { return swash_drain_speed; }
	void set_wet_sand_dry_time(float p_seconds);
	float get_wet_sand_dry_time() const { return wet_sand_dry_time; }

	// Optional shape: any flat mesh whose X/Z footprint (node-local) is the
	// water's outline -- a disc, a kidney bean, a lake with an island. When
	// set, the ripple grid is fitted to its bounds (domain_size is ignored),
	// cells outside it are dry land that waves reflect off, and the rendered
	// surface is the footprint resampled into an even grid clipped to its
	// outline. Unset = the square domain_size / ocean_domain_size plane.
	void set_surface_mesh(const Ref<Mesh> &p_mesh);
	Ref<Mesh> get_surface_mesh() const { return surface_mesh; }

	// Seabed: when on, each ripple cell's still-water depth is measured once
	// on the first physics tick by a ray straight down onto static bodies in
	// seabed_collision_mask. Waves then travel at the local shallow-water
	// speed, cells where the seabed is above the water are dry shore, and
	// the ocean chop fades out over the shallows. Off = the constant depth.
	void set_seabed_from_floor(bool p_enabled);
	bool get_seabed_from_floor() const { return seabed_from_floor; }
	void set_seabed_collision_mask(uint32_t p_mask);
	uint32_t get_seabed_collision_mask() const { return seabed_collision_mask; }
	// Water shallower than this fades the ocean chop out, to none at the
	// waterline.
	void set_shallow_fade_depth(float p_depth);
	float get_shallow_fade_depth() const { return shallow_fade_depth; }

	// Side length (m) of the rendered surface, centered on the simulation.
	// Past the simulated square the FFT ocean tiles seamlessly (it's
	// periodic) and the ripples settle to still water, so open water can run
	// to the horizon; vertex spacing grows with distance. 0 = just the
	// simulated square. Ignored with a surface_mesh.
	void set_render_extent(float p_extent);
	float get_render_extent() const { return render_extent; }

	void set_water_material(const Ref<ShaderMaterial> &p_material);
	Ref<ShaderMaterial> get_water_material() const { return water_material; }

	// Light-space caustics: a direct RenderingDevice draw pass that projects
	// refracted surface-grid samples into a texture, without a SubViewport.
	void set_caustics_enabled(bool p_enabled);
	bool get_caustics_enabled() const { return caustics_enabled; }
	void set_caustics_sun_direction(Vector3 p_direction);
	Vector3 get_caustics_sun_direction() const { return caustics_sun_direction; }
	void set_caustics_reference_depth(float p_depth);
	float get_caustics_reference_depth() const { return caustics_reference_depth; }
	// The resulting texture -- assign it to a floor material's albedo/
	// emission using get_caustics_light_right(), get_caustics_light_up(),
	// get_caustics_origin(), and get_caustics_half_extent() as projection
	// metadata. Null until caustics_enabled and the node has built.
	Ref<Texture2D> get_caustics_texture() const;
	Vector3 get_caustics_light_right() const { return solver.get_caustics_light_right(); }
	Vector3 get_caustics_light_up() const { return solver.get_caustics_light_up(); }
	Vector3 get_caustics_origin() const { return solver.get_caustics_origin(); }
	float get_caustics_half_extent() const { return solver.get_caustics_half_extent(); }
	// Period of the caustics (the ocean tile): see WaterSolver::get_caustics_tile_size().
	Vector2 get_caustics_tile_size() const { return solver.get_caustics_tile_size(); }

	// CPU-side, safe every physics tick -- bilinear samples of the last
	// texture refresh (see _refresh_textures()'s throttle), summed across
	// both layers (ripple + ocean), matching what the rendered mesh shows.
	// Returns -INF over dry cells when a surface_mesh is set.
	float sample_height(Vector3 p_world_pos) const;
	// The same, leaving out one wake (a boat's buoyancy reading the water
	// under it shouldn't sink into its own wake's dip).
	float sample_height_excluding_wake(Vector3 p_world_pos, ObjectID p_wake) const;
	// Moving wake grids (PhysXWaterWake3D): up to MAX_WAKES are drawn and
	// sampled. register_wake() returns a slot (-1 when full).
	static constexpr int MAX_WAKES = 4;
	int register_wake(ObjectID p_wake);
	void unregister_wake(int p_slot);
	// The wake's height texture and its rect (xy world xz corner, z side, w
	// shore-grid fade 0..1).
	void set_wake_slot(int p_slot, const Ref<Texture2D> &p_height, const Vector4 &p_rect);
	// False outside the surface_mesh footprint (always true without one).
	bool is_wet(Vector3 p_world_pos) const;

	void submit_sphere(int p_owner, Vector3 p_world_pos, float p_radius, float p_strength = 1.0f);
	void clear_sphere(int p_owner);
	void submit_impulse(Vector3 p_world_pos, float p_radius, float p_strength);

private:
	WaterSolver solver;
	// NOT cached from immediately after configure() -- configure() dispatches
	// asynchronously (a render-thread post, see water_solver.h), so
	// solver.is_available() only flips true a frame or two later. _update()
	// re-checks it fresh every tick instead of trusting a stale snapshot.
	bool warned_no_device = false;

	Vector2 domain_size = Vector2(20, 20);
	int grid_resolution = 96;
	float depth = 3.0f;
	float water_level = 0.0f;
	float damping = 0.08f; // see WaterSolver::Settings::damping's own note on this value
	float ripple_amplitude = 1.0f;

	int ocean_grid_resolution = 64;
	Vector2 ocean_domain_size = Vector2(40, 40);
	float wind_speed = 8.0f;
	Vector2 wind_direction = Vector2(1, 0);
	float wave_amplitude = 1.0f;
	float fetch = 0.0f;
	float render_extent = 0.0f;
	double water_time = 0.0; // shader clock (water_time), advanced only in a running game
	float choppiness = 1.0f;
	NormalMode normal_mode = NORMAL_MODE_PER_PIXEL;
	bool foam_enabled = true;
	float foam_threshold = 0.7f;
	float foam_persistence = 2.5f;
	float shore_foam_band = 0.5f;
	float shore_undertow = 0.3f;
	float swash_run_up = 0.3f;
	float swash_drain_speed = 0.12f;
	float wet_sand_dry_time = 12.0f;
	// Longest side of the surface_mesh footprint (0 = none), for auto fetch.
	real_t footprint_extent = 0.0;

	MeshInstance3D *mesh_instance = nullptr;
	Ref<Mesh> surface_mesh;
	Ref<Mesh> water_mesh; // what mesh_instance draws: a PlaneMesh, or the resampled surface_mesh footprint

	// World XZ the simulation grids are centered on (node origin, plus the
	// footprint's center when surface_mesh is set), and the ripple domain
	// actually in use -- both fixed at _rebuild().
	Vector2 grid_center;
	Vector2 active_domain_size;
	// Wet/dry cells from the surface_mesh footprint (empty = none set), and
	// the still-water depth per ripple cell actually in use (<= 0 = dry;
	// empty = the constant depth everywhere). CPU copies for is_wet() and
	// sample_height().
	PackedByteArray footprint_mask;
	PackedFloat32Array cell_depth;
	// WaterSolver::shallow_fade() of each cell_depth, for sample_height()'s
	// ocean fade (bilinear on this, so a wall cell doesn't drag it down).
	PackedFloat32Array cell_fade;
	void _update_cell_fade();
	bool seabed_from_floor = false;
	uint32_t seabed_collision_mask = 1;
	float shallow_fade_depth = 1.0f;
	// Seabed rays need the floor in the physics space, so with
	// seabed_from_floor the solver is configured on the first physics tick.
	bool seabed_pending = false;
	Ref<ShaderMaterial> water_material;
	ObjectID wakes[MAX_WAKES];
	Vector2 mesh_offset; // see _update_mesh_offset()
	void _update_mesh_offset();
	Ref<Texture2DRD> ripple_height_tex;
	Ref<Texture2DRD> ocean_height_tex;
	Ref<Texture2DRD> ocean_fade_tex;
	Ref<Texture2DRD> shore_depth_tex;
	Ref<Texture2DRD> ocean_disp_tex;
	Ref<Texture2DRD> ocean_deriv_tex;
	Ref<Texture2DRD> ocean_foam_tex;
	Ref<Texture2DRD> shore_foam_tex;
	Ref<Texture2DRD> swash_tex;
	Ref<Texture2DRD> caustics_texture;
	bool textures_bound = false; // set once, after the solver's RD textures actually exist (see _update())

	bool caustics_enabled = true;
	Vector3 caustics_sun_direction = Vector3(-0.35f, -1.0f, -0.25f);
	float caustics_reference_depth = 2.5f;

	Vector<float> cached_ripple_height;
	int cached_ripple_n = 0;
	Vector2 cached_ripple_domain;
	Vector<float> cached_ocean_height;
	Vector<float> cached_ocean_dx;
	Vector<float> cached_ocean_dz;
	int cached_ocean_n = 0;
	Vector2 cached_ocean_domain;

	int frames_since_refresh = 0;
	// CPU cache refresh for sample_height() only -- rendering is unthrottled/automatic via the zero-copy textures.
	// Every tick (upstream 9531b60da5): the readback lands every step anyway and the copy is a COW share, while
	// refreshing every 4th tick froze the heights in between and then jumped them several cm (rough seas), which
	// jolted buoyancy and anything resting on sampled water.
	static constexpr int REFRESH_EVERY_FRAMES = 1;

	void _rebuild();
	// Footprint triangles of surface_mesh in node-local XZ (degenerates dropped).
	Vector<Vector2> _collect_footprint() const;
	void _configure_solver();
	// Casts the seabed rays; fills cell_depth.
	void _sample_seabed();
	static PackedByteArray _rasterize_wet_mask(const Vector<Vector2> &p_tris, Vector2 p_center, Vector2 p_domain, int p_n);
	// Plane over [-extent/2, extent/2]^2: uniform cells over the simulated
	// p_inner size, then spacing growing geometrically outward.
	Ref<Mesh> _build_extended_plane(Vector2 p_inner, int p_inner_cells, real_t p_extent) const;
	Ref<Mesh> _build_footprint_mesh(const Vector<Vector2> &p_tris, const Rect2 &p_bounds, int p_cells) const;
	void _bind_textures();
	void _unbind_all_textures(); // WATER-1
	void _refresh_cpu_cache();
	void _update(double p_delta);
	// p_repeat: wrap around instead of clamping at the edge (the FFT ocean is
	// periodic).
	float _bilinear_sample(const Vector<float> &p_grid, int p_n, Vector2 p_domain, float p_world_x, float p_world_z, bool p_repeat = false) const;
	// The water carries on past the simulated square (render_extent), and
	// whether world XZ (relative to grid_center) is inside what's drawn.
	bool _open_beyond() const;
	bool _inside_render_extent(float p_rel_x, float p_rel_z) const;
};

VARIANT_ENUM_CAST(PhysXWaterSurface3D::NormalMode);

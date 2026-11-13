/**************************************************************************/
/*  physx_destructible_3d.h                                               */
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

#include "physx_blast_asset.h"

#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/material.h"

struct NvBlastAsset;
struct NvBlastFamily;
struct NvBlastActor;
class RenderingServer;
class TriangleMesh;

// Real node wrapping the NvBlast low-level runtime: the full damage ->
// fracture -> split -> spawn-rigid-bodies lifecycle. Lives in blast/ rather
// than nodes/ because it's unconditionally guarded by GODOT_PHYSX_BLAST --
// see SCsub.
//
// While intact, renders/collides as chunk 0 (the whole unfractured asset
// mesh) via one static PhysicsServer3D body. apply_radial_damage() runs the
// real fracture lifecycle; each newly-visible leaf chunk becomes its own
// rigid body with its own cooked convex collision hull and its own
// RenderingServer mesh instance (built directly from that chunk's render-
// mesh triangle soup), synced to the body's live transform every physics
// tick once anything has broken off.
//
// Node3D, not GeometryInstance3D -- tried the GeometryInstance3D route
// first (for the editor's viewport click-to-select and dashed selection-box
// outline, both normally driven through VisualInstance3D::get_aabb()), but
// a real A/B (a live in-editor Play session, profiled: PhysX GPU dynamics
// stayed pegged at 2-3x its normal per-tick cost -- 16-20ms vs 5-7ms at a
// matching piece/draw-call count -- for the entire time any pieces were
// still settling, confirmed via the engine's own CPU Profiler tab, not just
// a one-off spike) showed a real, reproducible, sustained physics-tick cost
// increase from it, even though the base class's own RenderingServer
// instance never receives a mesh and the per-tick code path
// (_sync_transforms(), NOTIFICATION_INTERNAL_PHYSICS_PROCESS) is completely
// unchanged either way -- so whatever the mechanism, it's real. Click-to-
// select is kept (generate_triangle_mesh() + PhysXDestructible3DGizmoPlugin
// registering real per-triangle collision geometry, same as before) since
// that only needs the gizmo system, not VisualInstance3D; the dashed
// selection-box outline is instead drawn manually by that same gizmo (see
// its own redraw()), since Node3D has no get_aabb() for the editor to call
// automatically.
class PhysXDestructible3D : public Node3D {
	GDCLASS(PhysXDestructible3D, Node3D);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_asset_path(const String &p_path);
	String get_asset_path() const { return asset_path; }
	void set_chunks_path(const String &p_path);
	String get_chunks_path() const { return chunks_path; }

	// Preferred over asset_path/chunks_path when set -- a real Resource
	// (PhysXBlastAuthoring::fracture_mesh()'s output, or one loaded from
	// disk) instead of the old two-file convention. asset_path/chunks_path
	// remain supported as a fallback when this isn't set, not deprecated.
	void set_blast_asset(const Ref<PhysXBlastAsset> &p_asset);
	Ref<PhysXBlastAsset> get_blast_asset() const { return blast_asset; }

	void set_material_override(const Ref<Material> &p_material);
	Ref<Material> get_material_override() const { return material_override_res; }

	// Every piece's RenderingServer instance is created directly via
	// instance_create2() (see _spawn_piece()), not through a real
	// GeometryInstance3D node -- so unlike a MeshInstance3D, nothing was
	// ever calling instance_geometry_set_flag(INSTANCE_FLAG_USE_BAKED_LIGHT/
	// USE_DYNAMIC_GI), and a VoxelGI had no way to tell these pieces apart
	// from ordinary invisible-to-GI geometry. Mirrors
	// GeometryInstance3D::GIMode exactly (same enum, same two flags) so it's
	// a drop-in familiar property.
	//
	// Defaults to Static, matching both GeometryInstance3D's own default
	// and -- more importantly -- the RenderingServer Instance's built-in
	// defaults before this property existed at all (baked_light = true,
	// dynamic_gi = false; see renderer_scene_cull.h). Dynamic would make
	// more physical sense for something that's often about to go tumbling
	// as debris, but a single fracture can spawn a couple dozen pieces at
	// once and a VoxelGI's dynamic-object tracking cost scales with how many
	// of those it has to follow every frame -- opting a pile of debris into
	// that by default risked being expensive well before anyone asked for
	// it, and silently changing existing scenes' behavior underneath them.
	// Opt into Dynamic per-node instead.
	enum GIMode {
		GI_MODE_DISABLED,
		GI_MODE_STATIC,
		GI_MODE_DYNAMIC,
	};
	void set_gi_mode(GIMode p_mode);
	GIMode get_gi_mode() const { return gi_mode; }

	// The rest of these mirror GeometryInstance3D's own equivalent
	// properties (same names/values/RenderingServer calls) for the exact
	// same reason gi_mode does -- this class isn't one, so nothing else
	// ever applied them to any piece's real instance. Each pushes straight
	// to every existing piece on set, same as material_override/gi_mode.
	enum ShadowCastingSetting {
		SHADOW_CASTING_SETTING_OFF,
		SHADOW_CASTING_SETTING_ON,
		SHADOW_CASTING_SETTING_DOUBLE_SIDED,
		SHADOW_CASTING_SETTING_SHADOWS_ONLY,
	};
	void set_cast_shadow(ShadowCastingSetting p_setting);
	ShadowCastingSetting get_cast_shadow() const { return cast_shadow; }

	void set_transparency(float p_transparency);
	float get_transparency() const { return transparency; }

	void set_material_overlay(const Ref<Material> &p_material);
	Ref<Material> get_material_overlay() const { return material_overlay_res; }

	void set_extra_cull_margin(float p_margin);
	float get_extra_cull_margin() const { return extra_cull_margin; }

	void set_lod_bias(float p_bias);
	float get_lod_bias() const { return lod_bias; }

	void set_ignore_occlusion_culling(bool p_ignore);
	bool get_ignore_occlusion_culling() const { return ignore_occlusion_culling; }

	enum VisibilityRangeFadeMode {
		VISIBILITY_RANGE_FADE_DISABLED,
		VISIBILITY_RANGE_FADE_SELF,
		VISIBILITY_RANGE_FADE_DEPENDENCIES,
	};
	void set_visibility_range_begin(float p_distance);
	float get_visibility_range_begin() const { return visibility_range_begin; }
	void set_visibility_range_end(float p_distance);
	float get_visibility_range_end() const { return visibility_range_end; }
	void set_visibility_range_begin_margin(float p_distance);
	float get_visibility_range_begin_margin() const { return visibility_range_begin_margin; }
	void set_visibility_range_end_margin(float p_distance);
	float get_visibility_range_end_margin() const { return visibility_range_end_margin; }
	void set_visibility_range_fade_mode(VisibilityRangeFadeMode p_mode);
	VisibilityRangeFadeMode get_visibility_range_fade_mode() const { return visibility_range_fade_mode; }

	void set_shatter_speed(float p_speed) { shatter_speed = p_speed; }
	float get_shatter_speed() const { return shatter_speed; }

	// Same meaning as RigidBody3D/StaticBody3D/Area3D's own collision_layer/
	// collision_mask -- this class had neither at all until now, so every
	// body it created (intact placeholder or split-off piece) just used
	// whatever PhysicsServer3D's own flat default happened to be, with zero
	// user control. Applies uniformly to every body this class creates;
	// unlike material_override/gi_mode (which this class's own setters
	// already re-push to every existing piece too, see those setters'
	// notes), these are just as immediate.
	void set_collision_layer(uint32_t p_layer);
	uint32_t get_collision_layer() const { return collision_layer; }
	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_mask() const { return collision_mask; }

	// The whole intact object's mass (same meaning as RigidBody3D.mass).
	// Every piece used to get PhysicsServer3D's own flat default (1.0)
	// regardless of size -- checked how Unreal's own Blast integration
	// handles this (BlastMeshComponent.cpp) and it distributes mass across
	// split pieces proportional to each one's actual volume, conserving the
	// intact object's total mass; this does the same (see
	// _compute_chunk_volumes()/_chunk_mass()) rather than giving a huge
	// remaining chunk and a tiny sliver the same weight.
	//
	// mass is auto-computed (a fixed internal density, not its own exposed
	// property -- neither Godot nor Unreal actually expose raw density as a
	// per-component number; Unreal's lives on a shared PhysicalMaterial
	// asset, and Godot has no density concept anywhere) from the intact
	// mesh's real volume while auto_mass is true (the default) -- read-only
	// in the Inspector then, so it reads as "here's what the object would
	// weigh," not an editable field. Uncheck auto_mass for a true override:
	// mass becomes a plain editable value, exactly what you set it to,
	// until you check auto_mass again (which recomputes fresh and locks it
	// back to read-only) -- see set_auto_mass()'s own note on the one real
	// bug this went through before landing here (the Inspector not
	// noticing auto_mass had changed).
	// A script calling this directly (not through the Inspector, which is
	// read-only whenever auto_mass is true and so can't reach this in the
	// first place) still counts as an explicit override -- turns auto_mass
	// off too, so it can't get silently overwritten by a later reload.
	void set_mass(float p_mass);
	float get_mass() const { return mass; }

	// See set_mass()'s note. Calls notify_property_list_changed() --
	// without it, the Inspector never re-checks whether `mass` should still
	// be read-only after this toggles, so unchecking auto_mass looked like
	// it did nothing (the field stayed visually grayed out even though the
	// underlying state was correct) -- the actual bug in the first version
	// of this, not the read-only-while-auto design itself.
	void set_auto_mass(bool p_auto);
	bool get_auto_mass() const { return auto_mass; }

	// If true, the intact piece is a real dynamic (BODY_MODE_RIGID) body --
	// it falls under gravity and collides normally, like any other physics
	// object, instead of hanging in place until something explicitly calls
	// apply_radial_damage() on it. Also enables impact_strength/
	// impact_damage_scale: a hard enough collision auto-triggers fracture,
	// matching stacked destructible props in UE's Blast integration. False
	// keeps the static-intact-placeholder behavior (fracture only ever
	// explicit).
	void set_dynamic(bool p_dynamic) { dynamic = p_dynamic; }
	bool get_dynamic() const { return dynamic; }

	// Initial NvBlast bond/chunk health (both, uniformly) -- how much
	// cumulative damage the support structure can absorb before bonds start
	// breaking. Higher health needs more damage (a harder impact, or more of
	// them) to fracture the same object.
	void set_health(float p_health) { health = p_health; }
	float get_health() const { return health; }

	// dynamic-only: the minimum contact impulse magnitude (mass * velocity
	// change, in Godot's physics units) that counts as an impact at all --
	// below this, collisions (landing softly, gentle bumps) are ignored.
	void set_impact_strength(float p_strength) { impact_strength = p_strength; }
	float get_impact_strength() const { return impact_strength; }

	// Static destructibles only (dynamic ones burst into kicked debris):
	// which pieces of a fractured wall stay put instead of falling. FRAMED
	// keeps every piece still touching the intact mesh's side/bottom/top
	// bounds (a wall built into something holds its place), GROUNDED keeps
	// only bottom-touching pieces (a freestanding wall drops its upper half
	// as a rigid, kick-less chunk), NONE drops everything.
	enum Anchor {
		ANCHOR_FRAMED,
		ANCHOR_GROUNDED,
		ANCHOR_NONE,
	};
	void set_anchor(Anchor p_anchor) { anchor = p_anchor; }
	Anchor get_anchor() const { return anchor; }

	// dynamic-only: damage fed into apply_radial_damage() per unit of
	// impulse magnitude above impact_strength -- how readily an impact that
	// does exceed the threshold actually breaks bonds.
	void set_impact_damage_scale(float p_scale) { impact_damage_scale = p_scale; }
	float get_impact_damage_scale() const { return impact_damage_scale; }

	// dynamic-only: max_radius passed to the auto-triggered apply_radial_damage
	// call, centered on the contact point -- how far a hard impact's damage
	// reaches into the rest of the object.
	void set_impact_radius(float p_radius) { impact_radius = p_radius; }
	float get_impact_radius() const { return impact_radius; }

	// A piece (fractured debris, or the intact body if dynamic) whose world Y
	// falls below this is freed automatically -- bounds memory/body count for
	// a scene where debris can fall indefinitely (no floor everywhere, a
	// gap, missing the pile entirely) instead of accumulating forever.
	// Default is low enough not to trigger on an ordinary scene; set it to
	// match this scene's actual floor/void.
	void set_kill_y(float p_y) { kill_y = p_y; }
	float get_kill_y() const { return kill_y; }

	// Debris clean-up (see the members' own comment): all off by default.
	// get_piece_velocity: per-piece linear velocity diagnostics (like the
	// vehicle stack's get_wheel_jounce); index order is stable within a
	// frame only. Count comes from the existing get_piece_count().
	Vector3 get_piece_velocity(int p_index) const;

	void set_debris_lifetime(float p_seconds) { debris_lifetime = MAX(p_seconds, 0.0f); }
	float get_debris_lifetime() const { return debris_lifetime; }
	void set_debris_shrink(float p_seconds) { debris_shrink = MAX(p_seconds, 0.0f); }
	float get_debris_shrink() const { return debris_shrink; }
	void set_debris_freeze(bool p_freeze) { debris_freeze = p_freeze; }
	bool get_debris_freeze() const { return debris_freeze; }
	// True once a hit or damage has broken anything off (stays true; agrees
	// with the chunks_fractured signal).
	bool is_fractured() const { return fractured; }

	// p_world_position: world-space damage origin (converted to this node's
	// local space internally, since chunk geometry is authored local-space).
	// Returns how many new rigid-body pieces this call produced. Each new
	// piece gets a real outward velocity -- radial direction from the damage
	// origin to that piece's own centroid, scaled by shatter_speed and the
	// same distance falloff the damage itself uses, plus a slight upward
	// bias -- computed here so it's intrinsic to fracturing rather than a
	// separate demo-script concern layered on top.
	int apply_radial_damage(const Vector3 &p_world_position, float p_damage, float p_min_radius, float p_max_radius);

	// (Signal "chunks_fractured", declared via ADD_SIGNAL in _bind_methods:
	// emitted at the end of apply_radial_damage() when real chunks broke off
	// -- never for a no-op hit. Args: world_position (damage origin),
	// pieces_spawned, damage. This is the module's engine-level destruction
	// event: the Flow bridge consumes it for dust/smoke, and scripts may
	// use it directly.)

	PackedStringArray get_configuration_warnings() const override;

	// Always chunk 0's (the whole intact mesh's) local-space bounds,
	// regardless of `fractured` -- chunk_points[0] stays loaded even after
	// splitting, and it's the one stable, meaningful "size of this object"
	// an editor tool (selection box, scale gizmo, click-to-select) could
	// want; once genuinely fractured, pieces are scattered independently
	// under live physics, where no single local-space box would mean much
	// and these editor-only tools aren't in play anyway (nothing is
	// mid-explosion while you're editing it in the Inspector). Not an
	// override of anything (this class isn't a VisualInstance3D) -- called
	// directly by PhysXDestructible3DGizmoPlugin::redraw() to draw the
	// selection-box outline manually, since Node3D has no get_aabb() the
	// editor would call on its own.
	AABB get_aabb() const;

	// Real per-triangle collision geometry (chunk 0's, same scope as
	// get_aabb()'s own note) for the editor's own gizmo plugin
	// (PhysXDestructible3DGizmoPlugin, physx_editor_plugin.cpp) to register
	// via EditorNode3DGizmo::add_collision_triangles() -- exactly the same
	// mechanism MeshInstance3DGizmoPlugin uses
	// (mesh->generate_triangle_mesh()) for real per-triangle viewport
	// click-to-select, not just a loose bounding-box hit test. Not an
	// override either, same reason as get_aabb() above.
	Ref<TriangleMesh> generate_triangle_mesh() const;

	/// True once the Blast asset parsed, a family was created and the first
	/// actor exists. False after a failed load (e.g. corrupt/truncated asset
	/// bytes) — the node then renders nothing and cannot be damaged.
	bool is_loaded() const { return loaded; }

	// Live-piece diagnostics (tests, tooling). A piece is one spawned chunk
	// body (or its render-only editor counterpart).
	int get_piece_count() const { return (int)pieces.size(); }
	// World transform of piece p_index's body; identity for a render-only
	// (editor) piece or an out-of-range index.
	Transform3D get_piece_transform(int p_index) const;
	// Which authored chunk piece p_index renders/simulates; UINT32_MAX when
	// the index is out of range.
	uint32_t get_piece_chunk(int p_index) const;

	PhysXDestructible3D();
	~PhysXDestructible3D();

private:
	String asset_path;
	String chunks_path;
	Ref<PhysXBlastAsset> blast_asset;
	Ref<Material> material_override_res;
	GIMode gi_mode = GI_MODE_STATIC;
	ShadowCastingSetting cast_shadow = SHADOW_CASTING_SETTING_ON;
	float transparency = 0.0f;
	Ref<Material> material_overlay_res;
	float extra_cull_margin = 0.0f;
	float lod_bias = 1.0f;
	bool ignore_occlusion_culling = false;
	float visibility_range_begin = 0.0f;
	float visibility_range_end = 0.0f;
	float visibility_range_begin_margin = 0.0f;
	float visibility_range_end_margin = 0.0f;
	VisibilityRangeFadeMode visibility_range_fade_mode = VISIBILITY_RANGE_FADE_DISABLED;
	float shatter_speed = 8.0f;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	float mass = 1.0f;
	bool auto_mass = true;
	bool dynamic = false;
	Anchor anchor = ANCHOR_FRAMED;
	float health = 1.0f;
	float impact_strength = 5.0f;
	float impact_damage_scale = 1.0f;
	float impact_radius = 5.0f;
	float kill_y = -1000.0f;

	void *asset_mem = nullptr;
	void *family_mem = nullptr;
	NvBlastFamily *family = nullptr;
	LocalVector<NvBlastActor *> live_actors;
	uint32_t asset_chunk_count = 0;
	uint32_t asset_bond_count = 0;

	// chunk_points[chunk_index] = that chunk's render-mesh triangle-soup
	// positions (object-local space).
	LocalVector<PackedVector3Array> chunk_points;

	// chunk_volumes[chunk_index] = that chunk's volume (object-local space,
	// so no transform/scale applied); total_leaf_volume = sum of every leaf
	// chunk's volume (excludes chunk 0, the whole unfractured mesh). Both
	// computed once in _compute_chunk_volumes(), called from _load(). See
	// set_mass() for why.
	LocalVector<double> chunk_volumes;
	double total_leaf_volume = 0.0;

	// Per-chunk touching-face bits (ANCHOR_FACE_*), computed once per load
	// against chunk 0's (the intact mesh's) bounds with a 2% tolerance.
	LocalVector<uint8_t> chunk_faces;

	struct ChunkVisual {
		RID body;
		RID shape;
		RID mesh;
		RID instance;
		uint32_t chunk_index = 0; // which authored chunk this piece renders/simulates
		// The piece's velocity as of the end of the previous physics tick --
		// i.e. BEFORE the step whose contacts trigger an impact fracture. A
		// split hands these to its children: the live velocity already holds
		// that impact's push, which would fling far-away chunks with it.
		Vector3 last_linear_velocity;
		Vector3 last_angular_velocity;
		// Debris lifecycle (debris_lifetime/debris_shrink/debris_freeze): the
		// piece_clock when the piece spawned, whether it has been frozen
		// (static) on sleep, and whether it is shrinking toward expiry.
		double born = 0.0;
		bool frozen = false;
		bool shrinking = false;
		// Anchored-island chunk: its hull lives on the SHARED static body and
		// piece.body is RID() -- such a piece has no direct body state to sync
		// and must be exempt from the debris lifecycle (freeing it would free
		// the shared island body out from under the other chunks).
		bool anchored = false;
		// True only for the intact placeholder piece spawned by _spawn_intact
		// (see PhysXDestructible3D::placeholder_alive).
		bool placeholder = false;
		// The live NvBlast actor this piece stands for (the intact placeholder
		// records live_actors[0]; see _spawn_intact). Lets the kill_y cleanup
		// retire an actor once none of its chunks has a live piece left (see
		// _actor_has_live_piece).
		NvBlastActor *actor = nullptr;
	};
	LocalVector<ChunkVisual> pieces;
	// Pieces currently spawned per live NvBlast actor (by chunk index). When
	// an actor splits, its recorded pieces are retired before the successor
	// actors re-spawn their own visible chunks — otherwise an interior/support
	// chunk that stays visible across the split gets a second, duplicate
	// piece (BLAST-1; reachable with multi-depth assets).
	HashMap<NvBlastActor *, LocalVector<uint32_t>> actor_pieces;

	// generate_triangle_mesh() cooks chunk 0's triangle soup into a
	// TriangleMesh; the destructible gizmo registers it for click-to-select
	// on every editor redraw. Cook once per load instead of per redraw
	// (EDIT-3); invalidated wherever chunk_points changes.
	mutable Ref<TriangleMesh> cached_collision_mesh;

	bool loaded = false;
	bool fractured = false;
	// The intact placeholder piece (chunk 0, spawned by _spawn_intact) is
	// still alive in `pieces`: apply_radial_damage() retires exactly that
	// piece when the first split commits. Kill-floor drops and partial-load
	// failures clear this WITHOUT setting fractured, so the placeholder
	// removal can never touch a real fragment.
	bool placeholder_alive = false;
	// Debris clean-up, all off by default: a piece is removed debris_lifetime
	// seconds after it spawned (0 = never), shrinking to nothing over the
	// last debris_shrink seconds of them instead of popping out;
	// debris_freeze makes a piece static once PhysX has put it to sleep (it
	// stays, solid, but never moves or wakes again). Advanced in
	// NOTIFICATION_INTERNAL_PHYSICS_PROCESS; pieces' birth times use it.
	float debris_lifetime = 0.0f;
	float debris_shrink = 0.0f;
	bool debris_freeze = false;
	double piece_clock = 0.0;

	// PxTransform (a physics body's pose) never carries scale -- confirmed
	// directly: a real repro with a (20, 10, 1) scaled destructible showed
	// the node's own global_transform basis reduced to identity after just
	// a few physics ticks. Every place that pushes a physics-read transform
	// onto the node or a piece's RenderingServer instance (the dynamic
	// intact body's own sync, and _sync_transforms() for fractured debris)
	// only has a scale-free rotation to work with at that point -- this is
	// the scale to re-apply on top of it, captured fresh whenever the
	// intact piece is (re)spawned or its transform is pushed by something
	// other than physics (see _spawn_intact()/NOTIFICATION_TRANSFORM_CHANGED),
	// i.e. always from a moment before physics has had any chance to
	// overwrite it.
	Vector3 spawn_scale = Vector3(1, 1, 1);

	bool _load();
	bool _load_asset_bytes(const PackedByteArray &p_bytes);
	// Frees the native Blast state (family, actors, asset block) and all
	// pieces. Shared by the destructor, _reload() and the failure paths of
	// _load_asset_bytes() — a failed load must leave nothing behind, or a
	// retry (world re-entry) accumulates (BLAST-2).
	void _unload_native_state();
	// Retires every piece recorded for p_actor (see actor_pieces).
	void _free_pieces_of_actor(NvBlastActor *p_actor);
	// True if at least one of p_actor's recorded chunks still has a live
	// piece. A fractured actor whose pieces were all freed (kill_y) is dead:
	// damaging it again would split it and re-spawn its chunks as ghosts at a
	// stale pose, so it gets deactivated instead.
	bool _actor_has_live_piece(NvBlastActor *p_actor) const;
	// Removes p_actor from the live set and deactivates it in the family
	// (freeing its family slot); also drops its actor_pieces record.
	void _retire_actor(NvBlastActor *p_actor);
	void _compute_chunk_volumes();
	void _compute_chunk_faces();
	bool _island_anchored(const LocalVector<uint32_t> &p_chunks) const;
	// mass distributed proportional to p_chunk_index's share of
	// total_leaf_volume (floored so a sliver never gets a near-zero mass),
	// except chunk 0 (the whole intact mesh), which just gets `mass` itself.
	float _chunk_mass(uint32_t p_chunk_index) const;
	// Tears down any existing family/pieces and, if already inside the world,
	// immediately re-loads and re-spawns from whatever asset_path/chunks_path/
	// blast_asset now point at -- called from those setters so assigning a
	// new asset (or dragging one onto an empty node already in the tree)
	// shows up right away instead of only on the next time the node enters
	// the tree (e.g. reopening the scene).
	void _reload();
	void _spawn_intact();
	// p_physics: create a PhysicsServer3D body+shape for this piece too, not
	// just its RenderingServer mesh instance. False in the editor (no physics
	// simulation runs there anyway) so the node still shows *something* in
	// the viewport -- ChunkVisual::body/shape stay RID() in that case.
	// p_actor: the live NvBlast actor this piece stands for, recorded for
	// kill_y retirement (see _actor_has_live_piece).
	void _spawn_piece(uint32_t p_chunk_index, const Transform3D &p_transform, const Vector3 &p_linear_velocity, bool p_physics = true, NvBlastActor *p_actor = nullptr,
			PhysicsServer3D::BodyMode p_body_mode = PhysicsServer3D::BODY_MODE_RIGID, RID p_attach_body = RID());
	// See set_gi_mode()'s own note on why this is needed at all.
	void _apply_gi_mode(RenderingServer *p_rs, RID p_instance) const;
	// Same idea as _apply_gi_mode(), covering the rest of the
	// GeometryInstance3D-equivalent properties added above (cast_shadow,
	// transparency, material_overlay, extra_cull_margin, lod_bias,
	// ignore_occlusion_culling, visibility_range_*) in one place rather than
	// seven near-identical one-off pushes.
	void _apply_extra_render_settings(RenderingServer *p_rs, RID p_instance) const;
	// The visibility_range_* setters each only change one of five values
	// that all get pushed together in a single RenderingServer call --
	// shared so none of them has to repeat the other four.
	void _apply_visibility_range(RenderingServer *p_rs, RID p_instance) const;
	Vector3 _chunk_centroid_local(uint32_t p_chunk_index) const;
	void _free_all_pieces();
	void _sync_transforms();
	// Each piece's live velocity as its last_* (end of a physics tick).
	void _record_piece_velocities();
	// dynamic-only, called every physics tick while still intact: reads the
	// intact body's reported contacts and auto-triggers apply_radial_damage()
	// if any single contact's impulse exceeds impact_strength.
	void _check_impact_fracture();
};

VARIANT_ENUM_CAST(PhysXDestructible3D::Anchor);
VARIANT_ENUM_CAST(PhysXDestructible3D::GIMode);
VARIANT_ENUM_CAST(PhysXDestructible3D::ShadowCastingSetting);
VARIANT_ENUM_CAST(PhysXDestructible3D::VisibilityRangeFadeMode);

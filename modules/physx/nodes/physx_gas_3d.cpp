/**************************************************************************/
/*  physx_gas_3d.cpp                                                      */
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

#include "physx_gas_3d.h"

// PART-2: density readback + texture upload throttle (per physics tick).
// Volumetric fog reads fine at 30 Hz; the readback is the dominant cost.
static constexpr int GAS_DENSITY_READBACK_EVERY = 2;

#include "../particles/gas_solver.h"
#include "physx_gas_emitter_3d.h"

#include "core/io/image.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/3d/fog_volume.h"
#include "scene/3d/physics/collision_object_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/fog_material.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/3d/world_boundary_shape_3d.h"
#include "scene/resources/environment.h"
#include "scene/resources/gradient.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include "servers/rendering/rendering_server.h"

namespace {
// Maps the SAME density field a stock FogMaterial would render through a
// hot-core-to-smoke colour ramp with emission, instead of a flat uniform
// albedo -- see PhysXGas3D::fire_look's header comment for why this is
// "good enough" without a real second temperature channel: density is
// already highest right at an emitter and falls off outward/upward, so
// ramping colour by density alone reads as a hot core cooling into smoke.
const char *GAS_FIRE_SHADER_SOURCE = R"(
shader_type fog;

uniform sampler3D density_tex;
uniform float density_scale = 24.0;
// Raw density isn't normalized to 0..1 (an emitter's own "density" property
// can be set arbitrarily, e.g. 3.0 for a strong burst) -- this is "what raw
// density value counts as fully hot," so the colour ramp below stays
// correctly shaped regardless of how a scene's emitters are tuned, instead
// of hardcoding a 0..1 assumption that washes out at higher densities.
uniform float density_norm = 2.0;
// A full artist-authored multi-stop ramp (PhysXGas3D.fire_color_ramp) instead
// of a hardcoded 2-threshold mix -- sampled along its length by normalized
// density. Alpha at each stop doubles as the emission-intensity control (0 =
// pure albedo/no glow, 1 = full EMISSION) -- same convention GPUParticles3D's
// own colour ramps use for fade, which is also what lets the SAME Gradient
// resource be shared with a GPUParticles3D spark layer's
// ParticleProcessMaterial.color_ramp and stay visually consistent.
uniform sampler2D color_ramp_tex : source_color;
uniform float emission_strength = 6.0;

void fog() {
	float d = texture(density_tex, UVW).r;
	// Same edge-falloff shape stock FogMaterial uses (see fog_material.cpp) so
	// the box's boundary doesn't show a hard cutoff -- SDF is a raw signed
	// distance (negative inside), not a ready-made 0..1 factor.
	float edge = pow(clamp(-2.0 * SDF / min(min(SIZE.x, SIZE.y), SIZE.z), 0.0, 1.0), 1.0);
	DENSITY = d * density_scale * edge;
	float dn = clamp(d / max(density_norm, 0.0001), 0.0, 1.0);
	vec4 ramp = texture(color_ramp_tex, vec2(dn, 0.5));
	ALBEDO = ramp.rgb;
	EMISSION = ramp.rgb * ramp.a * emission_strength;
}
)";
} //namespace

PhysXGas3D::~PhysXGas3D() {
	if (solver != nullptr) {
		memdelete(solver);
		solver = nullptr;
	}
}

void PhysXGas3D::set_domain_size(const Vector3 &p_size) {
	domain_size = p_size.maxf(0.1f);
	configured = false; // reconfigure next tick
}

bool PhysXGas3D::is_domain_configured() const {
	return solver != nullptr && solver->is_configured();
}

Vector3 PhysXGas3D::get_configured_domain_anchor() const {
	return solver != nullptr ? solver->get_grid_anchor() : Vector3();
}

Vector3 PhysXGas3D::get_configured_domain_size() const {
	return solver != nullptr ? solver->get_world_size() : domain_size;
}

void PhysXGas3D::set_cell_size(float p_size) {
	cell_size = MAX(p_size, 0.005f);
	configured = false;
}

void PhysXGas3D::set_volumetric_render(bool p_enabled) {
	volumetric_render = p_enabled;
	if (!volumetric_render && fog_volume != nullptr) {
		fog_volume->queue_free();
		fog_volume = nullptr;
		density_texture.unref();
		density_texture_dims = Vector3i();
	}
}

void PhysXGas3D::set_fog_density(float p_density) {
	fog_density = MAX(p_density, 0.0f);
	if (fog_material.is_valid()) {
		fog_material->set_density(fog_density);
	}
	if (fire_material.is_valid()) {
		fire_material->set_shader_parameter("density_scale", fog_density);
	}
}

void PhysXGas3D::set_fog_albedo(const Color &p_color) {
	fog_albedo = p_color;
	if (fog_material.is_valid()) {
		fog_material->set_albedo(fog_albedo);
	}
}

void PhysXGas3D::set_fire_look(bool p_enabled) {
	fire_look = p_enabled;
	if (fog_volume == nullptr) {
		return; // picked up next _ensure_fog_volume()
	}
	if (fire_look) {
		_ensure_fire_material();
		fog_volume->set_material(fire_material);
	} else if (fog_material.is_valid()) {
		fog_volume->set_material(fog_material);
	}
}

void PhysXGas3D::set_fire_color_ramp(const Ref<Gradient> &p_ramp) {
	fire_color_ramp = p_ramp;
	if (fire_color_ramp.is_valid()) {
		if (fire_color_ramp_tex.is_null()) {
			fire_color_ramp_tex.instantiate();
		}
		fire_color_ramp_tex->set_gradient(fire_color_ramp);
	} else {
		fire_color_ramp_tex.unref();
	}
	if (fire_material.is_valid()) {
		fire_material->set_shader_parameter("color_ramp_tex", fire_color_ramp_tex);
	}
}

void PhysXGas3D::set_fire_emission_strength(float p_s) {
	fire_emission_strength = MAX(p_s, 0.0f);
	if (fire_material.is_valid()) {
		fire_material->set_shader_parameter("emission_strength", fire_emission_strength);
	}
}

// Matches the old hardcoded ramp's look (smoke-grey -> orange -> warm
// yellow-white) so fire_look's default appearance is unchanged -- alpha
// ramps 0->1 across the same span, standing in for the old HOT_THRESHOLD..1.0
// emission-intensity range.
void PhysXGas3D::_init_default_fire_color_ramp() {
	fire_color_ramp.instantiate();
	fire_color_ramp->set_offsets({ 0.0f, 0.3f, 1.0f });
	fire_color_ramp->set_colors({
			Color(0.85f, 0.85f, 0.9f, 0.0f),
			Color(1.0f, 0.5f, 0.12f, 0.0f),
			Color(1.0f, 0.85f, 0.5f, 1.0f),
	});
}

void PhysXGas3D::_ensure_fire_material() {
	if (fire_material.is_valid()) {
		return;
	}
	if (fire_shader.is_null()) {
		fire_shader.instantiate();
		fire_shader->set_code(GAS_FIRE_SHADER_SOURCE);
	}
	if (fire_color_ramp.is_null()) {
		_init_default_fire_color_ramp();
	}
	if (fire_color_ramp_tex.is_null()) {
		fire_color_ramp_tex.instantiate();
		fire_color_ramp_tex->set_gradient(fire_color_ramp);
	}
	fire_material.instantiate();
	fire_material->set_shader(fire_shader);
	fire_material->set_shader_parameter("density_scale", fog_density);
	fire_material->set_shader_parameter("color_ramp_tex", fire_color_ramp_tex);
	fire_material->set_shader_parameter("emission_strength", fire_emission_strength);
	if (density_texture.is_valid()) {
		fire_material->set_shader_parameter("density_tex", density_texture);
	}
}

void PhysXGas3D::set_debug_diagnostics(bool p_enabled) {
	debug_diagnostics = p_enabled;
}

bool PhysXGas3D::get_debug_diagnostics() const {
	return debug_diagnostics;
}

int PhysXGas3D::get_solver_step_submits() const {
	return solver != nullptr ? solver->get_last_step_submits() : 0;
}

void PhysXGas3D::set_debug_point_cloud(bool p_enabled) {
	debug_point_cloud = p_enabled;
	if (!debug_point_cloud && multimesh.is_valid()) {
		RenderingServer *rs = RenderingServer::get_singleton();
		if (mm_instance.is_valid()) {
			rs->free(mm_instance);
			mm_instance = RID();
		}
		rs->free(multimesh);
		multimesh = RID();
	}
}

void PhysXGas3D::_ensure_configured() {
	if (configured || solver == nullptr) {
		return;
	}
	// Round the domain up to whole 4-cell blocks, at least one block per axis.
	Vector3i box_blocks(
			MAX(1, (int)Math::ceil(domain_size.x / (cell_size * 4.0f))),
			MAX(1, (int)Math::ceil(domain_size.y / (cell_size * 4.0f))),
			MAX(1, (int)Math::ceil(domain_size.z / (cell_size * 4.0f))));

	GasSolver::Settings s;
	s.box_blocks = box_blocks;
	s.cell_size = cell_size;
	s.buoyancy = buoyancy;
	s.vorticity_strength = vorticity_strength;
	s.dissipation = dissipation;
	s.turbulence_strength = turbulence_strength;
	s.turbulence_scale = turbulence_scale;

	solver->configure(s, get_global_transform());
	configured = true;
}

// A CollisionShape3D (or CollisionObject3D with one) carrying a
// Sphere/Box/WorldBoundary(plane) shape gives its own analytic collider;
// anything else (a plain Node3D, or a shape this collider doesn't understand
// -- capsule/etc.) falls back to a sphere using the shared collider_radius.
// Mirrors MPMFluidSolver's Collider catalog, minus rotation (axis-aligned box
// only) -- a roof/floor/wall is exactly the case a rotation-free box/plane
// already covers, so that's not a real loss for this solver's use cases.
void PhysXGas3D::_resolve_collider(Node3D *p_node, int &r_shape, Vector3 &r_position, Vector3 &r_extents) const {
	Ref<Shape3D> shape;
	if (CollisionShape3D *cshape = Object::cast_to<CollisionShape3D>(p_node)) {
		shape = cshape->get_shape();
	} else if (CollisionObject3D *co = Object::cast_to<CollisionObject3D>(p_node)) {
		List<uint32_t> owners;
		co->get_shape_owners(&owners);
		if (owners.size() > 0) {
			const uint32_t owner = owners.front()->get();
			if (co->shape_owner_get_shape_count(owner) > 0) {
				shape = co->shape_owner_get_shape(owner, 0);
			}
		}
	}
	const Transform3D xform = p_node->get_global_transform();
	if (Ref<WorldBoundaryShape3D> plane_shape = shape; plane_shape.is_valid()) {
		const Plane world_plane = xform.xform(plane_shape->get_plane());
		r_shape = 2; // COLLIDER_PLANE
		r_position = world_plane.get_center();
		r_extents = world_plane.normal;
		return;
	}
	if (Ref<BoxShape3D> box_shape = shape; box_shape.is_valid()) {
		const Vector3 scale = xform.basis.get_scale().abs();
		r_shape = 1; // COLLIDER_BOX
		r_position = xform.origin;
		r_extents = (box_shape->get_size() * 0.5) * scale;
		return;
	}
	if (Ref<SphereShape3D> sphere_shape = shape; sphere_shape.is_valid()) {
		const Vector3 scale = xform.basis.get_scale().abs();
		const float uniform_scale = MAX(scale.x, MAX(scale.y, scale.z));
		r_shape = 0; // COLLIDER_SPHERE
		r_position = xform.origin;
		r_extents = Vector3(MAX(sphere_shape->get_radius() * uniform_scale, 0.01f), 0, 0);
		return;
	}
	r_shape = 0; // COLLIDER_SPHERE
	r_position = xform.origin;
	r_extents = Vector3(collider_radius, 0, 0);
}

// A PhysXGasEmitter3D gives its own independent shape/size/velocity/density;
// any other Node3D falls back to a sphere using emitter_radius/
// emitter_velocity/emitter_density -- same fallback pattern as
// _resolve_collider_radius. Velocity is rotated (not scaled) by the node's
// own basis so a PhysXGasEmitter3D's "up" can be aimed by orienting it.
void PhysXGas3D::_resolve_emitter(Node3D *p_node, int &r_shape, Vector3 &r_size, Vector3 &r_velocity, float &r_density, float &r_divergence, float &r_swirl) const {
	if (PhysXGasEmitter3D *e = Object::cast_to<PhysXGasEmitter3D>(p_node)) {
		r_shape = (int)e->get_shape();
		r_size = e->get_shape() == PhysXGasEmitter3D::SHAPE_BOX ? e->get_size() * 0.5f : Vector3(e->get_radius(), e->get_radius(), e->get_radius());
		r_velocity = e->get_global_transform().basis.get_rotation_quaternion().xform(e->get_velocity());
		r_density = e->get_density();
		r_divergence = e->get_divergence();
		r_swirl = e->get_swirl();
		return;
	}
	r_shape = 0; // sphere
	r_size = Vector3(emitter_radius, emitter_radius, emitter_radius);
	r_velocity = emitter_velocity;
	r_density = emitter_density;
	r_divergence = 0.0f;
	r_swirl = 0.0f;
}

void PhysXGas3D::_step(double p_delta) {
	if (solver == nullptr || !solver->is_available()) {
		return;
	}
	if (pending_reconfigure) {
		reconfigure_settle_timer -= p_delta;
		if (reconfigure_settle_timer <= 0.0) {
			pending_reconfigure = false;
			// A move alone doesn't need domain_size/cell_size's full
			// free+rebuild (see GasSolver::reposition's header comment for
			// why that path specifically is worth avoiding) -- just re-lock
			// the existing grid at the new position. configured stays true;
			// _ensure_configured() below only fires on an actual size change.
			solver->reposition(get_global_transform());
		}
	}
	_ensure_configured();

	Vector<GasSolver::Collider> resolved_colliders;
	for (int i = 0; i < colliders.size() && i < GasSolver::MAX_COLLIDERS; i++) {
		Node3D *n = Object::cast_to<Node3D>(get_node_or_null(colliders[i]));
		if (n == nullptr) {
			continue;
		}
		int shape = 0;
		Vector3 position, extents;
		_resolve_collider(n, shape, position, extents);
		GasSolver::Collider c;
		c.shape = (GasSolver::ColliderShape)shape;
		c.position = position;
		c.extents = extents;
		resolved_colliders.push_back(c);
	}
	solver->set_colliders(resolved_colliders);

	Vector<GasSolver::Emitter> resolved_emitters;
	for (int i = 0; i < emitters.size() && resolved_emitters.size() < GasSolver::MAX_EMITTERS; i++) {
		Node3D *n = Object::cast_to<Node3D>(get_node_or_null(emitters[i]));
		if (n == nullptr) {
			continue;
		}
		if (PhysXGasEmitter3D *ge = Object::cast_to<PhysXGasEmitter3D>(n)) {
			if (!ge->is_enabled()) {
				continue;
			}
		}
		GasSolver::Emitter em;
		em.enabled = true;
		em.world_position = n->get_global_position();
		int shape = 0;
		_resolve_emitter(n, shape, em.size, em.velocity, em.density, em.divergence, em.swirl);
		em.shape = shape == 1 ? GasSolver::EMITTER_BOX : GasSolver::EMITTER_SPHERE;
		resolved_emitters.push_back(em);
	}
	solver->set_emitters(resolved_emitters);

	solver->step(p_delta, get_global_transform());
	_update_render();
}

void PhysXGas3D::_update_render() {
	if (solver == nullptr) {
		return;
	}
	if (volumetric_render) {
		_ensure_fog_volume();
		_update_volumetric_render();
	}
	if (debug_point_cloud) {
		_ensure_point_cloud();
		_update_point_cloud_render();
	}
}

void PhysXGas3D::_ensure_fog_volume() {
	if (fog_volume != nullptr) {
		return;
	}
	fog_material.instantiate();
	fog_material->set_density(fog_density);
	fog_material->set_albedo(fog_albedo);

	fog_volume = memnew(FogVolume);
	fog_volume->set_shape(RS::FOG_VOLUME_SHAPE_BOX);
	if (fire_look) {
		_ensure_fire_material();
		fog_volume->set_material(fire_material);
	} else {
		fog_volume->set_material(fog_material);
	}
	// The solver's box is frozen in WORLD space at configure() (see
	// GasSolver::configure / _grow_if_needed) and does not follow this
	// node's transform -- top_level keeps the FogVolume's own position/size
	// in world space too, set directly from grid_anchor each update instead
	// of composing with this node's (possibly since-moved) transform.
	fog_volume->set_as_top_level(true);
	add_child(fog_volume, false, INTERNAL_MODE_BACK);
}

void PhysXGas3D::_update_volumetric_render() {
	// PART-2 readback throttle: the density readback (whole current domain,
	// several MB) runs at half the physics rate -- volumetric fog visuals do
	// not read at 60 Hz, and the readback is the dominant CPU cost of this
	// path. last_max_density and the texture simply lag one extra tick.
	density_readback_tick++;
	if (density_readback_tick % GAS_DENSITY_READBACK_EVERY != 0) {
		return;
	}

	Vector<float> density;
	Vector3i dims;
	Vector3 anchor;
	float grid_cell_size = cell_size;
	solver->get_density_grid(density, dims, anchor, grid_cell_size);
	if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0) {
		return;
	}

	// NaN diagnostics are opt-in (debug_diagnostics): an unconditional
	// print_line here spammed the console EVERY TICK once a NaN appeared.
	last_max_density = 0.0f;
	int nan_count = 0;
	for (int i = 0; i < density.size(); i++) {
		if (!Math::is_finite(density[i])) {
			nan_count++;
			continue;
		}
		last_max_density = MAX(last_max_density, density[i]);
	}
	if (nan_count > 0 && debug_diagnostics) {
		WARN_PRINT_ONCE(vformat("[gas-diag] %d/%d cells are NaN/Inf!", nan_count, density.size()));
	}

	// One Image slice per Z layer (dims.x * dims.y each) -- ImageTexture3D's
	// native layout. The slice Images are PERSISTENT (rebuilt only when the
	// dims change): allocating dims.z Images + byte vectors every tick was
	// pure per-frame churn.
	if (slice_images.size() != dims.z || slice_dims != dims) {
		slice_images.clear();
		slice_images.resize(dims.z);
		slice_dims = dims;
	}
	const int slice_floats = dims.x * dims.y;
	for (int z = 0; z < dims.z; z++) {
		const uint8_t *src = (const uint8_t *)(density.ptr() + z * slice_floats);
		if (slice_images.write[z].is_null() || slice_images[z]->get_data().size() != slice_floats * (int)sizeof(float)) {
			Vector<uint8_t> bytes;
			bytes.resize(slice_floats * (int)sizeof(float));
			memcpy(bytes.ptrw(), src, bytes.size());
			slice_images.write[z] = Image::create_from_data(dims.x, dims.y, false, Image::FORMAT_RF, bytes);
		} else {
			memcpy(slice_images.write[z]->get_data().ptrw(), src, slice_floats * sizeof(float));
		}
	}

	if (density_texture.is_null() || density_texture_dims != dims) {
		density_texture.instantiate();
		density_texture->create(Image::FORMAT_RF, dims.x, dims.y, dims.z, false, slice_images);
		density_texture_dims = dims;
		fog_material->set_density_texture(density_texture);
		if (fire_material.is_valid()) {
			fire_material->set_shader_parameter("density_tex", density_texture);
		}
	} else {
		density_texture->update(slice_images);
	}

	const Vector3 world_size = Vector3(dims) * grid_cell_size;
	fog_volume->set_size(world_size);
	fog_volume->set_position(anchor + world_size * 0.5f);
}

void PhysXGas3D::_ensure_point_cloud() {
	if (multimesh.is_valid()) {
		return;
	}
	Ref<BoxMesh> box;
	box.instantiate();
	box->set_size(Vector3(1, 1, 1) * (cell_size * 0.9f));
	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	mat->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	box->surface_set_material(0, mat);
	cell_mesh = box;

	RenderingServer *rs = RenderingServer::get_singleton();
	// Sized to the ACTUAL configured domain (PART-3): the old fixed 128^3 =
	// 2,097,152-instance cap allocated ~134 MB of instance buffer for a debug
	// view regardless of the real domain (a 6x10x6-block box needs ~14k
	// instances). Grows if the domain's vertical growth pass expands it.
	int cap = 1 << 16; // fallback when the solver is unreachable
	if (solver != nullptr) {
		cap = MAX(solver->get_domain_cells(), 1);
	}
	multimesh = rs->multimesh_create();
	rs->multimesh_allocate_data(multimesh, cap, RS::MULTIMESH_TRANSFORM_3D, true);
	multimesh_cap = cap;
	rs->multimesh_set_mesh(multimesh, cell_mesh->get_rid());
	rs->multimesh_set_visible_instances(multimesh, 0);

	// World-space positions are written directly into the instance
	// transforms (see _update_point_cloud_render), so the RS instance itself
	// never moves -- same pattern as the fluid node's foam layer.
	mm_instance = rs->instance_create2(multimesh, get_world_3d()->get_scenario());
	rs->instance_set_transform(mm_instance, Transform3D());
	rs->instance_set_custom_aabb(mm_instance, AABB(Vector3(-100000, -100000, -100000), Vector3(200000, 200000, 200000)));
}

void PhysXGas3D::_update_point_cloud_render() {
	if (multimesh.is_null()) {
		return;
	}
	Vector<Vector3> positions;
	Vector<float> density;
	solver->get_render_cells(render_threshold, positions, density);

	RenderingServer *rs = RenderingServer::get_singleton();
	// Grow the instance buffer if the domain grew (vertical growth pass).
	const int domain_cells = solver != nullptr ? MAX(solver->get_domain_cells(), 1) : 0;
	if (domain_cells > multimesh_cap) {
		rs->multimesh_allocate_data(multimesh, domain_cells, RS::MULTIMESH_TRANSFORM_3D, true);
		multimesh_cap = domain_cells;
	}
	const int n = MIN((int)positions.size(), multimesh_cap);
	// Packed buffer upload (PART-3): one multimesh_set_buffer call instead of
	// 2 RS calls PER CELL per tick. Layout for MULTIMESH_TRANSFORM_3D + color:
	// 12 floats basis+origin, then 4 floats color.
	PackedFloat32Array buffer;
	buffer.resize(n * 16);
	float *w = buffer.ptrw();
	for (int i = 0; i < n; i++) {
		float *t = w + i * 16;
		t[0] = 1;
		t[1] = 0;
		t[2] = 0;
		t[3] = 0;
		t[4] = 1;
		t[5] = 0;
		t[6] = 0;
		t[7] = 0;
		t[8] = 1;
		t[9] = positions[i].x;
		t[10] = positions[i].y;
		t[11] = positions[i].z;
		const float a = CLAMP(density[i], 0.0f, 1.0f);
		// Dim-blue -> white heat-ish ramp so the density gradient actually
		// reads (kept from the per-instance version).
		t[12] = 0.5f + 0.5f * a;
		t[13] = 0.6f + 0.4f * a;
		t[14] = 1.0f;
		t[15] = CLAMP(a * 1.4f, 0.0f, 1.0f);
	}
	rs->multimesh_set_buffer(multimesh, buffer);
	rs->multimesh_set_visible_instances(multimesh, n);
}

void PhysXGas3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			if (solver == nullptr) {
				solver = memnew(GasSolver);
			}
			// The FogVolume and the point-cloud MultiMesh are both created
			// lazily, in _update_render, gated on volumetric_render /
			// debug_point_cloud -- no point paying for either until the
			// corresponding render path is actually enabled.
			set_physics_process_internal(true);
			set_notify_transform(true); // for NOTIFICATION_TRANSFORM_CHANGED below
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			// The domain freezes in world space at configure() and does NOT
			// track this node afterward (see GasSolver::configure's own
			// note) -- without this, moving the node after the very first
			// physics tick would do nothing at all, making it useless as an
			// authoring/placement tool the moment the scene has ever run
			// once. Debounced (see RECONFIGURE_SETTLE_SECONDS) rather than
			// reconfiguring immediately -- a live editor drag fires this
			// every tick the mouse moves, and wiping the grid on every one
			// of those left it permanently empty for as long as you were
			// actively dragging. Only debounced once actually configured --
			// before that there's nothing to wipe, so the very first
			// configure (on entering the tree) stays immediate.
			if (configured) {
				pending_reconfigure = true;
				reconfigure_settle_timer = RECONFIGURE_SETTLE_SECONDS;
			}
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			set_physics_process_internal(false);
			if (mm_instance.is_valid()) {
				RenderingServer::get_singleton()->free(mm_instance);
				mm_instance = RID();
			}
			if (multimesh.is_valid()) {
				RenderingServer::get_singleton()->free(multimesh);
				multimesh = RID();
			}
			if (fog_volume != nullptr) {
				fog_volume->queue_free();
				fog_volume = nullptr;
			}
			density_texture.unref();
			density_texture_dims = Vector3i();
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			_step(get_physics_process_delta_time());
		} break;
	}
}

PackedStringArray PhysXGas3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	if (!volumetric_render) {
		return warnings;
	}
	if (OS::get_singleton()->get_current_rendering_method() != "forward_plus") {
		warnings.push_back(RTR("PhysXGas3D's volumetric render needs the Forward+ renderer (it draws through a FogVolume)."));
		return warnings;
	}
	if (is_inside_tree() && get_viewport() != nullptr && get_viewport()->find_world_3d().is_valid()) {
		Ref<Environment> environment = get_viewport()->find_world_3d()->get_environment();
		if (environment.is_valid() && !environment->is_volumetric_fog_enabled()) {
			warnings.push_back(RTR("PhysXGas3D needs volumetric fog enabled in the scene's Environment to be visible (it renders through a FogVolume) -- otherwise it silently draws nothing. Turn off Volumetric Render to use the debug point cloud instead."));
		}
	}
	return warnings;
}

void PhysXGas3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_domain_size", "size"), &PhysXGas3D::set_domain_size);
	ClassDB::bind_method(D_METHOD("get_domain_size"), &PhysXGas3D::get_domain_size);
	ClassDB::bind_method(D_METHOD("is_domain_configured"), &PhysXGas3D::is_domain_configured);
	ClassDB::bind_method(D_METHOD("get_configured_domain_anchor"), &PhysXGas3D::get_configured_domain_anchor);
	ClassDB::bind_method(D_METHOD("get_configured_domain_size"), &PhysXGas3D::get_configured_domain_size);
	ClassDB::bind_method(D_METHOD("get_last_max_density"), &PhysXGas3D::get_last_max_density);
	ClassDB::bind_method(D_METHOD("set_cell_size", "size"), &PhysXGas3D::set_cell_size);
	ClassDB::bind_method(D_METHOD("get_cell_size"), &PhysXGas3D::get_cell_size);
	ClassDB::bind_method(D_METHOD("set_emitters", "paths"), &PhysXGas3D::set_emitters);
	ClassDB::bind_method(D_METHOD("get_emitters"), &PhysXGas3D::get_emitters);
	ClassDB::bind_method(D_METHOD("set_emitter_radius", "radius"), &PhysXGas3D::set_emitter_radius);
	ClassDB::bind_method(D_METHOD("get_emitter_radius"), &PhysXGas3D::get_emitter_radius);
	ClassDB::bind_method(D_METHOD("set_emitter_velocity", "velocity"), &PhysXGas3D::set_emitter_velocity);
	ClassDB::bind_method(D_METHOD("get_emitter_velocity"), &PhysXGas3D::get_emitter_velocity);
	ClassDB::bind_method(D_METHOD("set_emitter_density", "density"), &PhysXGas3D::set_emitter_density);
	ClassDB::bind_method(D_METHOD("get_emitter_density"), &PhysXGas3D::get_emitter_density);
	ClassDB::bind_method(D_METHOD("set_buoyancy", "buoyancy"), &PhysXGas3D::set_buoyancy);
	ClassDB::bind_method(D_METHOD("get_buoyancy"), &PhysXGas3D::get_buoyancy);
	ClassDB::bind_method(D_METHOD("set_vorticity_strength", "strength"), &PhysXGas3D::set_vorticity_strength);
	ClassDB::bind_method(D_METHOD("get_vorticity_strength"), &PhysXGas3D::get_vorticity_strength);
	ClassDB::bind_method(D_METHOD("set_dissipation", "dissipation"), &PhysXGas3D::set_dissipation);
	ClassDB::bind_method(D_METHOD("get_dissipation"), &PhysXGas3D::get_dissipation);
	ClassDB::bind_method(D_METHOD("set_turbulence_strength", "strength"), &PhysXGas3D::set_turbulence_strength);
	ClassDB::bind_method(D_METHOD("get_turbulence_strength"), &PhysXGas3D::get_turbulence_strength);
	ClassDB::bind_method(D_METHOD("set_turbulence_scale", "scale"), &PhysXGas3D::set_turbulence_scale);
	ClassDB::bind_method(D_METHOD("get_turbulence_scale"), &PhysXGas3D::get_turbulence_scale);
	ClassDB::bind_method(D_METHOD("set_colliders", "paths"), &PhysXGas3D::set_colliders);
	ClassDB::bind_method(D_METHOD("get_colliders"), &PhysXGas3D::get_colliders);
	ClassDB::bind_method(D_METHOD("set_collider_radius", "radius"), &PhysXGas3D::set_collider_radius);
	ClassDB::bind_method(D_METHOD("get_collider_radius"), &PhysXGas3D::get_collider_radius);
	ClassDB::bind_method(D_METHOD("set_volumetric_render", "enabled"), &PhysXGas3D::set_volumetric_render);
	ClassDB::bind_method(D_METHOD("get_volumetric_render"), &PhysXGas3D::get_volumetric_render);
	ClassDB::bind_method(D_METHOD("set_fog_density", "density"), &PhysXGas3D::set_fog_density);
	ClassDB::bind_method(D_METHOD("get_fog_density"), &PhysXGas3D::get_fog_density);
	ClassDB::bind_method(D_METHOD("set_fog_albedo", "color"), &PhysXGas3D::set_fog_albedo);
	ClassDB::bind_method(D_METHOD("get_fog_albedo"), &PhysXGas3D::get_fog_albedo);
	ClassDB::bind_method(D_METHOD("set_fire_look", "enabled"), &PhysXGas3D::set_fire_look);
	ClassDB::bind_method(D_METHOD("get_fire_look"), &PhysXGas3D::get_fire_look);
	ClassDB::bind_method(D_METHOD("set_fire_color_ramp", "ramp"), &PhysXGas3D::set_fire_color_ramp);
	ClassDB::bind_method(D_METHOD("get_fire_color_ramp"), &PhysXGas3D::get_fire_color_ramp);
	ClassDB::bind_method(D_METHOD("set_fire_emission_strength", "strength"), &PhysXGas3D::set_fire_emission_strength);
	ClassDB::bind_method(D_METHOD("get_fire_emission_strength"), &PhysXGas3D::get_fire_emission_strength);
	ClassDB::bind_method(D_METHOD("set_debug_point_cloud", "enabled"), &PhysXGas3D::set_debug_point_cloud);
	ClassDB::bind_method(D_METHOD("get_debug_point_cloud"), &PhysXGas3D::get_debug_point_cloud);
	ClassDB::bind_method(D_METHOD("set_debug_diagnostics", "enabled"), &PhysXGas3D::set_debug_diagnostics);
	ClassDB::bind_method(D_METHOD("get_debug_diagnostics"), &PhysXGas3D::get_debug_diagnostics);
	ClassDB::bind_method(D_METHOD("get_solver_step_submits"), &PhysXGas3D::get_solver_step_submits);
	ClassDB::bind_method(D_METHOD("set_render_threshold", "threshold"), &PhysXGas3D::set_render_threshold);
	ClassDB::bind_method(D_METHOD("get_render_threshold"), &PhysXGas3D::get_render_threshold);

	ADD_GROUP("Domain", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "domain_size", PROPERTY_HINT_NONE, "suffix:m"), "set_domain_size", "get_domain_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size", PROPERTY_HINT_RANGE, "0.01,0.5,0.005,suffix:m"), "set_cell_size", "get_cell_size");
	ADD_GROUP("Emitters", "emitter");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "emitters", PROPERTY_HINT_ARRAY_TYPE, "NodePath"), "set_emitters", "get_emitters");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emitter_radius", PROPERTY_HINT_RANGE, "0.01,2.0,0.01,suffix:m"), "set_emitter_radius", "get_emitter_radius");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emitter_velocity", PROPERTY_HINT_NONE, "suffix:m/s"), "set_emitter_velocity", "get_emitter_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emitter_density", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_emitter_density", "get_emitter_density");
	ADD_GROUP("Motion", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "buoyancy", PROPERTY_HINT_RANGE, "-20.0,20.0,0.1"), "set_buoyancy", "get_buoyancy");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vorticity_strength", PROPERTY_HINT_RANGE, "0.0,30.0,0.1"), "set_vorticity_strength", "get_vorticity_strength");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "dissipation", PROPERTY_HINT_RANGE, "0.9,1.0,0.0005"), "set_dissipation", "get_dissipation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turbulence_strength", PROPERTY_HINT_RANGE, "0.0,8.0,0.05,suffix:m/s"), "set_turbulence_strength", "get_turbulence_strength");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turbulence_scale", PROPERTY_HINT_RANGE, "0.2,10.0,0.1"), "set_turbulence_scale", "get_turbulence_scale");
	ADD_GROUP("Collider", "collider");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "colliders", PROPERTY_HINT_ARRAY_TYPE, "NodePath"), "set_colliders", "get_colliders");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collider_radius", PROPERTY_HINT_RANGE, "0.0,3.0,0.01,suffix:m"), "set_collider_radius", "get_collider_radius");
	ADD_GROUP("Rendering", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "volumetric_render"), "set_volumetric_render", "get_volumetric_render");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_density", PROPERTY_HINT_RANGE, "0.0,32.0,0.1"), "set_fog_density", "get_fog_density");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "fog_albedo"), "set_fog_albedo", "get_fog_albedo");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fire_look"), "set_fire_look", "get_fire_look");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fire_color_ramp", PROPERTY_HINT_RESOURCE_TYPE, "Gradient"), "set_fire_color_ramp", "get_fire_color_ramp");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fire_emission_strength", PROPERTY_HINT_RANGE, "0.0,20.0,0.1"), "set_fire_emission_strength", "get_fire_emission_strength");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_point_cloud"), "set_debug_point_cloud", "get_debug_point_cloud");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_diagnostics"), "set_debug_diagnostics", "get_debug_diagnostics");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "render_threshold", PROPERTY_HINT_RANGE, "0.0,1.0,0.005"), "set_render_threshold", "get_render_threshold");
}

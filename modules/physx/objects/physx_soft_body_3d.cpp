/**************************************************************************/
/*  physx_soft_body_3d.cpp                                                */
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

/**
 * @file physx_soft_body_3d.cpp
 * @brief Soft body implementation: GPU PxDeformableVolume or CPU XPBD solver.
 */

#include "physx_soft_body_3d.h"

#include "../physx_project_settings.h"
#include "../physx_server.h"
#include "../spaces/physx_direct_space_state_3d.h"
#include "../spaces/physx_space_3d.h"
#include "physx_body_3d.h"

#include "core/config/project_settings.h"
#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "servers/physics_3d/physics_server_3d.h"
#include "servers/rendering/rendering_server.h"

PhysXSoftBody3D::~PhysXSoftBody3D() {
	set_space(nullptr);
	if (volume) {
		memdelete(volume);
	}
	if (collision_sphere.is_valid()) {
		PhysicsServer3D::get_singleton()->free(collision_sphere);
	}
}

void PhysXSoftBody3D::set_space(PhysXSpace3D *p_space) {
	if (space == p_space) {
		return;
	}
	if (space) {
		space->unregister_soft_body(this);
	}
	// The GPU volume is bound to a scene; drop it on space change and let the
	// next set_mesh / rebuild decide the path again.
	if (volume) {
		point_attachments.clear(); // volume dtor releases the attachments
		memdelete(volume);
		volume = nullptr;
		using_gpu = false;
	}
	space = p_space;
	if (space) {
		space->register_soft_body(this);
		if (mesh_ready) {
			_rebuild_from_mesh();
		}
	}
}

void PhysXSoftBody3D::notify_space_destroyed() {
	space = nullptr;
	if (volume) {
		point_attachments.clear(); // volume dtor releases the attachments
		memdelete(volume);
		volume = nullptr;
		using_gpu = false;
	}
}

void PhysXSoftBody3D::_apply_solver_settings() {
	if (using_gpu && volume) {
		volume->apply_params(_gpu_params());
		return;
	}
	XPBDClothSolver::Settings s = solver.settings;
	s.substeps = MAX(simulation_precision, 1);
	// linear_stiffness in [0,1] -> stretch compliance: 1 is rigid (0 compliance),
	// lower is springier. Shear/bend ride along, softer.
	const float k = CLAMP((float)linear_stiffness, 0.0f, 1.0f);
	s.stretch_compliance = (1.0f - k) * 1.0e-3f;
	s.shear_compliance = MAX(s.stretch_compliance, 2.0e-4f);
	s.bend_compliance = 1.0e-3f + (1.0f - k) * 4.0e-3f;
	s.damping = MAX((float)damping_coefficient, 0.0f);
	s.drag = MAX((float)drag_coefficient, 0.0f);
	s.lift = 0.0f;
	// Cap per-vertex speed so a fast body can't outrun the once-per-frame
	// contact refresh and tunnel through thin geometry.
	s.max_speed = 12.0f;
	// pressure_coefficient (Godot: ~0..100, 0 = off) preserves the rest volume;
	// larger values just make that preservation stiffer, not bigger.
	if (pressure_coefficient > CMP_EPSILON) {
		s.pressure = 1.0f; // hold the rest volume
		s.pressure_stiffness = CLAMP((float)pressure_coefficient / 60.0f, 0.05f, 1.0f);
	} else {
		s.pressure = 0.0f;
	}
	if (mesh_ready) {
		const float area = solver.rest_surface_area();
		s.density = area > CMP_EPSILON ? MAX((float)total_mass, 0.001f) / area : 0.2f;
	}
	solver.settings = s;
}

void PhysXSoftBody3D::_rebuild_from_mesh(bool p_keep_state) {
	const bool was_running = p_keep_state && mesh_ready && placed;
	Vector<Vector3> prev_positions;
	Vector<Vector3> prev_velocities;
	if (was_running) {
		const LocalVector<Vector3> &sp = solver.get_positions();
		const LocalVector<Vector3> &sv = solver.get_velocities();
		prev_positions.resize(sp.size());
		prev_velocities.resize(sv.size());
		for (uint32_t i = 0; i < sp.size(); i++) {
			prev_positions.write[i] = sp[i];
			prev_velocities.write[i] = sv[i];
		}
	}

	mesh_ready = false;
	map_visual_to_physics.clear();
	visual_vertex_count = 0;
	solver.clear();
	if (volume) {
		point_attachments.clear(); // volume dtor releases the attachments
		memdelete(volume);
		volume = nullptr;
	}
	using_gpu = false;

	if (mesh.is_null()) {
		return;
	}

	Array arrays = RenderingServer::get_singleton()->mesh_surface_get_arrays(mesh, 0);
	if (arrays.is_empty()) {
		return;
	}
	const Vector<Vector3> vertices = arrays[RS::ARRAY_VERTEX];
	const Vector<int> indices = arrays[RS::ARRAY_INDEX];
	ERR_FAIL_COND_MSG(vertices.is_empty(), "Soft body mesh needs vertices.");
	ERR_FAIL_COND_MSG(indices.is_empty(), "Soft body mesh needs indices.");

	visual_vertex_count = vertices.size();
	map_visual_to_physics.resize(visual_vertex_count);

	// Weld coincident render vertices (UV/normal seams) to one solver vertex.
	HashMap<Vector3, uint32_t> unique;
	PackedVector3Array welded;
	for (uint32_t i = 0; i < visual_vertex_count; i++) {
		const Vector3 &v = vertices[i];
		HashMap<Vector3, uint32_t>::Iterator e = unique.find(v);
		uint32_t id;
		if (e) {
			id = e->value;
		} else {
			id = welded.size();
			unique[v] = id;
			welded.push_back(v);
		}
		map_visual_to_physics[i] = id;
	}

	PackedInt32Array welded_indices;
	welded_indices.resize(indices.size());
	for (int i = 0; i < indices.size(); i++) {
		welded_indices.write[i] = (int32_t)map_visual_to_physics[indices[i]];
	}

	// GPU path first (per-body Auto): a PxDeformableVolume if the mesh cooks and
	// CUDA is up. Only once the node has handed us its world transform -- a
	// volume is a live scene actor the moment it's added, so building it at the
	// origin would have it eject off the floor. Before that, fall through to the
	// CPU solver (which is gated in step()); set_transform() rebuilds.
	if (placed && _try_build_gpu(welded, welded_indices)) {
		using_gpu = true;
		mesh_ready = true;
		_sync_gpu_pins(); // re-apply any pins set before the volume existed
		return;
	}

	solver.build_mesh(welded, welded_indices);
	if (!solver.is_built()) {
		return;
	}
	mesh_ready = true;
	normals.resize(solver.vertex_count());

	// Re-apply pins requested before the mesh was available (scene load order,
	// deferred pin_point calls) -- matches the other backends' behavior.
	for (const int rp : pinned_render_points) {
		if (rp >= 0 && rp < (int)visual_vertex_count) {
			solver.set_pinned((int)map_visual_to_physics[rp], true);
		}
	}

	_apply_solver_settings();
	solver.set_rest_length_scale(1.0f - CLAMP((float)shrinking_factor, 0.0f, 0.99f));
	solver.reset(transform);
	if (was_running && prev_positions.size() == solver.vertex_count()) {
		// Same geometry, just a mesh-handle swap -- carry the live state over.
		LocalVector<Vector3> &p = solver.positions_mut();
		LocalVector<Vector3> &v = solver.velocities_mut();
		for (int i = 0; i < prev_positions.size(); i++) {
			p[i] = prev_positions[i];
			v[i] = prev_velocities[i];
		}
	}
	_update_normals_and_bounds();
}

PhysXSoftVolume3D::Params PhysXSoftBody3D::_gpu_params() const {
	PhysXSoftVolume3D::Params p;
	const float k = CLAMP((float)linear_stiffness, 0.0f, 1.0f);
	p.youngs_modulus = Math::lerp(2.0e4f, 5.0e7f, k * k);
	// pressure_coefficient (0..~100) -> Poisson ratio toward the 0.49 near-
	// incompressible limit; 0 leaves it a soft, compressible blob.
	p.poisson_ratio = CLAMP(0.30f + (float)pressure_coefficient / 100.0f * 0.19f, 0.05f, 0.49f);
	p.damping = MAX((float)damping_coefficient, 0.0f);
	p.dynamic_friction = 0.5f;
	p.total_mass = MAX((float)total_mass, 0.001f);
	p.solver_iterations = CLAMP(simulation_precision, 4, 60);
	p.max_speed = 30.0f;
	p.collision_layer = collision_layer;
	p.collision_mask = collision_mask;
	p.exception_slot = exception_slot;
	return p;
}

bool PhysXSoftBody3D::_try_build_gpu(const PackedVector3Array &p_welded, const PackedInt32Array &p_indices) {
	if (!space || !space->get_px_cuda()) {
		return false;
	}
	// Mode resolution: the per-body override (soft_body_set_mode) wins, else
	// the physics/physx_3d/soft_body/mode project setting. Read live so a
	// runtime change takes effect on the next rebuild.
	const int mode = mode_override >= 0 ? mode_override : PhysXProjectSettings::soft_body_mode; // 0 Auto, 1 CPU, 2 GPU
	if (mode == 1) {
		return false;
	}

	Vector<Vector3> wv;
	wv.resize(p_welded.size());
	for (int i = 0; i < p_welded.size(); i++) {
		wv.write[i] = p_welded[i];
	}
	Vector<int32_t> idx;
	idx.resize(p_indices.size());
	for (int i = 0; i < p_indices.size(); i++) {
		idx.write[i] = p_indices[i];
	}

	PhysXSoftVolume3D *v = memnew(PhysXSoftVolume3D);
	if (!v->build(space, wv, idx, transform, _gpu_params())) {
		memdelete(v);
		if (mode == 2) {
			ERR_PRINT("PhysX: SoftBody3D forced to GPU but the mesh could not be tetrahedralized; it will be inert.");
		}
		return false;
	}
	volume = v;
	print_verbose("PhysX: SoftBody3D -> GPU PxDeformableVolume.");
	if (PhysXProjectSettings::solver_type != 1) { // not TGS
		static bool warned = false;
		if (!warned) {
			warned = true;
			WARN_PRINT("PhysX: GPU soft bodies (PxDeformableVolume) resolve soft-vs-soft contact reliably only under "
					   "the TGS solver. On PGS the contact is soft and transient -- a pile settles but slowly compacts "
					   "as bodies sink through each other. Set physics/physx_3d/simulation/solver_type = TGS for firm "
					   "soft-vs-soft stacking. Soft-vs-rigid is unaffected.");
		}
	}
	return true;
}

void PhysXSoftBody3D::set_mesh(RID p_mesh) {
	if (p_mesh == mesh) {
		return;
	}
	// The node swaps in a private duplicate of the mesh on its first draw. Same
	// geometry, so keep the running sim rather than snapping back to the rest
	// pose -- _rebuild_from_mesh() preserves positions when the vertex count is
	// unchanged.
	mesh = p_mesh;
	_rebuild_from_mesh(true);
}

void PhysXSoftBody3D::set_transform(const Transform3D &p_transform) {
	if (placed) {
		// After its first placement the node makes itself top-level and re-sends
		// its transform as identity -- that is bookkeeping, not a move. Only a
		// real, different transform (an explicit runtime reposition) re-seeds
		// the body.
		if (p_transform.is_equal_approx(Transform3D()) || p_transform.is_equal_approx(transform)) {
			return;
		}
	}
	const bool first_placement = !placed;
	transform = p_transform;
	placed = true;
	if (mesh_ready && (using_gpu || first_placement)) {
		// GPU: the volume bakes its placement into the cooked vertices, so any
		// move is a rebuild. First real placement: rebuild too, so the GPU path
		// (which was skipped while unplaced) now gets its shot.
		_rebuild_from_mesh();
	} else if (mesh_ready) {
		solver.reset(transform);
		_update_normals_and_bounds();
	}
}

void PhysXSoftBody3D::set_simulation_precision(int p_precision) {
	simulation_precision = MAX(p_precision, 1);
	_apply_solver_settings();
}

void PhysXSoftBody3D::set_total_mass(real_t p_mass) {
	total_mass = MAX(p_mass, 0.001);
	_apply_solver_settings();
}

void PhysXSoftBody3D::set_linear_stiffness(real_t p_stiffness) {
	linear_stiffness = CLAMP(p_stiffness, 0.0, 1.0);
	_apply_solver_settings();
}

void PhysXSoftBody3D::set_shrinking_factor(real_t p_factor) {
	shrinking_factor = p_factor;
	if (using_gpu) {
		// The GPU deformable volume has no rest-length knob; shrinking only
		// takes effect on the CPU solver path. Say so instead of silently
		// dropping the property.
		if (shrinking_factor > CMP_EPSILON) {
			WARN_PRINT_ONCE("PhysX: SoftBody3D shrinking_factor has no effect on the GPU (PxDeformableVolume) solver path; it applies on CPU soft bodies only.");
		}
		return;
	}
	if (mesh_ready) {
		solver.set_rest_length_scale(1.0f - CLAMP((float)shrinking_factor, 0.0f, 0.99f));
	}
}

void PhysXSoftBody3D::set_pressure_coefficient(real_t p_pressure) {
	pressure_coefficient = MAX(p_pressure, 0.0);
	_apply_solver_settings();
}

void PhysXSoftBody3D::set_damping_coefficient(real_t p_damping) {
	damping_coefficient = MAX(p_damping, 0.0);
	_apply_solver_settings();
}

void PhysXSoftBody3D::set_drag_coefficient(real_t p_drag) {
	drag_coefficient = MAX(p_drag, 0.0);
	_apply_solver_settings();
}

void PhysXSoftBody3D::_sync_gpu_pins() {
	if (!using_gpu || !volume) {
		return;
	}
	Vector<int> idx;
	Vector<Vector3> targets;
	for (const int rp : pinned_render_points) {
		idx.push_back(rp);
		const HashMap<int, Vector3>::ConstIterator t = pin_targets.find(rp);
		targets.push_back(t ? t->value : Vector3(NAN, 0, 0));
	}
	volume->set_pins(idx, targets);
}

void PhysXSoftBody3D::move_point(int p_point_index, const Vector3 &p_global_position) {
	if (p_point_index < 0) {
		return;
	}
	pinned_render_points.insert(p_point_index); // a moved point is a pinned point
	pin_targets[p_point_index] = p_global_position;
	if (using_gpu) {
		_sync_gpu_pins();
	} else if (mesh_ready && p_point_index < (int)visual_vertex_count) {
		solver.set_pin_target((int)map_visual_to_physics[p_point_index], p_global_position);
	}
}

Vector3 PhysXSoftBody3D::get_point_global_position(int p_point_index) const {
	ERR_FAIL_INDEX_V(p_point_index, (int)visual_vertex_count, Vector3());
	const uint32_t v = map_visual_to_physics[p_point_index];
	if (using_gpu && volume) {
		return volume->get_vertex_position(v);
	}
	return solver.get_positions()[v];
}

void PhysXSoftBody3D::pin_point(int p_point_index, bool p_pin) {
	if (p_point_index < 0) {
		return;
	}
	if (p_pin) {
		pinned_render_points.insert(p_point_index);
	} else {
		pinned_render_points.erase(p_point_index);
		pin_targets.erase(p_point_index);
	}
	if (using_gpu) {
		_sync_gpu_pins();
	} else if (mesh_ready && p_point_index < (int)visual_vertex_count) {
		solver.set_pinned((int)map_visual_to_physics[p_point_index], p_pin);
	}
}

bool PhysXSoftBody3D::is_point_pinned(int p_point_index) const {
	// pinned_render_points is the authoritative set on BOTH paths: pin_point/
	// move_point/unpin_all maintain it and every (re)build re-applies from it.
	// The CPU solver's own pin state mirrors it; querying the solver here used
	// to always answer false on the GPU path (the CPU solver is not built
	// there), making pins look lost.
	return pinned_render_points.has(p_point_index);
}

void PhysXSoftBody3D::unpin_all() {
	pinned_render_points.clear();
	pin_targets.clear();
	if (using_gpu) {
		_sync_gpu_pins();
		return;
	}
	const int n = solver.vertex_count();
	for (int i = 0; i < n; i++) {
		solver.set_pinned(i, false);
	}
}

void PhysXSoftBody3D::apply_point_impulse(int p_point_index, const Vector3 &p_impulse) {
	ERR_FAIL_INDEX(p_point_index, (int)visual_vertex_count);
	const uint32_t v = map_visual_to_physics[p_point_index];
	if (using_gpu && volume) {
		volume->add_point_impulse(v, p_impulse);
		return;
	}
	if (solver.is_pinned((int)v)) {
		return;
	}
	// Per-vertex mass is total_mass / N (the solver distributes it evenly), so
	// the velocity kick of a point impulse is impulse * N / total_mass. The
	// previous expression multiplied and divided by N, canceling to
	// impulse / total_mass -- an N-times-too-weak nudge on any real mesh.
	const float n = (float)MAX(solver.vertex_count(), 1);
	const float inv_mass = n / MAX((float)total_mass, 0.001f);
	solver.velocities_mut()[v] += p_impulse * inv_mass;
}

void PhysXSoftBody3D::apply_point_force(int p_point_index, const Vector3 &p_force, double p_delta) {
	apply_point_impulse(p_point_index, p_force * p_delta);
}

void PhysXSoftBody3D::apply_central_impulse(const Vector3 &p_impulse) {
	if (using_gpu && volume) {
		volume->add_central_impulse(p_impulse);
		return;
	}
	const int n = solver.vertex_count();
	if (n == 0) {
		return;
	}
	const Vector3 dv = p_impulse / MAX((float)total_mass, 0.001f);
	LocalVector<Vector3> &vel = solver.velocities_mut();
	for (int i = 0; i < n; i++) {
		if (!solver.is_pinned(i)) {
			vel[i] += dv;
		}
	}
}

void PhysXSoftBody3D::apply_central_force(const Vector3 &p_force, double p_delta) {
	apply_central_impulse(p_force * p_delta);
}

void PhysXSoftBody3D::_update_normals_and_bounds() {
	const LocalVector<Vector3> &pos = solver.get_positions();
	const LocalVector<int> &idx = solver.get_indices();
	const int n = pos.size();
	if (n == 0) {
		bounds = AABB();
		return;
	}
	normals.resize(n);
	for (int i = 0; i < n; i++) {
		normals[i] = Vector3();
	}
	for (uint32_t t = 0; t + 2 < idx.size(); t += 3) {
		const int a = idx[t];
		const int b = idx[t + 1];
		const int c = idx[t + 2];
		const Vector3 fn = (pos[b] - pos[a]).cross(pos[c] - pos[a]);
		normals[a] += fn;
		normals[b] += fn;
		normals[c] += fn;
	}
	AABB b(pos[0], Vector3());
	for (int i = 0; i < n; i++) {
		const float len = normals[i].length();
		normals[i] = len > CMP_EPSILON ? normals[i] / len : Vector3(0, 1, 0);
		b.expand_to(pos[i]);
	}
	bounds = b;
}

// One physics query per vertex per frame -- the expensive part -- caching each
// contact as a plane. _resolve_contacts() then re-projects against those cached
// planes every substep for free, which is what stops a fast body tunnelling
// through thin geometry (a once-per-frame push can't).
void PhysXSoftBody3D::_refresh_contacts() {
	const int n = solver.vertex_count();
	contact_n.resize(n);
	contact_p.resize(n);
	contact_hit.resize(n);
	// Grow the per-vertex tunneling memory (value-initialized; shrink is just
	// a truncation).
	while (contact_n_prev.size() < (uint32_t)n) {
		contact_n_prev.push_back(Vector3());
		contact_p_prev.push_back(Vector3());
	}
	contact_n_prev.resize(n);
	contact_p_prev.resize(n);
	for (int i = 0; i < n; i++) {
		contact_hit[i] = 0;
	}
	contact_count = 0;

	if (!space) {
		return;
	}
	PhysicsDirectSpaceState3D *ss = space->get_direct_state();
	if (!ss) {
		return;
	}
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	const float radius = MAX(solver.vertex_radius(), 0.02f);
	if (collision_sphere.is_null()) {
		collision_sphere = ps->sphere_shape_create();
	}
	ps->shape_set_data(collision_sphere, radius);

	PhysicsDirectSpaceState3D::ShapeParameters params;
	params.shape_rid = collision_sphere;
	params.collision_mask = collision_mask;
	params.collide_with_bodies = true;
	params.collide_with_areas = false;
	params.exclude = collision_exceptions;

	const LocalVector<Vector3> &pos = solver.get_positions();
	for (int i = 0; i < n; i++) {
		if (solver.is_pinned(i)) {
			contact_n_prev[i] = Vector3();
			continue;
		}
		params.transform = Transform3D(Basis(), pos[i]);
		PhysicsDirectSpaceState3D::ShapeRestInfo info;
		const bool has_hit = ss->rest_info(params, &info);

		// Anti-tunnelling memory: a vertex squeezed past a thin collider's
		// mid-plane flips rest_info's deepest-hit MTD to the FAR face, whose
		// plane sits UNDER the vertex -- _resolve_contacts() would then read
		// that as the allowed side and release it (a cascade blob escaped at
		// y = -10 this way). While the vertex is still behind LAST frame's
		// contact plane AND the query still sees a collider, that remembered
		// plane is the truth for a static track: keep it, unless this frame's
		// plane points in ROUGHLY the same direction (dot > 0.25 -- a genuinely
		// rolling vertex's normal changes gradually, while a far face or a
		// perpendicular side face sits at/below zero; accepting perpendicular
		// planes would let a squeeze walk the memory around the box face by
		// face and out). Once back on the plane's allowed side (or free of the
		// collider), contact memory updates normally again.
		if (contact_n_prev[i] != Vector3() &&
				(pos[i] - contact_p_prev[i]).dot(contact_n_prev[i]) < 0.0f) {
			if (has_hit && info.normal.dot(contact_n_prev[i]) > 0.25f) {
				// A same-side hit: the fresh plane is the truth.
				contact_n[i] = info.normal;
				contact_p[i] = info.point;
				contact_n_prev[i] = info.normal;
				contact_p_prev[i] = info.point;
			} else {
				// Far-side plane, or no hit at all: hold the remembered
				// top-side plane so the resolver keeps pushing the vertex out.
				contact_n[i] = contact_n_prev[i];
				contact_p[i] = contact_p_prev[i];
			}
			contact_hit[i] = 1;
			contact_count++;
			continue;
		}
		if (!has_hit) {
			contact_n_prev[i] = Vector3();
			continue;
		}
		// This backend's rest_info reports info.point as the closest point ON
		// the collider's surface to the query center and info.normal as the
		// outward push direction. The sphere's penetration is therefore
		// radius minus the center's distance to that surface point, and the
		// surface plane _resolve_contacts() needs is exactly (info.point,
		// info.normal). The old projection formula misread the contract and
		// cached a plane ~radius above the surface, so resting vertices
		// hovered and deep ones stayed stuck.
		const float dist_to_surface = pos[i].distance_to(info.point);
		const float penetration = radius - dist_to_surface;
		contact_n[i] = info.normal;
		contact_p[i] = info.point;
		contact_n_prev[i] = info.normal;
		contact_p_prev[i] = info.point;
		contact_hit[i] = 1;
		if (penetration > 0.0f) {
			contact_count++;
		}
	}
}

void PhysXSoftBody3D::_resolve_contacts() {
	LocalVector<Vector3> &pos = solver.positions_mut();
	LocalVector<Vector3> &vel = solver.velocities_mut();
	const int n = solver.vertex_count();
	for (int i = 0; i < n; i++) {
		if (!contact_hit[i] || solver.is_pinned(i)) {
			continue;
		}
		const Vector3 dir = contact_n[i];
		const float sd = (pos[i] - contact_p[i]).dot(dir); // signed dist to surface
		if (sd >= 0.0f) {
			continue; // on the allowed side
		}
		pos[i] -= dir * sd; // back up to the surface plane
		const float vn = vel[i].dot(dir);
		const Vector3 v_tangent = vel[i] - dir * vn;
		// Light kinetic friction only -- a body slides down a ramp freely.
		// Settling (resting creep/spin) is handled by _damp_rigid_drift(), so
		// this must NOT clamp slow tangential motion or a body can't get moving.
		vel[i] = v_tangent * 0.88f + dir * MAX(vn, 0.0f);
	}
}

void PhysXSoftBody3D::step(double p_delta, const Vector3 &p_gravity) {
	if (using_gpu) {
		return; // the PxDeformableVolume advances inside px_scene->simulate()
	}
	// Wait for the node to hand us its world transform -- stepping from the
	// mesh-local rest pose would drop the body through the floor and the
	// collision pass would then fling it out.
	if (!mesh_ready || !placed || p_delta <= 0.0) {
		return;
	}
	solver.step((float)p_delta, p_gravity, Vector3());
	// One contact query per frame, then project positions out. The max_speed
	// clamp keeps a body slow enough that this once-per-frame pass still catches
	// it before it tunnels through thin geometry; a per-substep re-query is too
	// expensive at useful body counts.
	_refresh_contacts();
	_resolve_contacts();
	_damp_rigid_drift();
	_update_normals_and_bounds();
}

void PhysXSoftBody3D::read_back() {
	if (!using_gpu || !volume) {
		return;
	}
	volume->read_back();
	bounds = volume->get_bounds();
}

// Gauss-Seidel constraint ordering and the volume solve each leave a tiny,
// undamped rigid-body component in the velocities every step. A soft body has
// no angular damping to shed it, so a resting one slowly spins and wanders off.
// Split the motion into its rigid part (centre-of-mass translation + rotation)
// and its deformation part, and bleed the rigid part -- hard when the body is
// nearly at rest, gently otherwise so a thrown blob can still tumble.
void PhysXSoftBody3D::_damp_rigid_drift() {
	// Only meaningful when the body is resting on something -- an airborne body
	// falling or thrown IS rigid motion and must be left alone.
	if (contact_count == 0) {
		return;
	}
	LocalVector<Vector3> &pos = solver.positions_mut();
	LocalVector<Vector3> &vel = solver.velocities_mut();
	const int n = solver.vertex_count();
	if (n < 2) {
		return;
	}
	Vector3 com;
	Vector3 com_vel;
	int free_count = 0;
	for (int i = 0; i < n; i++) {
		if (solver.is_pinned(i)) {
			continue;
		}
		com += pos[i];
		com_vel += vel[i];
		free_count++;
	}
	if (free_count < 2) {
		return;
	}
	com /= (float)free_count;
	com_vel /= (float)free_count;

	Vector3 ang_mom;
	float inertia = 0.0f;
	for (int i = 0; i < n; i++) {
		if (solver.is_pinned(i)) {
			continue;
		}
		const Vector3 r = pos[i] - com;
		ang_mom += r.cross(vel[i] - com_vel);
		inertia += r.length_squared();
	}
	const Vector3 omega = inertia > 1.0e-6f ? ang_mom / inertia : Vector3();

	const float rigid_speed = com_vel.length() + omega.length();
	// Bleed ramps smoothly from a barely-there trickle while the body is still
	// travelling up to a strong pull as it comes to rest -- no braking of a
	// moving body, but resting creep/spin still dies.
	const float t = CLAMP((0.45f - rigid_speed) / 0.45f, 0.0f, 1.0f);
	const float damp = 0.004f + (0.55f - 0.004f) * t * t;
	for (int i = 0; i < n; i++) {
		if (solver.is_pinned(i)) {
			continue;
		}
		const Vector3 rigid_v = com_vel + omega.cross(pos[i] - com);
		vel[i] -= rigid_v * damp;
	}
}

void PhysXSoftBody3D::update_rendering_server(PhysicsServer3DRenderingServerHandler *p_handler) {
	if (!mesh_ready || !p_handler) {
		return;
	}
	if (using_gpu && volume) {
		for (uint32_t i = 0; i < visual_vertex_count; i++) {
			const uint32_t v = map_visual_to_physics[i];
			p_handler->set_vertex(i, volume->get_vertex_position(v));
			p_handler->set_normal(i, volume->get_vertex_normal(v));
		}
		p_handler->set_aabb(volume->get_bounds());
		return;
	}
	const LocalVector<Vector3> &pos = solver.get_positions();
	for (uint32_t i = 0; i < visual_vertex_count; i++) {
		const uint32_t v = map_visual_to_physics[i];
		p_handler->set_vertex(i, pos[v]);
		p_handler->set_normal(i, normals[v]);
	}
	p_handler->set_aabb(bounds);
}

void PhysXSoftBody3D::set_state(PhysicsServer3D::BodyState p_state, const Variant &p_variant) {
	if (p_state == PhysicsServer3D::BODY_STATE_TRANSFORM) {
		set_transform(p_variant);
	}
	// Linear/angular velocity and sleeping have no meaning for a vertex cloud.
}

Variant PhysXSoftBody3D::get_state(PhysicsServer3D::BodyState p_state) const {
	switch (p_state) {
		case PhysicsServer3D::BODY_STATE_TRANSFORM: {
			// Origin at the current bounds center; basis stays identity (the sim
			// runs in world space, like every other backend's soft body).
			return Transform3D(Basis(), bounds.get_center());
		}
		case PhysicsServer3D::BODY_STATE_LINEAR_VELOCITY:
		case PhysicsServer3D::BODY_STATE_ANGULAR_VELOCITY:
			return Vector3();
		case PhysicsServer3D::BODY_STATE_SLEEPING:
		case PhysicsServer3D::BODY_STATE_CAN_SLEEP:
			return false;
		default:
			return Variant();
	}
}

void PhysXSoftBody3D::set_exception_slot(uint32_t p_slot) {
	// PhysXObject3D::exception_slot holds the value; the GPU volume needs it
	// baked into its collision shape's filter data word2.
	if (volume) {
		volume->set_exception_slot(p_slot);
	}
}

// ---------------------------------------------------------------------------
// Rigid attachments (GAP-15)
// ---------------------------------------------------------------------------

void PhysXSoftBody3D::attach_point_to_body(int p_point_index, const RID &p_body) {
	if (p_point_index < 0 || (uint32_t)p_point_index >= visual_vertex_count) {
		ERR_FAIL_MSG("PhysXSoftBody3D: attach_point_to_body index out of range.");
		return;
	}
	if (!using_gpu || !volume) {
		// The CPU XPBD solver has no SDK constraint facility; a rigid
		// attachment is a GPU-path capability (warn once per body).
		WARN_PRINT_ONCE("PhysXSoftBody3D: attach_point_to_body requires the GPU solver path (project setting physics/physx_3d/soft_body/mode=GPU or the node's physx_soft_mode metadata).");
		return;
	}
	PhysXBody3D *body = PhysXServer3D::get_singleton()->get_body(p_body);
	if (!body || !body->get_px_actor()) {
		ERR_FAIL_MSG("PhysXSoftBody3D: attach_point_to_body target is not a valid body.");
		return;
	}
	if (space && body->get_space() != space) {
		ERR_FAIL_MSG("PhysXSoftBody3D: attach_point_to_body target lives in a different space (the attachment is only active when both actors share a scene).");
		return;
	}
	// Re-attaching a point replaces its previous attachment.
	detach_point_from_body(p_point_index);
	Vector<int> welded;
	welded.push_back((int)map_visual_to_physics[p_point_index]);
	physx::PxDeformableAttachment *att = volume->create_rigid_attachment(welded, body->get_px_actor());
	if (att) {
		point_attachments.insert(p_point_index, att);
	}
}

void PhysXSoftBody3D::detach_point_from_body(int p_point_index) {
	HashMap<int, physx::PxDeformableAttachment *>::Iterator it = point_attachments.find(p_point_index);
	if (!it) {
		return;
	}
	if (volume) {
		volume->release_attachment(it->value);
	}
	point_attachments.erase(p_point_index);
}

void PhysXSoftBody3D::release_attachments_for(physx::PxActor *p_actor) {
	if (!volume || !p_actor) {
		return;
	}
	// Collect-then-erase: Godot HashMaps cannot erase while iterating.
	LocalVector<int> dead;
	for (const KeyValue<int, physx::PxDeformableAttachment *> &kv : point_attachments) {
		physx::PxActor *a0 = nullptr;
		physx::PxActor *a1 = nullptr;
		kv.value->getActors(a0, a1);
		if (a0 == p_actor || a1 == p_actor) {
			dead.push_back(kv.key);
		}
	}
	for (int idx : dead) {
		detach_point_from_body(idx);
	}
}

void PhysXSoftBody3D::get_collision_exceptions(List<RID> *p_exceptions) const {
	for (const RID &e : collision_exceptions) {
		p_exceptions->push_back(e);
	}
}

void PhysXSoftBody3D::_update_shapes() {
	// A live filter change re-applies through the volume params (which now
	// also rewrite the shape's filter data), while the CPU solver consults
	// the stored bits on every contact query.
	if (using_gpu && volume) {
		volume->apply_params(_gpu_params());
	}
}

/**************************************************************************/
/*  physx_cloth_3d.cpp                                                    */
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

#include "physx_cloth_3d.h"

#include "../physx_server.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "scene/3d/physics/area_3d.h"
#include "scene/resources/3d/world_3d.h"
#include "servers/physics_3d/physics_server_3d.h"
#include "servers/rendering/rendering_server.h"

// Map a friendly 0..1 stiffness to XPBD compliance (m/N). 1 -> rigid, 0 -> slack.
static float _stiffness_to_compliance(float p_stiffness, float p_max_compliance) {
	const float s = CLAMP(p_stiffness, 0.0f, 1.0f);
	return (1.0f - s) * (1.0f - s) * p_max_compliance;
}

void PhysXCloth3D::_apply_solver_settings() {
	XPBDClothSolver::Settings s;
	s.substeps = MAX(substeps, 1);
	s.stretch_compliance = _stiffness_to_compliance(stiffness, 5.0e-3f);
	s.shear_compliance = _stiffness_to_compliance(shear_stiffness, 2.0e-1f);
	s.bend_compliance = _stiffness_to_compliance(bend_stiffness, 6.0e-1f);
	s.damping = MAX(damping, 0.0f);
	s.drag = MAX(drag, 0.0f);
	s.lift = MAX(lift, 0.0f);
	s.density = MAX(density, 1.0e-3f);
	solver.settings = s;
}

void PhysXCloth3D::_rebuild() {
	_destroy();

	_apply_solver_settings();

	if (source_mesh.is_valid() && source_mesh->get_surface_count() > 0) {
		const Array arr = source_mesh->surface_get_arrays(0);
		const PackedVector3Array verts = arr[Mesh::ARRAY_VERTEX];
		PackedInt32Array idx = arr[Mesh::ARRAY_INDEX];
		if (idx.is_empty()) {
			idx.resize(verts.size());
			for (int i = 0; i < verts.size(); i++) {
				idx.write[i] = i;
			}
		}
		solver.build_mesh(verts, idx);
	} else {
		solver.build_grid(grid_columns, grid_rows, grid_size);
	}
	if (!solver.is_built()) {
		return;
	}

	built = true;
	_resolve_pins();
	solver.reset(get_global_transform());

	mesh = RenderingServer::get_singleton()->mesh_create();
	set_base(mesh);

	// Prefer the PhysX GPU deformable surface; fall back to the solver above.
	const bool want_gpu = !Engine::get_singleton()->is_editor_hint() && is_inside_tree();
	if (want_gpu && _try_build_gpu()) {
		_update_gpu_mesh();
	} else {
		_update_mesh();
	}
	update_gizmos();
}

bool PhysXCloth3D::_gpu_available() {
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	return server && server->is_gpu_dynamics_enabled();
}

bool PhysXCloth3D::_try_build_gpu() {
	if (simulation_mode == SIM_CPU) {
		return false;
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || !server->is_gpu_dynamics_enabled() || get_world_3d().is_null()) {
		return false;
	}
	gpu_cloth = server->cloth_create();
	if (gpu_cloth.is_null()) {
		return false;
	}
	server->cloth_set_space(gpu_cloth, get_world_3d()->get_space());
	const float th = MAX(solver.vertex_radius() * 0.5f, 0.004f);
	server->cloth_set_params(gpu_cloth, th, density, stiffness, bend_stiffness, damping, collision_mask);

	// World-space rest mesh + topology from the solver.
	const LocalVector<Vector3> &wp = solver.get_positions();
	const LocalVector<int> &tri = solver.get_indices();
	Vector<Vector3> positions;
	positions.resize(wp.size());
	for (uint32_t i = 0; i < wp.size(); i++) {
		positions.write[i] = wp[i];
	}
	Vector<int32_t> idx;
	idx.resize(tri.size());
	for (uint32_t i = 0; i < tri.size(); i++) {
		idx.write[i] = tri[i];
	}
	server->cloth_build(gpu_cloth, positions, idx, get_global_transform());
	if (!server->cloth_is_ready(gpu_cloth)) {
		server->free(gpu_cloth);
		gpu_cloth = RID();
		return false;
	}

	Vector<int32_t> pins;
	for (uint32_t i = 0; i < pinned_index_cache.size(); i++) {
		pins.push_back(pinned_index_cache[i]);
	}
	server->cloth_set_pinned(gpu_cloth, pins);
	gpu_mesh_version = 0;
	// Keep the solver's rest data around for the gizmo and vertex count; it is
	// just not stepped while the GPU owns the simulation.
	return true;
}

void PhysXCloth3D::_destroy() {
	built = false;
	if (gpu_cloth.is_valid()) {
		PhysXServer3D *server = PhysXServer3D::get_singleton();
		if (server) {
			server->free(gpu_cloth);
		}
		gpu_cloth = RID();
	}
	if (mesh.is_valid()) {
		set_base(RID());
		RenderingServer::get_singleton()->free(mesh);
		mesh = RID();
	}
	if (collision_sphere.is_valid()) {
		PhysicsServer3D::get_singleton()->free(collision_sphere);
		collision_sphere = RID();
	}
	solver.clear();
	pinned_index_cache.clear();
}

void PhysXCloth3D::_resolve_pins() {
	pinned_index_cache.clear();
	const int n = solver.vertex_count();
	for (int i = 0; i < n; i++) {
		solver.set_pinned(i, false);
	}

	const int c = solver.grid_cols();
	const int r = solver.grid_rows();
	auto pin = [&](int v) {
		if (v >= 0 && v < n && !solver.is_pinned(v)) {
			solver.set_pinned(v, true);
			pinned_index_cache.push_back(v);
		}
	};

	if (c > 0 && r > 0) {
		switch (pin_mode) {
			case PIN_TOP_EDGE:
				for (int x = 0; x < c; x++) {
					pin(x);
				}
				break;
			case PIN_TOP_CORNERS:
				pin(0);
				pin(c - 1);
				break;
			case PIN_LEFT_EDGE:
				for (int y = 0; y < r; y++) {
					pin(y * c);
				}
				break;
			case PIN_RIGHT_EDGE:
				for (int y = 0; y < r; y++) {
					pin(y * c + (c - 1));
				}
				break;
			case PIN_NONE:
			default:
				break;
		}
	}
	for (int i = 0; i < pinned_vertices.size(); i++) {
		pin(pinned_vertices[i]);
	}

	if (gpu_cloth.is_valid()) {
		PhysXServer3D *server = PhysXServer3D::get_singleton();
		if (server) {
			Vector<int32_t> pins;
			for (uint32_t i = 0; i < pinned_index_cache.size(); i++) {
				pins.push_back(pinned_index_cache[i]);
			}
			server->cloth_set_pinned(gpu_cloth, pins);
		}
	}

	// The anchor is followed incrementally in _step by its frame-to-frame delta;
	// just record where it starts.
	Node3D *anchor = Object::cast_to<Node3D>(get_node_or_null(anchor_path));
	last_anchor_xform = anchor ? anchor->get_global_transform() : Transform3D();
}

Vector3 PhysXCloth3D::_sample_wind(double p_delta) {
	if (!wind_enabled) {
		return Vector3();
	}
	wind_time += p_delta;
	Vector3 w = wind;

	// The PhysX module does not expose areas to point queries, so read the
	// assigned wind Area3D's settings directly (also keeps this backend-agnostic).
	Area3D *area = Object::cast_to<Area3D>(get_node_or_null(wind_area));
	if (area && area->get_wind_force_magnitude() > 0.0f) {
		Node3D *src_node = Object::cast_to<Node3D>(area->get_node_or_null(area->get_wind_source_path()));
		Vector3 dir(-1, 0, 0);
		Vector3 src;
		if (src_node) {
			const Transform3D sx = src_node->get_global_transform();
			dir = -sx.basis.get_column(Vector3::AXIS_Z).normalized();
			src = sx.origin;
		}
		// Cloth centroid, for the downwind attenuation.
		const LocalVector<Vector3> &sp = solver.get_positions();
		Vector3 center = get_global_transform().origin;
		if (!sp.is_empty()) {
			center = Vector3();
			for (uint32_t i = 0; i < sp.size(); i++) {
				center += sp[i];
			}
			center /= (float)sp.size();
		}
		const float att = area->get_wind_attenuation_factor();
		const float proj = MAX((center - src).dot(dir), 1.0f);
		w += dir * (area->get_wind_force_magnitude() * Math::pow(proj, -att));
	}

	if (wind_turbulence > 0.0f && w != Vector3()) {
		const float speed = w.length();
		const Vector3 dir = w / speed;
		Vector3 perp = dir.cross(Vector3(0, 1, 0));
		if (perp.length_squared() < 1.0e-4f) {
			perp = dir.cross(Vector3(1, 0, 0));
		}
		perp.normalize();
		const Vector3 up = dir.cross(perp).normalized();
		const float t = (float)wind_time;
		const float gust = 0.6f * Math::sin(t * 1.7f) + 0.4f * Math::sin(t * 4.3f + 1.3f);
		const float sway = 0.5f * Math::sin(t * 2.9f + 0.7f);
		w += dir * (speed * wind_turbulence * gust);
		w += perp * (speed * wind_turbulence * 0.5f * sway);
		w += up * (speed * wind_turbulence * 0.3f * Math::sin(t * 3.7f));
	}
	return w;
}

void PhysXCloth3D::_step(double p_delta) {
	if (!built || !simulating || p_delta <= 0.0) {
		return;
	}

	const Vector3 wind_now = _sample_wind(p_delta);

	// --- GPU path: the deformable surface is stepped by the physics server. ---
	if (gpu_cloth.is_valid()) {
		PhysXServer3D *server = PhysXServer3D::get_singleton();
		if (!server) {
			return;
		}
		Node3D *anchor = Object::cast_to<Node3D>(get_node_or_null(anchor_path));
		if (anchor && !pinned_index_cache.is_empty()) {
			// Feed the pinned vertices' target world positions from the last mesh.
			const Transform3D now = anchor->get_global_transform();
			const Transform3D delta = now * last_anchor_xform.affine_inverse();
			Vector<Vector3> targets;
			targets.resize(render_verts.size());
			const Transform3D gt = get_global_transform();
			for (int i = 0; i < render_verts.size(); i++) {
				targets.write[i] = Vector3(NAN, 0, 0);
			}
			for (uint32_t i = 0; i < pinned_index_cache.size(); i++) {
				const int v = pinned_index_cache[i];
				if (v >= 0 && v < targets.size()) {
					targets.write[v] = delta.xform(gt.xform(render_verts[v]));
				}
			}
			server->cloth_set_pin_targets(gpu_cloth, targets);
			last_anchor_xform = now;
		}
		server->cloth_apply_wind(gpu_cloth, wind_now, drag, lift, p_delta);
		_update_gpu_mesh();
		return;
	}

	// --- CPU path ---
	Node3D *anchor = Object::cast_to<Node3D>(get_node_or_null(anchor_path));
	if (anchor && !pinned_index_cache.is_empty()) {
		const Transform3D now = anchor->get_global_transform();
		const Transform3D delta = now * last_anchor_xform.affine_inverse();
		LocalVector<Vector3> &pos = solver.positions_mut();
		for (uint32_t i = 0; i < pinned_index_cache.size(); i++) {
			const int v = pinned_index_cache[i];
			solver.set_pin_target(v, delta.xform(pos[v]));
		}
		last_anchor_xform = now;
	}

	// Static StringNames: removes the per-tick string hashing from this
	// every-physics-tick lookup while keeping the values live (a changed
	// project setting takes effect on the next tick, as before).
	static StringName s_gravity_vec_sn("physics/3d/default_gravity_vector");
	static StringName s_gravity_mag_sn("physics/3d/default_gravity");
	Vector3 gravity_vec = GLOBAL_GET(s_gravity_vec_sn);
	const float gravity_mag = GLOBAL_GET(s_gravity_mag_sn);
	if (gravity_vec == Vector3()) {
		gravity_vec = Vector3(0, -1, 0);
	}
	const Vector3 gravity = gravity_vec.normalized() * gravity_mag;

	solver.step((float)p_delta, gravity, wind_now);

	if (collision_enabled) {
		_collide();
	}
	_update_mesh();
}

void PhysXCloth3D::_update_gpu_mesh() {
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || gpu_cloth.is_null() || mesh.is_null()) {
		return;
	}
	PackedVector3Array wpos;
	PackedInt32Array idx;
	const int tris = server->cloth_get_mesh(gpu_cloth, wpos, idx, gpu_mesh_version);
	if (tris < 0) {
		return; // unchanged
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	rs->mesh_clear(mesh);
	if (tris == 0 || wpos.size() < 3) {
		return;
	}

	const Transform3D inv = get_global_transform().affine_inverse();
	const int n = wpos.size();
	render_verts.resize(n);
	render_normals.resize(n);
	render_indices = idx;
	Vector3 *vw = render_verts.ptrw();
	Vector3 *nw = render_normals.ptrw();
	const Vector3 *sp = wpos.ptr();
	for (int i = 0; i < n; i++) {
		vw[i] = inv.xform(sp[i]);
		nw[i] = Vector3();
	}
	const int *ip = idx.ptr();
	for (int t = 0; t + 2 < idx.size(); t += 3) {
		const int a = ip[t], b = ip[t + 1], c = ip[t + 2];
		if (a >= n || b >= n || c >= n) {
			continue;
		}
		const Vector3 fn = (vw[b] - vw[a]).cross(vw[c] - vw[a]);
		nw[a] += fn;
		nw[b] += fn;
		nw[c] += fn;
	}
	for (int i = 0; i < n; i++) {
		nw[i] = nw[i].normalized();
	}

	Array arrays;
	arrays.resize(RS::ARRAY_MAX);
	arrays[RS::ARRAY_VERTEX] = render_verts;
	arrays[RS::ARRAY_NORMAL] = render_normals;
	arrays[RS::ARRAY_INDEX] = render_indices;
	rs->mesh_add_surface_from_arrays(mesh, RS::PRIMITIVE_TRIANGLES, arrays);

	AABB box(render_verts[0], Vector3());
	for (int i = 1; i < n; i++) {
		box.expand_to(render_verts[i]);
	}
	rs->mesh_set_custom_aabb(mesh, box.grow(0.2));
}

void PhysXCloth3D::_collide() {
	Ref<World3D> world = get_world_3d();
	if (world.is_null()) {
		return;
	}
	PhysicsServer3D *ps = PhysicsServer3D::get_singleton();
	PhysicsDirectSpaceState3D *ss = world->get_direct_space_state();
	if (!ss) {
		return; // not available yet during world setup
	}

	const float radius = MAX(solver.vertex_radius(), 0.005f);
	if (collision_sphere.is_null()) {
		collision_sphere = ps->sphere_shape_create();
	}
	ps->shape_set_data(collision_sphere, radius);

	PhysicsDirectSpaceState3D::ShapeParameters params;
	params.shape_rid = collision_sphere;
	params.collision_mask = collision_mask;
	params.collide_with_bodies = true;
	params.collide_with_areas = false;
	// Collision exceptions (parity with soft bodies / the GPU path): excluded
	// bodies are ignored by the per-vertex push-out query.
	for (int i = 0; i < collision_exceptions.size(); i++) {
		params.exclude.insert(collision_exceptions[i]);
	}

	LocalVector<Vector3> &pos = solver.positions_mut();
	LocalVector<Vector3> &vel = solver.velocities_mut();
	const int n = solver.vertex_count();
	for (int i = 0; i < n; i++) {
		if (solver.is_pinned(i)) {
			continue;
		}
		params.transform = Transform3D(Basis(), pos[i]);
		PhysicsDirectSpaceState3D::ShapeRestInfo info;
		if (!ss->rest_info(params, &info)) {
			continue;
		}
		// The backend's rest_info reports info.point as the closest point ON
		// the collider's surface to the query center and info.normal as the
		// outward push direction — the same contract the soft body's
		// _refresh_contacts documents (the old projection formula misread it
		// and left resting vertices hovering and deep ones stuck inside).
		// Resolve against the (point, normal) surface plane: a vertex whose
		// center is on the back side of the plane is pushed back out; the
		// tiny lift avoids re-penetrating the next frame.
		const float sd = (pos[i] - info.point).dot(info.normal); // signed dist to surface
		if (sd < 0.0f) {
			pos[i] -= info.normal * (sd + 0.001f);
			const float vn = vel[i].dot(info.normal);
			Vector3 v_tangent = vel[i] - info.normal * vn;
			v_tangent *= (1.0f - friction); // Coulomb-style sliding friction
			vel[i] = v_tangent + info.normal * MAX(vn, 0.0f); // no inward velocity
		}
	}
}

void PhysXCloth3D::_update_mesh() {
	if (mesh.is_null()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	rs->mesh_clear(mesh);

	const LocalVector<Vector3> &wpos = solver.get_positions();
	const LocalVector<int> &tri = solver.get_indices();
	const int n = wpos.size();
	if (n < 3 || tri.size() < 3) {
		return;
	}

	const Transform3D inv = get_global_transform().affine_inverse();
	render_verts.resize(n);
	render_normals.resize(n);
	render_indices.resize(tri.size());
	Vector3 *vw = render_verts.ptrw();
	Vector3 *nw = render_normals.ptrw();
	int *iw = render_indices.ptrw();
	for (int i = 0; i < n; i++) {
		vw[i] = inv.xform(wpos[i]);
		nw[i] = Vector3();
	}
	for (int t = 0; t + 2 < (int)tri.size(); t += 3) {
		const int a = tri[t];
		const int b = tri[t + 1];
		const int c = tri[t + 2];
		iw[t] = a;
		iw[t + 1] = b;
		iw[t + 2] = c;
		const Vector3 fn = (vw[b] - vw[a]).cross(vw[c] - vw[a]);
		nw[a] += fn;
		nw[b] += fn;
		nw[c] += fn;
	}
	for (int i = 0; i < n; i++) {
		nw[i] = nw[i].normalized();
	}

	Array arrays;
	arrays.resize(RS::ARRAY_MAX);
	arrays[RS::ARRAY_VERTEX] = render_verts;
	arrays[RS::ARRAY_NORMAL] = render_normals;
	arrays[RS::ARRAY_INDEX] = render_indices;
	rs->mesh_add_surface_from_arrays(mesh, RS::PRIMITIVE_TRIANGLES, arrays);

	AABB box(render_verts[0], Vector3());
	for (int i = 1; i < n; i++) {
		box.expand_to(render_verts[i]);
	}
	rs->mesh_set_custom_aabb(mesh, box.grow(solver.vertex_radius() * 4.0f));
}

PackedVector3Array PhysXCloth3D::get_pinned_positions() const {
	PackedVector3Array out;
	if (!built) {
		return out;
	}
	const Transform3D inv = get_global_transform().affine_inverse();
	const LocalVector<Vector3> &wpos = solver.get_positions();
	for (uint32_t i = 0; i < pinned_index_cache.size(); i++) {
		const int v = pinned_index_cache[i];
		if (v >= 0 && v < (int)wpos.size()) {
			out.push_back(inv.xform(wpos[v]));
		}
	}
	return out;
}

void PhysXCloth3D::reset() {
	if (!built) {
		return;
	}
	_resolve_pins();
	solver.reset(get_global_transform());
	last_anchor_xform = Transform3D();
	Node3D *anchor = Object::cast_to<Node3D>(get_node_or_null(anchor_path));
	if (anchor) {
		last_anchor_xform = anchor->get_global_transform();
	}
	_update_mesh();
}

void PhysXCloth3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			_rebuild();
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(true);
			}
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			_destroy();
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			_step(get_physics_process_delta_time());
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			// Only the rest pose lives in local space; a running sim owns world
			// positions, so a mid-sim move should not teleport it.
		} break;
	}
}

AABB PhysXCloth3D::get_aabb() const {
	if (render_verts.size() >= 2) {
		AABB box(render_verts[0], Vector3());
		for (int i = 1; i < render_verts.size(); i++) {
			box.expand_to(render_verts[i]);
		}
		return box.grow(0.1);
	}
	const Vector3 e(grid_size.x * 0.5, grid_size.y * 0.5, MAX(grid_size.x, grid_size.y) * 0.5);
	return AABB(-e, e * 2.0);
}

PackedStringArray PhysXCloth3D::get_configuration_warnings() const {
	PackedStringArray w = GeometryInstance3D::get_configuration_warnings();
	if (pin_mode == PIN_NONE && pinned_vertices.is_empty() && anchor_path.is_empty()) {
		w.push_back(RTR("No pinned vertices: the cloth will fall freely. Set \"Pin Mode\" or \"Pinned Vertices\", or an \"Anchor Path\"."));
	}
	if (source_mesh.is_valid() && source_mesh->get_surface_count() == 0) {
		w.push_back(RTR("\"Source Mesh\" has no surfaces."));
	}
	if (simulation_mode == SIM_GPU && !_gpu_available()) {
		w.push_back(RTR("\"Simulation Mode\" is GPU but no CUDA device is available; the cloth runs on the CPU solver."));
	}
	return w;
}

void PhysXCloth3D::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("simulation_mode") && !_gpu_available()) {
		// Hide the GPU option (value 1) when there is no CUDA device, keeping the
		// stored value stable across machines.
		p_property.hint_string = "Auto:0,CPU:2";
	}
}

void PhysXCloth3D::set_simulation_mode(SimulationMode p_mode) {
	if (simulation_mode == p_mode) {
		return;
	}
	simulation_mode = p_mode;
	if (is_inside_world()) {
		_rebuild();
	}
	update_configuration_warnings();
}

void PhysXCloth3D::set_source_mesh(const Ref<Mesh> &p_mesh) {
	source_mesh = p_mesh;
	if (is_inside_world()) {
		_rebuild();
	}
	update_configuration_warnings();
}

void PhysXCloth3D::set_grid_columns(int p_v) {
	grid_columns = CLAMP(p_v, 2, 256);
	if (is_inside_world()) {
		_rebuild();
	}
}

void PhysXCloth3D::set_grid_rows(int p_v) {
	grid_rows = CLAMP(p_v, 2, 256);
	if (is_inside_world()) {
		_rebuild();
	}
}

void PhysXCloth3D::set_grid_size(const Vector2 &p_v) {
	grid_size = p_v.maxf(0.01);
	if (is_inside_world()) {
		_rebuild();
	}
}

void PhysXCloth3D::set_substeps(int p_v) {
	substeps = CLAMP(p_v, 1, 40);
	_apply_solver_settings();
}

void PhysXCloth3D::set_stiffness(float p_v) {
	stiffness = CLAMP(p_v, 0.0f, 1.0f);
	_apply_solver_settings();
}

void PhysXCloth3D::set_shear_stiffness(float p_v) {
	shear_stiffness = CLAMP(p_v, 0.0f, 1.0f);
	_apply_solver_settings();
}

void PhysXCloth3D::set_bend_stiffness(float p_v) {
	bend_stiffness = CLAMP(p_v, 0.0f, 1.0f);
	_apply_solver_settings();
}

void PhysXCloth3D::set_damping(float p_v) {
	damping = MAX(p_v, 0.0f);
	_apply_solver_settings();
}

void PhysXCloth3D::set_density(float p_v) {
	density = MAX(p_v, 0.001f);
	if (is_inside_world()) {
		_rebuild();
	}
}

void PhysXCloth3D::set_wind_enabled(bool p_v) {
	wind_enabled = p_v;
}

void PhysXCloth3D::set_wind_area(const NodePath &p_v) {
	wind_area = p_v;
	update_gizmos();
}

void PhysXCloth3D::set_wind(const Vector3 &p_v) {
	wind = p_v;
	update_gizmos();
}

void PhysXCloth3D::set_wind_turbulence(float p_v) {
	wind_turbulence = MAX(p_v, 0.0f);
}

void PhysXCloth3D::set_drag(float p_v) {
	drag = MAX(p_v, 0.0f);
	_apply_solver_settings();
}

void PhysXCloth3D::set_lift(float p_v) {
	lift = MAX(p_v, 0.0f);
	_apply_solver_settings();
}

void PhysXCloth3D::set_pin_mode(PinMode p_v) {
	pin_mode = p_v;
	if (built) {
		// _resolve_pins pushes the new pin set to the GPU cloth (cloth_set_pinned);
		// re-seeding the CPU solver there would repaint the render mesh with the
		// REST pose, replacing the live GPU-deformed surface for a frame and
		// desyncing the pin gizmo positions until the next read-back.
		_resolve_pins();
		if (gpu_cloth.is_valid()) {
			_update_gpu_mesh();
		} else {
			solver.reset(get_global_transform());
			_update_mesh();
		}
	}
	update_configuration_warnings();
	update_gizmos();
}

void PhysXCloth3D::set_pinned_vertices(const PackedInt32Array &p_v) {
	pinned_vertices = p_v;
	if (built) {
		_resolve_pins();
		if (gpu_cloth.is_valid()) {
			_update_gpu_mesh();
		} else {
			solver.reset(get_global_transform());
			_update_mesh();
		}
	}
	update_configuration_warnings();
	update_gizmos();
}

void PhysXCloth3D::set_anchor_path(const NodePath &p_v) {
	anchor_path = p_v;
	if (built) {
		reset();
	}
	update_configuration_warnings();
}

void PhysXCloth3D::set_collision_enabled(bool p_v) {
	collision_enabled = p_v;
}

void PhysXCloth3D::set_collision_mask(uint32_t p_v) {
	collision_mask = p_v;
	if (gpu_cloth.is_valid()) {
		if (PhysXServer3D *server = PhysXServer3D::get_singleton()) {
			server->cloth_set_collision_layer_and_mask(gpu_cloth, collision_layer, collision_mask);
		}
	}
}

void PhysXCloth3D::set_collision_layer(uint32_t p_v) {
	collision_layer = p_v;
	if (gpu_cloth.is_valid()) {
		if (PhysXServer3D *server = PhysXServer3D::get_singleton()) {
			server->cloth_set_collision_layer_and_mask(gpu_cloth, collision_layer, collision_mask);
		}
	}
}

uint32_t PhysXCloth3D::get_collision_layer() const {
	return collision_layer;
}

void PhysXCloth3D::add_collision_exception(const RID &p_body) {
	if (!p_body.is_valid() || collision_exceptions.has(p_body)) {
		return;
	}
	collision_exceptions.append(p_body);
	if (gpu_cloth.is_valid()) {
		if (PhysXServer3D *server = PhysXServer3D::get_singleton()) {
			server->cloth_add_collision_exception(gpu_cloth, p_body);
		}
	}
}

void PhysXCloth3D::remove_collision_exception(const RID &p_body) {
	collision_exceptions.erase(p_body);
	if (gpu_cloth.is_valid()) {
		if (PhysXServer3D *server = PhysXServer3D::get_singleton()) {
			server->cloth_remove_collision_exception(gpu_cloth, p_body);
		}
	}
}

Array PhysXCloth3D::get_collision_exceptions() const {
	return collision_exceptions.duplicate();
}

void PhysXCloth3D::set_friction(float p_v) {
	friction = CLAMP(p_v, 0.0f, 1.0f);
}

void PhysXCloth3D::set_simulating(bool p_v) {
	simulating = p_v;
}

void PhysXCloth3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("reset"), &PhysXCloth3D::reset);
	ClassDB::bind_method(D_METHOD("get_vertex_count"), &PhysXCloth3D::get_vertex_count);
	ClassDB::bind_method(D_METHOD("get_pinned_positions"), &PhysXCloth3D::get_pinned_positions);
	ClassDB::bind_method(D_METHOD("is_gpu_accelerated"), &PhysXCloth3D::is_gpu_accelerated);

	ClassDB::bind_method(D_METHOD("set_simulation_mode", "mode"), &PhysXCloth3D::set_simulation_mode);
	ClassDB::bind_method(D_METHOD("get_simulation_mode"), &PhysXCloth3D::get_simulation_mode);
	ClassDB::bind_method(D_METHOD("set_source_mesh", "mesh"), &PhysXCloth3D::set_source_mesh);
	ClassDB::bind_method(D_METHOD("get_source_mesh"), &PhysXCloth3D::get_source_mesh);
	ClassDB::bind_method(D_METHOD("set_grid_columns", "columns"), &PhysXCloth3D::set_grid_columns);
	ClassDB::bind_method(D_METHOD("get_grid_columns"), &PhysXCloth3D::get_grid_columns);
	ClassDB::bind_method(D_METHOD("set_grid_rows", "rows"), &PhysXCloth3D::set_grid_rows);
	ClassDB::bind_method(D_METHOD("get_grid_rows"), &PhysXCloth3D::get_grid_rows);
	ClassDB::bind_method(D_METHOD("set_grid_size", "size"), &PhysXCloth3D::set_grid_size);
	ClassDB::bind_method(D_METHOD("get_grid_size"), &PhysXCloth3D::get_grid_size);

	ClassDB::bind_method(D_METHOD("set_substeps", "substeps"), &PhysXCloth3D::set_substeps);
	ClassDB::bind_method(D_METHOD("get_substeps"), &PhysXCloth3D::get_substeps);
	ClassDB::bind_method(D_METHOD("set_stiffness", "stiffness"), &PhysXCloth3D::set_stiffness);
	ClassDB::bind_method(D_METHOD("get_stiffness"), &PhysXCloth3D::get_stiffness);
	ClassDB::bind_method(D_METHOD("set_shear_stiffness", "stiffness"), &PhysXCloth3D::set_shear_stiffness);
	ClassDB::bind_method(D_METHOD("get_shear_stiffness"), &PhysXCloth3D::get_shear_stiffness);
	ClassDB::bind_method(D_METHOD("set_bend_stiffness", "stiffness"), &PhysXCloth3D::set_bend_stiffness);
	ClassDB::bind_method(D_METHOD("get_bend_stiffness"), &PhysXCloth3D::get_bend_stiffness);
	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &PhysXCloth3D::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &PhysXCloth3D::get_damping);
	ClassDB::bind_method(D_METHOD("set_density", "density"), &PhysXCloth3D::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &PhysXCloth3D::get_density);

	ClassDB::bind_method(D_METHOD("set_wind_enabled", "enabled"), &PhysXCloth3D::set_wind_enabled);
	ClassDB::bind_method(D_METHOD("is_wind_enabled"), &PhysXCloth3D::is_wind_enabled);
	ClassDB::bind_method(D_METHOD("set_wind_area", "path"), &PhysXCloth3D::set_wind_area);
	ClassDB::bind_method(D_METHOD("get_wind_area"), &PhysXCloth3D::get_wind_area);
	ClassDB::bind_method(D_METHOD("set_wind", "wind"), &PhysXCloth3D::set_wind);
	ClassDB::bind_method(D_METHOD("get_wind"), &PhysXCloth3D::get_wind);
	ClassDB::bind_method(D_METHOD("set_wind_turbulence", "turbulence"), &PhysXCloth3D::set_wind_turbulence);
	ClassDB::bind_method(D_METHOD("get_wind_turbulence"), &PhysXCloth3D::get_wind_turbulence);
	ClassDB::bind_method(D_METHOD("set_drag", "drag"), &PhysXCloth3D::set_drag);
	ClassDB::bind_method(D_METHOD("get_drag"), &PhysXCloth3D::get_drag);
	ClassDB::bind_method(D_METHOD("set_lift", "lift"), &PhysXCloth3D::set_lift);
	ClassDB::bind_method(D_METHOD("get_lift"), &PhysXCloth3D::get_lift);

	ClassDB::bind_method(D_METHOD("set_pin_mode", "mode"), &PhysXCloth3D::set_pin_mode);
	ClassDB::bind_method(D_METHOD("get_pin_mode"), &PhysXCloth3D::get_pin_mode);
	ClassDB::bind_method(D_METHOD("set_pinned_vertices", "vertices"), &PhysXCloth3D::set_pinned_vertices);
	ClassDB::bind_method(D_METHOD("get_pinned_vertices"), &PhysXCloth3D::get_pinned_vertices);
	ClassDB::bind_method(D_METHOD("set_anchor_path", "path"), &PhysXCloth3D::set_anchor_path);
	ClassDB::bind_method(D_METHOD("get_anchor_path"), &PhysXCloth3D::get_anchor_path);

	ClassDB::bind_method(D_METHOD("set_collision_enabled", "enabled"), &PhysXCloth3D::set_collision_enabled);
	ClassDB::bind_method(D_METHOD("is_collision_enabled"), &PhysXCloth3D::is_collision_enabled);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &PhysXCloth3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &PhysXCloth3D::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &PhysXCloth3D::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &PhysXCloth3D::get_collision_layer);
	ClassDB::bind_method(D_METHOD("add_collision_exception", "body"), &PhysXCloth3D::add_collision_exception);
	ClassDB::bind_method(D_METHOD("remove_collision_exception", "body"), &PhysXCloth3D::remove_collision_exception);
	ClassDB::bind_method(D_METHOD("get_collision_exceptions"), &PhysXCloth3D::get_collision_exceptions);
	ClassDB::bind_method(D_METHOD("set_friction", "friction"), &PhysXCloth3D::set_friction);
	ClassDB::bind_method(D_METHOD("get_friction"), &PhysXCloth3D::get_friction);

	ClassDB::bind_method(D_METHOD("set_simulating", "simulating"), &PhysXCloth3D::set_simulating);
	ClassDB::bind_method(D_METHOD("is_simulating"), &PhysXCloth3D::is_simulating);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "simulation_mode", PROPERTY_HINT_ENUM, "Auto,GPU,CPU"), "set_simulation_mode", "get_simulation_mode");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "source_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_source_mesh", "get_source_mesh");
	ADD_GROUP("Grid", "grid_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "grid_columns", PROPERTY_HINT_RANGE, "2,256,1"), "set_grid_columns", "get_grid_columns");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "grid_rows", PROPERTY_HINT_RANGE, "2,256,1"), "set_grid_rows", "get_grid_rows");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "grid_size", PROPERTY_HINT_NONE, "suffix:m"), "set_grid_size", "get_grid_size");
	ADD_GROUP("Simulation", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "simulating"), "set_simulating", "is_simulating");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "substeps", PROPERTY_HINT_RANGE, "1,40,1"), "set_substeps", "get_substeps");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stiffness", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_stiffness", "get_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shear_stiffness", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_shear_stiffness", "get_shear_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bend_stiffness", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_bend_stiffness", "get_bend_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater"), "set_damping", "get_damping");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density", PROPERTY_HINT_RANGE, "0.001,5,0.001,or_greater,suffix:kg/m²"), "set_density", "get_density");
	ADD_GROUP("Wind", "wind_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "wind_enabled"), "set_wind_enabled", "is_wind_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "wind_area", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Area3D"), "set_wind_area", "get_wind_area");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "wind", PROPERTY_HINT_NONE, "suffix:m/s"), "set_wind", "get_wind");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wind_turbulence", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_wind_turbulence", "get_wind_turbulence");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "drag", PROPERTY_HINT_RANGE, "0,4,0.01"), "set_drag", "get_drag");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lift", PROPERTY_HINT_RANGE, "0,2,0.01"), "set_lift", "get_lift");
	ADD_GROUP("Pinning", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "pin_mode", PROPERTY_HINT_ENUM, "None,Top Edge,Top Corners,Left Edge,Right Edge"), "set_pin_mode", "get_pin_mode");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "pinned_vertices"), "set_pinned_vertices", "get_pinned_vertices");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "anchor_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Node3D"), "set_anchor_path", "get_anchor_path");
	ADD_GROUP("Collision", "collision_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collision_enabled"), "set_collision_enabled", "is_collision_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_friction", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_friction", "get_friction");

	BIND_ENUM_CONSTANT(PIN_NONE);
	BIND_ENUM_CONSTANT(PIN_TOP_EDGE);
	BIND_ENUM_CONSTANT(PIN_TOP_CORNERS);
	BIND_ENUM_CONSTANT(PIN_LEFT_EDGE);
	BIND_ENUM_CONSTANT(PIN_RIGHT_EDGE);

	BIND_ENUM_CONSTANT(SIM_AUTO);
	BIND_ENUM_CONSTANT(SIM_GPU);
	BIND_ENUM_CONSTANT(SIM_CPU);
}

PhysXCloth3D::PhysXCloth3D() {
	set_notify_transform(true);
}

PhysXCloth3D::~PhysXCloth3D() {
}

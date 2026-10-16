/**************************************************************************/
/*  physx_editor_plugin.cpp                                               */
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

#include "physx_editor_plugin.h"

#include "../nodes/physx_cloth_3d.h"
#include "../nodes/physx_gas_3d.h"
#include "../nodes/physx_gas_emitter_3d.h"
#include "../nodes/physx_particle_fluid_3d.h"
#include "../vehicles/physx_vehicle_wheel_3d.h"

#ifdef GODOT_PHYSX_BLAST
#include "../blast/physx_destructible_3d.h"
#include "physx_blast_asset_inspector_plugin.h"
#include "physx_blast_context_menu_plugin.h"
#include "physx_blast_fracture_dialog.h"
#include "physx_blast_icons.h"
#endif

#include "editor/editor_undo_redo_manager.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "editor/settings/editor_settings.h"
#include "scene/3d/physics/area_3d.h"

#ifdef GODOT_PHYSX_BLAST
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/inspector/editor_context_menu_plugin.h"
#endif

PhysXParticleFluid3DGizmoPlugin::PhysXParticleFluid3DGizmoPlugin() {
	helper.instantiate();
	create_material("spawn_region", Color(0.4, 0.7, 1.0));
	create_material("emission", Color(0.3, 0.85, 1.0));
	create_handle_material("handles");
}

bool PhysXParticleFluid3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<PhysXParticleFluid3D>(p_spatial) != nullptr;
}

String PhysXParticleFluid3DGizmoPlugin::get_gizmo_name() const {
	return "PhysXParticleFluid3D";
}

int PhysXParticleFluid3DGizmoPlugin::get_priority() const {
	return -1;
}

bool PhysXParticleFluid3DGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

void PhysXParticleFluid3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	PhysXParticleFluid3D *fluid = Object::cast_to<PhysXParticleFluid3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();

	const Ref<Material> region_material = get_material("spawn_region", p_gizmo);
	const Ref<Material> emission_material = get_material("emission", p_gizmo);

	// Spawn region: a wireframe box centered on the node.
	const Vector3 size = fluid->get_spawn_region_size();
	AABB aabb;
	aabb.size = size;
	aabb.position = size * -0.5;

	Vector<Vector3> lines;
	for (int i = 0; i < 12; i++) {
		Vector3 a, b;
		aabb.get_edge(i, a, b);
		lines.push_back(a);
		lines.push_back(b);
	}
	p_gizmo->add_lines(lines, region_material);
	p_gizmo->add_collision_segments(lines);
	p_gizmo->add_handles(helper->box_get_handles(size), get_material("handles"));

	// Emission: a ring for the radius and an arrow for the velocity. The moving
	// particle preview (a CPU animation on the node itself) shows the actual jet.
	const float radius = fluid->get_emission_radius();
	const Vector3 vel = fluid->get_emission_velocity();
	Vector3 dir = vel.normalized();
	if (dir.is_zero_approx()) {
		dir = Vector3(0, -1, 0);
	}
	Vector3 ortho_a = dir.cross(Vector3(0, 1, 0));
	if (ortho_a.is_zero_approx()) {
		ortho_a = dir.cross(Vector3(1, 0, 0));
	}
	ortho_a.normalize();
	const Vector3 ortho_b = dir.cross(ortho_a).normalized();

	Vector<Vector3> em;
	const int segs = 28;
	for (int i = 0; i < segs; i++) {
		const float a0 = Math::TAU * i / segs;
		const float a1 = Math::TAU * (i + 1) / segs;
		em.push_back((ortho_a * Math::cos(a0) + ortho_b * Math::sin(a0)) * radius);
		em.push_back((ortho_a * Math::cos(a1) + ortho_b * Math::sin(a1)) * radius);
	}
	if (!vel.is_zero_approx()) {
		const float len = CLAMP(vel.length() * 0.3f, 0.4f, MAX(size.length(), 1.0f));
		em.push_back(Vector3());
		em.push_back(dir * len);
		const float h = len * 0.12f;
		em.push_back(dir * len);
		em.push_back(dir * (len - h) + ortho_a * h);
		em.push_back(dir * len);
		em.push_back(dir * (len - h) - ortho_a * h);
	}
	p_gizmo->add_lines(em, emission_material);
}

String PhysXParticleFluid3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return helper->box_get_handle_name(p_id);
}

Variant PhysXParticleFluid3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return Object::cast_to<PhysXParticleFluid3D>(p_gizmo->get_node_3d())->get_spawn_region_size();
}

void PhysXParticleFluid3DGizmoPlugin::begin_handle_action(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) {
	helper->initialize_handle_action(get_handle_value(p_gizmo, p_id, p_secondary), p_gizmo->get_node_3d()->get_global_transform());
}

void PhysXParticleFluid3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	PhysXParticleFluid3D *fluid = Object::cast_to<PhysXParticleFluid3D>(p_gizmo->get_node_3d());
	Vector3 size = fluid->get_spawn_region_size();

	Vector3 sg[2];
	helper->get_segment(p_camera, p_point, sg);

	Vector3 position; // ignored: the region stays centered on the node
	helper->box_set_handle(sg, p_id, size, position);
	fluid->set_spawn_region_size(size);
}

void PhysXParticleFluid3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	PhysXParticleFluid3D *fluid = Object::cast_to<PhysXParticleFluid3D>(p_gizmo->get_node_3d());
	if (p_cancel) {
		fluid->set_spawn_region_size(p_restore);
		return;
	}
	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Change Fluid Spawn Region"));
	ur->add_do_method(fluid, "set_spawn_region_size", fluid->get_spawn_region_size());
	ur->add_undo_method(fluid, "set_spawn_region_size", p_restore);
	ur->commit_action();
}

// --- PhysXCloth3D ------------------------------------------------------------

PhysXCloth3DGizmoPlugin::PhysXCloth3DGizmoPlugin() {
	helper.instantiate();
	create_material("cloth_outline", Color(0.55, 0.8, 0.45));
	create_material("cloth_wind", Color(0.5, 0.85, 1.0));
	create_material("cloth_pins", Color(1.0, 0.55, 0.2), false, true); // on top, so pins show through the cloth
	create_handle_material("handles");
}

bool PhysXCloth3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<PhysXCloth3D>(p_spatial) != nullptr;
}

String PhysXCloth3DGizmoPlugin::get_gizmo_name() const {
	return "PhysXCloth3D";
}

int PhysXCloth3DGizmoPlugin::get_priority() const {
	return -1;
}

bool PhysXCloth3DGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

void PhysXCloth3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	PhysXCloth3D *cloth = Object::cast_to<PhysXCloth3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();

	const Vector2 gs = cloth->get_grid_size();
	const Vector3 hx = Vector3(gs.x * 0.5, 0, 0);
	const Vector3 hy = Vector3(0, gs.y * 0.5, 0);

	// Rest grid outline (the node itself draws the filled rest mesh).
	Vector<Vector3> outline;
	const Vector3 c00 = -hx - hy;
	const Vector3 c10 = hx - hy;
	const Vector3 c11 = hx + hy;
	const Vector3 c01 = -hx + hy;
	outline.push_back(c00);
	outline.push_back(c10);
	outline.push_back(c10);
	outline.push_back(c11);
	outline.push_back(c11);
	outline.push_back(c01);
	outline.push_back(c01);
	outline.push_back(c00);
	p_gizmo->add_lines(outline, get_material("cloth_outline", p_gizmo));
	p_gizmo->add_collision_segments(outline);

	// Size handles: treat the grid as a flat box so the tested box handle logic
	// can drive width/height.
	p_gizmo->add_handles(helper->box_get_handles(Vector3(gs.x, gs.y, 0.02)), get_material("handles", p_gizmo));

	// Pinned vertices: a small cross at each.
	const PackedVector3Array pins = cloth->get_pinned_positions();
	if (!pins.is_empty()) {
		const float s = MAX(gs.length() * 0.02f, 0.02f);
		Vector<Vector3> marks;
		for (const Vector3 &p : pins) {
			marks.push_back(p - Vector3(s, 0, 0));
			marks.push_back(p + Vector3(s, 0, 0));
			marks.push_back(p - Vector3(0, s, 0));
			marks.push_back(p + Vector3(0, s, 0));
			marks.push_back(p - Vector3(0, 0, s));
			marks.push_back(p + Vector3(0, 0, s));
		}
		p_gizmo->add_lines(marks, get_material("cloth_pins", p_gizmo));
	}

	// Wind arrow: the constant wind, or the assigned Area3D's direction.
	Vector3 wdir = cloth->get_wind();
	Area3D *area = Object::cast_to<Area3D>(cloth->get_node_or_null(cloth->get_wind_area()));
	if (area) {
		Node3D *src = Object::cast_to<Node3D>(area->get_node_or_null(area->get_wind_source_path()));
		if (src) {
			const Transform3D wt = cloth->get_global_transform().affine_inverse() * src->get_global_transform();
			wdir += -wt.basis.get_column(Vector3::AXIS_Z).normalized() * MAX(area->get_wind_force_magnitude(), 1.0);
		}
	}
	if (!wdir.is_zero_approx()) {
		Vector3 dir = wdir.normalized();
		Vector3 up = dir.cross(Vector3(0, 1, 0));
		if (up.is_zero_approx()) {
			up = dir.cross(Vector3(1, 0, 0));
		}
		up.normalize();
		const float len = CLAMP(wdir.length() * 0.15f, 0.5f, MAX(gs.length(), 1.0f));
		const float head = len * 0.18f;
		const Vector3 base = -dir * len * 0.5;
		const Vector3 tip = dir * len * 0.5;
		Vector<Vector3> arrow;
		arrow.push_back(base);
		arrow.push_back(tip);
		arrow.push_back(tip);
		arrow.push_back(tip - dir * head + up * head);
		arrow.push_back(tip);
		arrow.push_back(tip - dir * head - up * head);
		p_gizmo->add_lines(arrow, get_material("cloth_wind", p_gizmo));
	}
}

String PhysXCloth3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	return helper->box_get_handle_name(p_id);
}

Variant PhysXCloth3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	const Vector2 gs = Object::cast_to<PhysXCloth3D>(p_gizmo->get_node_3d())->get_grid_size();
	return Vector3(gs.x, gs.y, 0.02);
}

void PhysXCloth3DGizmoPlugin::begin_handle_action(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) {
	helper->initialize_handle_action(get_handle_value(p_gizmo, p_id, p_secondary), p_gizmo->get_node_3d()->get_global_transform());
}

void PhysXCloth3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	PhysXCloth3D *cloth = Object::cast_to<PhysXCloth3D>(p_gizmo->get_node_3d());
	Vector3 size(cloth->get_grid_size().x, cloth->get_grid_size().y, 0.02);

	Vector3 sg[2];
	helper->get_segment(p_camera, p_point, sg);
	Vector3 position;
	helper->box_set_handle(sg, p_id, size, position);
	cloth->set_grid_size(Vector2(MAX(size.x, 0.05), MAX(size.y, 0.05)));
}

void PhysXCloth3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	PhysXCloth3D *cloth = Object::cast_to<PhysXCloth3D>(p_gizmo->get_node_3d());
	const Vector3 restore = p_restore;
	if (p_cancel) {
		cloth->set_grid_size(Vector2(restore.x, restore.y));
		return;
	}
	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Change Cloth Grid Size"));
	ur->add_do_method(cloth, "set_grid_size", cloth->get_grid_size());
	ur->add_undo_method(cloth, "set_grid_size", Vector2(restore.x, restore.y));
	ur->commit_action();
}

#ifdef GODOT_PHYSX_BLAST
bool PhysXDestructible3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<PhysXDestructible3D>(p_spatial) != nullptr;
}

String PhysXDestructible3DGizmoPlugin::get_gizmo_name() const {
	return "PhysXDestructible3D";
}

int PhysXDestructible3DGizmoPlugin::get_priority() const {
	return -1;
}

bool PhysXDestructible3DGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

void PhysXDestructible3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	PhysXDestructible3D *destructible = Object::cast_to<PhysXDestructible3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();
	const Ref<TriangleMesh> tm = destructible->generate_triangle_mesh();
	if (tm.is_valid()) {
		p_gizmo->add_collision_triangles(tm);
	}
	// Manual selection-box outline -- the node is not a VisualInstance3D, so
	// the editor won't draw this on its own. The is_selected() gate matches
	// the real dashed selection box's own behavior (only visible while the
	// node is actually selected) -- without it this drew unconditionally on
	// every instance, all the time.
	const AABB aabb = destructible->get_aabb();
	if (p_gizmo->is_selected() && aabb.size != Vector3()) {
		Vector<Vector3> lines;
		for (int i = 0; i < 12; i++) {
			Vector3 a, b;
			aabb.get_edge(i, a, b);
			lines.push_back(a);
			lines.push_back(b);
		}
		p_gizmo->add_lines(lines, get_material("selection_box", p_gizmo));
	}
}

PhysXDestructible3DGizmoPlugin::PhysXDestructible3DGizmoPlugin() {
	create_material("selection_box", Color(1.0, 0.8, 0.2));
}
#endif

// ---------------------------------------------------------------------------
// Gas / gas-emitter / vehicle-wheel gizmos (ported from the reference module's
// editor plugin). Read-only wireframes: enough to see and select the otherwise
// invisible simulation volumes before Play.
// ---------------------------------------------------------------------------

static void physx_ed_add_box_wireframe(EditorNode3DGizmo *p_gizmo, const Vector3 &p_size, const Ref<Material> &p_material) {
	AABB aabb;
	aabb.size = p_size;
	aabb.position = p_size * -0.5;
	Vector<Vector3> lines;
	for (int i = 0; i < 12; i++) {
		Vector3 a, b;
		aabb.get_edge(i, a, b);
		lines.push_back(a);
		lines.push_back(b);
	}
	p_gizmo->add_lines(lines, p_material);
	p_gizmo->add_collision_segments(lines);
}

static void physx_ed_add_sphere_wireframe(EditorNode3DGizmo *p_gizmo, float p_radius, const Ref<Material> &p_material) {
	const int segs = 24;
	Vector<Vector3> lines;
	const Vector3 axes[3][2] = {
		{ Vector3(1, 0, 0), Vector3(0, 1, 0) },
		{ Vector3(1, 0, 0), Vector3(0, 0, 1) },
		{ Vector3(0, 1, 0), Vector3(0, 0, 1) },
	};
	for (int ring = 0; ring < 3; ring++) {
		for (int i = 0; i < segs; i++) {
			const float a0 = Math::TAU * i / segs;
			const float a1 = Math::TAU * (i + 1) / segs;
			lines.push_back((axes[ring][0] * Math::cos(a0) + axes[ring][1] * Math::sin(a0)) * p_radius);
			lines.push_back((axes[ring][0] * Math::cos(a1) + axes[ring][1] * Math::sin(a1)) * p_radius);
		}
	}
	p_gizmo->add_lines(lines, p_material);
	p_gizmo->add_collision_segments(lines);
}

// A simple arrow (shaft + small head) along p_dir, scaled by its own length —
// a quick visual read of the injected-velocity direction/speed.
static void physx_ed_add_velocity_arrow(EditorNode3DGizmo *p_gizmo, const Vector3 &p_local_velocity, const Ref<Material> &p_material) {
	if (p_local_velocity.is_zero_approx()) {
		return;
	}
	const Vector3 dir = p_local_velocity.normalized();
	const float len = CLAMP(p_local_velocity.length() * 0.3f, 0.1f, 2.0f);
	Vector3 ortho_a = dir.cross(Vector3(0, 1, 0));
	if (ortho_a.is_zero_approx()) {
		ortho_a = dir.cross(Vector3(1, 0, 0));
	}
	ortho_a.normalize();
	const Vector3 tip = dir * len;
	Vector<Vector3> lines;
	lines.push_back(Vector3());
	lines.push_back(tip);
	lines.push_back(tip);
	lines.push_back(tip - dir * (len * 0.25f) + ortho_a * (len * 0.12f));
	lines.push_back(tip);
	lines.push_back(tip - dir * (len * 0.25f) - ortho_a * (len * 0.12f));
	p_gizmo->add_lines(lines, p_material);
}

PhysXGas3DGizmoPlugin::PhysXGas3DGizmoPlugin() {
	create_material("domain", Color(0.5, 0.8, 1.0));
}

bool PhysXGas3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<PhysXGas3D>(p_spatial) != nullptr;
}

String PhysXGas3DGizmoPlugin::get_gizmo_name() const {
	return "PhysXGas3D";
}

int PhysXGas3DGizmoPlugin::get_priority() const {
	return -1;
}

bool PhysXGas3DGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

// Box wireframe drawn from WORLD-space corners, transformed into the node's
// local space by p_node_global_inv — renders at a fixed world position once
// the gas domain has actually configured (the domain freezes in world space
// at that point and stops tracking the node, see GasSolver::configure).
static void physx_ed_add_world_aabb_wireframe(EditorNode3DGizmo *p_gizmo, const Transform3D &p_node_global_inv, const Vector3 &p_world_anchor, const Vector3 &p_world_size, const Ref<Material> &p_material) {
	AABB aabb(p_world_anchor, p_world_size);
	Vector<Vector3> lines;
	for (int i = 0; i < 12; i++) {
		Vector3 a, b;
		aabb.get_edge(i, a, b);
		lines.push_back(p_node_global_inv.xform(a));
		lines.push_back(p_node_global_inv.xform(b));
	}
	p_gizmo->add_lines(lines, p_material);
	p_gizmo->add_collision_segments(lines);
}

void PhysXGas3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	PhysXGas3D *gas = Object::cast_to<PhysXGas3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();
	if (gas->is_domain_configured()) {
		physx_ed_add_world_aabb_wireframe(p_gizmo, gas->get_global_transform().affine_inverse(),
				gas->get_configured_domain_anchor(), gas->get_configured_domain_size(),
				get_material("domain", p_gizmo));
	} else {
		// Not configured yet (before the scene has ever run, or right after a
		// domain_size/cell_size change forces a reconfigure) — the node's
		// current transform IS where it'll end up, so follow it.
		physx_ed_add_box_wireframe(p_gizmo, gas->get_domain_size(), get_material("domain", p_gizmo));
	}
}

PhysXGasEmitter3DGizmoPlugin::PhysXGasEmitter3DGizmoPlugin() {
	create_material("emitter", Color(1.0, 0.7, 0.3));
	create_material("velocity", Color(1.0, 0.9, 0.5));
}

bool PhysXGasEmitter3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<PhysXGasEmitter3D>(p_spatial) != nullptr;
}

String PhysXGasEmitter3DGizmoPlugin::get_gizmo_name() const {
	return "PhysXGasEmitter3D";
}

int PhysXGasEmitter3DGizmoPlugin::get_priority() const {
	return -1;
}

bool PhysXGasEmitter3DGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

void PhysXGasEmitter3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	PhysXGasEmitter3D *emitter = Object::cast_to<PhysXGasEmitter3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();
	const Ref<Material> emitter_material = get_material("emitter", p_gizmo);
	if (emitter->get_shape() == PhysXGasEmitter3D::SHAPE_BOX) {
		physx_ed_add_box_wireframe(p_gizmo, emitter->get_size(), emitter_material);
	} else {
		physx_ed_add_sphere_wireframe(p_gizmo, emitter->get_radius(), emitter_material);
	}
	physx_ed_add_velocity_arrow(p_gizmo, emitter->get_velocity(), get_material("velocity", p_gizmo));
}

// A wireframe disc in the Y-Z plane at p_center_y along local Y — the wheel's
// axle runs along local X, so its tire silhouette lies in Y-Z.
static void physx_ed_add_wheel_disc_wireframe(EditorNode3DGizmo *p_gizmo, float p_radius, float p_center_y, const Ref<Material> &p_material) {
	const int segs = 24;
	Vector<Vector3> lines;
	for (int i = 0; i < segs; i++) {
		const float a0 = Math::TAU * i / segs;
		const float a1 = Math::TAU * (i + 1) / segs;
		lines.push_back(Vector3(0.0f, p_center_y + Math::sin(a0) * p_radius, Math::cos(a0) * p_radius));
		lines.push_back(Vector3(0.0f, p_center_y + Math::sin(a1) * p_radius, Math::cos(a1) * p_radius));
	}
	p_gizmo->add_lines(lines, p_material);
}

PhysXVehicleWheel3DGizmoPlugin::PhysXVehicleWheel3DGizmoPlugin() {
	// Green: the wheel's own silhouette (radius) at its authored resting
	// position — the node's origin IS where the wheel sits at rest (authored
	// wheel transforms feed the vehicle setup directly), so this is exact.
	create_material("rest_estimate", Color(0.3, 1.0, 0.4));
}

bool PhysXVehicleWheel3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<PhysXVehicleWheel3D>(p_spatial) != nullptr;
}

String PhysXVehicleWheel3DGizmoPlugin::get_gizmo_name() const {
	return "PhysXVehicleWheel3D";
}

int PhysXVehicleWheel3DGizmoPlugin::get_priority() const {
	return -1;
}

bool PhysXVehicleWheel3DGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

void PhysXVehicleWheel3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	PhysXVehicleWheel3D *wheel = Object::cast_to<PhysXVehicleWheel3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();
	physx_ed_add_wheel_disc_wireframe(p_gizmo, (float)wheel->get_radius(), 0.0f, get_material("rest_estimate", p_gizmo));

	// Forward arrow from the ground-contact point (bottom of the tire,
	// y = -radius), pointing toward the vehicle's nose (+Z).
	Vector<Vector3> fwd_arrow;
	const float len = (float)wheel->get_radius() * 1.5f;
	const float contact_y = -(float)wheel->get_radius();
	fwd_arrow.push_back(Vector3(0, contact_y, 0));
	fwd_arrow.push_back(Vector3(0, contact_y, len));
	fwd_arrow.push_back(Vector3(0, contact_y, len));
	fwd_arrow.push_back(Vector3(0.08f * len / 1.5f, contact_y, len * 0.8f));
	fwd_arrow.push_back(Vector3(0, contact_y, len));
	fwd_arrow.push_back(Vector3(-0.08f * len / 1.5f, contact_y, len * 0.8f));
	p_gizmo->add_lines(fwd_arrow, get_material("rest_estimate", p_gizmo));
}

PhysXEditorPlugin::PhysXEditorPlugin() {
	Ref<PhysXParticleFluid3DGizmoPlugin> fluid_gizmo;
	fluid_gizmo.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(fluid_gizmo);

	Ref<PhysXCloth3DGizmoPlugin> cloth_gizmo;
	cloth_gizmo.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(cloth_gizmo);

	Ref<PhysXGas3DGizmoPlugin> gas_gizmo;
	gas_gizmo.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(gas_gizmo);

	Ref<PhysXGasEmitter3DGizmoPlugin> gas_emitter_gizmo;
	gas_emitter_gizmo.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(gas_emitter_gizmo);

	Ref<PhysXVehicleWheel3DGizmoPlugin> vehicle_wheel_gizmo;
	vehicle_wheel_gizmo.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(vehicle_wheel_gizmo);

#ifdef GODOT_PHYSX_BLAST
	Ref<PhysXDestructible3DGizmoPlugin> destructible_gizmo;
	destructible_gizmo.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(destructible_gizmo);

	blast_fracture_dialog = memnew(PhysXBlastFractureDialog);
	EditorNode::get_singleton()->get_gui_base()->add_child(blast_fracture_dialog);

	Ref<PhysXBlastFractureMenuPlugin> blast_scene_tree_menu;
	blast_scene_tree_menu.instantiate(EditorContextMenuPlugin::CONTEXT_SLOT_SCENE_TREE, blast_fracture_dialog);
	EditorContextMenuPluginManager::get_singleton()->add_plugin(EditorContextMenuPlugin::CONTEXT_SLOT_SCENE_TREE, blast_scene_tree_menu);

	Ref<PhysXBlastFractureMenuPlugin> blast_filesystem_menu;
	blast_filesystem_menu.instantiate(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, blast_fracture_dialog);
	EditorContextMenuPluginManager::get_singleton()->add_plugin(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, blast_filesystem_menu);

	Ref<EditorInspectorPluginPhysXBlastAsset> blast_asset_inspector;
	blast_asset_inspector.instantiate();
	add_inspector_plugin(blast_asset_inspector);

	// Otherwise PhysXBlastAsset falls back to the engine's generic
	// Resource/"blank paper" icon everywhere (FileSystem dock, Inspector
	// header) since nothing in the editor theme matches its class name.
	Ref<Texture2D> blast_asset_icon = physx_blast_asset_make_icon();
	if (blast_asset_icon.is_valid()) {
		EditorNode::get_singleton()->get_editor_theme()->set_icon("PhysXBlastAsset", EditorStringName(EditorIcons), blast_asset_icon);
	}
#endif
}

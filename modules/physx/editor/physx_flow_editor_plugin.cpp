/**************************************************************************/
/*  physx_flow_editor_plugin.cpp                                          */
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

#include "physx_flow_editor_plugin.h"

#ifdef GODOT_PHYSX_FLOW

#include "../flow/physx_flow_collider_3d.h"
#include "../flow/physx_flow_emitter_3d.h"
#include "../flow/physx_flow_simulation_3d.h"

// ---- Emitter gizmo ---------------------------------------------------------

PhysXFlowEmitter3DGizmoPlugin::PhysXFlowEmitter3DGizmoPlugin() {
	helper.instantiate();
	create_material("shape", Color(1.0, 0.55, 0.2));
	create_material("velocity", Color(1.0, 0.75, 0.3));
}

bool PhysXFlowEmitter3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<PhysXFlowEmitter3D>(p_spatial) != nullptr;
}

String PhysXFlowEmitter3DGizmoPlugin::get_gizmo_name() const {
	return "PhysXFlowEmitter3D";
}

int PhysXFlowEmitter3DGizmoPlugin::get_priority() const {
	return -1;
}

bool PhysXFlowEmitter3DGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

void PhysXFlowEmitter3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	PhysXFlowEmitter3D *emitter = Object::cast_to<PhysXFlowEmitter3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();

	const Ref<Material> shape_material = get_material("shape", p_gizmo);
	const Ref<Material> velocity_material = get_material("velocity", p_gizmo);

	Vector<Vector3> lines;
	if (emitter->get_shape() == PhysXFlowEmitter3D::SHAPE_BOX) {
		const Vector3 size = emitter->get_size();
		AABB aabb(-size * 0.5, size);
		for (int i = 0; i < 12; i++) {
			Vector3 a, b;
			aabb.get_edge(i, a, b);
			lines.push_back(a);
			lines.push_back(b);
		}
	} else {
		// Three great-circle rings around the sphere radius.
		const float r = emitter->get_radius();
		for (int axis = 0; axis < 3; axis++) {
			const int segs = 32;
			Vector3 prev;
			for (int s = 0; s <= segs; s++) {
				const float a = (float)s / segs * Math::TAU;
				Vector3 p(Math::cos(a) * r, Math::sin(a) * r, 0);
				if (axis == 1) {
					p = Vector3(p.x, 0, p.y);
				} else if (axis == 2) {
					p = Vector3(0, p.x, p.y);
				}
				if (s > 0) {
					lines.push_back(prev);
					lines.push_back(p);
				}
				prev = p;
			}
		}
	}
	p_gizmo->add_lines(lines, shape_material);
	p_gizmo->add_collision_segments(lines);

	// Velocity arrow (+Y local, rotated with the node -- same convention the
	// simulation resolves with).
	const Vector3 dir = Vector3(0, 1, 0);
	const float length = 0.8f;
	p_gizmo->add_lines({
							   dir * 0.2f,
							   dir * length,
							   dir * length,
							   dir * (length - 0.18f) + Vector3(0.1f, 0, 0),
							   dir * length,
							   dir * (length - 0.18f) + Vector3(-0.1f, 0, 0),
							   dir * length,
							   dir * (length - 0.18f) + Vector3(0, 0, 0.1f),
							   dir * length,
							   dir * (length - 0.18f) + Vector3(0, 0, -0.1f),
					   },
			velocity_material);
}

// ---- Collider gizmo ----------------------------------------------------------

PhysXFlowCollider3DGizmoPlugin::PhysXFlowCollider3DGizmoPlugin() {
	helper.instantiate();
	create_material("shape", Color(0.45, 0.8, 1.0));
}

bool PhysXFlowCollider3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<PhysXFlowCollider3D>(p_spatial) != nullptr;
}

String PhysXFlowCollider3DGizmoPlugin::get_gizmo_name() const {
	return "PhysXFlowCollider3D";
}

int PhysXFlowCollider3DGizmoPlugin::get_priority() const {
	return -1;
}

bool PhysXFlowCollider3DGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

void PhysXFlowCollider3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	PhysXFlowCollider3D *collider = Object::cast_to<PhysXFlowCollider3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();

	const Ref<Material> shape_material = get_material("shape", p_gizmo);
	Vector<Vector3> lines;
	if (collider->get_shape() == PhysXFlowCollider3D::SHAPE_BOX) {
		const Vector3 size = collider->get_size();
		AABB aabb(-size * 0.5, size);
		for (int i = 0; i < 12; i++) {
			Vector3 a, b;
			aabb.get_edge(i, a, b);
			lines.push_back(a);
			lines.push_back(b);
		}
	} else {
		// Sphere + From-Shape3D fallback visuals: radius rings (the shape3d
		// case shows its enclosing radius -- the true footprint comes from
		// the mesh built at simulation time).
		const float r = collider->get_shape() == PhysXFlowCollider3D::SHAPE_SPHERE
				? collider->get_radius()
				: collider->get_radius();
		for (int axis = 0; axis < 3; axis++) {
			const int segs = 32;
			Vector3 prev;
			for (int s = 0; s <= segs; s++) {
				const float a = (float)s / segs * Math::TAU;
				Vector3 p(Math::cos(a) * r, Math::sin(a) * r, 0);
				if (axis == 1) {
					p = Vector3(p.x, 0, p.y);
				} else if (axis == 2) {
					p = Vector3(0, p.x, p.y);
				}
				if (s > 0) {
					lines.push_back(prev);
					lines.push_back(p);
				}
				prev = p;
			}
		}
	}
	p_gizmo->add_lines(lines, shape_material);
	p_gizmo->add_collision_segments(lines);
}

// ---- Simulation gizmo ----------------------------------------------------------

PhysXFlowSimulation3DGizmoPlugin::PhysXFlowSimulation3DGizmoPlugin() {
	create_material("bounds", Color(0.6, 1.0, 0.75));
}

bool PhysXFlowSimulation3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<PhysXFlowSimulation3D>(p_spatial) != nullptr;
}

String PhysXFlowSimulation3DGizmoPlugin::get_gizmo_name() const {
	return "PhysXFlowSimulation3D";
}

int PhysXFlowSimulation3DGizmoPlugin::get_priority() const {
	return -1;
}

bool PhysXFlowSimulation3DGizmoPlugin::is_selectable_when_hidden() const {
	return true;
}

void PhysXFlowSimulation3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	PhysXFlowSimulation3D *sim = Object::cast_to<PhysXFlowSimulation3D>(p_gizmo->get_node_3d());
	p_gizmo->clear();

	// Only the live active-bounds box: a Flow grid has no authorable domain
	// (blocks allocate around the emitters), so there is nothing to draw
	// before the simulation has run.
	const Vector3 size = sim->get_active_bounds_size();
	if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) {
		return;
	}
	const Vector3 pos = sim->get_active_bounds_min();
	AABB aabb(pos, size);
	// Gizmo lines draw in the node's LOCAL space while the bounds are
	// world-space (the node may have moved since the frame) -- convert the
	// 8 corners to local once, then emit edges from the transformed corners.
	const Transform3D to_local = sim->get_global_transform().affine_inverse();
	Vector3 corners[8];
	for (int c = 0; c < 8; c++) {
		corners[c] = to_local.xform(pos + Vector3((c & 1) ? size.x : 0.0f, (c & 2) ? size.y : 0.0f, (c & 4) ? size.z : 0.0f));
	}
	static const int edges[12][2] = {
		{ 0, 1 },
		{ 2, 3 },
		{ 4, 5 },
		{ 6, 7 },
		{ 0, 2 },
		{ 1, 3 },
		{ 4, 6 },
		{ 5, 7 },
		{ 0, 4 },
		{ 1, 5 },
		{ 2, 6 },
		{ 3, 7 },
	};
	Vector<Vector3> lines;
	for (const int *e : edges) {
		lines.push_back(corners[e[0]]);
		lines.push_back(corners[e[1]]);
	}
	p_gizmo->add_lines(lines, get_material("bounds", p_gizmo));
	p_gizmo->add_collision_segments(lines);
}

// ---- Plugin -----------------------------------------------------------------------

PhysXFlowEditorPlugin::PhysXFlowEditorPlugin() {
	add_node_3d_gizmo_plugin(memnew(PhysXFlowEmitter3DGizmoPlugin));
	add_node_3d_gizmo_plugin(memnew(PhysXFlowCollider3DGizmoPlugin));
	add_node_3d_gizmo_plugin(memnew(PhysXFlowSimulation3DGizmoPlugin));
}

#endif // GODOT_PHYSX_FLOW

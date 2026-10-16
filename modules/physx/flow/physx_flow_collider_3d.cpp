/**************************************************************************/
/*  physx_flow_collider_3d.cpp                                            */
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

#include "physx_flow_collider_3d.h"

#ifdef GODOT_PHYSX_FLOW

#include "core/object/class_db.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"

void PhysXFlowCollider3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_EXIT_TREE: {
			velocity_tracked = false;
		} break;
		default:
			break;
	}
}

// Icosphere (subdivided icosahedron): enough triangles for a voxelizer, few
// enough to keep the collision-emitter update cheap.
void PhysXFlowCollider3D::build_icosphere_soup(float p_radius, int p_subdivisions, LocalVector<float> &r_positions) {
	static const float t = 0.6180339887498949f; // (sqrt(5)-1)/2
	static const Vector3 base_verts[12] = {
		Vector3(-1, t, 0),
		Vector3(1, t, 0),
		Vector3(-1, -t, 0),
		Vector3(1, -t, 0),
		Vector3(0, -1, t),
		Vector3(0, 1, t),
		Vector3(0, -1, -t),
		Vector3(0, 1, -t),
		Vector3(t, 0, -1),
		Vector3(t, 0, 1),
		Vector3(-t, 0, -1),
		Vector3(-t, 0, 1),
	};
	static const int base_faces[20][3] = {
		{ 0, 11, 5 },
		{ 0, 5, 1 },
		{ 0, 1, 7 },
		{ 0, 7, 10 },
		{ 0, 10, 11 },
		{ 1, 5, 9 },
		{ 5, 11, 4 },
		{ 11, 10, 2 },
		{ 10, 7, 6 },
		{ 7, 1, 8 },
		{ 3, 9, 4 },
		{ 3, 4, 2 },
		{ 3, 2, 6 },
		{ 3, 6, 8 },
		{ 3, 8, 9 },
		{ 4, 9, 5 },
		{ 2, 4, 11 },
		{ 6, 2, 10 },
		{ 8, 6, 7 },
		{ 9, 8, 1 },
	};

	LocalVector<Vector3> verts;
	for (const Vector3 &v : base_verts) {
		verts.push_back(v.normalized() * p_radius);
	}
	LocalVector<int> faces;
	for (const int *f : base_faces) {
		faces.push_back(f[0]);
		faces.push_back(f[1]);
		faces.push_back(f[2]);
	}

	for (int sub = 0; sub < p_subdivisions; sub++) {
		// Midpoint subdivision with vertex dedup via a simple hash map.
		HashMap<uint64_t, int> midpoints;
		LocalVector<int> new_faces;
		for (uint32_t f = 0; f + 2 < faces.size(); f += 3) {
			int mid[3];
			for (int e = 0; e < 3; e++) {
				const int a = faces[f + e];
				const int b = faces[f + (e + 1) % 3];
				const uint64_t key = ((uint64_t)MIN(a, b) << 32) | (uint32_t)MAX(a, b);
				if (!midpoints.has(key)) {
					midpoints[key] = verts.size();
					verts.push_back(((verts[a] + verts[b]) * 0.5f).normalized() * p_radius);
				}
				mid[e] = midpoints[key];
			}
			const int tri[4][3] = { { faces[f], mid[0], mid[2] }, { mid[0], faces[f + 1], mid[1] }, { mid[2], mid[1], faces[f + 2] }, { mid[0], mid[1], mid[2] } };
			for (const int *tf : tri) {
				new_faces.push_back(tf[0]);
				new_faces.push_back(tf[1]);
				new_faces.push_back(tf[2]);
			}
		}
		faces = new_faces;
	}

	r_positions.clear();
	for (uint32_t f = 0; f + 2 < faces.size(); f += 3) {
		for (int e = 0; e < 3; e++) {
			const Vector3 &v = verts[faces[f + e]];
			r_positions.push_back(v.x);
			r_positions.push_back(v.y);
			r_positions.push_back(v.z);
		}
	}
}

bool PhysXFlowCollider3D::_build_shape3d_mesh(CollisionShape3D *p_cs) {
	Ref<Shape3D> s = p_cs->get_shape();
	if (s.is_null()) {
		return false;
	}
	// Only raw triangle data voxelizes cleanly; primitives fall back to a
	// box/sphere approximation derived from the shape's data.
	if (Ref<ConvexPolygonShape3D> convex = s; convex.is_valid()) {
		const Vector<Vector3> points = convex->get_points();
		if (points.size() < 3) {
			return false;
		}
		// Convex point clouds have no faces -- emit the points as degenerate
		// triangles (1-2-1); Flow's mesh voxelizer only needs surface
		// coverage, and a thin sliver per point voxelizes as the hull's
		// boundary points. (A full quickhull is future polish.)
		mesh_positions.clear();
		for (int i = 1; i + 1 < points.size(); i++) {
			const Vector3 &a = points[0];
			const Vector3 &b = points[i];
			const Vector3 &c = points[i + 1];
			for (const Vector3 &v : { a, b, c }) {
				mesh_positions.push_back(v.x);
				mesh_positions.push_back(v.y);
				mesh_positions.push_back(v.z);
			}
		}
		return true;
	}
	if (Ref<ConcavePolygonShape3D> concave = s; concave.is_valid()) {
		const Vector<Vector3> faces = concave->get_faces(); // triangle soup, a,b,c per face
		if (faces.size() < 3 || faces.size() % 3 != 0) {
			return false;
		}
		mesh_positions.clear();
		for (const Vector3 &v : faces) {
			mesh_positions.push_back(v.x);
			mesh_positions.push_back(v.y);
			mesh_positions.push_back(v.z);
		}
		return true;
	}
	if (Ref<BoxShape3D> box = s; box.is_valid()) {
		// Emit an explicit box mesh (24 verts, 12 tris) in place of the
		// analytic path so SHAPE_SHAPE3D handles it uniformly.
		const Vector3 h = box->get_size() * 0.5f;
		static const int axis_pairs[6][2] = {
			{ 0, 1 },
			{ 0, 1 },
			{ 0, 2 },
			{ 0, 2 },
			{ 1, 2 },
			{ 1, 2 },
		};
		static const float signs[6][2] = {
			{ -1, -1 },
			{ 1, 1 },
			{ -1, -1 },
			{ 1, 1 },
			{ -1, -1 },
			{ 1, 1 },
		};
		mesh_positions.clear();
		for (int face = 0; face < 6; face++) {
			const int u = axis_pairs[face][0];
			const int v = axis_pairs[face][1];
			const int w = 3 - u - v;
			const float su = signs[face][0];
			const float sv = signs[face][1];
			const float sw = (face % 2 == 0) ? -1.0f : 1.0f;
			Vector3 corners[4];
			for (int c = 0; c < 4; c++) {
				const float du = (c == 1 || c == 2) ? su : -su;
				const float dv = (c >= 2) ? sv : -sv;
				corners[c][u] = du * h[u];
				corners[c][v] = dv * h[v];
				corners[c][w] = sw * h[w];
			}
			const int tris[2][3] = { { 0, 1, 2 }, { 0, 2, 3 } };
			for (const int *tri : tris) {
				for (int e = 0; e < 3; e++) {
					mesh_positions.push_back(corners[tri[e]].x);
					mesh_positions.push_back(corners[tri[e]].y);
					mesh_positions.push_back(corners[tri[e]].z);
				}
			}
		}
		return true;
	}
	// Sphere/capsule/cylinder/etc.: approximate with an icosphere of the
	// shape's bounding radius (good enough for smoke collision).
	float bound_radius = 0.0f;
	if (s->is_class("SphereShape3D")) {
		bound_radius = (float)s->get("radius");
	} else if (s->is_class("CapsuleShape3D")) {
		bound_radius = ((float)s->get("radius")) + 0.5f * ((float)s->get("height"));
	} else {
		bound_radius = (float)s->get_enclosing_radius();
	}
	if (bound_radius <= 0.0f) {
		return false;
	}
	build_icosphere_soup(bound_radius, 2, mesh_positions);
	return true;
}

bool PhysXFlowCollider3D::_ensure_mesh() {
	if (shape == SHAPE_BOX) {
		return false; // analytic, no mesh needed
	}
	if (shape == SHAPE_SPHERE) {
		if (mesh_cache_key != 1) {
			build_icosphere_soup(radius, 2, mesh_positions);
			mesh_cache_key = 1;
		}
		mesh_valid = true;
		return true;
	}
	// SHAPE_SHAPE3D: rebuild when the referenced shape's RID changes (cheap
	// identity check -- RIDs are stable for a given Shape3D resource).
	CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(get_node_or_null(shape_source));
	if (cs == nullptr) {
		mesh_valid = false;
		return false;
	}
	Ref<Shape3D> s = cs->get_shape();
	if (s.is_null()) {
		mesh_valid = false;
		return false;
	}
	const uint64_t key = (uint64_t)s->get_rid().get_id();
	if (mesh_cache_key != key || !mesh_valid) {
		if (!_build_shape3d_mesh(cs)) {
			mesh_valid = false;
			return false;
		}
		mesh_cache_key = key;
	}
	mesh_valid = true;
	return true;
}

void PhysXFlowCollider3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &PhysXFlowCollider3D::set_enabled);
	ClassDB::bind_method(D_METHOD("get_enabled"), &PhysXFlowCollider3D::get_enabled);
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &PhysXFlowCollider3D::set_shape);
	BIND_ENUM_CONSTANT(SHAPE_BOX);
	BIND_ENUM_CONSTANT(SHAPE_SPHERE);
	BIND_ENUM_CONSTANT(SHAPE_SHAPE3D);
	ClassDB::bind_method(D_METHOD("get_shape"), &PhysXFlowCollider3D::get_shape);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &PhysXFlowCollider3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &PhysXFlowCollider3D::get_size);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &PhysXFlowCollider3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &PhysXFlowCollider3D::get_radius);
	ClassDB::bind_method(D_METHOD("set_shape_source", "path"), &PhysXFlowCollider3D::set_shape_source);
	ClassDB::bind_method(D_METHOD("get_shape_source"), &PhysXFlowCollider3D::get_shape_source);
	ClassDB::bind_method(D_METHOD("set_auto_velocity", "enabled"), &PhysXFlowCollider3D::set_auto_velocity);
	ClassDB::bind_method(D_METHOD("get_auto_velocity"), &PhysXFlowCollider3D::get_auto_velocity);
	ClassDB::bind_method(D_METHOD("set_manual_linear_velocity", "velocity"), &PhysXFlowCollider3D::set_manual_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_manual_linear_velocity"), &PhysXFlowCollider3D::get_manual_linear_velocity);
	ClassDB::bind_method(D_METHOD("set_manual_angular_velocity", "velocity"), &PhysXFlowCollider3D::set_manual_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_manual_angular_velocity"), &PhysXFlowCollider3D::get_manual_angular_velocity);
	ClassDB::bind_method(D_METHOD("get_estimated_linear_velocity"), &PhysXFlowCollider3D::get_estimated_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_estimated_angular_velocity"), &PhysXFlowCollider3D::get_estimated_angular_velocity);

	ADD_GROUP("Collider", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "get_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shape", PROPERTY_HINT_ENUM, "Box,Sphere,From Shape3D"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,16.0,0.01,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "shape_source", PROPERTY_HINT_NODE_TYPE, "CollisionShape3D"), "set_shape_source", "get_shape_source");
	ADD_GROUP("Velocity", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_velocity"), "set_auto_velocity", "get_auto_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "manual_linear_velocity", PROPERTY_HINT_NONE, "suffix:m/s"), "set_manual_linear_velocity", "get_manual_linear_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "manual_angular_velocity", PROPERTY_HINT_NONE, "suffix:rad/s"), "set_manual_angular_velocity", "get_manual_angular_velocity");
}

#endif // GODOT_PHYSX_FLOW

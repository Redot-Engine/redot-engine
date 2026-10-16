/**************************************************************************/
/*  physx_concave_polygon_shape_3d.cpp                                    */
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

#include "physx_concave_polygon_shape_3d.h"
#include "../physx_server.h"
#include <cooking/PxCooking.h>

#include "core/templates/local_vector.h"

PhysXConcavePolygonShape3D::~PhysXConcavePolygonShape3D() {
	_release_triangle_mesh();
}

AABB PhysXConcavePolygonShape3D::_calculate_aabb() const {
	AABB result;
	for (int i = 0; i < faces.size(); ++i) {
		const Vector3 &vertex = faces[i];
		if (i == 0) {
			result.position = vertex;
		} else {
			result.expand_to(vertex);
		}
	}
	return result;
}

void PhysXConcavePolygonShape3D::set_data(const Variant &p_data) {
	ERR_FAIL_COND(p_data.get_type() != Variant::DICTIONARY);

	const Dictionary data = p_data;

	const Variant maybe_faces = data.get("faces", Variant());
	ERR_FAIL_COND(maybe_faces.get_type() != Variant::PACKED_VECTOR3_ARRAY);

	const Variant maybe_back_face_collision = data.get("backface_collision", Variant());
	ERR_FAIL_COND(maybe_back_face_collision.get_type() != Variant::BOOL);

	faces = maybe_faces;
	back_face_collision = maybe_back_face_collision;

	aabb = _calculate_aabb();

	_release_triangle_mesh();
	_notify_shape_changed();
}

Variant PhysXConcavePolygonShape3D::get_data() const {
	Dictionary data;
	data["faces"] = faces;
	data["backface_collision"] = back_face_collision;
	return data;
}

bool PhysXConcavePolygonShape3D::get_physx_geometry(physx::PxGeometryHolder &holder, const physx::PxVec3 &scale) const {
	if (!_ensure_triangle_mesh()) {
		return false;
	}

	// Backface handling is baked at cook time (reversed-winding triangle
	// duplicates in _ensure_triangle_mesh), so the geometry stays
	// single-sided: eDOUBLE_SIDED here would generate a second, conflicting
	// contact set against the duplicated back-facing copies.
	physx::PxMeshGeometryFlags flags;

	holder.storeAny(
			physx::PxTriangleMeshGeometry(
					triangle_mesh,
					physx::PxMeshScale(scale),
					flags));

	return true;
}

void PhysXConcavePolygonShape3D::_release_triangle_mesh() {
	if (!triangle_mesh) {
		return;
	}

	triangle_mesh->release();
	triangle_mesh = nullptr;
}

bool PhysXConcavePolygonShape3D::_ensure_triangle_mesh() const {
	if (triangle_mesh) {
		return true;
	}
	const int vertex_count = faces.size();
	const int triangle_count = vertex_count / 3;
	const int excess_vertex_count = vertex_count % 3;

	if (unlikely(vertex_count == 0)) {
		return false;
	}

	ERR_FAIL_COND_V_MSG(vertex_count < 3, false, "Failed to build PhysX concave polygon: vertex count < 3.");
	ERR_FAIL_COND_V_MSG(excess_vertex_count != 0, false, "Failed to build PhysX concave polygon: vertex count not divisible by 3.");

	// 1. Prepare PhysX data arrays. When backface collision is enabled,
	// every triangle is duplicated with reversed winding: PhysX generates
	// back-face contacts following the triangle winding, so a single-winding
	// mesh pushes bodies THROUGH its back side even with eDOUBLE_SIDED.
	// Duplicated front-facing geometry guarantees a correctly-pushing contact
	// normal from either side, regardless of SDK back-face normal policy.
	LocalVector<physx::PxVec3> px_vertices;
	LocalVector<physx::PxU32> px_indices;

	px_vertices.resize(vertex_count);

	for (int i = 0; i < vertex_count; ++i) {
		const Vector3 &v = faces[i];
		px_vertices[i] = physx::PxVec3(v.x, v.y, v.z);
	}

	px_indices.reserve(back_face_collision ? vertex_count * 2 : vertex_count);
	for (int t = 0; t < triangle_count; ++t) {
		const int i = t * 3;
		// Godot front faces are clockwise (seen from outside); PhysX meshes
		// are counter-clockwise - swizzle the winding so the front face points
		// where the author intended. The Jolt module applies the same swizzle
		// when cooking concave meshes, which is why backface-disabled demo
		// floors collide correctly there but fell through here.
		px_indices.push_back(i + 0);
		px_indices.push_back(i + 2);
		px_indices.push_back(i + 1);
		if (back_face_collision) {
			// Original winding provides the opposite side.
			px_indices.push_back(i + 0);
			px_indices.push_back(i + 1);
			px_indices.push_back(i + 2);
		}
	}

	// 2. Set up the mesh descriptor
	physx::PxTriangleMeshDesc mesh_desc;
	mesh_desc.points.count = vertex_count;
	mesh_desc.points.stride = sizeof(physx::PxVec3);
	mesh_desc.points.data = px_vertices.ptr();

	mesh_desc.triangles.count = px_indices.size() / 3;
	mesh_desc.triangles.stride = 3 * sizeof(physx::PxU32);
	mesh_desc.triangles.data = px_indices.ptr();

	// 3. Fetch singletons
	const physx::PxCookingParams &cooking_params = PhysXServer3D::get_singleton()->get_cooking_params();
	physx::PxPhysics &physics = PhysXServer3D::get_singleton()->get_physics();

	ERR_FAIL_NULL_V_MSG(&physics, false, "PhysX PxPhysics is not initialized.");

	// 4. Cook the mesh directly — no cooking object, just params + insertion callback
	triangle_mesh = PxCreateTriangleMesh(cooking_params, mesh_desc, physics.getPhysicsInsertionCallback());

	if (!triangle_mesh) {
		ERR_PRINT("PhysX failed to create triangle mesh.");
		return false;
	}

	return true;
}

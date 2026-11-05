/**************************************************************************/
/*  physx_convex_polygon_shape_3d.cpp                                     */
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

#include "physx_convex_polygon_shape_3d.h"
#include "../physx_server.h"
#include "core/templates/local_vector.h"
#include <cooking/PxCooking.h>

PhysXConvexPolygonShape3D::~PhysXConvexPolygonShape3D() {
	_release_convex_mesh();
}

AABB PhysXConvexPolygonShape3D::_calculate_aabb() const {
	AABB result;
	for (int i = 0; i < points.size(); ++i) {
		const Vector3 &vertex = points[i];
		if (i == 0) {
			result.position = vertex;
		} else {
			result.expand_to(vertex);
		}
	}
	return result;
}

void PhysXConvexPolygonShape3D::set_data(const Variant &p_data) {
	// Godot's ConvexPolygon data is just a PackedVector3Array of point cloud vertices,
	// though sometimes it's wrapped in a dictionary depending on the server call context.
	// We handle both just to be safe, but typically it is passed as a direct array or a dict with "points".

	PackedVector3Array new_points;

	if (p_data.get_type() == Variant::DICTIONARY) {
		const Dictionary data = p_data;
		const Variant maybe_points = data.get("points", Variant());
		ERR_FAIL_COND(maybe_points.get_type() != Variant::PACKED_VECTOR3_ARRAY);
		new_points = maybe_points;
	} else if (p_data.get_type() == Variant::PACKED_VECTOR3_ARRAY) {
		new_points = p_data;
	} else {
		ERR_FAIL_MSG("Invalid data type for PhysXConvexPolygonShape3D.");
	}

	points = new_points;
	aabb = _calculate_aabb();

	_release_convex_mesh();
	_notify_shape_changed();
}

Variant PhysXConvexPolygonShape3D::get_data() const {
	return points;
}

bool PhysXConvexPolygonShape3D::get_physx_geometry(physx::PxGeometryHolder &holder, const physx::PxVec3 &scale) const {
	// This shape receives the SIGNED scale (see
	// PhysXShapedObject3D::_shape_geometry_scale_for). PhysX rejects negative
	// scale components on convex geometries (PxMeshScale::isValidForConvexMesh
	// — unlike triangle meshes), so a negative-determinant scale is baked by
	// cooking a point-reflected hull and using the absolute scale:
	//     pose * |scale| * reflected  ==  pose * scale.
	// All current callers pass a uniformly-signed scale (Godot's
	// Basis::get_scale() convention); a mixed sign pattern (unreachable via
	// the module plumbing) degrades to the unmirrored magnitudes.
	const bool mirrored = scale.minElement() < 0.0f;
	if (!_ensure_convex_mesh(mirrored)) {
		return false;
	}

	holder.storeAny(
			physx::PxConvexMeshGeometry(
					mirrored ? convex_mesh_mirrored : convex_mesh,
					physx::PxMeshScale(
							physx::PxVec3(physx::PxAbs(scale.x), physx::PxAbs(scale.y), physx::PxAbs(scale.z)))));

	return true;
}

void PhysXConvexPolygonShape3D::_release_convex_mesh() {
	if (convex_mesh) {
		convex_mesh->release();
		convex_mesh = nullptr;
	}
	if (convex_mesh_mirrored) {
		convex_mesh_mirrored->release();
		convex_mesh_mirrored = nullptr;
	}
}

bool PhysXConvexPolygonShape3D::_ensure_convex_mesh(bool p_mirrored) const {
	if (p_mirrored ? convex_mesh_mirrored != nullptr : convex_mesh != nullptr) {
		return true;
	}

	const int vertex_count = points.size();

	if (unlikely(vertex_count == 0)) {
		return false;
	}

	// A 3D convex hull requires at least 4 non-coplanar points, but PhysX can
	// compute planar convex hulls with 3.
	ERR_FAIL_COND_V_MSG(vertex_count < 3, false, "Failed to build PhysX convex polygon: vertex count < 3.");

	// 1. Prepare PhysX data arrays. The mirrored variant cooks the hull from
	// the point-reflected cloud (p' = -p): pose * |scale| * (-p) reproduces
	// pose * scale for the uniformly-signed scales this module passes.
	LocalVector<physx::PxVec3> px_vertices;
	px_vertices.resize(vertex_count);

	for (int i = 0; i < vertex_count; ++i) {
		const Vector3 &v = points[i];
		if (p_mirrored) {
			px_vertices[i] = physx::PxVec3(-v.x, -v.y, -v.z);
		} else {
			px_vertices[i] = physx::PxVec3(v.x, v.y, v.z);
		}
	}

	// 2. Set up the mesh descriptor
	physx::PxConvexMeshDesc mesh_desc;
	mesh_desc.points.count = vertex_count;
	mesh_desc.points.stride = sizeof(physx::PxVec3);
	mesh_desc.points.data = px_vertices.ptr();

	// eCOMPUTE_CONVEX tells PhysX to generate the hull from our point cloud.
	// eQUANTIZE_INPUT welds near-coincident points (hulls decomposed from
	// arbitrary Godot meshes routinely carry duplicates that break the strict
	// hull path); eDISABLE_MESH_VALIDATION skips re-validating already
	// validated Godot mesh data (same flag the reference PhysX backend cooks
	// with). eFAST_INERTIA_COMPUTATION is deliberately NOT set: it trades
	// tensor accuracy for speed the explicit-mass path does not need.
	mesh_desc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX | physx::PxConvexFlag::eQUANTIZE_INPUT |
			physx::PxConvexFlag::eDISABLE_MESH_VALIDATION;

	// 3. Fetch singletons -- a real pointer check: ERR_FAIL_NULL on the address
	// of a reference is dead code (and a -Wtautological-undefined-compare
	// error under werror), while get_physics() dereferences before it runs.
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	ERR_FAIL_NULL_V(server, false);
	physx::PxPhysics *physics_ptr = server->try_get_physics();
	ERR_FAIL_NULL_V_MSG(physics_ptr, false, "PhysX PxPhysics is not initialized.");
	const physx::PxCookingParams &cooking_params = server->get_cooking_params();
	physx::PxPhysics &physics = *physics_ptr;

	// 4. Cook the convex mesh. PhysX caps cooked hulls at 255 polygons; dense
	// point clouds can exceed it and fail outright. ePLANE_SHIFTING is the
	// SDK's dedicated algorithm for that case (it shifts hull planes to merge
	// facets) — retry with it before giving up.
	physx::PxConvexMesh *mesh = PxCreateConvexMesh(cooking_params, mesh_desc, physics.getPhysicsInsertionCallback());
	if (!mesh) {
		mesh_desc.flags |= physx::PxConvexFlag::ePLANE_SHIFTING;
		mesh = PxCreateConvexMesh(cooking_params, mesh_desc, physics.getPhysicsInsertionCallback());
	}

	if (!mesh) {
		ERR_PRINT("PhysX failed to create convex mesh (hull exceeds the 255-polygon limit even with plane shifting?).");
		return false;
	}

	if (p_mirrored) {
		convex_mesh_mirrored = mesh;
	} else {
		convex_mesh = mesh;
	}

	return true;
}

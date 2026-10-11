/**************************************************************************/
/*  physx_concave_polygon_shape_3d.h                                      */
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

#include "physx_shape_3d.h"

class PhysXConcavePolygonShape3D : public PhysXShape3D {
public:
	PhysXConcavePolygonShape3D() {}
	virtual ~PhysXConcavePolygonShape3D();

	virtual PhysicsServer3D::ShapeType get_type() const override { return PhysicsServer3D::SHAPE_CONCAVE_POLYGON; }
	virtual bool is_convex() const override { return false; }

	/// Backface cooking duplicates every triangle (forward + reversed winding):
	/// raw PhysX face indices address the DOUBLED mesh, Godot consumers expect
	/// indices into their own faces array.
	virtual int translate_face_index(int p_face_index) const override {
		return back_face_collision ? p_face_index / 2 : p_face_index;
	}
	virtual bool has_backface_collision() const override { return back_face_collision; }

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	virtual AABB get_aabb() const override { return aabb; }

	// Generates a Concave Polygon as a fast fallback
	virtual bool get_physx_geometry(physx::PxGeometryHolder &p_geometry_holder, const physx::PxVec3 &p_scale) const override;

private:
	AABB aabb;
	PackedVector3Array faces;
	bool back_face_collision = false;

	mutable physx::PxTriangleMesh *triangle_mesh = nullptr;

	bool _ensure_triangle_mesh() const;
	void _release_triangle_mesh();

	AABB _calculate_aabb() const;
};

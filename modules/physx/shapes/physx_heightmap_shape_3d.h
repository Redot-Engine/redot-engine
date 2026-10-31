/**************************************************************************/
/*  physx_heightmap_shape_3d.h                                            */
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

/**
 * @file physx_heightmap_shape_3d.h
 * @brief Heightfield collision shape (terrain).
 *
 * Builds a PxHeightField from Godot's PackedFloat32Array height data. The
 * heightfield is cooked lazily on first use and cached until the data changes.
 *
 * Note: PhysX heightfields originate at a local corner, while Godot expects
 * them centered. get_center_offset() provides the translation that the body
 * must apply when attaching the shape.
 */

#ifndef PHYSX_HEIGHT_MAP_SHAPE_3D_H
#define PHYSX_HEIGHT_MAP_SHAPE_3D_H

#include "physx_shape_3d.h"

class PhysXHeightMapShape3D : public PhysXShape3D {
public:
	PhysXHeightMapShape3D() {}
	virtual ~PhysXHeightMapShape3D();

	virtual PhysicsServer3D::ShapeType get_type() const override { return PhysicsServer3D::SHAPE_HEIGHTMAP; }
	virtual bool is_convex() const override { return false; }

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	virtual AABB get_aabb() const override { return aabb; }

	virtual bool get_physx_geometry(physx::PxGeometryHolder &p_geometry_holder, const physx::PxVec3 &p_scale) const override;

	virtual physx::PxTransform get_local_pose(const physx::PxVec3 &p_scale = physx::PxVec3(1.0f)) const override;

private:
	AABB aabb;
	int width = 0;
	int depth = 0;
	PackedFloat32Array heights;
	float min_height = 0.0f;
	float max_height = 0.0f;
	float height_scale = 1.0f;

	mutable physx::PxHeightField *height_field = nullptr;

	bool _ensure_physx_height_field() const;
	void _release_height_field();

	AABB _calculate_aabb() const;
};
#endif // PHYSX_HEIGHT_MAP_SHAPE_3D_H

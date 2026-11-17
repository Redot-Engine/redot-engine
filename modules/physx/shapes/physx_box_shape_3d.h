/**************************************************************************/
/*  physx_box_shape_3d.h                                                  */
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

// --- PhysX API ---
#include <PxPhysicsAPI.h>

class PhysXBoxShape3D : public PhysXShape3D {
	Vector3 half_extents; // Stored as half-extents (PxBoxGeometry expects half-extents; set_data() receives full-size extents and halves them)
public:
	PhysXBoxShape3D();
	virtual ~PhysXBoxShape3D() {}

	virtual PhysicsServer3D::ShapeType get_type() const override { return PhysicsServer3D::SHAPE_BOX; }
	virtual bool is_convex() const override { return true; }

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	virtual AABB get_aabb() const override;

	// The key function that generates the PhysX geometry
	virtual bool get_physx_geometry(physx::PxGeometryHolder &p_geometry_holder, const physx::PxVec3 &p_scale) const override;
};

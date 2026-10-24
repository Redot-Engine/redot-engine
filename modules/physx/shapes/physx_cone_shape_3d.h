/**************************************************************************/
/*  physx_cone_shape_3d.h                                                 */
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

#include "physx_custom_geometry_callback.h"

class PhysXConeShape3D : public PhysXCustomGeometryCallback<physx::PxCustomGeometryExt::ConeCallbacks> {
public:
	PhysXConeShape3D();
	~PhysXConeShape3D() override;

	virtual PhysicsServer3D::ShapeType get_type() const override {
		return PhysicsServer3D::SHAPE_CUSTOM;
	}

	virtual bool is_convex() const override {
		return true;
	}

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	virtual AABB get_aabb() const override;

protected:
	virtual physx::PxCustomGeometryExt::ConeCallbacks *_create_callbacks() const override;
	virtual void _apply_scale_to_callbacks(physx::PxCustomGeometryExt::ConeCallbacks &cb, const physx::PxVec3 &scale) const override;
	virtual void _apply_params_to_callbacks(physx::PxCustomGeometryExt::ConeCallbacks &cb) const override;

private:
	float radius = 0.5f;
	float height = 1.0f;
};

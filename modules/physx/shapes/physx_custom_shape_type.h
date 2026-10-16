/**************************************************************************/
/*  physx_custom_shape_type.h                                             */
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

#include "core/string/string_name.h"
#include "core/templates/hash_map.h"

#include "physx_shape_3d.h"

#include <functional>
#include <memory>

class PhysXCustomShapeType : public PhysXShape3D {
private:
	StringName current_type;
	std::unique_ptr<PhysXShape3D, void (*)(PhysXShape3D *)> internal_shape{ nullptr, memdelete_shape };

	static void memdelete_shape(PhysXShape3D *p_shape) {
		if (p_shape) {
			memdelete(p_shape);
		}
	}

public:
	using FactoryFunc = std::function<std::unique_ptr<PhysXShape3D, void (*)(PhysXShape3D *)>()>;
	// Static registry so it's shared across all custom shapes
	static HashMap<StringName, FactoryFunc> shape_factories;

	/// Registers the built-in custom shapes ("cone"). Called once from
	/// PhysXServer3D::init(); idempotent.
	static void register_builtin_shapes();

	PhysXCustomShapeType();
	~PhysXCustomShapeType() override;

	// To Godot, this is a custom shape
	virtual PhysicsServer3D::ShapeType get_type() const override {
		return PhysicsServer3D::SHAPE_CUSTOM;
	}

	virtual bool is_convex() const override;

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	virtual AABB get_aabb() const override;

	virtual bool get_physx_geometry(physx::PxGeometryHolder &holder, const physx::PxVec3 &scale) const override;

	/// Keeps the inner shape's margin in sync with the wrapper's (the wrapper
	/// margin governs the attached PxShape's contact offset).
	virtual void set_margin(float p_margin) override;

	// Backend access to check the exact shape type internally
	StringName get_custom_type() const { return current_type; }
};

/**************************************************************************/
/*  physx_gpu_cloth_3d.h                                                  */
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
 * @file physx_gpu_cloth_3d.h
 * @brief GPU cloth: a PhysX 5 PxDeformableSurface (FEM/XPBD, runs on CUDA).
 *
 * GPU-only; PhysXCloth3D falls back to its built-in CPU XPBD solver when this
 * cannot be built (no GPU build, no CUDA device, or the active engine is not
 * PhysX). The surface simulates in world space; positions are read back to
 * host each step for the node to draw as an ArrayMesh.
 */

#ifndef PHYSX_GPU_CLOTH_3D_H
#define PHYSX_GPU_CLOTH_3D_H

#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/os/mutex.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"
#include "core/templates/vector.h"

namespace physx {
class PxDeformableSurface;
class PxDeformableSurfaceMaterial;
class PxShape;
class PxTriangleMesh;
class PxCudaContextManager;
} // namespace physx

class PhysXSpace3D;

class PhysXGPUCloth3D {
	RID self;
	PhysXSpace3D *space = nullptr;
	physx::PxCudaContextManager *cuda = nullptr;

	physx::PxDeformableSurface *surface = nullptr;
	physx::PxDeformableSurfaceMaterial *material = nullptr;
	physx::PxShape *shape = nullptr;
	physx::PxTriangleMesh *tri_mesh = nullptr;

	uint32_t vertex_count = 0;
	LocalVector<int32_t> indices; // cooked triangle list, for the node's mesh
	LocalVector<uint8_t> pinned; // 1 == pinned
	// Maps PRE-cook input vertex indices to cooked-mesh vertex indices
	// (cooking welds/reorders; pins are authored in input space).
	LocalVector<int32_t> input_to_cooked;
	LocalVector<Vector3> pin_target; // world position, NAN.x == none

	void *host_pos = nullptr; // pinned host mirror (PxVec4*): pos.xyz + invMass.w
	void *host_vel = nullptr; // pinned host mirror (PxVec4*): velocity.xyz

	float thickness = 0.01f;
	float density = 0.2f; // kg/m^2
	float stretch_stiffness = 0.9f; // 0..1
	float bend_stiffness = 0.1f; // 0..1
	float damping = 0.03f;
	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;
	uint32_t exception_slot = 0;

	mutable Mutex mesh_mutex;
	LocalVector<Vector3> read_positions; // world space, published to the node
	uint32_t mesh_version = 0;

	bool params_dirty = true;
	bool simulated_once = false; // device buffers only exist after the first simulate()

	void _destroy_surface();
	void _apply_material();
	bool _cook_and_create(const Vector<Vector3> &p_positions, const Vector<int32_t> &p_indices);

public:
	void set_self(const RID &p_self) { self = p_self; }
	RID get_self() const { return self; }
	void set_space(PhysXSpace3D *p_space);
	PhysXSpace3D *get_space() const { return space; }

	bool is_ready() const { return surface != nullptr; }
	uint32_t get_vertex_count() const { return vertex_count; }

	void set_params(float p_thickness, float p_density, float p_stretch, float p_bend, float p_damping, uint32_t p_collision_mask);
	void set_collision_layer(uint32_t p_layer);
	void set_collision_mask(uint32_t p_mask);
	uint32_t get_collision_layer() const { return collision_layer; }
	void set_exception_slot(uint32_t p_slot);
	uint32_t get_or_alloc_exception_slot();
	uint32_t get_exception_slot() const { return exception_slot; }
	// Build (or rebuild) the surface from a world-space triangle mesh.
	void build(const Vector<Vector3> &p_positions, const Vector<int32_t> &p_indices, const Transform3D &p_xform);
	void set_pinned(const Vector<int32_t> &p_pinned_indices);
	void set_pin_targets(const Vector<Vector3> &p_world_targets); // NAN.x releases
	void apply_wind(const Vector3 &p_wind, float p_drag, float p_lift, float p_dt);
	void clear();

	// Called by the space after fetchResults().
	void read_back();
	// Thread-safe: copy the latest mesh. Returns the triangle count, or
	// UINT32_MAX if unchanged since p_have_version.
	uint32_t copy_mesh(LocalVector<Vector3> &r_positions, LocalVector<int32_t> &r_indices, uint32_t &p_have_version) const;

	PhysXGPUCloth3D() {}
	~PhysXGPUCloth3D();
};

#endif // PHYSX_GPU_CLOTH_3D_H

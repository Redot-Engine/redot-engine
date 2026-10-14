/**************************************************************************/
/*  physx_soft_volume_3d.cpp                                              */
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

/**
 * @file physx_soft_volume_3d.cpp
 * @brief GPU soft body volume implementation (PxDeformableVolume on CUDA).
 */

#include "physx_soft_volume_3d.h"

#include "../physx_conversions.h"
#include "../physx_server.h"
#include "../spaces/physx_space_3d.h"

#include "core/error/error_macros.h"
#include "core/math/math_funcs.h"
#include "core/templates/hash_map.h"

#include <PxPhysicsAPI.h>
#include <extensions/PxCudaHelpersExt.h>
#include <extensions/PxDeformableVolumeExt.h>

using namespace physx;

PhysXSoftVolume3D::~PhysXSoftVolume3D() {
	_destroy();
}

void PhysXSoftVolume3D::_destroy() {
	// Attachments must die BEFORE the deformable actor: a PxDeformableAttachment
	// references both of its actors, and releasing it after the volume would
	// leave the constraint pointing at freed memory.
	for (physx::PxDeformableAttachment *att : attachments) {
		if (att) {
			att->release();
		}
	}
	attachments.clear();
	if (volume) {
		// Routed through the space (queued when a solve is in flight).
		if (space) {
			space->remove_actor(volume);
			space->release_actor(volume);
		} else {
			volume->release();
		}
		volume = nullptr;
	}
	if (shape) {
		// The explicit release is REQUIRED even after the volume release:
		// PxDeformableVolume derives from PxActor (NOT PxRigidActor), and
		// PxActor::release() does not destroy attached shapes. Skipping this
		// release leaks the exclusive shape into PxPhysics::release(), where
		// ~NpShape then runs against this already-released material and
		// crashes at shutdown.
		shape->release();
		shape = nullptr;
	}
	if (material) {
		material->release();
		material = nullptr;
	}
	if (volume_mesh) {
		volume_mesh->release();
		volume_mesh = nullptr;
	}
	if (readback && cuda) {
		PxVec4 *rb = static_cast<PxVec4 *>(readback);
		PX_EXT_PINNED_MEMORY_FREE(*cuda, rb);
	}
	if (sim_pos_scratch) {
		PxVec4 *sp = static_cast<PxVec4 *>(sim_pos_scratch);
		PX_EXT_PINNED_MEMORY_FREE(*cuda, sp);
		sim_pos_scratch = nullptr;
	}
	if (sim_vel_scratch) {
		PxVec4 *sv = static_cast<PxVec4 *>(sim_vel_scratch);
		PX_EXT_PINNED_MEMORY_FREE(*cuda, sv);
		sim_vel_scratch = nullptr;
	}
	pos_scratch_valid = false;
	vel_scratch_valid = false;
	readback = nullptr;
	coll_vertex_count = 0;
	simulated_once = false;
	welded_to_coll.clear();
	base_inv_mass.clear();
	read_positions.clear();
	read_normals.clear();
	surface_indices.clear();
}

bool PhysXSoftVolume3D::build(PhysXSpace3D *p_space, const Vector<Vector3> &p_world_verts,
		const Vector<int32_t> &p_indices, const Transform3D &p_xform, const Params &p_params) {
	_destroy();
	space = p_space;
	if (!space) {
		return false;
	}
	PxPhysics *physics = space->get_px_physics();
	PxScene *scene = space->get_px_scene();
	cuda = space->get_px_cuda();
	if (!physics || !scene || !cuda) {
		return false;
	}
	const int nv = p_world_verts.size();
	const int ni = p_indices.size();
	if (nv < 4 || ni < 12 || (ni % 3) != 0) {
		return false;
	}

	// The volume simulates in world space -- bake the render mesh's placement in.
	LocalVector<PxVec3> verts;
	verts.resize(nv);
	for (int i = 0; i < nv; i++) {
		verts[i] = godot_to_px(p_xform.xform(p_world_verts[i]));
	}
	LocalVector<PxU32> tris;
	tris.resize(ni);
	surface_indices.resize(ni);
	for (int i = 0; i < ni; i++) {
		tris[i] = (PxU32)p_indices[i];
		surface_indices[i] = p_indices[i];
	}

	PxCookingParams cook(physics->getTolerancesScale());
	cook.buildGPUData = true;
	cook.meshWeldTolerance = 1.0e-5f;
	cook.meshPreprocessParams = PxMeshPreprocessingFlags(PxMeshPreprocessingFlag::eWELD_VERTICES);

	PxSimpleTriangleMesh surface_mesh;
	surface_mesh.points.count = (PxU32)nv;
	surface_mesh.points.stride = sizeof(PxVec3);
	surface_mesh.points.data = verts.ptr();
	surface_mesh.triangles.count = (PxU32)(ni / 3);
	surface_mesh.triangles.stride = 3 * sizeof(PxU32);
	surface_mesh.triangles.data = tris.ptr();

	// Conforming tet mesh: its collision-mesh surface vertices match the input,
	// so readback maps straight onto the render mesh (no skinning pass).
	volume_mesh = PxDeformableVolumeExt::createDeformableVolumeMeshNoVoxels(
			cook, surface_mesh, physics->getPhysicsInsertionCallback());
	if (!volume_mesh) {
		return false; // mesh not watertight / manifold -> caller falls back to CPU
	}

	volume = physics->createDeformableVolume(*cuda);
	if (!volume) {
		_destroy();
		return false;
	}

	material = physics->createDeformableVolumeMaterial(
			p_params.youngs_modulus, p_params.poisson_ratio, p_params.dynamic_friction, p_params.damping);

	const PxShapeFlags shape_flags = PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE;
	shape = physics->createShape(PxTetrahedronMeshGeometry(volume_mesh->getCollisionMesh()), &material, 1, true, shape_flags);
	if (!shape) {
		_destroy();
		return false;
	}
	volume->attachShape(*shape);
	// Same layer/mask convention as the module's rigid bodies (word0 = layer,
	// word1 = mask) so the simulation filter shader lets it collide with them.
	const PxFilterData fd(p_params.collision_layer, p_params.collision_mask, p_params.exception_slot, 0);
	shape->setSimulationFilterData(fd);
	shape->setQueryFilterData(fd);
	// A contact margin so volume-vs-volume and volume-vs-rigid contacts are
	// found before full interpenetration.
	shape->setContactOffset(0.05f);
	shape->setRestOffset(0.02f);
	volume->attachSimulationMesh(*volume_mesh->getSimulationMesh(), *volume_mesh->getDeformableVolumeAuxData());
	space->add_actor(volume);

	volume->setDeformableBodyFlag(PxDeformableBodyFlag::eDISABLE_SELF_COLLISION, true);
	// Same floor as apply_params below — a precision below 15 used to be
	// raised here and silently dropped back on the first apply_params call.
	volume->setSolverIterationCounts(MAX(p_params.solver_iterations, 1));
	volume->setMaxLinearVelocity(p_params.max_speed);
	volume->setMaxDepenetrationVelocity(3.0f);

	// Host mirror: place, mass, push to device, then free (we keep only a
	// readback buffer of our own).
	PxVec4 *sim_pos = nullptr;
	PxVec4 *sim_vel = nullptr;
	PxVec4 *coll_pos = nullptr;
	PxVec4 *rest_pos = nullptr;
	PxDeformableVolumeExt::allocateAndInitializeHostMirror(*volume, cuda, sim_pos, sim_vel, coll_pos, rest_pos);

	// The vertices already carry the world placement, so only apply mass here.
	const PxTetrahedronMesh *sim_mesh = volume_mesh->getSimulationMesh();
	total_mass = MAX((float)p_params.total_mass, 0.001f);
	const float volume_m3 = MAX((float)_estimate_mesh_volume(p_world_verts, p_indices, p_xform), 1.0e-4f);
	const float density = total_mass / volume_m3;
	PxDeformableVolumeExt::updateMass(*volume, density, 50.0f, sim_pos);
	PxDeformableVolumeExt::copyToDevice(*volume, PxDeformableVolumeDataFlag::eALL, sim_pos, sim_vel, coll_pos, rest_pos);

	// Keep the per-vertex inverse mass PhysX assigned -- pinning zeroes it and
	// unpinning restores from here. (NoVoxels: sim mesh == collision mesh.)
	const uint32_t sim_nv = sim_mesh->getNbVertices();
	base_inv_mass.resize(sim_nv);
	for (uint32_t i = 0; i < sim_nv; i++) {
		base_inv_mass[i] = sim_pos[i].w;
	}

	// Build render(welded) -> collision-vertex map from rest positions.
	const PxTetrahedronMesh *coll_mesh = volume_mesh->getCollisionMesh();
	coll_vertex_count = coll_mesh->getNbVertices();
	const PxVec3 *coll_v = coll_mesh->getVertices();
	// Hash collisions without verification used to bind a render vertex to the
	// WRONG collision vertex (silently misplaced pins); store the quantized
	// position in the value and verify on hit, nearest fallback on mismatch.
	HashMap<uint64_t, Vector4i> lut; // xyz = quantized position, w = vertex
	const float q = 1000.0f; // 1mm quantization for the position match
	auto key_of = [q](const PxVec3 &p, Vector3i &r_qpos) -> uint64_t {
		const int64_t x = (int64_t)Math::round(p.x * q);
		const int64_t y = (int64_t)Math::round(p.y * q);
		const int64_t z = (int64_t)Math::round(p.z * q);
		r_qpos = Vector3i((int32_t)x, (int32_t)y, (int32_t)z);
		return ((uint64_t)(uint32_t)x) ^ (((uint64_t)(uint32_t)y) << 21) ^ (((uint64_t)(uint32_t)z) << 42);
	};
	for (uint32_t i = 0; i < coll_vertex_count; i++) {
		Vector3i qpos;
		const uint64_t key = key_of(coll_v[i], qpos);
		lut[key] = Vector4i(qpos.x, qpos.y, qpos.z, (int32_t)i);
	}
	auto nearest_of = [&coll_v, this](const PxVec3 &w) -> uint32_t {
		uint32_t best = 0;
		float best_d = 1.0e30f;
		for (uint32_t c = 0; c < coll_vertex_count; c++) {
			const float d = (coll_v[c] - w).magnitudeSquared();
			if (d < best_d) {
				best_d = d;
				best = c;
			}
		}
		return best;
	};
	welded_to_coll.resize(nv);
	for (int i = 0; i < nv; i++) {
		const PxVec3 &w = verts[i];
		Vector3i qpos;
		const HashMap<uint64_t, Vector4i>::ConstIterator it = lut.find(key_of(w, qpos));
		if (it && it->value.x == qpos.x && it->value.y == qpos.y && it->value.z == qpos.z) {
			welded_to_coll[i] = (uint32_t)it->value.w;
		} else {
			// Miss or hash collision: nearest vertex (cook nudged past the
			// quant cell, or two vertices shared a cell).
			welded_to_coll[i] = nearest_of(w);
		}
	}

	PX_EXT_PINNED_MEMORY_FREE(*cuda, sim_pos);
	PX_EXT_PINNED_MEMORY_FREE(*cuda, sim_vel);
	PX_EXT_PINNED_MEMORY_FREE(*cuda, coll_pos);
	PX_EXT_PINNED_MEMORY_FREE(*cuda, rest_pos);

	readback = PX_EXT_PINNED_MEMORY_ALLOC(PxVec4, *cuda, coll_vertex_count);
	read_positions.resize(coll_vertex_count);
	read_normals.resize(coll_vertex_count);
	for (uint32_t i = 0; i < coll_vertex_count; i++) {
		read_positions[i] = Vector3(coll_v[i].x, coll_v[i].y, coll_v[i].z);
		read_normals[i] = Vector3(0, 1, 0);
	}
	simulated_once = false;
	_recompute_normals_and_bounds();
	return true;
}

void PhysXSoftVolume3D::apply_params(const Params &p_params) {
	if (!volume) {
		return;
	}
	if (material) {
		material->setYoungsModulus(p_params.youngs_modulus);
		material->setPoissons(CLAMP(p_params.poisson_ratio, 0.0f, 0.49f));
		material->setDynamicFriction(p_params.dynamic_friction);
		material->setElasticityDamping(p_params.damping);
	}
	volume->setSolverIterationCounts(MAX(p_params.solver_iterations, 1));
	volume->setMaxLinearVelocity(p_params.max_speed);
	// Runtime layer/mask/exception-slot changes must reach the shape's filter
	// data too -- the deformable solver consults the simulation filter, not
	// this material, when pairing against rigid bodies.
	if (shape) {
		const PxFilterData fd(p_params.collision_layer, p_params.collision_mask, p_params.exception_slot, 0);
		shape->setSimulationFilterData(fd);
		shape->setQueryFilterData(fd);
	}
}

void PhysXSoftVolume3D::read_back() {
	// The device buffers changed during the solve: the pin/impulse scratch
	// no longer mirrors them (SOFT-4 validity).
	pos_scratch_valid = false;
	vel_scratch_valid = false;
	if (!volume || coll_vertex_count == 0) {
		return;
	}
	simulated_once = true;
	PxVec4 *hp = static_cast<PxVec4 *>(readback);
	Ext::PxCudaHelpersExt::copyDToH(*cuda, hp, volume->getPositionInvMassBufferD(), coll_vertex_count);
	for (uint32_t i = 0; i < coll_vertex_count; i++) {
		read_positions[i] = Vector3(hp[i].x, hp[i].y, hp[i].z);
	}
	_recompute_normals_and_bounds();
}

void PhysXSoftVolume3D::_recompute_normals_and_bounds() {
	const uint32_t n = read_positions.size();
	if (n == 0) {
		bounds = AABB();
		return;
	}
	for (uint32_t i = 0; i < n; i++) {
		read_normals[i] = Vector3();
	}
	for (uint32_t t = 0; t + 2 < surface_indices.size(); t += 3) {
		const uint32_t a = welded_to_coll[surface_indices[t]];
		const uint32_t b = welded_to_coll[surface_indices[t + 1]];
		const uint32_t c = welded_to_coll[surface_indices[t + 2]];
		const Vector3 fn = (read_positions[b] - read_positions[a]).cross(read_positions[c] - read_positions[a]);
		read_normals[a] += fn;
		read_normals[b] += fn;
		read_normals[c] += fn;
	}
	AABB bb(read_positions[0], Vector3());
	for (uint32_t i = 0; i < n; i++) {
		const float len = read_normals[i].length();
		read_normals[i] = len > CMP_EPSILON ? read_normals[i] / len : Vector3(0, 1, 0);
		bb.expand_to(read_positions[i]);
	}
	bounds = bb;
}

Vector3 PhysXSoftVolume3D::get_vertex_position(uint32_t p_welded_index) const {
	if (p_welded_index >= welded_to_coll.size()) {
		return Vector3();
	}
	return read_positions[welded_to_coll[p_welded_index]];
}

Vector3 PhysXSoftVolume3D::get_vertex_normal(uint32_t p_welded_index) const {
	if (p_welded_index >= welded_to_coll.size()) {
		return Vector3(0, 1, 0);
	}
	return read_normals[welded_to_coll[p_welded_index]];
}

void PhysXSoftVolume3D::set_pins(const Vector<int> &p_welded_indices, const Vector<Vector3> &p_targets) {
	if (!volume || base_inv_mass.is_empty()) {
		return;
	}
	const uint32_t sim_nv = base_inv_mass.size();
	if (!sim_pos_scratch) {
		sim_pos_scratch = PX_EXT_PINNED_MEMORY_ALLOC(PxVec4, *cuda, sim_nv);
	}
	PxVec4 *sp = static_cast<PxVec4 *>(sim_pos_scratch);
	if (!pos_scratch_valid) {
		Ext::PxCudaHelpersExt::copyDToH(*cuda, sp, volume->getSimPositionInvMassBufferD(), sim_nv);
	}

	for (uint32_t i = 0; i < sim_nv; i++) {
		sp[i].w = base_inv_mass[i]; // restore first, then re-pin below
	}
	for (int k = 0; k < p_welded_indices.size(); k++) {
		const int wi = p_welded_indices[k];
		if (wi < 0 || (uint32_t)wi >= welded_to_coll.size()) {
			continue;
		}
		const uint32_t s = welded_to_coll[wi];
		sp[s].w = 0.0f;
		if (k < p_targets.size() && Math::is_finite(p_targets[k].x)) {
			sp[s].x = p_targets[k].x;
			sp[s].y = p_targets[k].y;
			sp[s].z = p_targets[k].z;
		}
	}

	Ext::PxCudaHelpersExt::copyHToD(*cuda, volume->getSimPositionInvMassBufferD(), sp, sim_nv);
	volume->markDirty(PxDeformableVolumeDataFlag::eSIM_POSITION_INVMASS);
	pos_scratch_valid = true; // device now equals the scratch
}

void PhysXSoftVolume3D::add_central_impulse(const Vector3 &p_impulse) {
	if (!volume || base_inv_mass.is_empty()) {
		return;
	}
	const uint32_t sim_nv = base_inv_mass.size();
	const PxVec3 dv = godot_to_px(p_impulse / total_mass);
	if (!sim_vel_scratch) {
		sim_vel_scratch = PX_EXT_PINNED_MEMORY_ALLOC(PxVec4, *cuda, sim_nv);
	}
	PxVec4 *sv = static_cast<PxVec4 *>(sim_vel_scratch);
	if (!vel_scratch_valid) {
		Ext::PxCudaHelpersExt::copyDToH(*cuda, sv, volume->getSimVelocityBufferD(), sim_nv);
	}
	for (uint32_t i = 0; i < sim_nv; i++) {
		if (base_inv_mass[i] > 0.0f) {
			sv[i].x += dv.x;
			sv[i].y += dv.y;
			sv[i].z += dv.z;
		}
	}
	Ext::PxCudaHelpersExt::copyHToD(*cuda, volume->getSimVelocityBufferD(), sv, sim_nv);
	volume->markDirty(PxDeformableVolumeDataFlag::eSIM_VELOCITY);
	vel_scratch_valid = true; // device now equals the scratch
}

void PhysXSoftVolume3D::add_point_impulse(uint32_t p_welded_index, const Vector3 &p_impulse) {
	if (!volume || p_welded_index >= welded_to_coll.size()) {
		return;
	}
	const uint32_t s = welded_to_coll[p_welded_index];
	if (base_inv_mass[s] <= 0.0f) {
		return; // pinned
	}
	const uint32_t sim_nv = base_inv_mass.size();
	const PxVec3 dv = godot_to_px(p_impulse * base_inv_mass[s]);
	if (!sim_vel_scratch) {
		sim_vel_scratch = PX_EXT_PINNED_MEMORY_ALLOC(PxVec4, *cuda, sim_nv);
	}
	PxVec4 *sv = static_cast<PxVec4 *>(sim_vel_scratch);
	if (!vel_scratch_valid) {
		Ext::PxCudaHelpersExt::copyDToH(*cuda, sv, volume->getSimVelocityBufferD(), sim_nv);
	}
	sv[s].x += dv.x;
	sv[s].y += dv.y;
	sv[s].z += dv.z;
	Ext::PxCudaHelpersExt::copyHToD(*cuda, volume->getSimVelocityBufferD(), sv, sim_nv);
	volume->markDirty(PxDeformableVolumeDataFlag::eSIM_VELOCITY);
	vel_scratch_valid = true; // device now equals the scratch
}

double PhysXSoftVolume3D::_estimate_mesh_volume(const Vector<Vector3> &p_verts, const Vector<int32_t> &p_indices, const Transform3D &p_xform) {
	double v = 0.0;
	for (int t = 0; t + 2 < p_indices.size(); t += 3) {
		const Vector3 a = p_xform.xform(p_verts[p_indices[t]]);
		const Vector3 b = p_xform.xform(p_verts[p_indices[t + 1]]);
		const Vector3 c = p_xform.xform(p_verts[p_indices[t + 2]]);
		v += (double)a.dot(b.cross(c));
	}
	return Math::abs(v) / 6.0;
}

void PhysXSoftVolume3D::set_exception_slot(uint32_t p_slot) {
	if (!shape) {
		return;
	}
	// Re-write the full filter data with the exception slot in word2; the
	// layer/mask fields keep the values the volume was built with.
	const PxFilterData current = shape->getSimulationFilterData();
	PxFilterData fd(current.word0, current.word1, p_slot, current.word3);
	shape->setSimulationFilterData(fd);
}

// ---------------------------------------------------------------------------
// Rigid attachments (GAP-15) — PxDeformableAttachment between welded soft-body
// vertices and a rigid actor. SDK positional constraints; active only while
// both actors live in the same scene.
// ---------------------------------------------------------------------------

physx::PxDeformableAttachment *PhysXSoftVolume3D::create_rigid_attachment(const Vector<int> &p_welded_indices, physx::PxRigidActor *p_rigid) {
	if (!volume || !p_rigid || p_welded_indices.is_empty()) {
		return nullptr;
	}
	// Translate welded render indices -> sim (collision) mesh vertex indices,
	// the same mapping set_pins() consumes. Rigid side: per SDK table 2 the
	// eRIGID pose is LOCAL to the rigid actor ("local pose for eRIGID") and
	// the coords are expressed in that frame — so pose[1] stays identity and
	// each world-space soft vertex position is transformed into the rigid's
	// current local frame: the vertex sticks to the body exactly where it
	// attaches, and follows it 1:1 from then on.
	const PxTransform rigid_global = p_rigid->getGlobalPose();
	// The SDK takes the deformable side of an eVERTEX attachment from the sim
	// mesh's CURRENT vertices, so the rigid-side point must be captured from
	// the same fresh state — read the sim position buffer directly (same
	// access path set_pins() uses). The last read-back (read_positions) lags
	// a frame and made the constraint start with a visible violation.
	const uint32_t sim_nv = base_inv_mass.size();
	LocalVector<PxVec4> fresh;
	fresh.resize(sim_nv);
	{
		PxVec4 *sp = PX_EXT_PINNED_MEMORY_ALLOC(PxVec4, *cuda, sim_nv);
		Ext::PxCudaHelpersExt::copyDToH(*cuda, sp, volume->getSimPositionInvMassBufferD(), sim_nv);
		for (uint32_t i = 0; i < sim_nv; i++) {
			fresh[i] = sp[i];
		}
		PX_EXT_PINNED_MEMORY_FREE(*cuda, sp);
	}
	LocalVector<PxU32> sim_indices;
	LocalVector<PxVec4> rigid_coords;
	for (int i = 0; i < p_welded_indices.size(); i++) {
		const int wi = p_welded_indices[i];
		if (wi < 0 || (uint32_t)wi >= welded_to_coll.size()) {
			continue;
		}
		const uint32_t s = welded_to_coll[wi];
		sim_indices.push_back(s);
		const PxVec3 &wp = (uint32_t)s < fresh.size()
				? PxVec3(fresh[s].x, fresh[s].y, fresh[s].z)
				: PxVec3(0.0f);
		const PxVec3 local = rigid_global.transformInv(wp);
		rigid_coords.push_back(PxVec4(local.x, local.y, local.z, 0.0f));
	}
	if (sim_indices.is_empty()) {
		return nullptr;
	}

	PxDeformableAttachmentData desc;
	desc.actor[0] = volume;
	desc.type[0] = PxDeformableAttachmentTargetType::eVERTEX;
	desc.indices[0].count = (PxU32)sim_indices.size();
	desc.indices[0].data = sim_indices.ptr();

	desc.actor[1] = p_rigid;
	desc.type[1] = PxDeformableAttachmentTargetType::eRIGID;
	desc.pose[1] = PxTransform(PxIdentity);
	desc.coords[1].count = (PxU32)rigid_coords.size();
	desc.coords[1].data = rigid_coords.ptr();

	physx::PxPhysics &physics = PhysXServer3D::get_singleton()->get_physics();
	PxDeformableAttachment *att = physics.createDeformableAttachment(desc);
	if (!att) {
		ERR_PRINT("PhysXSoftVolume3D: PxPhysics::createDeformableAttachment returned null (same-scene requirement, or vertex indices invalid).");
		return nullptr;
	}
	attachments.push_back(att);
	return att;
}

void PhysXSoftVolume3D::release_attachment(physx::PxDeformableAttachment *&p_attachment) {
	if (!p_attachment) {
		return;
	}
	for (uint32_t i = 0; i < attachments.size(); i++) {
		if (attachments[i] == p_attachment) {
			attachments.remove_at(i);
			break;
		}
	}
	p_attachment->release();
	p_attachment = nullptr;
}

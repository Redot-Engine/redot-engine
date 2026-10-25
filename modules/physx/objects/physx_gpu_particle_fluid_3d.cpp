/**************************************************************************/
/*  physx_gpu_particle_fluid_3d.cpp                                       */
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

#include "physx_gpu_particle_fluid_3d.h"

#include "../physx_conversions.h"
#include "../spaces/physx_space_3d.h"

#include "core/error/error_macros.h"
#include "core/math/math_defs.h"

#include <PxAnisotropy.h>
#include <PxIsosurfaceExtraction.h>
#include <PxPhysicsAPI.h>
#include <PxSmoothing.h>
#include <extensions/PxCudaHelpersExt.h>
#include <extensions/PxParticleExt.h>
#include <gpu/PxGpu.h>
#include <gpu/PxPhysicsGpu.h>

using namespace physx;

// PhysX's fluid density in kg/m^3; particle mass follows from the spacing.
static constexpr float FLUID_DENSITY = 1000.0f;

// GPU isosurface extraction: PhysX smooths the particle positions and
// marching-cubes a triangle mesh, all on the GPU
// via a particle-system callback. The result is read to host arrays each solve
// for the node to render as an ArrayMesh.
// RD-8: the full-host outlier-clamp round trip is throttled to every Nth
// extraction (reference solver's CLAMP_EVERY=4).
static constexpr uint32_t PBD_CLAMP_EVERY = 4;

struct PhysXFluidIsosurface : public PxParticleSystemCallback {
	PhysXGPUParticleFluid3D *owner = nullptr;
	PxCudaContextManager *cuda = nullptr;

	PxSparseGridIsosurfaceExtractor *extractor = nullptr;
	PxSmoothedPositionGenerator *smoothing = nullptr;
	PxAnisotropyGenerator *anisotropy = nullptr; // used only when owner->surface_anisotropy_enabled

	PxVec4 *dev_smoothed = nullptr;
	PxVec4 *dev_aniso1 = nullptr;
	PxVec4 *dev_aniso2 = nullptr;
	PxVec4 *dev_aniso3 = nullptr;

	uint32_t max_vertices = 0;
	uint32_t max_triangles = 0;
	LocalVector<PxVec4> host_vertices;
	LocalVector<PxU32> host_indices;
	LocalVector<PxVec4> host_normals;
	LocalVector<PxVec4> host_positions; // scratch for outlier clamping
	float clamp_reach = 3.5f; // max meters a particle may sit from the fluid's median before it is pinned
	uint32_t frame = 0; // isosurface is re-extracted every other solve to halve the cost
	bool surface_clip_warned = false;
	// Extractions are kicked async on the stream and their results read one
	// 30 Hz tick later, so the GPU is never stalled waiting on marching cubes.
	bool surface_pending = false;
	bool foam_pending = false;

	// Batched multi-fluid mode (see PhysXGPUParticleFluid3D::
	// set_deferred_extraction): onPostSolve kicks the smoothing kernel without
	// syncing and defers the sync + outlier clamp + extraction kick to
	// finish_extraction(), which the space calls once per fluid after
	// fetchResults. With N fluids PhysX invokes each onPostSolve in turn
	// during the solve, so syncing inline serializes N stalls -- system 2's
	// kernel would not even be issued until system 1's callback finished
	// blocking. Deferred, every fluid's kernel is already in flight when the
	// first finish_extraction syncs, so N stalls collapse into one pipelined
	// wait. mHostPtr / the stream are PhysX-owned resources that stay valid
	// until the next simulate() call, so stashing them across the callback
	// return is safe within this step's window.
	bool deferred = false;
	bool extraction_pending = false;
	PxGpuParticleSystem *pending_gps = nullptr;
	PxU32 pending_n = 0;
	CUstream pending_stream = 0;
	bool pending_aniso = false;

	// Second extractor over the diffuse (foam) particles. Coarser grid: foam is
	// meant to read as froth, not a smooth skin, and the particle count is low.
	PxSparseGridIsosurfaceExtractor *foam_extractor = nullptr;
	PxVec4 *dev_foam = nullptr;
	static constexpr uint32_t FOAM_MAX_PARTICLES = 65536;
	uint32_t foam_max_vertices = 0;
	uint32_t foam_max_triangles = 0;
	LocalVector<PxVec4> foam_host_vertices;
	LocalVector<PxU32> foam_host_indices;
	LocalVector<PxVec4> foam_host_normals;
	LocalVector<PxVec4> foam_host_positions;

	void init(PxCudaContextManager *p_cuda, uint32_t p_max_particles, float p_spacing, float p_foam_spacing) {
		cuda = p_cuda;
		const float rest_offset = 0.5f * p_spacing;
		const float foam_rest = 0.5f * p_foam_spacing;

		// PxGetPhysicsGpu is exported by the GPU-enabled core library on
		// Windows; the vendored Linux CPU libs and the prebuilt GPU runtime
		// do not carry it, so the smoothed-isosurface helper is unavailable
		// there and GPU fluids render their plain particle set instead.
		PxPhysicsGpu *gpu = nullptr;
#ifdef WINDOWS_ENABLED
		gpu = PxGetPhysicsGpu();
#endif
		ERR_FAIL_NULL_MSG(gpu, "PhysX: smoothed particle isosurface needs PxGetPhysicsGpu (Windows-only in the vendored SDK runtime).");

		// Moderately higher smoothing strength (default 0.5): the isosurface
		// follows a neighbour-averaged position field so a disturbed pool's edge
		// particles do not boil the surface. Too high and a thin spreading sheet
		// of fluid gets averaged away instead of pooling out.
		smoothing = gpu->createSmoothedPositionGenerator(cuda, p_max_particles, 0.65f);
		dev_smoothed = PX_EXT_DEVICE_MEMORY_ALLOC(PxVec4, *cuda, p_max_particles);
		smoothing->setResultBufferDevice(dev_smoothed);

		// min 1.0 keeps every ellipsoid at least a grid cell wide (smaller ones
		// flicker); max 1.4 is well below PhysX's default 2.0 -- a lone particle
		// at a stirred pool's rim otherwise stretches into a needle and the
		// surface crawls with tendrils.
		anisotropy = gpu->createAnisotropyGenerator(cuda, p_max_particles, 5.0f, 1.0f, 1.4f);
		dev_aniso1 = PX_EXT_DEVICE_MEMORY_ALLOC(PxVec4, *cuda, p_max_particles);
		dev_aniso2 = PX_EXT_DEVICE_MEMORY_ALLOC(PxVec4, *cuda, p_max_particles);
		dev_aniso3 = PX_EXT_DEVICE_MEMORY_ALLOC(PxVec4, *cuda, p_max_particles);
		anisotropy->setResultBufferDevice(dev_aniso1, dev_aniso2, dev_aniso3);

		max_vertices = 512 * 1024;
		max_triangles = 1024 * 1024;
		host_vertices.resize(max_vertices);
		host_indices.resize(3 * max_triangles);
		host_normals.resize(max_vertices);
		host_positions.resize(p_max_particles);

		PxSparseGridParams sgp;
		sgp.subgridSizeX = 16;
		sgp.subgridSizeY = 16;
		sgp.subgridSizeZ = 16;
		sgp.haloSize = 0;
		sgp.maxNumSubgrids = 2048;
		// Grid cell ~= 1.75x the particle diameter. Finer multiplies the
		// marching-cubes cost (and the subgrid / triangle budget) for a wide
		// spread of fluid with little visible gain once mesh smoothing runs.
		sgp.gridSpacing = 3.5f * rest_offset;

		PxIsosurfaceParams ip;
		// Well past the default 2x radius: a thin spreading pool is barely one
		// particle deep at its rim, so a tight iso distance breaks it into
		// wriggling isolated blobs. A wide reach fuses that rim into one
		// connected sheet (the bulk just renders a touch fat, which reads as
		// water anyway).
		ip.particleCenterToIsosurfaceDistance = 3.4f * rest_offset;
		// One Gaussian pass over the density field -- no GROW (it inflates the
		// falling stream into a slab and halves the frame rate) and no SHRINK
		// (it erodes thin sheets). The blur lifts the speckled rim of a stirred
		// pool above the iso level as one continuous surface instead of a mat of
		// wriggling tendrils; mesh smoothing then relaxes the remaining wobble.
		// Costs ~15 fps on a wide pool, which is the price of a calm edge.
		ip.clearFilteringPasses();
		ip.gridSmoothingRadius = sgp.gridSpacing;
		ip.addGridFilteringPass(PxIsosurfaceGridFilteringType::eSMOOTH);
		ip.numMeshSmoothingPasses = 9;
		ip.numMeshNormalSmoothingPasses = 5;

		extractor = gpu->createSparseGridIsosurfaceExtractor(cuda, sgp, ip, p_max_particles, max_vertices, max_triangles);
		if (extractor) {
			extractor->setResultBufferHost(host_vertices.ptr(), host_indices.ptr(), host_normals.ptr());
		}

		// Foam layer: a coarser grid over its own (small) position buffer.
		dev_foam = PX_EXT_DEVICE_MEMORY_ALLOC(PxVec4, *cuda, FOAM_MAX_PARTICLES);
		foam_max_vertices = 128 * 1024;
		foam_max_triangles = 256 * 1024;
		foam_host_vertices.resize(foam_max_vertices);
		foam_host_indices.resize(3 * foam_max_triangles);
		foam_host_normals.resize(foam_max_vertices);
		foam_host_positions.resize(FOAM_MAX_PARTICLES);

		PxSparseGridParams fsgp = sgp;
		fsgp.gridSpacing = 4.5f * foam_rest;
		PxIsosurfaceParams fip;
		// A wide reach so the sparse, scattered foam particles still blob together
		// into visible clumps of froth rather than isolated specks.
		fip.particleCenterToIsosurfaceDistance = 4.5f * foam_rest;
		fip.clearFilteringPasses();
		fip.numMeshSmoothingPasses = 4;
		fip.numMeshNormalSmoothingPasses = 4;
		foam_extractor = gpu->createSparseGridIsosurfaceExtractor(cuda, fsgp, fip, FOAM_MAX_PARTICLES, foam_max_vertices, foam_max_triangles);
		if (foam_extractor) {
			foam_extractor->setResultBufferHost(foam_host_vertices.ptr(), foam_host_indices.ptr(), foam_host_normals.ptr());
		}
	}

	void destroy() {
		if (extractor) {
			extractor->release();
			extractor = nullptr;
		}
		if (foam_extractor) {
			foam_extractor->release();
			foam_extractor = nullptr;
			PX_EXT_DEVICE_MEMORY_FREE(*cuda, dev_foam);
		}
		foam_host_vertices.reset();
		foam_host_indices.reset();
		foam_host_normals.reset();
		foam_host_positions.reset();
		if (smoothing) {
			smoothing->release();
			smoothing = nullptr;
			PX_EXT_DEVICE_MEMORY_FREE(*cuda, dev_smoothed);
		}
		if (anisotropy) {
			anisotropy->release();
			anisotropy = nullptr;
			PX_EXT_DEVICE_MEMORY_FREE(*cuda, dev_aniso1);
			PX_EXT_DEVICE_MEMORY_FREE(*cuda, dev_aniso2);
			PX_EXT_DEVICE_MEMORY_FREE(*cuda, dev_aniso3);
		}
		host_vertices.reset();
		host_indices.reset();
		host_normals.reset();
		host_positions.reset();
	}

	// Pin outliers: a particle that escapes the container drags the sparse
	// grid (and the mesh bounds) out to it -- the surface appears to stretch
	// to infinity. Clamp every particle to within clamp_reach meters of the
	// mean before feeding the extractor. Returns the mean (the foam kicker's
	// reference point). Shared by the inline and deferred extraction paths.
	// RD-8: the full-host round trip is throttled to every CLAMP_EVERY
	// extractions (reference solver's CLAMP_EVERY=4) -- skipped cycles run
	// with no sync at all, reusing the last clamped center. Escaped-particle
	// sprawl develops over many frames; a 4-frame throttle bounds it.
	uint32_t clamp_clock = 0;
	PxVec3 last_center = PxVec3(0.0f);

	PxVec3 clamp_outliers(PxU32 n) {
		clamp_clock++;
		if (clamp_clock % PBD_CLAMP_EVERY != 1 && !last_center.isZero()) {
			// Throttled cycle: reuse the last center, no device round trip.
			return last_center;
		}
		Ext::PxCudaHelpersExt::copyDToH(*cuda, host_positions.ptr(), dev_smoothed, n);
		PxVec3 center(0.0f);
		for (uint32_t i = 0; i < n; i++) {
			center += host_positions[i].getXYZ();
		}
		center *= 1.0f / (float)n;
		bool clamped_any = false;
		for (uint32_t i = 0; i < n; i++) {
			PxVec3 p = host_positions[i].getXYZ();
			const PxVec3 d = p - center;
			if (d.x < -clamp_reach || d.x > clamp_reach || d.y < -clamp_reach || d.y > clamp_reach || d.z < -clamp_reach || d.z > clamp_reach) {
				p.x = center.x + PxClamp(d.x, -clamp_reach, clamp_reach);
				p.y = center.y + PxClamp(d.y, -clamp_reach, clamp_reach);
				p.z = center.z + PxClamp(d.z, -clamp_reach, clamp_reach);
				host_positions[i] = PxVec4(p, host_positions[i].w);
				clamped_any = true;
			}
		}
		if (clamped_any) {
			Ext::PxCudaHelpersExt::copyHToD(*cuda, dev_smoothed, host_positions.ptr(), n);
		}
		last_center = center;
		return center;
	}

	void onBegin(const PxGpuMirroredPointer<PxGpuParticleSystem> &, CUstream) override {}
	void onAdvance(const PxGpuMirroredPointer<PxGpuParticleSystem> &, CUstream) override {}

	void onPostSolve(const PxGpuMirroredPointer<PxGpuParticleSystem> &p_gps, CUstream p_stream) override {
		if (!extractor || !owner) {
			return;
		}
		// Re-extract on every other solve -- 30 Hz surface updates are plenty for
		// water and this halves the cost.
		if ((frame++ & 1u) != 0u) {
			return;
		}

		// Publish the extractions kicked last cycle. Two full solves have run on
		// the stream since, so the host result buffers and counts are settled --
		// no synchronize needed. The mesh is one 30 Hz tick behind, invisible for
		// a liquid, and the GPU never stalls waiting on marching cubes.
		if (surface_pending) {
			publish_surface();
			surface_pending = false;
		}
		if (foam_pending) {
			publish_foam();
			foam_pending = false;
		}

		PxGpuParticleSystem &gps = *p_gps.mHostPtr;
		const PxU32 n = gps.mCommonData.mNumParticles;
		if (n == 0) {
			MutexLock lock(owner->mesh_mutex);
			// Bump the version with the clear: version-gated consumers skip
			// unchanged meshes, so without this an emptied fluid kept rendering
			// its last non-empty surface forever.
			owner->mesh_vertices.clear();
			owner->mesh_normals.clear();
			owner->mesh_indices.clear();
			owner->mesh_version++;
			return;
		}

		const bool use_aniso = owner->surface_anisotropy_enabled;

		smoothing->generateSmoothedPositions(p_gps.mDevicePtr, gps.mCommonData.mMaxParticles, p_stream);
		if (use_aniso) {
			anisotropy->generateAnisotropy(p_gps.mDevicePtr, gps.mCommonData.mMaxParticles, p_stream);
		}

		if (deferred) {
			// Batched mode: leave the kernels in flight and let the space's
			// finish_isosurface_extraction() pass (after fetchResults, once
			// every fluid's kernel is in flight) do the sync + clamp + extract.
			pending_gps = &gps;
			pending_n = n;
			pending_stream = p_stream;
			pending_aniso = use_aniso;
			extraction_pending = true;
			return;
		}

		// Inline mode (single-fluid spaces): the one unavoidable sync -- the
		// outlier clamp below reads the smoothed positions back to the host.
		cuda->getCudaContext()->streamSynchronize(p_stream);

		const PxVec3 center = clamp_outliers(n);

		// Anisotropy is passed only when the owner opts in (settled pools). For
		// emitting fluid it is left off -- fast particles along the emission
		// column stretch into ellipsoids that marching cubes meshes as needles.
		extractor->extractIsosurface(dev_smoothed, n, p_stream, gps.mUnsortedPhaseArray,
				PxParticlePhaseFlag::eParticlePhaseFluid, nullptr,
				use_aniso ? dev_aniso1 : nullptr,
				use_aniso ? dev_aniso2 : nullptr,
				use_aniso ? dev_aniso3 : nullptr,
				use_aniso ? gps.mCommonData.mParticleContactDistance : 1.0f);
		surface_pending = true;

		kick_foam(center, p_stream);
	}

	// Batched mode only (see onPostSolve): completes one fluid's deferred
	// extraction -- sync, outlier clamp, extraction + foam kicks. Called by
	// the space after fetchResults, once every fluid's smoothing kernel is
	// already in flight, so the first sync absorbs the whole batch's GPU work
	// instead of stalling per fluid mid-solve. No-op when nothing is pending.
	void finish_extraction() {
		if (!deferred || !extraction_pending || !owner) {
			return;
		}
		extraction_pending = false;
		PxGpuParticleSystem &gps = *pending_gps;
		const PxU32 n = pending_n;
		CUstream p_stream = pending_stream;
		const bool use_aniso = pending_aniso;

		// The one unavoidable sync (on an extraction cycle): the outlier clamp
		// below reads the smoothed positions back to the host. Every other
		// fluid's smoothing kernel was kicked before this runs, so this wait
		// overlaps the whole batch's GPU work.
		cuda->getCudaContext()->streamSynchronize(p_stream);

		const PxVec3 center = clamp_outliers(n);

		extractor->extractIsosurface(dev_smoothed, n, p_stream, gps.mUnsortedPhaseArray,
				PxParticlePhaseFlag::eParticlePhaseFluid, nullptr,
				use_aniso ? dev_aniso1 : nullptr,
				use_aniso ? dev_aniso2 : nullptr,
				use_aniso ? dev_aniso3 : nullptr,
				use_aniso ? gps.mCommonData.mParticleContactDistance : 1.0f);
		surface_pending = true;

		kick_foam(center, p_stream);
	}

	// Read the completed surface extraction (host buffers filled, counts settled)
	// into the owner's mesh arrays.
	void publish_surface() {
		uint32_t nv = MIN(extractor->getNumVertices(), max_vertices);
		uint32_t nt = MIN(extractor->getNumTriangles(), max_triangles);
		if (!surface_clip_warned && (extractor->getNumVertices() > max_vertices || extractor->getNumTriangles() > max_triangles)) {
			surface_clip_warned = true;
			WARN_PRINT("PhysXParticleFluid3D: isosurface mesh exceeded the buffer budget and was clipped -- part of the fluid will not be drawn. Reduce spawn_region_size / particle_count or coarsen the surface.");
		}
		for (uint32_t i = 0; i < nt * 3; i++) {
			if (host_indices[i] >= nv) {
				nt = 0;
				break;
			}
		}
		MutexLock lock(owner->mesh_mutex);
		owner->mesh_vertices.resize(nv);
		owner->mesh_normals.resize(nv);
		for (uint32_t i = 0; i < nv; i++) {
			const PxVec4 &v = host_vertices[i];
			const PxVec4 &nrm = host_normals[i];
			owner->mesh_vertices[i] = Vector3(v.x, v.y, v.z);
			owner->mesh_normals[i] = Vector3(nrm.x, nrm.y, nrm.z);
		}
		owner->mesh_indices.resize(nt * 3);
		for (uint32_t i = 0; i < nt * 3; i++) {
			owner->mesh_indices[i] = (int32_t)host_indices[i];
		}
		owner->mesh_version++;
	}

	// Kick the coarse diffuse-particle isosurface async on the stream.
	// p_fluid_center is the fluid median, used to pin foam spray that has flung
	// far from the body of water. Diffuse positions are final after the solve, so
	// the readback for the clamp needs no synchronize.
	void kick_foam(const PxVec3 &p_fluid_center, CUstream p_stream) {
		if (!foam_extractor) {
			return;
		}
		const PxU32 fn = (owner->foam_enabled && owner->px_buffer)
				? MIN(owner->px_buffer->getNbActiveDiffuseParticles(), FOAM_MAX_PARTICLES)
				: 0;
		PxVec4 *diffuse = (fn > 0) ? owner->px_buffer->getDiffusePositionLifeTime() : nullptr;
		if (!diffuse) {
			// No foam: clear the layer once (version stamp lets the node skip
			// while it stays empty).
			MutexLock lock(owner->foam_mesh_mutex);
			if (!owner->foam_mesh_indices.is_empty()) {
				owner->foam_mesh_vertices.clear();
				owner->foam_mesh_normals.clear();
				owner->foam_mesh_indices.clear();
				owner->foam_mesh_version++;
			}
			return;
		}
		Ext::PxCudaHelpersExt::copyDToH(*cuda, foam_host_positions.ptr(), diffuse, fn);
		const float reach = clamp_reach + 1.0f;
		for (uint32_t i = 0; i < fn; i++) {
			PxVec3 p = foam_host_positions[i].getXYZ();
			const PxVec3 d = p - p_fluid_center;
			p.x = p_fluid_center.x + PxClamp(d.x, -reach, reach);
			p.y = p_fluid_center.y + PxClamp(d.y, -reach, reach);
			p.z = p_fluid_center.z + PxClamp(d.z, -reach, reach);
			foam_host_positions[i] = PxVec4(p, 0.0f);
		}
		Ext::PxCudaHelpersExt::copyHToD(*cuda, dev_foam, foam_host_positions.ptr(), fn);

		foam_extractor->extractIsosurface(dev_foam, fn, p_stream, nullptr, 0, nullptr,
				nullptr, nullptr, nullptr, 1.0f);
		foam_pending = true;
	}

	void publish_foam() {
		uint32_t nv = MIN(foam_extractor->getNumVertices(), foam_max_vertices);
		uint32_t nt = MIN(foam_extractor->getNumTriangles(), foam_max_triangles);
		for (uint32_t i = 0; i < nt * 3; i++) {
			if (foam_host_indices[i] >= nv) {
				nt = 0;
				break;
			}
		}
		MutexLock lock(owner->foam_mesh_mutex);
		owner->foam_mesh_vertices.resize(nv);
		owner->foam_mesh_normals.resize(nv);
		for (uint32_t i = 0; i < nv; i++) {
			const PxVec4 &v = foam_host_vertices[i];
			const PxVec4 &nrm = foam_host_normals[i];
			owner->foam_mesh_vertices[i] = Vector3(v.x, v.y, v.z);
			owner->foam_mesh_normals[i] = Vector3(nrm.x, nrm.y, nrm.z);
		}
		owner->foam_mesh_indices.resize(nt * 3);
		for (uint32_t i = 0; i < nt * 3; i++) {
			owner->foam_mesh_indices[i] = (int32_t)foam_host_indices[i];
		}
		owner->foam_mesh_version++;
	}
};

static PxDiffuseParticleParams _diffuse_params(float p_lifetime, float p_threshold, float p_buoyancy) {
	PxDiffuseParticleParams params;
	params.threshold = p_threshold;
	params.lifetime = p_lifetime;
	params.buoyancy = p_buoyancy;
	params.airDrag = 0.0f;
	params.bubbleDrag = 0.9f;
	params.kineticEnergyWeight = 0.01f;
	params.pressureWeight = 1.0f;
	params.divergenceWeight = 10.0f;
	params.collisionDecay = 0.5f;
	params.useAccurateVelocity = false;
	return params;
}

PhysXGPUParticleFluid3D::~PhysXGPUParticleFluid3D() {
	set_space(nullptr);
}

void PhysXGPUParticleFluid3D::_destroy_isosurface() {
	if (!iso) {
		return;
	}
	// Extractor/device-buffer releases must not race an in-flight solve.
	if (space && space->is_stepping()) {
		space->ensure_synced();
	}
	if (px_system) {
		px_system->setParticleSystemCallback(nullptr);
	}
	iso->destroy();
	memdelete(iso);
	iso = nullptr;
	{
		MutexLock lock(mesh_mutex);
		mesh_vertices.clear();
		mesh_normals.clear();
		mesh_indices.clear();
	}
	{
		MutexLock lock(foam_mesh_mutex);
		foam_mesh_vertices.clear();
		foam_mesh_normals.clear();
		foam_mesh_indices.clear();
	}
}

void PhysXGPUParticleFluid3D::_ensure_isosurface() {
	if (iso || !surface_mesh_enabled || !px_system || !space) {
		return;
	}
	PxCudaContextManager *cuda = space->get_px_cuda();
	if (!cuda) {
		return;
	}
	iso = memnew(PhysXFluidIsosurface);
	iso->owner = this;
	iso->init(cuda, capacity, MAX((float)particle_size, 0.001f), MAX((float)foam_size, 0.001f));
	if (!iso->extractor) {
		_destroy_isosurface();
		return;
	}
	px_system->setParticleSystemCallback(iso);
}

void PhysXGPUParticleFluid3D::set_surface_mesh_enabled(bool p_enabled) {
	if (surface_mesh_enabled == p_enabled) {
		return;
	}
	surface_mesh_enabled = p_enabled;
	if (p_enabled) {
		_ensure_isosurface();
	} else {
		_destroy_isosurface();
	}
}

uint32_t PhysXGPUParticleFluid3D::get_surface_triangle_count() const {
	MutexLock lock(mesh_mutex);
	return (uint32_t)(mesh_indices.size() / 3);
}

uint32_t PhysXGPUParticleFluid3D::copy_surface_mesh(LocalVector<Vector3> &r_vertices, LocalVector<Vector3> &r_normals, LocalVector<int32_t> &r_indices, uint32_t &p_have_version) const {
	MutexLock lock(mesh_mutex);
	if (p_have_version == mesh_version) {
		return UINT32_MAX; // unchanged since the caller last asked -- skip the rebuild
	}
	p_have_version = mesh_version;
	r_vertices = mesh_vertices;
	r_normals = mesh_normals;
	r_indices = mesh_indices;
	return mesh_indices.size() / 3;
}

uint32_t PhysXGPUParticleFluid3D::copy_foam_mesh(LocalVector<Vector3> &r_vertices, LocalVector<Vector3> &r_normals, LocalVector<int32_t> &r_indices, uint32_t &p_have_version) const {
	MutexLock lock(foam_mesh_mutex);
	if (p_have_version == foam_mesh_version) {
		return UINT32_MAX;
	}
	p_have_version = foam_mesh_version;
	r_vertices = foam_mesh_vertices;
	r_normals = foam_mesh_normals;
	r_indices = foam_mesh_indices;
	return foam_mesh_indices.size() / 3;
}

void PhysXGPUParticleFluid3D::_destroy() {
	// The releases below are touched by an in-flight solve until it completes;
	// fetch first so they can't race it under async stepping.
	if (space && space->is_stepping()) {
		space->ensure_synced();
	}
	_destroy_isosurface();
	if (px_buffer && px_system) {
		px_system->removeParticleBuffer(px_buffer);
	}
	if (px_buffer) {
		px_buffer->release();
		px_buffer = nullptr;
	}
	if (px_system) {
		// Routed through the space so a mid-flight destroy (async stepping)
		// queues the scene removal and the release until after the fetch.
		if (space) {
			space->remove_actor(px_system);
			space->release_actor(px_system);
		} else {
			px_system->release();
		}
		px_system = nullptr;
	}
	if (px_material) {
		px_material->release();
		px_material = nullptr;
	}
	active_count = 0;
	foam_count = 0;
	read_scratch.clear();
	read_positions.clear();
	foam_scratch.clear();
	foam_positions.clear();
	dirty_material = true;
	dirty_foam = true;
}

void PhysXGPUParticleFluid3D::set_space(PhysXSpace3D *p_space) {
	if (space == p_space) {
		return;
	}
	_destroy();
	if (space) {
		space->unregister_fluid(this);
	}
	space = p_space;
	if (space) {
		space->register_fluid(this);
	}
}

void PhysXGPUParticleFluid3D::_ensure_system() {
	if (px_system) {
		return;
	}
	if (!space) {
		return;
	}
	PxCudaContextManager *cuda = space->get_px_cuda();
	if (!cuda) {
		WARN_PRINT_ONCE("PhysXParticleFluid3D needs GPU dynamics (a physx_gpu build with a CUDA device); the fluid stays inert.");
		return;
	}
	PxPhysics *physics = space->get_px_physics();
	PxScene *scene = space->get_px_scene();
	ERR_FAIL_NULL(physics);
	ERR_FAIL_NULL(scene);

	px_material = physics->createPBDMaterial(
			granular ? CLAMP((PxReal)granular_friction, 0.0f, 2.0f) : (PxReal)MAX(surface_friction, 0.0f), // friction
			0.0f, // damping
			granular ? 0.0f : (PxReal)adhesion,
			granular ? 0.0f : (PxReal)viscosity,
			granular ? 0.0f : (PxReal)vorticity,
			granular ? 0.0f : (PxReal)surface_tension,
			granular ? 0.0f : (PxReal)cohesion,
			0.0f, // lift (deprecated)
			0.0f); // drag (deprecated)
	ERR_FAIL_NULL(px_material);
	px_material->setGravityScale((PxReal)gravity_scale);
	if (granular) {
		// Grain-on-grain friction is a separate scale from the base coefficient;
		// PBD particle friction is weak for piling, so crank it hard (the range
		// is [0, inf)).
		px_material->setParticleFrictionScale(8.0f);
		px_material->setParticleAdhesionScale(0.0f);
		px_material->setDamping(0.5f);
	}

	px_system = physics->createPBDParticleSystem(*cuda, 96);
	ERR_FAIL_NULL_MSG(px_system, "PhysX: createPBDParticleSystem failed.");

	// Rest/contact offsets derived from the particle spacing, matching PhysX's
	// PBF snippet.
	const PxReal spacing = MAX((PxReal)particle_size, 0.001f);
	const PxReal rest_offset = 0.5f * spacing / 0.6f;
	const PxReal fluid_rest_offset = rest_offset * 0.6f;
	px_system->setRestOffset(rest_offset);
	px_system->setContactOffset(rest_offset + 0.01f);
	// PxPBDParticleSystem requires particleContactOffset > max(solidRestOffset,
	// fluidRestOffset) — the old fluid_rest_offset/0.6 resolved EXACTLY to
	// rest_offset (== solidRestOffset), violating the documented open range.
	// A small margin above satisfies the range check without changing behavior
	// meaningfully (the PBF snippet's spacing-derived offsets are approximate).
	px_system->setParticleContactOffset(rest_offset * 1.05f);
	px_system->setSolidRestOffset(rest_offset);
	px_system->setFluidRestOffset(fluid_rest_offset);
	px_system->setMaxLinearVelocity(rest_offset * 100.0f);
	if (granular) {
		// PBD friction only converges to a real angle of repose with many
		// position iterations; the fluid default is far too few for a pile.
		px_system->setSolverIterationCounts(16, 1);
		px_system->setMaxDepenetrationVelocity(rest_offset * 20.0f);
	}
	// Without this, speculative contacts let dense bodies rest on the fluid
	// surface instead of sinking through it (PhysX's PBF snippet also disables it).
	// Granular keeps CCD -- a solid pile benefits from it and there is no surface
	// for a body to falsely rest on.
	px_system->setParticleFlag(PxParticleFlag::eENABLE_SPECULATIVE_CCD, granular);

	// Fluid phase gets the density/cohesion constraints; granular drops the fluid
	// flag so the particles are solid grains that pile and hold a slope, keeping
	// only self-collision.
	fluid_phase = px_system->createPhase(px_material,
			granular
					? PxParticlePhaseFlags(PxParticlePhaseFlag::eParticlePhaseSelfCollide)
					: PxParticlePhaseFlags(PxParticlePhaseFlag::eParticlePhaseFluid | PxParticlePhaseFlag::eParticlePhaseSelfCollide));

	// Our scene filter shader suppresses any pair whose layer/mask cross-check is
	// zero; a particle system's filter data defaults to all-zero, so give it
	// layer bit 0 and a full mask so it collides with the standard body layers.
	px_system->setSimulationFilterData(PxFilterData(1, 0xFFFFFFFF, 0, 0));

	space->add_actor(px_system);
	dirty_material = false;
}

void PhysXGPUParticleFluid3D::_apply_material() {
	if (!px_material || !dirty_material) {
		return;
	}
	if (granular) {
		px_material->setFriction(CLAMP((PxReal)granular_friction, 0.0f, 2.0f));
		px_material->setViscosity(0.0f);
		px_material->setSurfaceTension(0.0f);
		px_material->setCohesion(0.0f);
		px_material->setAdhesion(0.0f);
		px_material->setVorticityConfinement(0.0f);
	} else {
		px_material->setViscosity((PxReal)viscosity);
		px_material->setSurfaceTension((PxReal)surface_tension);
		px_material->setFriction((PxReal)MAX(surface_friction, 0.0f));
		px_material->setCohesion((PxReal)cohesion);
		px_material->setAdhesion((PxReal)adhesion);
		px_material->setVorticityConfinement((PxReal)vorticity);
	}
	px_material->setGravityScale((PxReal)gravity_scale);
	dirty_material = false;
}

void PhysXGPUParticleFluid3D::_apply_foam_params() {
	if (!px_buffer || !dirty_foam) {
		return;
	}
	px_buffer->setDiffuseParticleParams(_diffuse_params((float)foam_lifetime, (float)foam_threshold, (float)foam_buoyancy));
	px_buffer->raiseFlags(PxParticleBufferFlag::eUPDATE_DIFFUSE_PARAM);
	dirty_foam = false;
}

void PhysXGPUParticleFluid3D::set_param(Param p_param, real_t p_value) {
	switch (p_param) {
		case PARAM_VISCOSITY:
			viscosity = p_value;
			break;
		case PARAM_SURFACE_TENSION:
			surface_tension = p_value;
			break;
		case PARAM_SURFACE_FRICTION:
			surface_friction = p_value;
			break;
		case PARAM_COHESION:
			cohesion = p_value;
			break;
		case PARAM_ADHESION:
			adhesion = p_value;
			break;
		case PARAM_VORTICITY:
			vorticity = p_value;
			break;
		case PARAM_GRAVITY_SCALE:
			gravity_scale = p_value;
			break;
		case PARAM_PARTICLE_SIZE:
			particle_size = MAX(p_value, (real_t)0.001);
			break;
		default:
			return;
	}
	dirty_material = true;
	_apply_material();
}

real_t PhysXGPUParticleFluid3D::get_param(Param p_param) const {
	switch (p_param) {
		case PARAM_VISCOSITY:
			return viscosity;
		case PARAM_SURFACE_TENSION:
			return surface_tension;
		case PARAM_SURFACE_FRICTION:
			return surface_friction;
		case PARAM_COHESION:
			return cohesion;
		case PARAM_ADHESION:
			return adhesion;
		case PARAM_VORTICITY:
			return vorticity;
		case PARAM_GRAVITY_SCALE:
			return gravity_scale;
		case PARAM_PARTICLE_SIZE:
			return particle_size;
		default:
			return 0.0;
	}
}

void PhysXGPUParticleFluid3D::set_granular(bool p_enabled, real_t p_friction) {
	granular_friction = p_friction;
	if (granular == p_enabled) {
		dirty_material = true;
		_apply_material();
		return;
	}
	granular = p_enabled;
	// The phase (fluid vs granular) is baked into the particle system at
	// creation; rebuild it. The node clears and re-seeds particles after this.
	if (px_system) {
		_destroy();
	}
	dirty_material = true;
}

void PhysXGPUParticleFluid3D::set_capacity(uint32_t p_capacity) {
	p_capacity = MAX(p_capacity, 1u);
	if (p_capacity == capacity) {
		return;
	}
	capacity = p_capacity;
	// The buffer is fixed size; drop it so the next spawn/emit rebuilds it.
	clear();
	// The isosurface scratch buffers (device + pinned host) are sized from the
	// capacity at init(), while onPostSolve feeds the NEW particle-buffer
	// capacity into the extractor — a stale extractor would overrun its
	// buffers. Rebuild it against the new capacity (same pattern as
	// set_foam_size). clear() left px_system alive, so _ensure_isosurface()
	// can re-register the callback.
	if (iso) {
		_destroy_isosurface();
		_ensure_isosurface();
	}
}

void PhysXGPUParticleFluid3D::clear() {
	// Releasing the buffer races an in-flight solve under async stepping.
	if (space && space->is_stepping()) {
		space->ensure_synced();
	}
	if (px_buffer && px_system) {
		px_system->removeParticleBuffer(px_buffer);
	}
	if (px_buffer) {
		px_buffer->release();
		px_buffer = nullptr;
	}
	active_count = 0;
	write_head = 0;
	foam_count = 0;
	read_scratch.clear();
	read_positions.clear();
	foam_scratch.clear();
	foam_positions.clear();
}

void PhysXGPUParticleFluid3D::set_foam_enabled(bool p_enabled) {
	if (foam_enabled == p_enabled) {
		return;
	}
	foam_enabled = p_enabled;
	if (p_enabled) {
		WARN_PRINT_ONCE(
				"PhysX: foam (diffuse particles) is currently inert — allocating an "
				"active diffuse budget corrupts the GPU runtime (CUDA error 700, all "
				"SDK versions tested through 5.11). See maxActiveDiffuseParticles in "
				"PhysXGPUParticleFluid3D::_ensure_buffer().");
	}
	// The diffuse capacity is baked into the buffer; rebuild it on the next fill.
	clear();
}

void PhysXGPUParticleFluid3D::set_foam_capacity(uint32_t p_capacity) {
	p_capacity = MAX(p_capacity, 1u);
	if (p_capacity == foam_capacity) {
		return;
	}
	foam_capacity = p_capacity;
	if (foam_enabled) {
		clear();
	}
}

void PhysXGPUParticleFluid3D::set_foam_lifetime(real_t p_v) {
	foam_lifetime = MAX(p_v, (real_t)0.01);
	dirty_foam = true;
	_apply_foam_params();
}

void PhysXGPUParticleFluid3D::set_foam_threshold(real_t p_v) {
	foam_threshold = MAX(p_v, (real_t)0.0);
	dirty_foam = true;
	_apply_foam_params();
}

void PhysXGPUParticleFluid3D::set_foam_buoyancy(real_t p_v) {
	foam_buoyancy = CLAMP(p_v, (real_t)0.0, (real_t)1.0);
	dirty_foam = true;
	_apply_foam_params();
}

void PhysXGPUParticleFluid3D::set_foam_size(real_t p_v) {
	p_v = MAX(p_v, (real_t)0.001);
	if (p_v == foam_size) {
		return;
	}
	foam_size = p_v;
	// The foam grid spacing is baked into the extractor -- rebuild it.
	if (iso) {
		_destroy_isosurface();
		_ensure_isosurface();
	}
}

bool PhysXGPUParticleFluid3D::supports_diffuse_particles() {
#if defined(_WIN32)
	return true;
#else
	// physx_extgpu_stub_linux.cpp stands in for the factory on this platform.
	return false;
#endif
}

void PhysXGPUParticleFluid3D::_ensure_buffer() {
	if (px_buffer) {
		return;
	}
	if (!supports_diffuse_particles()) {
		// The particle+diffuse buffer factory only ships in the GPU-enabled
		// Windows libraries (see physx_extgpu_stub_linux.cpp); without it no
		// PBD fluid buffer can exist on this platform, foam or not. The node's
		// solver resolution routes to the MPM backend before reaching here.
		WARN_PRINT_ONCE("PhysX: the CUDA particle fluid buffer factory is unavailable on this platform (Windows only through PhysX 5.11); the PBD fluid stays inert -- use the MPM solver.");
		return;
	}
	_ensure_system();
	if (!px_system) {
		return;
	}
	PxCudaContextManager *cuda = space->get_px_cuda();

	// Create an empty buffer at full capacity; particles are added later.
	PxVec4 seed_pos(0.0f);
	PxVec4 seed_vel(0.0f);
	PxU32 seed_phase = fluid_phase;
	const PxU32 max_diffuse = foam_enabled ? MAX(foam_capacity, 1u) : 0u;
	ExtGpu::PxParticleAndDiffuseBufferDesc desc;
	desc.maxParticles = capacity;
	desc.numActiveParticles = 0;
	desc.positions = &seed_pos;
	desc.velocities = &seed_vel;
	desc.phases = &seed_phase;
	desc.maxDiffuseParticles = max_diffuse;
	// NOTE: the ACTIVE diffuse allocation (maxActiveDiffuseParticles) must stay
	// zero. Re-verified on PhysX 5.11 (CUDA 12.8.2): setting it — via the desc,
	// right after creation, or deferred until after the buffer has simulated —
	// corrupts GPU state: every host<->device upload on the buffer then fails
	// with CUDA error 700 and PhysX aborts GPU simulation for the scene (the
	// module's GPU smoke test reproduces it: --stages=fluid runs clean; adding
	// foamearly storms error 700). The practical consequence: the solver has no
	// diffuse-particle budget, so foam does not spawn. Everything else (fluid
	// sim, emission, isosurface rendering, submersion) works with this
	// configuration. Re-test on every SDK upgrade.
	desc.maxActiveDiffuseParticles = 0u;
	desc.diffuseParams = _diffuse_params((float)foam_lifetime, (float)foam_threshold, (float)foam_buoyancy);

	px_buffer = ExtGpu::PxCreateAndPopulateParticleAndDiffuseBuffer(desc, cuda);
	ERR_FAIL_NULL_MSG(px_buffer, "PhysX: PxCreateAndPopulateParticleAndDiffuseBuffer failed.");
	px_system->addParticleBuffer(px_buffer);
	active_count = 0;
	write_head = 0;
	foam_count = 0;
	dirty_foam = false;

	_ensure_isosurface();
}

void PhysXGPUParticleFluid3D::_write_particles(uint32_t p_at, const Vector<Vector3> &p_positions, const Vector3 &p_velocity) {
	const uint32_t n = MIN((uint32_t)p_positions.size(), capacity);
	if (n == 0 || !px_buffer) {
		return;
	}
	// The host->device copies below are legal only between steps.
	if (space && space->is_stepping()) {
		space->ensure_synced();
	}
	PxCudaContextManager *cuda = space->get_px_cuda();
	const PxReal spacing = MAX((PxReal)particle_size, 0.001f);
	// Each particle stands in for a spacing^3 cell of fluid, so its mass is the
	// rest density times that volume -- this keeps the effective fluid density
	// near FLUID_DENSITY.
	const PxReal particle_mass = FLUID_DENSITY * spacing * spacing * spacing;
	const PxReal inv_mass = particle_mass > 0.0f ? 1.0f / particle_mass : 1.0f;
	const PxVec4 vel(godot_to_px(p_velocity), 0.0f);

	LocalVector<PxVec4> positions;
	LocalVector<PxVec4> velocities;
	LocalVector<PxU32> phases;
	positions.resize(n);
	velocities.resize(n);
	phases.resize(n);
	const Vector3 *src = p_positions.ptr();
	for (uint32_t i = 0; i < n; i++) {
		positions[i] = PxVec4(godot_to_px(src[i]), inv_mass);
		velocities[i] = vel;
		phases[i] = fluid_phase;
	}

	PxVec4 *dev_pos = px_buffer->getPositionInvMasses();
	PxVec4 *dev_vel = px_buffer->getVelocities();
	PxU32 *dev_phase = px_buffer->getPhases();

	// Copy in up to two runs so a write that crosses the end of the ring wraps.
	uint32_t done = 0;
	while (done < n) {
		const uint32_t slot = (p_at + done) % capacity;
		const uint32_t run = MIN(n - done, capacity - slot);
		Ext::PxCudaHelpersExt::copyHToD(*cuda, dev_pos + slot, positions.ptr() + done, run);
		Ext::PxCudaHelpersExt::copyHToD(*cuda, dev_vel + slot, velocities.ptr() + done, run);
		Ext::PxCudaHelpersExt::copyHToD(*cuda, dev_phase + slot, phases.ptr() + done, run);
		done += run;
	}
	px_buffer->raiseFlags(PxParticleBufferFlag::eUPDATE_POSITION);
	px_buffer->raiseFlags(PxParticleBufferFlag::eUPDATE_VELOCITY);
	px_buffer->raiseFlags(PxParticleBufferFlag::eUPDATE_PHASE);
}

void PhysXGPUParticleFluid3D::set_particles(const Vector<Vector3> &p_positions, const Vector3 &p_initial_velocity) {
	clear();
	_ensure_buffer();
	if (!px_buffer) {
		return;
	}
	const uint32_t n = MIN((uint32_t)p_positions.size(), capacity);
	_write_particles(0, p_positions, p_initial_velocity);
	active_count = n;
	write_head = n % capacity;
	px_buffer->setNbActiveParticles(active_count);
}

void PhysXGPUParticleFluid3D::emit(const Vector<Vector3> &p_positions, const Vector3 &p_velocity) {
	_ensure_buffer();
	if (!px_buffer) {
		return;
	}
	const uint32_t n = MIN((uint32_t)p_positions.size(), capacity);
	if (n == 0) {
		return;
	}
	_write_particles(write_head, p_positions, p_velocity);
	write_head = (write_head + n) % capacity;
	active_count = MIN(active_count + n, capacity);
	px_buffer->setNbActiveParticles(active_count);
}

void PhysXGPUParticleFluid3D::set_deferred_extraction(bool p_deferred) {
	if (iso) {
		iso->deferred = p_deferred;
	}
}

void PhysXGPUParticleFluid3D::finish_isosurface_extraction() {
	if (iso) {
		iso->finish_extraction();
	}
}

void PhysXGPUParticleFluid3D::read_back() {
	if (!px_buffer || !space) {
		return;
	}
	PxCudaContextManager *cuda = space->get_px_cuda();
	if (!cuda) {
		return;
	}
	active_count = px_buffer->getNbActiveParticles();
	if (active_count == 0) {
		read_positions.clear();
		return;
	}
	read_scratch.resize(active_count);
	Ext::PxCudaHelpersExt::copyDToH(*cuda,
			reinterpret_cast<PxVec4 *>(read_scratch.ptr()),
			px_buffer->getPositionInvMasses(),
			active_count);

	read_positions.resize(active_count);
	for (uint32_t i = 0; i < active_count; i++) {
		const Vector4 &p = read_scratch[i];
		read_positions[i] = Vector3(p.x, p.y, p.z);
	}

	foam_count = foam_enabled ? px_buffer->getNbActiveDiffuseParticles() : 0;
	if (foam_count == 0) {
		foam_positions.clear();
		return;
	}
	foam_scratch.resize(foam_count);
	Ext::PxCudaHelpersExt::copyDToH(*cuda,
			reinterpret_cast<PxVec4 *>(foam_scratch.ptr()),
			px_buffer->getDiffusePositionLifeTime(),
			foam_count);
	foam_positions.resize(foam_count);
	for (uint32_t i = 0; i < foam_count; i++) {
		const Vector4 &p = foam_scratch[i];
		foam_positions[i] = Vector3(p.x, p.y, p.z);
	}
}

real_t PhysXGPUParticleFluid3D::get_submersion(const AABB &p_world_aabb) const {
	const real_t box_vol = p_world_aabb.get_volume();
	if (box_vol <= 0.0 || read_positions.is_empty()) {
		return 0.0;
	}
	uint32_t inside = 0;
	const Vector3 *p = read_positions.ptr();
	for (uint32_t i = 0; i < read_positions.size(); i++) {
		if (p_world_aabb.has_point(p[i])) {
			inside++;
		}
	}
	// Each particle stands in for a particle_size^3 cell of fluid.
	const real_t s = MAX(particle_size, (real_t)0.001);
	const real_t filled = (real_t)inside * s * s * s;
	return CLAMP(filled / box_vol, (real_t)0.0, (real_t)1.0);
}

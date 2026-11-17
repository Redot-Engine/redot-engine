// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PHYSICS_GPU_H
#define PX_PHYSICS_GPU_H


#include "cudamanager/PxCudaContext.h"
#include "cudamanager/PxCudaContextManager.h"

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxVec4.h"
#include "PxParticleSystem.h"

#include "foundation/PxArray.h"
#include "PxParticleGpu.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

#if PX_SUPPORT_GPU_PHYSX		

	class PxSceneDesc;

	class PxIsosurfaceExtractor;
	class PxSparseGridIsosurfaceExtractor;
	class PxAnisotropyGenerator;
	class PxSmoothedPositionGenerator;
	class PxParticleNeighborhoodProvider;

	class PxArrayConverter;
	class PxSDFBuilder;
	class PxDeformableSkinning;

	struct PxIsosurfaceParams;
	struct PxSparseGridParams;
	
	class PxPhysicsGpu
	{
	public:
		/**
		\brief Creates an isosurface extractor operating on a dense grid

		\param[in] cudaContextManager A cuda context manager
		\param[in] worldBounds The bounds of the internally used dense grid. The isosurface can only be generated inside those bounds.
		\param[in] cellSize The size of a single grid cell
		\param[in] isosurfaceParams The isosurface parameters to control the isolevel etc.
		\param[in] maxNumParticles The maximal number of particles that can be processed
		\param[in] maxNumVertices The maximal number of vertices the output buffer can hold
		\param[in] maxNumTriangles The maximal number of triangles the output buffer can hold
		*/
		virtual PxIsosurfaceExtractor* createDenseGridIsosurfaceExtractor(PxCudaContextManager* cudaContextManager, const PxBounds3& worldBounds,
			PxReal cellSize, const PxIsosurfaceParams& isosurfaceParams, PxU32 maxNumParticles, PxU32 maxNumVertices = 512 * 1024, PxU32 maxNumTriangles = 1024 * 1024) = 0;

		/**
		\brief Creates an isosurface extractor operating on a sparse grid

		\param[in] cudaContextManager A cuda context manager
		\param[in] sparseGridParams The sparse grid parameters defining the cell size etc.
		\param[in] isosurfaceParams The isosurface parameters to control the isolevel etc.
		\param[in] maxNumParticles The maximal number of particles that can be processed
		\param[in] maxNumVertices The maximal number of vertices the output buffer can hold
		\param[in] maxNumTriangles The maximal number of triangles the output buffer can hold
		*/
		virtual PxSparseGridIsosurfaceExtractor* createSparseGridIsosurfaceExtractor(PxCudaContextManager* cudaContextManager, const PxSparseGridParams& sparseGridParams,
			const PxIsosurfaceParams& isosurfaceParams, PxU32 maxNumParticles, PxU32 maxNumVertices = 512 * 1024, PxU32 maxNumTriangles = 1024 * 1024) = 0;


		/**
		\brief Creates an anisotropy generator

		\param[in] cudaContextManager A cuda context manager
		\param[in] maxNumParticles The number of particles
		\param[in] anisotropyScale A uniform scaling factor to increase or decrease anisotropy
		\param[in] minAnisotropy The minimum scaling factor in any dimension that anisotropy can have
		\param[in] maxAnisotropy The maximum scaling factor in any dimension that anisotropy can have
		*/
		virtual PxAnisotropyGenerator* createAnisotropyGenerator(PxCudaContextManager* cudaContextManager, PxU32 maxNumParticles,
			PxReal anisotropyScale = 1.0f, PxReal minAnisotropy = 0.1f, PxReal maxAnisotropy = 2.0f) = 0;

		/**
		\brief Creates a smoothed position generator

		\param[in] cudaContextManager A cuda context manager
		\param[in] maxNumParticles The number of particles
		\param[in] smoothingStrength Controls the strength of the smoothing effect
		*/
		virtual PxSmoothedPositionGenerator* createSmoothedPositionGenerator(PxCudaContextManager* cudaContextManager, PxU32 maxNumParticles, PxReal smoothingStrength = 0.5f) = 0;


		/**
		\brief Creates a neighborhood provider

		\param[in] cudaContextManager A cuda context manager
		\param[in] maxNumParticles The number of particles
		\param[in] cellSize The grid cell size. Should be equal to 2*contactOffset for PBD particle systems.
		\param[in] maxNumSparseGridCells The maximal number of cells the internally used sparse grid can provide
		*/
		virtual PxParticleNeighborhoodProvider* createParticleNeighborhoodProvider(PxCudaContextManager* cudaContextManager, const PxU32 maxNumParticles,
			const PxReal cellSize, const PxU32 maxNumSparseGridCells = 262144) = 0;

		/**
		\brief Creates an array converter. If not used anymore, the caller needs to delete the returned pointer.

		\param[in] cudaContextManager A cuda context manager
		*/
		virtual PxArrayConverter* createArrayConverter(PxCudaContextManager* cudaContextManager) = 0;

		/**
		\brief Creates sdf builder to construct sdfs quickly on the GPU. If not used anymore, the caller needs to delete the returned pointer.

		\param[in] cudaContextManager A cuda context manager

		\return Pointer to a new instance of a PxSDFBuilder
		*/
		virtual PxSDFBuilder* createSDFBuilder(PxCudaContextManager* cudaContextManager) = 0;

		/**
		\brief Creates a deformable skinning instance to perform skinning operations on the GPU. If not used anymore, the caller needs to delete the returned pointer.

		\param[in] cudaContextManager A cuda context manager

		\return Pointer to a new instance of a PxDeformableSkinning
		*/
		virtual PxDeformableSkinning* createDeformableSkinning(PxCudaContextManager* cudaContextManager) = 0;

		virtual void release() = 0;

		virtual ~PxPhysicsGpu() {}
	};

#endif	

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

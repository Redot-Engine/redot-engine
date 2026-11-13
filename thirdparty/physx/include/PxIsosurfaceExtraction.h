// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_ISOSURFACE_EXTRACTION_H
#define PX_ISOSURFACE_EXTRACTION_H


#include "cudamanager/PxCudaContext.h"
#include "cudamanager/PxCudaContextManager.h"

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxVec4.h"
#include "PxParticleSystem.h"

#include "PxSparseGridParams.h"

#include "foundation/PxArray.h"
#include "PxParticleGpu.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
	
#if PX_SUPPORT_GPU_PHYSX
		
	/**
	\brief Identifies filter type to be applied on the isosurface grid.
	*/
	struct PxIsosurfaceGridFilteringType
	{
		enum Enum
		{
			eNONE = 0,		//!< No filtering
			eSMOOTH = 1,	//!< Gaussian-blur-like filtering
			eGROW = 2,		//!< A dilate/erode operation will be applied that makes the fluid grow approximately one cell size
			eSHRINK = 3	//!< A dilate/erode operation will be applied that makes the fluid shrink approximately one cell size
		};
	};

	/**
	\brief Parameters to define the isosurface extraction settings like isosurface level, filtering etc.
	*/
	struct PxIsosurfaceParams
	{
		/**
		\brief Default constructor.
		*/
		PX_INLINE PxIsosurfaceParams()
		{
			particleCenterToIsosurfaceDistance = 0.2f;
			numMeshSmoothingPasses = 4;
			numMeshNormalSmoothingPasses = 4;
			gridFilteringFlags = 0;
			gridSmoothingRadius = 0.2f;
		}

		/**
		\brief Clears the filtering operations
		*/
		PX_INLINE void clearFilteringPasses()
		{
			gridFilteringFlags = 0;
		}

		/**
		\brief Adds a smoothing pass after the existing ones

		At most 32 smoothing passes can be defined. Every pass can repeat itself up to 4 times.

		\param[in] operation The smoothing operation to add
		\return The index of the smoothing pass that was added
		*/
		PX_INLINE PxU64 addGridFilteringPass(PxIsosurfaceGridFilteringType::Enum operation)
		{
			for (PxU32 i = 0; i < 32; ++i)
			{
				PxIsosurfaceGridFilteringType::Enum o;				
				if (!getGridFilteringPass(i, o))
				{
					setGridFilteringPass(i, operation);
					return i;
				}
			}
			return 0xFFFFFFFFFFFFFFFF;
		}

		/**
		\brief Sets the operation of a smoothing pass

		\param[in] passIndex The index of the smoothing pass whose operation should be set <b>Range:</b> [0, 31]
		\param[in] operation The operation the modified smoothing will perform
		*/
		PX_INLINE void setGridFilteringPass(PxU32 passIndex, PxIsosurfaceGridFilteringType::Enum operation)
		{
			PX_ASSERT(passIndex < 32u);
			PxU32 shift = passIndex * 2;
			gridFilteringFlags &= ~(PxU64(3) << shift);
			gridFilteringFlags |= PxU64(operation) << shift;
		}

		/**
		\brief Returns the operation of a smoothing pass

		\param[in] passIndex The index of the smoothing pass whose operation is requested <b>Range:</b> [0, 31]
		\param[out] operation The operation the requested smoothing pass will perform
		\return true if the requested pass performs an operation
		*/
		PX_INLINE bool getGridFilteringPass(PxU32 passIndex, PxIsosurfaceGridFilteringType::Enum& operation) const
		{
			PxU32 shift = passIndex * 2;
			PxU64 v = gridFilteringFlags >> shift;
			v &= 3; //Extract last 2 bits
			operation = PxIsosurfaceGridFilteringType::Enum(v);
			return operation != PxIsosurfaceGridFilteringType::eNONE;
		}

		PxReal	particleCenterToIsosurfaceDistance;			//!< Distance form a particle center to the isosurface
		PxU32	numMeshSmoothingPasses;						//!< Number of Taubin mesh postprocessing smoothing passes. Using an even number of passes lead to less shrinking.			
		PxU32	numMeshNormalSmoothingPasses;				//!< Number of mesh normal postprocessing smoothing passes.
		PxU64	gridFilteringFlags;							//!< Encodes the smoothing steps to apply on the sparse grid. Use setGridSmoothingPass method to set up.
		PxReal	gridSmoothingRadius;						//!< Gaussian blur smoothing kernel radius used for smoothing operations on the grid
	};

	/**
	\brief Base class for isosurface extractors. Allows to register the data arrays for the isosurface and to obtain the number vertices/triangles in use.
	*/
	class PxIsosurfaceExtractor
	{
	public:
		/**
		\brief Returns the isosurface parameters. 

		\return The isosurfacesettings used for the isosurface extraction
		*/
		virtual PxIsosurfaceParams getIsosurfaceParams() const = 0;

		/**
		\brief Set the isosurface extraction parameters

		Allows to configure the isosurface extraction by controlling threshold value, smoothing options etc.

		\param[in] params A collection of settings to control the isosurface extraction
		*/
		virtual void setIsosurfaceParams(const PxIsosurfaceParams& params) = 0;

		/**
		\brief Returns the number of vertices that the current isosurface triangle mesh uses

		\return The number of vertices currently in use
		*/
		virtual PxU32 getNumVertices() const = 0;
			
		/**
		\brief Returns the number of triangles that the current isosurface triangle mesh uses

		\return The number of triangles currently in use
		*/
		virtual PxU32 getNumTriangles() const = 0;

		/**
		\brief Returns the maximum number of vertices that the isosurface triangle mesh can contain

		\return The maximum number of vertices that can be genrated
		*/
		virtual PxU32 getMaxVertices() const = 0;

		/**
		\brief Returns the maximum number of triangles that the isosurface triangle mesh can contain

		\return The maximum number of triangles that can be generated
		*/
		virtual PxU32 getMaxTriangles() const = 0;

		/**
		\brief Resizes the internal triangle mesh buffers. 
			
		If the output buffers are device buffers, nothing will get resized but new output buffers can be set using setResultBufferDevice. 
		For host side output buffers, temporary buffers will get resized. The new host side result buffers with the same size must be set using setResultBufferHost.

		\param[in] maxNumVertices The maximum number of vertices the output buffer can hold
		\param[in] maxNumTriangles The maximum number of triangles the ouput buffer can hold
		*/
		virtual void setMaxVerticesAndTriangles(PxU32 maxNumVertices, PxU32 maxNumTriangles) = 0;

		/**
		\brief The maximal number of particles the isosurface extractor can process

		\return The maximal number of particles
		*/
		virtual PxU32 getMaxParticles() const = 0;

		/**
		\brief Sets the maximal number of particles the isosurface extractor can process

		\param[in] maxParticles The maximal number of particles
		*/
		virtual void setMaxParticles(PxU32 maxParticles) = 0;

		/**
		\brief Releases the isosurface extractor instance and its data
		*/
		virtual void release() = 0;

		/**
		\brief Triggers the compuation of a new isosurface based on the specified particle locations

		\param[in] deviceParticlePos A gpu pointer pointing to the start of the particle array
		\param[in] numParticles The number of particles
		\param[in] stream The stream on which all the gpu work will be performed
		\param[in] phases A phase value per particle
		\param[in] validPhaseMask A mask that specifies which phases should contribute to the isosurface. If the binary and operation 
		between this mask and the particle phase is non zero, then the particle will contribute to the isosurface
		\param[in] activeIndices Optional array with indices of all active particles
		\param[in] anisotropy1 Optional anisotropy information, x axis direction (xyz) and scale in w component
		\param[in] anisotropy2 Optional anisotropy information, y axis direction (xyz) and scale in w component
		\param[in] anisotropy3 Optional anisotropy information, z axis direction (xyz) and scale in w component
		\param[in] anisotropyFactor A factor to multiply with the anisotropy scale
		*/
		virtual void extractIsosurface(PxVec4* deviceParticlePos, const PxU32 numParticles, CUstream stream, PxU32* phases = NULL, PxU32 validPhaseMask = PxParticlePhaseFlag::eParticlePhaseFluid,
			PxU32* activeIndices = NULL, PxVec4* anisotropy1 = NULL, PxVec4* anisotropy2 = NULL, PxVec4* anisotropy3 = NULL, PxReal anisotropyFactor = 1.0f) = 0;

		/**
		\brief Allows to register the host buffers into which the final isosurface triangle mesh will get stored

		\param[in] vertices A host buffer to store the vertices of the isosurface mesh
		\param[in] triIndices A host buffer to store the triangles of the isosurface mesh
		\param[in] normals A host buffer to store the normals of the isosurface mesh
		*/
		virtual void setResultBufferHost(PxVec4* vertices, PxU32* triIndices, PxVec4* normals = NULL) = 0;

		/**
		\brief Allows to register the host buffers into which the final isosurface triangle mesh will get stored

		\param[in] vertices A device buffer to store the vertices of the isosurface mesh
		\param[in] triIndices A device buffer to store the triangles of the isosurface mesh
		\param[in] normals A device buffer to store the normals of the isosurface mesh
		*/
		virtual void setResultBufferDevice(PxVec4* vertices, PxU32* triIndices, PxVec4* normals) = 0;

		/**
		\brief Enables or disables the isosurface extractor

		\param[in] enabled The boolean to set the extractor to enabled or disabled
		*/
		virtual void setEnabled(bool enabled) = 0;

		/**
		\brief Allows to query if the isosurface extractor is enabled

		\return True if enabled, false otherwise
		*/
		virtual bool isEnabled() const = 0;

		/**
		\brief Destructor
		*/
		virtual ~PxIsosurfaceExtractor() {}
	};

	/**
	\brief Base class for sparse grid based isosurface extractors. Allows to register the data arrays for the isosurface and to obtain the number vertices/triangles in use.
	*/
	class PxSparseGridIsosurfaceExtractor : public PxIsosurfaceExtractor
	{
		/**
		\brief Returns the sparse grid parameters.

		\return The sparse grid settings used for the isosurface extraction
		*/
		virtual PxSparseGridParams getSparseGridParams() const = 0;

		/**
		\brief Set the sparse grid parameters

		Allows to configure cell size, number of subgrids etc.

		\param[in] params A collection of settings to control the isosurface grid
		*/
		virtual void setSparseGridParams(const PxSparseGridParams& params) = 0;
	};
		
	/**
	\brief Default implementation of a particle system callback to trigger the isosurface extraction. A call to fetchResultsParticleSystem() on the 
	PxScene will synchronize the work such that the caller knows that the post solve task completed.
	*/
	class PxIsosurfaceCallback : public PxParticleSystemCallback
	{
	public:
		/**
		\brief Initializes the isosurface callback

		\param[in] isosurfaceExtractor The isosurface extractor
		\param[in] validPhaseMask The valid phase mask marking the phase bits that particles must have set in order to contribute to the isosurface
		*/
		void initialize(PxIsosurfaceExtractor* isosurfaceExtractor, PxU32 validPhaseMask = PxParticlePhaseFlag::eParticlePhaseFluid) 
		{
			mIsosurfaceExtractor = isosurfaceExtractor;
			mValidPhaseMask = validPhaseMask;
		}

		virtual void onPostSolve(const PxGpuMirroredPointer<PxGpuParticleSystem>& gpuParticleSystem, CUstream stream)
		{
			mIsosurfaceExtractor->extractIsosurface(reinterpret_cast<PxVec4*>(gpuParticleSystem.mHostPtr->mUnsortedPositions_InvMass),
				gpuParticleSystem.mHostPtr->mCommonData.mMaxParticles, stream, gpuParticleSystem.mHostPtr->mUnsortedPhaseArray, mValidPhaseMask);
		}

		virtual void onBegin(const PxGpuMirroredPointer<PxGpuParticleSystem>& /*gpuParticleSystem*/, CUstream /*stream*/) { }

		virtual void onAdvance(const PxGpuMirroredPointer<PxGpuParticleSystem>& /*gpuParticleSystem*/, CUstream /*stream*/) { }
		
	private:
		PxIsosurfaceExtractor* mIsosurfaceExtractor;
		PxU32 mValidPhaseMask;
	};

#endif	

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

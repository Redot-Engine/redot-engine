// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0


#ifndef PX_DEFORMABLE_SKINNING_H
#define PX_DEFORMABLE_SKINNING_H

#include "foundation/PxPreprocessor.h"
#include "foundation/PxVec2.h"
#include "foundation/PxVec3.h"
#include "foundation/PxFlags.h"
#include "PxNodeIndex.h"
#include "cudamanager/PxCudaContextManager.h"
#include "common/PxCoreUtilityTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

    /**
    \brief Structure for triangle mesh skinning embedding information.
    */
    PX_ALIGN_PREFIX(16)
        struct PxTriangleMeshEmbeddingInfo
    {
        /**
        \brief UV coordinates for skinning.
        */
        PxVec2 uv;

        /**
        \brief Offset along the interpolated normal.
        */
        PxReal offsetAlongInterpolatedNormal;

        /**
        \brief ID of the guide triangle.
        */
        PxU32 guideTriangleId;

        /**
        \brief Constructor for PxTriangleMeshEmbeddingInfo.
        \param uv_ UV coordinates.
        \param offsetAlongInterpolatedNormal_ Offset along the interpolated normal.
        \param guideTriangleId_ ID of the guide triangle.
        */
        PxTriangleMeshEmbeddingInfo(
            const PxVec2& uv_,
            PxReal offsetAlongInterpolatedNormal_, PxU32 guideTriangleId_) :
            uv(uv_), offsetAlongInterpolatedNormal(offsetAlongInterpolatedNormal_), guideTriangleId(guideTriangleId_)
        {}
    } PX_ALIGN_SUFFIX(16);

    /**
    \brief Structure for tetrahedron mesh skinning embedding information.
    */
    PX_ALIGN_PREFIX(16)
        struct PxTetrahedronMeshEmbeddingInfo
    {
        /**
        \brief UVW coordinates for skinning.
        */
        PxVec3 uvw;

        /**
        \brief ID of the guide tetrahedron.
        */
        PxU32 guideTetrahedronId;

        /**
        \brief Constructor for PxTetrahedronMeshEmbeddingInfo.
        \param uvw_ UVW coordinates.
        \param guideTetrahedronId_ ID of the guide tetrahedron.
        */
        PxTetrahedronMeshEmbeddingInfo(const PxVec3& uvw_, PxU32 guideTetrahedronId_) :
            uvw(uvw_), guideTetrahedronId(guideTetrahedronId_)
        {}
    } PX_ALIGN_SUFFIX(16);

#if PX_SUPPORT_GPU_PHYSX
    /**
    \brief Structure for GPU data related to tetmesh skinning.
    */
    struct PxTetmeshSkinningGpuData
    {
        /**
        \brief Pointer to guide vertices data on the GPU.
        */
        PxTypedBoundedData<PxVec3> guideVerticesD;

        /**
        \brief Pointer to guide tetrahedra data on the GPU.
        */
        PxU32* guideTetrahedraD;

        /**
        \brief Pointer to skinning information per vertex on the GPU.
        */
        PxTetrahedronMeshEmbeddingInfo* skinningInfoPerVertexD;

        /**
        \brief Pointer to embedded vertices data on the GPU.
        */
        PxTypedBoundedData<PxVec3> skinnedVerticesD;
    };

    /**
    \brief Structure for GPU data related to trimesh skinning.
    */
    struct PxTrimeshSkinningGpuData
    {
        /**
        \brief Pointer to guide vertices data on the GPU.
        */
        PxTypedBoundedData<PxVec3> guideVerticesD;

        /**
        \brief Pointer to guide normals data on the GPU.
        */
        PxTypedBoundedData<PxVec3> guideNormalsD;

        /**
        \brief Pointer to guide triangles data on the GPU.
        */
        PxU32* guideTrianglesD;

        /**
        \brief Pointer to skinning information per vertex on the GPU.
        */
        PxTriangleMeshEmbeddingInfo* skinningInfoPerVertexD;

        /**
        \brief Pointer to skinned vertices data on the GPU.
        */
        PxTypedBoundedData<PxVec3> skinnedVerticesD;

        /**
        \brief Half of the deformable surface thickness.
        */
        PxReal halfSurfaceThickness;

        /**
        \brief Number of guide triangles.
        */
        PxU32 nbGuideTriangles;
    };

    /**
    \brief Abstract base class for deformable skinning operations.
    */
    class PxDeformableSkinning
    {
    public:
        /**
        \brief Computes normal vectors for cloth skinning data.
        \param skinningDataArrayD Array of cloth skinning data structures.
        \param arrayLength The number of elements in the skinning data array.
        \param stream CUDA stream to be used for computation.
        \param nbGpuThreads Number of GPU threads to use per cloth (default is 8192).
        */
        virtual void computeNormalVectors(
            PxTrimeshSkinningGpuData* skinningDataArrayD, PxU32 arrayLength,
            CUstream stream, PxU32 nbGpuThreads = 8192) = 0;

        /**
        \brief Evaluates interpolated deformable surface vertices.
        \param skinningDataArrayD Array of deformable surface skinning data structures.
        \param arrayLength The number of elements in the skinning data array.
        \param stream CUDA stream to be used for computation.
        \param nbGpuThreads Number of GPU threads to use per deformable surface (default is 8192).
        */
        virtual void evaluateVerticesEmbeddedIntoSurface(
            PxTrimeshSkinningGpuData* skinningDataArrayD, PxU32 arrayLength,
            CUstream stream, PxU32 nbGpuThreads = 8192) = 0;

        /**
        \brief Evaluates interpolated deformable volume vertices.
        \param skinningDataArrayD Array of deformable volume skinning data structures.
        \param arrayLength Length of the skinning data array.
        \param stream CUDA stream to be used for computation.
        \param nbGpuThreads Number of GPU threads to use per deformable volume (default is 8192).
        */
        virtual void evaluateVerticesEmbeddedIntoVolume(
            PxTetmeshSkinningGpuData* skinningDataArrayD, PxU32 arrayLength,
            CUstream stream, PxU32 nbGpuThreads = 8192) = 0;

        /**
        \brief Virtual destructor for PxDeformableSkinning.
        */
        virtual ~PxDeformableSkinning() { }
    };
#endif

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

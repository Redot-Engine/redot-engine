// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SDF_BUILDER_H
#define PX_SDF_BUILDER_H

#include "cudamanager/PxCudaContext.h"
#include "cudamanager/PxCudaContextManager.h"

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxVec4.h"
#include "foundation/PxArray.h"
#include "cooking/PxSDFDesc.h"
#include "cudamanager/PxCudaTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Utility class to compute an SDF on the GPU

\note The sign of the SDF (inside vs. outside) is derived from the triangle winding of the input mesh.
Triangles must be wound consistently such that their normals, (v1 - v0).cross(v2 - v0) for a triangle
(v0, v1, v2), point away from the enclosed volume, i.e. counter-clockwise vertex order when seen from
outside. The builder does not analyze or repair the winding; inverted winding produces an inverted SDF.
Cooking through #PxCreateTriangleMesh() or #PxCookTriangleMesh() with a #PxSDFDesc normalizes the winding
of watertight, single-component meshes automatically; clients calling the builder directly must provide
correctly wound triangles.
*/
class PxSDFBuilder
{
public:

	/**
	\brief Constructs a dense grid SDF for a triangle mesh using the GPU
	\param[in] vertices The vertices of the triangle mesh
	\param[in] numVertices The number of vertices
	\param[in] indices The triangle indices. Triangles must be wound outward (see class note)
	\param[in] numTriangleIndices The number of triangle indices
	\param[in] width The number of samples along the x direction of the resulting SDF volume
	\param[in] height The number of samples along the y direction of the resulting SDF volume
	\param[in] depth The number of samples along the z direction of the resulting SDF volume	
	\param[in] minExtents The minimum corner location of the axis aligned box containing the SDF samples
	\param[in] maxExtents The maximum corner location of the axis aligned box containing the SDF samples
	\param[in] cellCenteredSamples Determines if the sample points are located at the center of a SDF cell or at the lower left (=min) corner of a cell
	\param[out] sdf The distance values. Must provide space for width*height*depth distance samples. Negative distance means the sample point is located inside of the triangle mesh
	\param[in] stream The cuda stream on which the conversion is processed. If the default stream (0) is used, a temporary stream will be created internally

	\return A boolean that indicates whether the SDF creation succeeded
	*/
	virtual bool buildSDF(const PxVec3* vertices, PxU32 numVertices, const PxU32* indices, PxU32 numTriangleIndices, PxU32 width, PxU32 height, PxU32 depth,
		const PxVec3& minExtents, const PxVec3& maxExtents, bool cellCenteredSamples, PxReal* sdf, CUstream stream = 0) = 0;

	/**
	\brief Constructs a sparse grid SDF for a triangle mesh using the GPU
	\param[in] vertices The vertices of the triangle mesh
	\param[in] numVertices The number of vertices
	\param[in] indices The triangle indices. Triangles must be wound outward (see class note)
	\param[in] numTriangleIndices The number of triangle indices
	\param[in] width The number of samples along the x direction of the resulting SDF volume
	\param[in] height The number of samples along the y direction of the resulting SDF volume
	\param[in] depth The number of samples along the z direction of the resulting SDF volume
	\param[in] minExtents The minimum corner location of the axis aligned box containing the SDF samples
	\param[in] maxExtents The maximum corner location of the axis aligned box containing the SDF samples
	\param[in] narrowBandThickness The thickness of the narrow band
	\param[in] cellsPerSubgrid The number of cells in a sparse subgrid block (full block has mSubgridSize^3 cells and (mSubgridSize+1)^3 samples)
	\param[in] bitsPerSubgridPixel Subgrid pixel compression
	\param[out] sdfCoarse Used to store the lower resolution, dense backround SDF to provide distance information further away from the mesh's surface
	\param[out] sdfSubgridsStartSlots Used to store the subgrids start indices
	\param[out] sdfDataSubgrids Used to store the subgrids
	\param[out] subgridsMinSdfValue Used to store the minimum sdf value over all subgrids
	\param[out] subgridsMaxSdfValue Used to store the maximum sdf value over all subgrids
	\param[out] sdfSubgrids3DTexBlockDimX Used to store x dimension of the texture block that stores the subgrids
	\param[out] sdfSubgrids3DTexBlockDimY Used to store y dimension of the texture block that stores the subgrids
	\param[out] sdfSubgrids3DTexBlockDimZ Used to store z dimension of the texture block that stores the subgrids
	\param[in] stream The cuda stream on which the conversion is processed. If the default stream (0) is used, a temporary stream will be created internally

	\return A boolean that indicates whether the SDF creation succeeded
	*/
	virtual bool buildSparseSDF(const PxVec3* vertices, PxU32 numVertices, const PxU32* indices, PxU32 numTriangleIndices, PxU32 width, PxU32 height, PxU32 depth,
		const PxVec3& minExtents, const PxVec3& maxExtents, PxReal narrowBandThickness, PxU32 cellsPerSubgrid, PxSdfBitsPerSubgridPixel::Enum bitsPerSubgridPixel,
		PxArray<PxReal>& sdfCoarse, PxArray<PxU32>& sdfSubgridsStartSlots, PxArray<PxU8>& sdfDataSubgrids, 
		PxReal& subgridsMinSdfValue, PxReal& subgridsMaxSdfValue, 
		PxU32& sdfSubgrids3DTexBlockDimX, PxU32& sdfSubgrids3DTexBlockDimY, PxU32& sdfSubgrids3DTexBlockDimZ, CUstream stream = 0) = 0;
	
	/**
	\brief Releases the memory including the this pointer
	*/
	virtual void release() = 0;

	/**
	\brief Destructor
	*/
	virtual ~PxSDFBuilder()	{ }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

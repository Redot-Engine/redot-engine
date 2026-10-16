// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_SKINNING_EXT_H
#define PX_DEFORMABLE_SKINNING_EXT_H

#include "PxDeformableSkinning.h"
#include "foundation/PxVec3.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxCookingParams;
class PxSimpleTriangleMesh;
class PxInsertionCallback;

/**
\brief Utility functions for deformable surface and volume skinning
*/
class PxDeformableSkinningExt
{
public:

	/**
	\brief Initializes the interpolated vertices from guide vertices, normals, and triangles for deformable surface skinning

	This function calculates the skinning embedding information for a deformable surface mesh, 
	where each embedded vertex is interpolated based on the surrounding guide vertices and triangles.
	
	\param[out] embeddingInfo A pointer to a PxTriangleMeshEmbeddingInfo object that will store the calculated embedding information. Must be of length numEmbeddedVertices
	\param[in] guideVertices A pointer to an array of guide vertices used for interpolation
	\param[in] guideNormals A pointer to an array of guide normals corresponding to the guide vertices
	\param[in] guideTriangles A pointer to an array of indices defining the guide triangles
	\param[in] nbGuideTriangles The number of guide triangles
	\param[in] embeddedVertices A pointer to an array of vertices that will be embedded and interpolated
	\param[in] nbEmbeddedVertices The number of embedded vertices
	
	\see PxTriangleMeshEmbeddingInfo
	*/
	static void initializeInterpolatedVertices(
		PxTriangleMeshEmbeddingInfo* embeddingInfo,
		const PxVec3* guideVertices, const PxVec3* guideNormals,
		const PxU32* guideTriangles, PxU32 nbGuideTriangles, const PxVec3* embeddedVertices,
		PxU32 nbEmbeddedVertices);

	/**
	\brief Initializes the interpolated vertices from guide vertices and tetrahedra for deformable volume skinning

	This function calculates the skinning embedding information for a deformable volume mesh,
	where each embedded vertex is interpolated based on the surrounding guide vertices and tetrahedra.

	\param[out] embeddingInfo A pointer to a PxTetrahedronMeshEmbeddingInfo object that will store the calculated embedding information. Must be of length numEmbeddedVertices
	\param[in] guideVertices A pointer to an array of guide vertices used for interpolation
	\param[in] guideTetrahedra A pointer to an array of indices defining the guide tetrahedra
	\param[in] nbGuideTetrahedra The number of guide tetrahedra
	\param[in] embeddedVertices A pointer to an array of vertices that will be embedded and interpolated
	\param[in] nbEmbeddedVertices The number of embedded vertices
	
	\see PxTetrahedronMeshEmbeddingInfo
	*/
	static void initializeInterpolatedVertices(
		PxTetrahedronMeshEmbeddingInfo* embeddingInfo,
		const PxVec3* guideVertices, 
		const PxU32* guideTetrahedra, PxU32 nbGuideTetrahedra, const PxVec3* embeddedVertices,
		PxU32 nbEmbeddedVertices);
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_DEFORMABLE_SKINNING_EXT_H

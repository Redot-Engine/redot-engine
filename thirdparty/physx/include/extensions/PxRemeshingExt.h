// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_REMESHING_EXT_H
#define PX_REMESHING_EXT_H

#include "foundation/PxVec3.h"
#include "foundation/PxArray.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
	
	/**
	\brief Provides methods to adjust the tessellation of meshes
	*/
	class PxRemeshingExt
	{
	public:
		/**
		\brief Processes a triangle mesh and makes sure that no triangle edge is longer than the maximal edge length specified

		To shorten edges that are too long, additional points get inserted at their center leading to a subdivision of the input mesh.
		This process is executed repeatedly until the maximum edge length criterion is satisfied
				
		\param[in,out] triangles			The triangles of the mesh where a maximum edge length should be enforced. They will be modified in place during the process.
		\param[in,out] points				The vertices of the mesh where a maximum edge length should be enforced. They will be modified in place during the process.
		\param[in] maxEdgeLength			The maximum edge length allowed after processing the input	
		\param[in] maxIterations			The maximum number of subdivision iterations
		\param[out] triangleMap				An optional map that provides the index of the original triangle for every triangle after the subdivision
		\param[in] triangleCountThreshold	Optional limit to the number of triangles. Not guaranteed to match exactly, the algorithm will just stop as soon as possible after reaching the limit.

		\return True if any remeshing was applied
		*/
		static bool limitMaxEdgeLength(PxArray<PxU32>& triangles, PxArray<PxVec3>& points, PxReal maxEdgeLength, 
			PxU32 maxIterations = 100, PxArray<PxU32>* triangleMap = NULL, PxU32 triangleCountThreshold = 0xFFFFFFFF);

		/**
		\brief Processes a triangle mesh and makes sure that no triangle edge is longer than the maximal edge length specified

		To shorten edges that are too long, additional points get inserted at their center leading to a subdivision of the input mesh.
		This process is executed repeatedly until the maximum edge length criterion is satisfied

		\param[in,out] triangles			The triangles of the mesh where a maximum edge length should be enforced. They will be modified in place during the process.
		\param[in,out] points				The vertices of the mesh where a maximum edge length should be enforced. They will be modified in place during the process.
		\param[in] maxEdgeLength			The maximum edge length allowed after processing the input
		\param[in] maxIterations			The maximum number of subdivision iterations
		\param[out] triangleMap				An optional map that provides the index of the original triangle for every triangle after the subdivision
		\param[in] triangleCountThreshold	Optional limit to the number of triangles. Not guaranteed to match exactly, the algorithm will just stop as soon as possible after reaching the limit.

		\return True if any remeshing was applied
		*/
		static bool reduceSliverTriangles(PxArray<PxU32>& triangles, PxArray<PxVec3>& points, PxReal maxEdgeLength,
			PxU32 maxIterations = 3, PxArray<PxU32>* triangleMap = NULL, PxU32 triangleCountThreshold = 0xFFFFFFFF);
	};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

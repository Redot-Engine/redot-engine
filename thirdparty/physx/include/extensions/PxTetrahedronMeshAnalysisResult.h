// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TETRAHEDRON_MESH_ANALYSIS_RESULT_H
#define PX_TETRAHEDRON_MESH_ANALYSIS_RESULT_H


#include "PxPhysXConfig.h"
#include "foundation/PxFlags.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief These flags indicate what kind of deficiencies a tetrahedron mesh has and describe if the mesh is considered ok, problematic or invalid for deformable volume cooking
	*/
	class PxTetrahedronMeshAnalysisResult
	{
	public:
		enum Enum
		{
			eVALID = 0,
			eDEGENERATE_TETRAHEDRON = (1 << 0),					//!< At least one tetrahedron has zero or negative volume. This can happen when the input triangle mesh contains triangles that are very elongated, e. g. one edge is a lot shorther than the other two.

			eMESH_IS_PROBLEMATIC = (1 << 1),					//!< flag is set if the mesh is categorized as problematic
			eMESH_IS_INVALID = (1 << 2)							//!< flag is set if the mesh is categorized as invalid
		};
	};
	typedef PxFlags<PxTetrahedronMeshAnalysisResult::Enum, PxU32> PxTetrahedronMeshAnalysisResults;
	PX_FLAGS_OPERATORS(PxTetrahedronMeshAnalysisResult::Enum, PxU32)

#if !PX_DOXYGEN
}
#endif

#endif

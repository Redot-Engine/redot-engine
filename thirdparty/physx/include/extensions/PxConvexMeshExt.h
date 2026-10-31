// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONVEX_MESH_EXT_H
#define PX_CONVEX_MESH_EXT_H

#include "PxPhysXConfig.h"
#include "common/PxPhysXCommonConfig.h"
#include "foundation/PxTransform.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	class PxConvexMeshGeometry;

	/**
	\brief Computes closest polygon of the convex hull geometry for a given impact point
	and impact direction. When doing sweeps against a scene, one might want to delay
	the rather expensive computation of the hit face index for convexes until it is clear
	the information is really needed and then use this method to get the corresponding
	face index.
	
	\param[in] convexGeom The convex mesh geometry.
	\param[in] geomPose Pose for the geometry object.
	\param[in] impactPos Impact position.
	\param[in] unitDir Normalized impact direction.

	\return Closest face index of the convex geometry.

	\see PxTransform PxConvexMeshGeometry
	*/
	PxU32 PxFindFaceIndex(const PxConvexMeshGeometry& convexGeom, 
								   const PxTransform& geomPose,
								   const PxVec3& impactPos, 
								   const PxVec3& unitDir);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

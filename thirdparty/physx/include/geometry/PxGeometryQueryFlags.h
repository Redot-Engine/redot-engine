// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_GEOMETRY_QUERY_FLAGS_H
#define PX_GEOMETRY_QUERY_FLAGS_H

#include "foundation/PxFlags.h"
#include "common/PxPhysXCommonConfig.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief Geometry-level query flags.

	\see PxScene::raycast PxScene::overlap PxScene::sweep PxBVH::raycast PxBVH::overlap PxBVH::sweep PxGeometryQuery::raycast PxGeometryQuery::overlap PxGeometryQuery::sweep
	\see PxGeometryQuery::computePenetration PxGeometryQuery::pointDistance PxGeometryQuery::computeGeomBounds
	\see PxMeshQuery::findOverlapTriangleMesh PxMeshQuery::findOverlapHeightField PxMeshQuery::sweep
	*/
	struct PxGeometryQueryFlag
	{
		enum Enum
		{
			eSIMD_GUARD	= (1<<0),	//!< Saves/restores SIMD control word for each query (safer but slower). Omit this if you took care of it yourself in your app.

			eDEFAULT	= eSIMD_GUARD
		};
	};

	/**
	\brief collection of set bits defined in PxGeometryQueryFlag.

	\see PxGeometryQueryFlag
	*/
	PX_FLAGS_TYPEDEF(PxGeometryQueryFlag, PxU32)

#if !PX_DOXYGEN
}
#endif

#endif

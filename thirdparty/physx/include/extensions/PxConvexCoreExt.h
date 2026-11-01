// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONVEX_GEOMETRY_EXT_H
#define PX_CONVEX_GEOMETRY_EXT_H

#include "foundation/PxMat33.h"
#include "foundation/PxTransform.h"
#include "geometry/PxConvexCoreGeometry.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxBounds3;
class PxRenderOutput;

/**
\brief Convex geometry helpers
*/
class PxConvexCoreExt
{
public:

	/**
	\brief Compute mass properties of the convex core geometry.
	\param convex The convex geometry.
	\param[out] density1Mass The mass of the geometry assuming unit density.
	\param[out] inertiaTensor The inertia tensor of the geometry.
	\param[out] centerOfMass The center of mass of the geometry.
	*/
	static void computeMassInfo(const PxConvexCoreGeometry& convex, PxReal& density1Mass, PxMat33& inertiaTensor, PxVec3& centerOfMass);

	/**
	\brief Visualize the convex core geometry
	\param convex The convex geometry.
	\param pose The pose of the geometry in world space
	\param drawCore If true, draw the core inside the full convex geometry including the margin
	\param cullbox The culling box for visualization
	\param out The render output object to use for visualization
	*/
	static void visualize(const PxConvexCoreGeometry& convex, const PxTransform& pose, bool drawCore, const PxBounds3& cullbox, PxRenderOutput& out);

};

#if !PX_DOXYGEN
}
#endif

#endif

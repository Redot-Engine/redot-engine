// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BVH_DESC_H
#define PX_BVH_DESC_H

#include "common/PxCoreUtilityTypes.h"
#include "foundation/PxTransform.h"
#include "foundation/PxBounds3.h"
#include "geometry/PxBVHBuildStrategy.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Descriptor class for #PxBVH.

\see PxBVH
*/
class PxBVHDesc
{
public:
	PX_INLINE PxBVHDesc();

	/**
	\brief Pointer to first bounding box.
	*/
	PxBoundedData bounds;

	/**
	\brief Bounds enlargement

	Passed bounds are slightly enlarged before creating the BVH. This is done to avoid numerical issues when
	e.g. raycasts just graze the bounds. The performed operation is:

	extents = (bounds.maximum - bounds.minimum)/2
	enlagedBounds.minimum = passedBounds.minium - extents * enlargement
	enlagedBounds.maximum = passedBounds.maxium + extents * enlargement

	Users can pass pre-enlarged bounds to the BVH builder, in which case just set the enlargement value to zero.

	<b>Default value:</b> 0.01
	*/
	float	enlargement;

	/**
	\brief Max primitives per leaf limit.

	<b>Range:</b> [0, 16)<br>
	<b>Default value:</b> 4
	*/
	PxU32	numPrimsPerLeaf;

	/**
	\brief Desired build strategy for the BVH

	<b>Default value:</b> eDEFAULT
	*/
	PxBVHBuildStrategy::Enum	buildStrategy;

	/**
	\brief	Initialize the BVH descriptor
	*/
	PX_INLINE void setToDefault();

	/**
	\brief Returns true if the descriptor is valid.
	\return true if the current settings are valid.
	*/
	PX_INLINE bool isValid() const;

protected:	
};

PX_INLINE PxBVHDesc::PxBVHDesc() : enlargement(0.01f), numPrimsPerLeaf(4), buildStrategy(PxBVHBuildStrategy::eDEFAULT)
{
}

PX_INLINE void PxBVHDesc::setToDefault()
{
	*this = PxBVHDesc();
}

PX_INLINE bool PxBVHDesc::isValid() const
{
	// Check BVH desc data
	if(!bounds.data)
		return false;
	if(bounds.stride < sizeof(PxBounds3))	//should be at least one bounds' worth of data
		return false;

	if(bounds.count == 0)
		return false;

	if(enlargement<0.0f)
		return false;

	if(numPrimsPerLeaf>=16)
		return false;

	return true;
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

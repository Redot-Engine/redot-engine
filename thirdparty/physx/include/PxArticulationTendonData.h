// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0


#ifndef PX_ARTICULATION_TENDON_DATA_H
#define PX_ARTICULATION_TENDON_DATA_H

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxVec3.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief PxGpuSpatialTendonData

This data structure is to be used by the direct GPU API for spatial tendon data updates.

\see PxArticulationSpatialTendon PxDirectGPUAPI::getArticulationData PxDirectGPUAPI::setArticulationData
*/
PX_ALIGN_PREFIX(16)
class PxGpuSpatialTendonData
{
public:
	PxReal		stiffness;
	PxReal		damping;
	PxReal		limitStiffness;
	PxReal		offset;
}
PX_ALIGN_SUFFIX(16);

/**
\brief PxGpuFixedTendonData

This data structure is to be used by the direct GPU API for fixed tendon data updates.

\see PxArticulationFixedTendon PxDirectGPUAPI::getArticulationData PxDirectGPUAPI::setArticulationData
*/
PX_ALIGN_PREFIX(16)
class PxGpuFixedTendonData : public PxGpuSpatialTendonData
{
public:
	PxReal		lowLimit;
	PxReal		highLimit;
	PxReal		restLength;
	PxReal		padding;
}
PX_ALIGN_SUFFIX(16);

/**
\brief PxGpuTendonJointCoefficientData

This data structure is to be used by the direct GPU API for fixed tendon joint data updates.

\see PxArticulationTendonJoint PxDirectGPUAPI::getArticulationData PxDirectGPUAPI::setArticulationData
*/
PX_ALIGN_PREFIX(16)
class PxGpuTendonJointCoefficientData
{
public:
	PxReal		coefficient;
	PxReal		recipCoefficient;
	PxU32			axis;
	PxU32			pad;
}
PX_ALIGN_SUFFIX(16);

/**
\brief PxGpuTendonAttachmentData

This data structure is to be used by the direct GPU API for spatial tendon attachment data updates.

\see PxArticulationAttachment PxDirectGPUAPI::getArticulationData PxDirectGPUAPI::setArticulationData
*/
PX_ALIGN_PREFIX(16)
class PxGpuTendonAttachmentData
{
public:
	PxVec3		relativeOffset;
	PxReal		restLength;

	PxReal		coefficient;
	PxReal		lowLimit;
	PxReal		highLimit;
	PxReal		padding;											
}
PX_ALIGN_SUFFIX(16);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

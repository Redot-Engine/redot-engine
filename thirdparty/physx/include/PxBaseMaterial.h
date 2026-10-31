// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BASE_MATERIAL_H
#define PX_BASE_MATERIAL_H

#include "PxPhysXConfig.h"
#include "common/PxBase.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief Base material class.

	\see PxPhysics.createMaterial PxPhysics.createDeformableSurfaceMaterial PxPhysics.createDeformableVolumeMaterial PxPhysics.createPBDMaterial
	*/
	class PxBaseMaterial : public PxRefCounted
	{
	public:
		PX_INLINE			PxBaseMaterial(PxType concreteType, PxBaseFlags baseFlags) : PxRefCounted(concreteType, baseFlags), userData(NULL) {}
		PX_INLINE			PxBaseMaterial(PxBaseFlags baseFlags) : PxRefCounted(baseFlags) {}
		virtual				~PxBaseMaterial() {}
		virtual		bool	isKindOf(const char* name) const { PX_IS_KIND_OF(name, "PxBaseMaterial", PxRefCounted); }

					void*	userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.
	};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

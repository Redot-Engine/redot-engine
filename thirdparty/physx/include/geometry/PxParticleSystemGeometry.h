// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PARTICLESYSTEM_GEOMETRY_H
#define PX_PARTICLESYSTEM_GEOMETRY_H
#include "geometry/PxGeometry.h"
#include "common/PxCoreUtilityTypes.h"
#include "foundation/PxBounds3.h"
#include "foundation/PxVec4.h"
#include "PxParticleSystem.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief Particle system geometry class.

	*/
	class PxParticleSystemGeometry : public PxGeometry
	{
	public:
		/**
		\brief Default constructor.

		Creates an empty object with no particles.
		*/
		PX_INLINE PxParticleSystemGeometry() : PxGeometry(PxGeometryType::ePARTICLESYSTEM){}

		/**
		\brief Copy constructor.

		\param[in] that		Other object
		*/
		PX_INLINE PxParticleSystemGeometry(const PxParticleSystemGeometry& that) : PxGeometry(that) {}

		/**
		\brief Assignment operator
		*/
		PX_INLINE void operator=(const PxParticleSystemGeometry& that)
		{
			mType = that.mType;
		}

		/**
		\brief Returns true if the geometry is valid.

		\return  True if the current settings are valid for shape creation.

		\see PxPhysics::createShape
		*/
		PX_FORCE_INLINE bool isValid() const
		{
			if(mType != PxGeometryType::ePARTICLESYSTEM)
				return false;

			return true;
		}

	};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

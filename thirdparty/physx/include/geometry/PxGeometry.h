// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_GEOMETRY_H
#define PX_GEOMETRY_H

#include "common/PxPhysXCommonConfig.h"
#include "foundation/PxFlags.h"
#include "foundation/PxMath.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief A geometry type.

Used to distinguish the type of a ::PxGeometry object.
*/
struct PxGeometryType
{
	enum Enum
	{
		eSPHERE,
		ePLANE,
		eCAPSULE,
		eBOX,
		eCONVEXCORE,
		eCONVEXMESH,
		ePARTICLESYSTEM,
		eTETRAHEDRONMESH,
		eTRIANGLEMESH,
		eHEIGHTFIELD,
		eCUSTOM,
		
		eGEOMETRY_COUNT,	//!< internal use only!
		eINVALID = -1		//!< internal use only!
	};
};

/**
\brief A geometry object.

A geometry object defines the characteristics of a spatial object, but without any information
about its placement in the world.

\note This is an abstract class.  You cannot create instances directly.  Create an instance of one of the derived classes instead.
*/
class PxGeometry
{
public:
	/**
	\brief Returns the type of the geometry.
	\return The type of the object.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxGeometryType::Enum getType() const	{ return mType; }

	/**
	\brief Assignment operator
	*/
	PX_INLINE void operator=(const PxGeometry& that)
	{
		mType = that.mType;
	}

protected:
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxGeometry(PxGeometryType::Enum type) : mType(type)	{}
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxGeometry(const PxGeometry& that) : mType(that.mType)	{}

	PxGeometryType::Enum mType;

public:
	float	mTypePadding;	// PT: padding bytes on x64, used internally
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

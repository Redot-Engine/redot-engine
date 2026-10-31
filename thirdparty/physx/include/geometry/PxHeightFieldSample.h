// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_HEIGHT_FIELD_SAMPLE_H
#define PX_HEIGHT_FIELD_SAMPLE_H

#include "common/PxPhysXCommonConfig.h"
#include "foundation/PxBitAndData.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Special material index values for height field samples.

\see PxHeightFieldSample.materialIndex0 PxHeightFieldSample.materialIndex1
*/
struct PxHeightFieldMaterial
{
	enum Enum
	{
		eHOLE = 127  //!< A material indicating that the triangle should be treated as a hole in the mesh.
	};
};

/**
\brief Heightfield sample format.

This format corresponds to the #PxHeightFieldFormat member PxHeightFieldFormat::eS16_TM.

An array of heightfield samples are used when creating a PxHeightField to specify
the elevation of the heightfield points. In addition the material and tessellation of the adjacent 
triangles are specified.

\see PxHeightField PxHeightFieldDesc PxHeightFieldDesc.samples
*/
struct PxHeightFieldSample
{
	/**
	\brief The height of the heightfield sample

	This value is scaled by PxHeightFieldGeometry::heightScale.

	\see PxHeightFieldGeometry
	*/
	PxI16			height;

	/**
	\brief The triangle material index of the quad's lower triangle + tesselation flag

	An index pointing into the material table of the shape which instantiates the heightfield.
	This index determines the material of the lower of the quad's two triangles (i.e. the quad whose 
	upper-left corner is this sample, see the Guide for illustrations).

	Special values of the 7 data bits are defined by PxHeightFieldMaterial

	The tesselation flag specifies which way the quad is split whose upper left corner is this sample.
	If the flag is set, the diagonal of the quad will run from this sample to the opposite vertex; if not,
	it will run between the other two vertices (see the Guide for illustrations).

	\see PxHeightFieldGeometry materialIndex1 PxShape.setmaterials() PxShape.getMaterials()
	*/
	PxBitAndByte	materialIndex0;

	PX_CUDA_CALLABLE PX_FORCE_INLINE	PxU8	tessFlag()	const	{ return PxU8(materialIndex0.isBitSet() ? 1 : 0);		}	// PT: explicit conversion to make sure we don't break the code
	PX_CUDA_CALLABLE PX_FORCE_INLINE	void	setTessFlag()		{ materialIndex0.setBit();						}
	PX_CUDA_CALLABLE PX_FORCE_INLINE	void	clearTessFlag()		{ materialIndex0.clearBit();					}

	/**
	\brief The triangle material index of the quad's upper triangle + reserved flag

	An index pointing into the material table of the shape which instantiates the heightfield.
	This index determines the material of the upper of the quad's two triangles (i.e. the quad whose 
	upper-left corner is this sample, see the Guide for illustrations).

	\see PxHeightFieldGeometry materialIndex0 PxShape.setmaterials() PxShape.getMaterials()
	*/
	PxBitAndByte	materialIndex1;
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

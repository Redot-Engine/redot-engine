// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_HEIGHT_FIELD_GEOMETRY_H
#define PX_HEIGHT_FIELD_GEOMETRY_H
#include "geometry/PxTriangleMeshGeometry.h"
#include "common/PxCoreUtilityTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

#define PX_MIN_HEIGHTFIELD_XZ_SCALE 1e-8f
#define PX_MIN_HEIGHTFIELD_Y_SCALE (0.0001f / PxReal(0xFFFF))

class PxHeightField;

/**
\brief Height field geometry class.

This class allows to create a scaled height field geometry instance.

There is a minimum allowed value for Y and XZ scaling - PX_MIN_HEIGHTFIELD_XZ_SCALE, heightfield creation will fail if XZ value is below this value.
*/
class PxHeightFieldGeometry : public PxGeometry 
{
public:
	/**
	\brief Constructor.
	*/
	PX_INLINE PxHeightFieldGeometry(PxHeightField* hf = NULL,
									PxMeshGeometryFlags flags = PxMeshGeometryFlag::Enum(0),
									PxReal heightScale_ = 1.0f,
									PxReal rowScale_ = 1.0f,
									PxReal columnScale_ = 1.0f) :
		PxGeometry			(PxGeometryType::eHEIGHTFIELD), 
		heightField			(hf),
		heightScale			(heightScale_),
		rowScale			(rowScale_),
		columnScale			(columnScale_),
		heightFieldFlags	(flags)
		{
		}

	/**
	\brief Copy constructor.

	\param[in] that		Other object
	*/
	PX_INLINE PxHeightFieldGeometry(const PxHeightFieldGeometry& that) :
		PxGeometry			(that), 
		heightField			(that.heightField),
		heightScale			(that.heightScale),
		rowScale			(that.rowScale),
		columnScale			(that.columnScale),
		heightFieldFlags	(that.heightFieldFlags)
		{
		}

	/**
	\brief Assignment operator
	*/
	PX_INLINE void operator=(const PxHeightFieldGeometry& that)
	{
		mType = that.mType;
		heightField = that.heightField;
		heightScale = that.heightScale;
		rowScale = that.rowScale;
		columnScale = that.columnScale;
		heightFieldFlags = that.heightFieldFlags;
	}

	/**
	\brief Returns true if the geometry is valid.

	\return True if the current settings are valid

	\note A valid height field has a positive scale value in each direction (heightScale > 0, rowScale > 0, columnScale > 0).
	It is illegal to call PxPhysics::createShape with a height field that has zero extents in any direction.

	\see PxPhysics::createShape
	*/
	PX_INLINE bool isValid() const;

public:
	/**
	\brief The height field data.
	*/
	PxHeightField*		heightField;

	/**
	\brief The scaling factor for the height field in vertical direction (y direction in local space).
	*/
	PxReal				heightScale;

	/**
	\brief The scaling factor for the height field in the row direction (x direction in local space).
	*/
	PxReal				rowScale;

	/**
	\brief The scaling factor for the height field in the column direction (z direction in local space).
	*/
	PxReal				columnScale;

	/**
	\brief Flags to specify some collision properties for the height field.
	*/
	PxMeshGeometryFlags	heightFieldFlags;

	PxPadding<3>		paddingFromFlags;	//!< padding for mesh flags.
};


PX_INLINE bool PxHeightFieldGeometry::isValid() const
{
	if(mType != PxGeometryType::eHEIGHTFIELD)
		return false;
	if(!PxIsFinite(heightScale) || !PxIsFinite(rowScale) || !PxIsFinite(columnScale))
		return false;
	if(rowScale < PX_MIN_HEIGHTFIELD_XZ_SCALE || columnScale < PX_MIN_HEIGHTFIELD_XZ_SCALE || heightScale < PX_MIN_HEIGHTFIELD_Y_SCALE)
		return false;
	if(!heightField)
		return false;

	return true;
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

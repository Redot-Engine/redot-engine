// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONVEX_MESH_GEOMETRY_H
#define PX_CONVEX_MESH_GEOMETRY_H
#include "geometry/PxGeometry.h"
#include "geometry/PxMeshScale.h"
#include "common/PxCoreUtilityTypes.h"
#include "geometry/PxConvexMesh.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxConvexMesh;

/**
\brief Flags controlling the simulated behavior of the convex mesh geometry.

Used in ::PxConvexMeshGeometryFlags.
*/
struct PxConvexMeshGeometryFlag
{
	enum Enum
	{
		eTIGHT_BOUNDS = (1<<0)	//!< Use tighter (but more expensive to compute) bounds around the convex geometry.
	};
};

/**
\brief collection of set bits defined in PxConvexMeshGeometryFlag.

\see PxConvexMeshGeometryFlag
*/
typedef PxFlags<PxConvexMeshGeometryFlag::Enum,PxU8> PxConvexMeshGeometryFlags;
PX_FLAGS_OPERATORS(PxConvexMeshGeometryFlag::Enum,PxU8)

/**
\brief Convex mesh geometry class.

This class unifies a convex mesh object with a scaling transform, and 
lets the combined object be used anywhere a PxGeometry is needed.

The scaling is a transform along arbitrary axes contained in the scale object.
The vertices of the mesh in geometry (or shape) space is the 
PxMeshScale::toMat33() transform, multiplied by the vertex space vertices 
in the PxConvexMesh object.
*/
class PxConvexMeshGeometry : public PxGeometry 
{
public:
	/**
	\brief Constructor. By default creates an empty object with a NULL mesh and identity scale.

	\param[in] mesh		Mesh pointer. May be NULL, though this will not make the object valid for shape construction.
	\param[in] scaling	Scale factor.
	\param[in] flags	Mesh flags.
	\
	*/
	PX_INLINE PxConvexMeshGeometry(	PxConvexMesh* mesh = NULL,
									const PxMeshScale& scaling = PxMeshScale(),
									PxConvexMeshGeometryFlags flags = PxConvexMeshGeometryFlag::eTIGHT_BOUNDS) :
		PxGeometry	(PxGeometryType::eCONVEXMESH),
		scale		(scaling),
		convexMesh	(mesh),
		meshFlags	(flags)
	{
	}

	/**
	\brief Copy constructor.

	\param[in] that		Other object
	*/
	PX_INLINE PxConvexMeshGeometry(const PxConvexMeshGeometry& that) :
		PxGeometry	(that),
		scale		(that.scale),
		convexMesh	(that.convexMesh),
		meshFlags	(that.meshFlags)
	{
	}

	/**
	\brief Assignment operator
	*/
	PX_INLINE void operator=(const PxConvexMeshGeometry& that)
	{
		mType = that.mType;
		scale = that.scale;
		convexMesh = that.convexMesh;
		meshFlags = that.meshFlags;
	}

	/**
	\brief Returns true if the geometry is valid.

	\return True if the current settings are valid for shape creation.

	\note A valid convex mesh has a positive scale value in each direction (scale.x > 0, scale.y > 0, scale.z > 0).
	It is illegal to call PxPhysics::createShape with a convex that has zero extent in any direction.

	\see PxPhysics::createShape
	*/
	PX_INLINE bool isValid() const;

public:
	PxMeshScale					scale;				//!< The scaling transformation (from vertex space to shape space).
	PxConvexMesh*				convexMesh;			//!< A reference to the convex mesh object.
	PxConvexMeshGeometryFlags	meshFlags;			//!< Mesh flags.
	PxPadding<3>				paddingFromFlags;	//!< padding for mesh flags
};


PX_INLINE bool PxConvexMeshGeometry::isValid() const
{
	if(mType != PxGeometryType::eCONVEXMESH)
		return false;
	if(!scale.scale.isFinite() || !scale.rotation.isUnit())
		return false;
	if(!scale.isValidForConvexMesh())
		return false;
	if(!convexMesh)
		return false;
	
	return true;
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

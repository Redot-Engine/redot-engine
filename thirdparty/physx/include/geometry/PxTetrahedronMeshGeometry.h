// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TETRAHEDRON_GEOMETRY_H
#define PX_TETRAHEDRON_GEOMETRY_H
#include "geometry/PxGeometry.h"
#include "geometry/PxMeshScale.h"
#include "common/PxCoreUtilityTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
	class PxTetrahedronMesh;

	/**
	\brief Tetrahedron mesh geometry class.

	This class wraps a tetrahedron mesh such that it can be used in contexts where a PxGeometry type is needed.
	*/
	class PxTetrahedronMeshGeometry : public PxGeometry
	{
	public:
		/**
		\brief Constructor. By default creates an empty object with a NULL mesh and identity scale.
		*/
		PX_INLINE PxTetrahedronMeshGeometry(PxTetrahedronMesh* mesh = NULL) :
			PxGeometry(PxGeometryType::eTETRAHEDRONMESH),
			tetrahedronMesh(mesh)
		{}

		/**
		\brief Copy constructor.

		\param[in] that		Other object
		*/
		PX_INLINE PxTetrahedronMeshGeometry(const PxTetrahedronMeshGeometry& that) :
			PxGeometry(that),
			tetrahedronMesh(that.tetrahedronMesh)
		{}

		/**
		\brief Assignment operator
		*/
		PX_INLINE void operator=(const PxTetrahedronMeshGeometry& that)
		{
			mType = that.mType;
			tetrahedronMesh = that.tetrahedronMesh;
		}

		/**
		\brief Returns true if the geometry is valid.

		\return  True if the current settings are valid for shape creation.

		\note A valid tetrahedron mesh has a positive scale value in each direction (scale.scale.x > 0, scale.scale.y > 0, scale.scale.z > 0).
		It is illegal to call PxPhysics::createShape with a tetrahedron mesh that has zero extents in any direction.

		\see PxPhysics::createShape
		*/
		PX_INLINE bool isValid() const;

	public:
		PxTetrahedronMesh*		tetrahedronMesh;		//!< A reference to the mesh object.
	};

	PX_INLINE bool PxTetrahedronMeshGeometry::isValid() const
	{
		if(mType != PxGeometryType::eTETRAHEDRONMESH)
			return false;

		if(!tetrahedronMesh)
			return false;

		return true;
	}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

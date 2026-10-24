// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TETRAHEDRON_H
#define PX_TETRAHEDRON_H

#include "common/PxPhysXCommonConfig.h"
#include "foundation/PxVec3.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief Tetrahedron class.
	*/
	class PxTetrahedron
	{
	public:
		/**
		\brief Constructor
		*/
		PX_FORCE_INLINE			PxTetrahedron() {}

		/**
		\brief Constructor

		\param[in] p0 Point 0
		\param[in] p1 Point 1
		\param[in] p2 Point 2
		\param[in] p3 Point 3
		*/
		PX_FORCE_INLINE			PxTetrahedron(const PxVec3& p0, const PxVec3& p1, const PxVec3& p2, const PxVec3& p3)
		{
			verts[0] = p0;
			verts[1] = p1;
			verts[2] = p2;
			verts[3] = p3;
		}

		/**
		\brief Copy constructor

		\param[in] tetrahedron copy
		*/
		PX_FORCE_INLINE			PxTetrahedron(const PxTetrahedron& tetrahedron)
		{
			verts[0] = tetrahedron.verts[0];
			verts[1] = tetrahedron.verts[1];
			verts[2] = tetrahedron.verts[2];
			verts[3] = tetrahedron.verts[3];
		}

		/**
		\brief Destructor
		*/
		PX_FORCE_INLINE			~PxTetrahedron() {}

		/**
		\brief Assignment operator
		*/
		PX_FORCE_INLINE void operator=(const PxTetrahedron& tetrahedron)
		{
			verts[0] = tetrahedron.verts[0];
			verts[1] = tetrahedron.verts[1];
			verts[2] = tetrahedron.verts[2];
			verts[3] = tetrahedron.verts[3];
		}
		/**
		\brief Array of Vertices.
		*/
		PxVec3		verts[4];

	};


#if !PX_DOXYGEN
}
#endif

#endif

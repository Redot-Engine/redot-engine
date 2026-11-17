// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TETRAHEDRON_MESH_DESC_H
#define PX_TETRAHEDRON_MESH_DESC_H

#include "PxPhysXConfig.h"
#include "foundation/PxVec3.h"
#include "foundation/PxFlags.h"
#include "common/PxCoreUtilityTypes.h"
#include "geometry/PxSimpleTriangleMesh.h"
#include "foundation/PxArray.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
	/**
	\brief Descriptor class for #PxTetrahedronMesh (contains only pure geometric data).

	\see PxTetrahedronMesh PxShape
	*/
	class PxTetrahedronMeshDesc
	{
	public:

		/**
		\brief Defines the tetrahedron structure of a mesh. 
		*/
		enum PxMeshFormat
		{
			eTET_MESH,	//!< Normal tetmesh with arbitrary tetrahedra
			eHEX_MESH 	//!< 5 or 6 tetrahedra in a row will form a hexahedron
		};


		/**
		Optional pointer to first material index, or NULL. There are PxTetrahedronMesh::numTriangles indices in total.
		Caller may add materialIndexStride bytes to the pointer to access the next triangle.

		When a tetrahedron mesh collides with another object, a material is required at the collision point.
		If materialIndices is NULL, then the material of the PxShape instance is used.
		Otherwise, if the point of contact is on a tetrahedron with index i, then the material index is determined as:
		PxDeformableMaterialTableIndex	index = *(PxDeformableMaterialTableIndex *)(((PxU8*)materialIndices) + materialIndexStride * i);

		If the contact point falls on a vertex or an edge, a tetrahedron adjacent to the vertex or edge is selected, and its index
		used to look up a material. The selection is arbitrary but consistent over time.

		<b>Default:</b> NULL

		\see materialIndexStride
		*/
		PxTypedBoundedData<PxDeformableMaterialTableIndex> materialIndices;

		/**
		\brief Pointer to first vertex point.
		*/
		PxBoundedData points;

		/**
		\brief Pointer to first tetrahedron.

		Caller may add tetrhedronStrideBytes bytes to the pointer to access the next tetrahedron.

		These are quadruplets of 0 based indices:
		vert0 vert1 vert2 vert3
		vert0 vert1 vert2 vert3
		vert0 vert1 vert2 vert3
		...

		where vertex is either a 32 or 16 bit unsigned integer. There are numTetrahedrons*4 indices.

		This is declared as a void pointer because it is actually either an PxU16 or a PxU32 pointer.
		*/
		PxBoundedData tetrahedrons;


		/**
		\brief Flags bits, combined from values of the enum ::PxMeshFlag
		*/
		PxMeshFlags flags;

		/**
		\brief Used for simulation meshes only. Defines if this tet mesh should be simulated as a tet mesh,
		or if a set of tetrahedra should be used to represent another shape, e.g. a hexahedral mesh constructed 
		from 5 or 6 elements.
		*/
		PxU16 tetsPerElement;

		/**
		\brief Constructor to build an empty tetmesh description
		*/
		PxTetrahedronMeshDesc()
		{
			points.count = 0;
			points.stride = 0;
			points.data = NULL;

			tetrahedrons.count = 0;
			tetrahedrons.stride = 0;
			tetrahedrons.data = NULL;

			tetsPerElement = 1;
		}

		/**
		\brief Constructor to build a tetmeshdescription that links to the vertices and indices provided
		*/
		PxTetrahedronMeshDesc(physx::PxArray<physx::PxVec3>& meshVertices, physx::PxArray<physx::PxU32>& meshTetIndices, 
			const PxTetrahedronMeshDesc::PxMeshFormat meshFormat = eTET_MESH, PxU16 numberOfTetsPerHexElement = 5)
		{
			points.count = meshVertices.size();
			points.stride = sizeof(float) * 3;
			points.data = meshVertices.begin();

			tetrahedrons.count = meshTetIndices.size() / 4;
			tetrahedrons.stride = sizeof(int) * 4;
			tetrahedrons.data = meshTetIndices.begin();

			if (meshFormat == eTET_MESH)
				tetsPerElement = 1;
			else
				tetsPerElement = numberOfTetsPerHexElement;
		}

		PX_INLINE bool isValid() const
		{
			// Check geometry of the collision mesh
			if (points.count < 4) 	//at least 1 tetrahedron's worth of points
				return false;
			if ((!tetrahedrons.data) && (points.count % 4))		// Non-indexed mesh => we must ensure the geometry defines an implicit number of tetrahedrons // i.e. numVertices can't be divided by 4
				return false;
			if (points.count > 0xffff && flags & PxMeshFlag::e16_BIT_INDICES)
				return false;
			if (!points.data)
				return false;
			if (points.stride < sizeof(PxVec3))	//should be at least one point's worth of data
				return false;		

			//add more validity checks here
			if (materialIndices.data && materialIndices.stride < sizeof(PxDeformableMaterialTableIndex))
				return false;

			// The tetrahedrons pointer is not mandatory
			if (tetrahedrons.data)
			{
				// Indexed collision mesh
				PxU32 limit = (flags & PxMeshFlag::e16_BIT_INDICES) ? sizeof(PxU16) * 4 : sizeof(PxU32) * 4;
				if (tetrahedrons.stride < limit)
					return false;
			}

			//The model can only be either a tetmesh (1 tet per element), or have 5 or 6 tets per hex element, otherwise invalid.
			if (tetsPerElement != 1 && tetsPerElement != 5 && tetsPerElement != 6)
				return false;

			return true;
		}
	};

	/**
	\brief Descriptor class for #PxDeformableVolumeMesh (contains only additional data used for deformable volume simulation).

	\see PxDeformableVolumeMesh PxShape
	*/
	class PxDeformableVolumeSimulationDataDesc
	{
	public:
		/**
		\brief Pointer to first index of tetrahedron that contains the vertex at the same location in the vertex buffer.		
		if left unassigned it will be computed automatically. If a point is inside multiple tetrahedra (ambiguous case), the frist one found will be taken.
		*/
		PxBoundedData vertexToTet;

		/**
		\brief Constructor to build an empty simulation description
		*/
		PxDeformableVolumeSimulationDataDesc()
		{
			vertexToTet.count = 0;
			vertexToTet.stride = 0;
			vertexToTet.data = NULL;
		}

		/**
		\brief Constructor to build a simulation description with a defined vertex to tetrahedron mapping
		*/
		PxDeformableVolumeSimulationDataDesc(physx::PxArray<physx::PxI32>& vertToTet)
		{
			vertexToTet.count = vertToTet.size();
			vertexToTet.stride = sizeof(PxI32);
			vertexToTet.data = vertToTet.begin();
		}

		PX_INLINE bool isValid() const
		{
			return true;
		}
	};

	
#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

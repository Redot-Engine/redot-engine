// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TETRAHEDRON_MESH_EXT_H
#define PX_TETRAHEDRON_MESH_EXT_H

#include "foundation/PxVec3.h"
#include "foundation/PxVec4.h"
#include "foundation/PxArray.h"


#if !PX_DOXYGEN
namespace physx
{
#endif
	class PxTetrahedronMesh;

	/**
	\brief utility functions for use with PxTetrahedronMesh and subclasses
	*/
	class PxTetrahedronMeshExt
	{
	public:
		/** Returns the index of the tetrahedron that contains a point

		\param[in] mesh The tetmesh
		\param[in] point The point to find the enclosing tetrahedron for
		\param[in] bary The barycentric coordinates of the point inside the enclosing tetrahedron
		\param[in] tolerance Tolerance value used classify points as inside if they lie exactly a tetrahedron's surface
		\return The index of the tetrahedron containing the point, -1 if not tetrahedron contains the opoint
		*/
		static PxI32 findTetrahedronContainingPoint(const PxTetrahedronMesh* mesh, const PxVec3& point, PxVec4& bary, PxReal tolerance = 1e-6f);

		/** Returns the index of the tetrahedron closest to a point

		\param[in] mesh The tetmesh
		\param[in] point The point to find the closest tetrahedron for
		\param[out] bary The barycentric coordinates of the point in the tetrahedron
		\return The index of the tetrahedron closest to the point
		*/
		static PxI32 findTetrahedronClosestToPoint(const PxTetrahedronMesh* mesh, const PxVec3& point, PxVec4& bary);

		/** Associates points with closest tetrahedra from input tetrahedral mesh. If the tetmesh does not have any tetrahedra
		or points, a warning will be generated and the result arrays will be empty, even if there are query points passed into the method.

		\param[in] tetMeshVertices The tetrahedral mesh vertices
		\param[in] tetMeshIndices The tetraheral mesh indices
		\param[in] pointsToEmbed  The points for which the embedding should be created
        \param[in] barycentricCoordinates  The output barycentric coordinates for each input point relative to its closest tetrahedron
        \param[in] tetLinks The output indices of the closest tetrahedron for each input point
		*/
		static void createPointsToTetrahedronMap(const PxArray<PxVec3>& tetMeshVertices, const PxArray<PxU32>& tetMeshIndices, const PxArray<PxVec3>& pointsToEmbed, PxArray<PxVec4>& barycentricCoordinates, PxArray<PxU32>& tetLinks);

		/** Extracts the surface triangles of a tetmesh

		The extracted triangle's vertex indices point to the vertex buffer of the tetmesh.

		\param[in] tetrahedra The tetrahedra indices
		\param[in] numTetrahedra The number of tetrahedra
		\param[in] sixteenBitIndices If set to true, the tetrahedra indices are read as 16bit integers, otherwise 32bit integers are used
		\param[in] surfaceTriangles The resulting surface triangles
		\param[in] surfaceTriangleToTet Optional array to get the index of a tetrahedron that is adjacent to the surface triangle with the corresponding index
		\param[in] flipTriangleOrientation Reverses the orientation of the ouput triangles
		*/
		static void extractTetMeshSurface(const void* tetrahedra, PxU32 numTetrahedra, bool sixteenBitIndices, PxArray<PxU32>& surfaceTriangles, PxArray<PxU32>* surfaceTriangleToTet = NULL, bool flipTriangleOrientation = false);

		/** Extracts the surface triangles of a tetmesh

		The extracted triangle's vertex indices point to the vertex buffer of the tetmesh.

		\param[in] mesh The mesh from which the surface shall be computed
		\param[in] surfaceTriangles The resulting surface triangles
		\param[in] surfaceTriangleToTet Optional array to get the index of a tetrahedron that is adjacent to the surface triangle with the corresponding index
		\param[in] flipTriangleOrientation Reverses the orientation of the ouput triangles
		*/
		static void extractTetMeshSurface(const PxTetrahedronMesh* mesh, PxArray<PxU32>& surfaceTriangles, PxArray<PxU32>* surfaceTriangleToTet = NULL, bool flipTriangleOrientation = false);
	};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

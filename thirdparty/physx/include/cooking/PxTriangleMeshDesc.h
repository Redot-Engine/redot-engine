// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TRIANGLE_MESH_DESC_H
#define PX_TRIANGLE_MESH_DESC_H

#include "PxPhysXConfig.h"
#include "geometry/PxSimpleTriangleMesh.h"
#include "PxSDFDesc.h"

#if !PX_DOXYGEN
namespace physx
{
#endif


/**
\brief Descriptor class for #PxTriangleMesh.

Note that this class is derived from PxSimpleTriangleMesh which contains the members that describe the basic mesh.
The mesh data is *copied* when an PxTriangleMesh object is created from this descriptor. After the call the
user may discard the triangle data.

\see PxTriangleMesh PxTriangleMeshGeometry PxShape
*/
class PxTriangleMeshDesc : public PxSimpleTriangleMesh
{
public:

	/**
	Optional pointer to first material index, or NULL. There are PxSimpleTriangleMesh::numTriangles indices in total.
	Caller may add materialIndexStride bytes to the pointer to access the next triangle.

	When a triangle mesh collides with another object, a material is required at the collision point.
	If materialIndices is NULL, then the material of the PxShape instance is used.
	Otherwise, if the point of contact is on a triangle with index i, then the material index is determined as: 
	PxMaterialTableIndex	index = *(PxMaterialTableIndex *)(((PxU8*)materialIndices) + materialIndexStride * i);

	If the contact point falls on a vertex or an edge, a triangle adjacent to the vertex or edge is selected, and its index
	used to look up a material. The selection is arbitrary but consistent over time. 

	<b>Default:</b> NULL

	\see materialIndexStride
	*/
	PxTypedBoundedData<const PxMaterialTableIndex> materialIndices;

	/**
	\brief SDF descriptor. When this descriptor is set, a signed distance field (SDF) is calculated. SDF collisions only 
	work when the GPU solver is used to run the simulation. The GPU solver is enabled by setting the flag PxSceneFlag::eENABLE_GPU_DYNAMICS in the scene description.

	<b>Default:</b> NULL
	*/
	PxSDFDesc* sdfDesc;

	/**
	\brief Optional user-defined geometry epsilon for ray-triangle intersection tolerance.

	The geometry epsilon controls how much the barycentric bounds of each triangle are enlarged
	during raycasts and other scene queries. By default (0.0) it is auto-computed from the mesh's
	local bounding box. Set a positive value to override the automatic computation.

	<b>Default:</b> 0.0

	<b>Range:</b> [0.0, PX_MAX_F32)
	*/
	PxReal geomEpsilon;

	/**
	\brief Constructor sets to default.
	*/
	PX_INLINE PxTriangleMeshDesc();

	/**
	\brief (re)sets the structure to the default.	
	*/
	PX_INLINE void setToDefault();

	/**
	\brief Returns true if the descriptor is valid.
	\return true if the current settings are valid
	*/
	PX_INLINE bool isValid() const;
};


PX_INLINE PxTriangleMeshDesc::PxTriangleMeshDesc()	//constructor sets to default
{
	PxSimpleTriangleMesh::setToDefault();
	sdfDesc = NULL;
	geomEpsilon = 0.0f;
}

PX_INLINE void PxTriangleMeshDesc::setToDefault()
{
	*this = PxTriangleMeshDesc();
}

PX_INLINE bool PxTriangleMeshDesc::isValid() const
{
	if(points.count < 3) 	//at least 1 trig's worth of points
		return false;
	if ((!triangles.data) && (points.count%3))		// Non-indexed mesh => we must ensure the geometry defines an implicit number of triangles // i.e. numVertices can't be divided by 3
		return false;
	//add more validity checks here
	if (materialIndices.data && materialIndices.stride < sizeof(PxMaterialTableIndex))
		return false;

	if (sdfDesc && !sdfDesc->isValid())
			return false;
	
	return PxSimpleTriangleMesh::isValid();
}


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

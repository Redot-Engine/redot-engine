// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONVEX_MESH_H
#define PX_CONVEX_MESH_H

#include "foundation/PxVec3.h"
#include "foundation/PxMat33.h"
#include "common/PxBase.h"

#if !PX_DOXYGEN
namespace physx
{
class PxBounds3;
#endif

/**
\brief Polygon data

Plane format: (mPlane[0],mPlane[1],mPlane[2]).dot(x) + mPlane[3] = 0
With the normal outward-facing from the hull.
*/
struct PxHullPolygon
{
	PxReal			mPlane[4];		//!< Plane equation for this polygon
	PxU16			mNbVerts;		//!< Number of vertices/edges in the polygon
	PxU16			mIndexBase;		//!< Offset in index buffer
};

/**
\brief A convex mesh.

Internally represented as a list of convex polygons. The number
of polygons is limited to 256.

To avoid duplicating data when you have several instances of a particular
mesh positioned differently, you do not use this class to represent a
convex object directly. Instead, you create an instance of this mesh via
the PxConvexMeshGeometry and PxShape classes.

<h3>Creation</h3>

To create an instance of this class call PxPhysics::createConvexMesh(),
and PxConvexMesh::release() to delete it. This is only possible
once you have released all of its #PxShape instances.

<h3>Visualizations:</h3>
\li #PxVisualizationParameter::eCOLLISION_AABBS
\li #PxVisualizationParameter::eCOLLISION_SHAPES
\li #PxVisualizationParameter::eCOLLISION_AXES
\li #PxVisualizationParameter::eCOLLISION_FNORMALS
\li #PxVisualizationParameter::eCOLLISION_EDGES

\see PxConvexMeshDesc PxPhysics.createConvexMesh()
*/
class PxConvexMesh : public PxRefCounted
{
public:

	/**
	\brief Returns the number of vertices.
	\return	Number of vertices.
	\see getVertices()
	*/
	virtual	PxU32	getNbVertices()	const	= 0;

	/**
	\brief Returns the vertices.
	\return	Array of vertices.
	\see getNbVertices()
	*/
	virtual	const PxVec3*	getVertices()	const	= 0;

	/**
	\brief Returns the index buffer.
	\return	Index buffer.
	\see getNbPolygons() getPolygonData()
	*/
	virtual	const PxU8*		getIndexBuffer()	const	= 0;

	/**
	\brief Returns the number of polygons.
	\return	Number of polygons.
	\see getIndexBuffer() getPolygonData()
	*/
	virtual	PxU32	getNbPolygons()	const	= 0;

	/**
	\brief Returns the polygon data.
	\param[in] index	Polygon index in [0 ; getNbPolygons()[.
	\param[out] data	Polygon data.
	\return	True if success.
	\see getIndexBuffer() getNbPolygons()
	*/
	virtual	bool	getPolygonData(PxU32 index, PxHullPolygon& data)	const	= 0;

	/**
	\brief Decrements the reference count of a convex mesh and releases it if the new reference count is zero.	
	
	\see PxPhysics.createConvexMesh() PxConvexMeshGeometry PxShape
	*/
	virtual	void	release()	PX_OVERRIDE	= 0;

	/**
	\brief Returns the mass properties of the mesh assuming unit density.

	The following relationship holds between mass and volume:

		mass = volume * density

	The mass of a unit density mesh is equal to its volume, so this function returns the volume of the mesh.

	Similarly, to obtain the localInertia of an identically shaped object with a uniform density of d, simply multiply the
	localInertia of the unit density mesh by d.

	\param[out] mass The mass of the mesh assuming unit density.
	\param[out] localInertia The inertia tensor in mesh local space assuming unit density.
	\param[out] localCenterOfMass Position of center of mass (or centroid) in mesh local space.
	*/
	virtual void	getMassInformation(PxReal& mass, PxMat33& localInertia, PxVec3& localCenterOfMass)	const	= 0;

	/**
	\brief Returns the local-space (vertex space) AABB from the convex mesh.

	\return	local-space bounds
	*/
	virtual	PxBounds3	getLocalBounds()	const	= 0;

	/**
	\brief Returns the local-space Signed Distance Field for this mesh if it has one.
	\return local-space SDF.
	*/
	virtual const PxReal* getSDF() const = 0;


	virtual	const char*	getConcreteTypeName() const	PX_OVERRIDE	PX_FINAL	{ return "PxConvexMesh"; }

	/**
	\brief This method decides whether a convex mesh is gpu compatible. If the total number of vertices are more than 64 or any number of vertices in a polygon is more than 32, or
	convex hull data was not cooked with GPU data enabled during cooking or was loaded from a serialized collection, the convex hull is incompatible with GPU collision detection. Otherwise
	it is compatible.

	\return True if the convex hull is gpu compatible
	*/
	virtual bool		isGpuCompatible() const = 0;


protected:
	PX_INLINE			PxConvexMesh(PxType concreteType, PxBaseFlags baseFlags) : PxRefCounted(concreteType, baseFlags) {}
	PX_INLINE			PxConvexMesh(PxBaseFlags baseFlags) : PxRefCounted(baseFlags) {}
	virtual				~PxConvexMesh() {}
	virtual	bool		isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxConvexMesh", PxRefCounted); }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

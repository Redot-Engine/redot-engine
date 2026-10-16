// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_MESH_SCALE_H
#define PX_MESH_SCALE_H

#include "common/PxPhysXCommonConfig.h"
#include "foundation/PxMat33.h"
#include "foundation/PxAssert.h"

/** \brief Minimum allowed absolute magnitude for each of mesh scale's components (x,y,z).
	\note Only positive scale values are allowed for convex meshes. */
#define PX_MESH_SCALE_MIN 1e-6f

/** \brief Maximum allowed absolute magnitude for each of mesh scale's components (x,y,z).
	\note Only positive scale values are allowed for convex meshes. */
#define PX_MESH_SCALE_MAX 1e6f

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief A class expressing a nonuniform scaling transformation.

The scaling is along arbitrary axes that are specified by PxMeshScale::rotation. Specifically, PxMeshScale::rotation
describes the rotation from the scaling-axes frame to the mesh-local frame, i.e. PxMeshScale::rotation.rotate(v) transforms
the coordinates of vertex v from the mesh-local frame to the scaling-axes frame.

\note	Negative scale values are supported for PxTriangleMeshGeometry
		with absolute values for each component within [PX_MIN_ABS_MESH_SCALE, PX_MAX_ABS_MESH_SCALE] range.
		Negative scale causes a reflection around the specified axis, in addition PhysX will flip the normals
		for mesh triangles when scale.x*scale.y*scale.z < 0.
\note	Only positive scale values are supported for PxConvexMeshGeometry
		with values for each component within [PX_MIN_ABS_MESH_SCALE, PX_MAX_ABS_MESH_SCALE] range).

\see PxConvexMeshGeometry PxTriangleMeshGeometry
*/
class PxMeshScale
{
public:
	/**
	\brief Constructor initializes to identity scale.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxMeshScale(): scale(1.0f), rotation(PxIdentity) 
	{
	}

	/**
	\brief Constructor from scalar.
	*/
	explicit PX_CUDA_CALLABLE PX_FORCE_INLINE PxMeshScale(PxReal r): scale(r), rotation(PxIdentity) 
	{
	}

	/**
	\brief Constructor to initialize to arbitrary scale and identity scale rotation.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxMeshScale(const PxVec3& s)
	{
		scale = s;
		rotation = PxQuat(PxIdentity);
	}

	/**
	\brief Constructor to initialize to arbitrary scaling.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxMeshScale(const PxVec3& s, const PxQuat& r)
	{
		PX_ASSERT(r.isUnit());
		scale = s;
		rotation = r;
	}

	/**
	\brief Returns true if the scaling is an identity transformation.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool isIdentity()	const
	{
		return (scale.x == 1.0f && scale.y == 1.0f && scale.z == 1.0f);
	}

	/**
	\brief Returns the inverse of this scaling transformation.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE  PxMeshScale getInverse() const 
	{
		return PxMeshScale(PxVec3(1.0f/scale.x, 1.0f/scale.y, 1.0f/scale.z), rotation);
	}

	/**
	\brief Converts this transformation to a 3x3 matrix representation.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxMat33 toMat33() const 
	{
		PxMat33 rot(rotation);
		PxMat33 trans = rot.getTranspose();
		trans.column0 *= scale[0];
		trans.column1 *= scale[1];
		trans.column2 *= scale[2];
		return trans * rot;
	}

	/**
	\brief Returns true if combination of negative scale components will cause the triangle normal to flip. The SDK will flip the normals internally.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool hasNegativeDeterminant() const
	{
		return (scale.x * scale.y * scale.z < 0.0f);
	}

	PxVec3		transform(const PxVec3& v) const
	{
		return rotation.rotateInv(scale.multiply(rotation.rotate(v)));
	}

	bool		isValidForTriangleMesh() const
	{
		PxVec3 absXYZ = scale.abs();
		return (absXYZ.maxElement() <= PX_MESH_SCALE_MAX) && (absXYZ.minElement() >= PX_MESH_SCALE_MIN);
	}

	bool		isValidForConvexMesh() const
	{
		return (scale.maxElement() <= PX_MESH_SCALE_MAX) && (scale.minElement() >= PX_MESH_SCALE_MIN);
	}

	PxVec3	scale;		//!< A nonuniform scaling
	PxQuat	rotation;	//!< The orientation of the scaling axes
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

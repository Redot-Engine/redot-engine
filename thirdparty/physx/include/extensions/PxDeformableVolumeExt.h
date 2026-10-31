// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_VOLUME_EXT_H
#define PX_DEFORMABLE_VOLUME_EXT_H

#include "foundation/PxTransform.h"
#include "foundation/PxUserAllocated.h"
#include "PxDeformableVolume.h"
#include "PxDeformableVolumeFlag.h"
#include "cudamanager/PxCudaContextManager.h"
#include "cudamanager/PxCudaTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxCookingParams;
class PxSimpleTriangleMesh;
class PxInsertionCallback;
class PxDeformableVolumeMesh;

/**
\brief Utility functions for use with PxDeformableVolume and subclasses
*/
class PxDeformableVolumeExt
{
public:
	/** 
	\brief Computes the deformable volume's vertex masses from the provided density and the volume of the tetrahedra

	The buffers affected by this operation can be obtained from the deformable volume using the methods getSimPositionInvMassBufferD() and getSimVelocityBufferD()

	The inverse mass is stored in the 4th component (the first three components are x, y, z coordinates) of the simulation mesh's position buffer. 

	\param[in] deformableVolume The deformable volume which will get its mass updated
	\param[in] density The density to used to calculate the mass from the body's volume
	\param[in] maxInvMassRatio Maximum allowed ratio defined as max(vertexMasses) / min(vertexMasses) where vertexMasses is a list of float values with a mass for every vertex in the simulation mesh
	\param[in] simPositionsPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex of the simulation mesh.

	\see PxDeformableVolume PxDeformableVolume::getSimPositionInvMassBufferD()
	*/
	static void updateMass(PxDeformableVolume& deformableVolume, const PxReal density, const PxReal maxInvMassRatio, PxVec4* simPositionsPinned);

	/**
	\brief Computes the deformable volume's vertex masses such that the sum of all masses is equal to the provided mass

	The buffers affected by this operation can be obtained from the deformable volume using the methods getSimPositionInvMassBufferD()) and getSimVelocityBufferD()

	The inverse mass is stored in the 4th component (the first three components are x, y, z coordinates) of the simulation mesh's position buffer.

	\param[in] deformableVolume The deformable volume which will get its mass updated
	\param[in] mass The deformable volume's mass
	\param[in] maxInvMassRatio Maximum allowed ratio defined as max(vertexMasses) / min(vertexMasses) where vertexMasses is a list of float values with a mass for every vertex in the simulation mesh
	\param[in] simPositionsPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex of the simulation mesh.

	\see PxDeformableVolume PxDeformableVolume::getSimPositionInvMassBufferD()
	*/
	static void setMass(PxDeformableVolume& deformableVolume, const PxReal mass, const PxReal maxInvMassRatio, PxVec4* simPositionsPinned);

	/**
	\brief Transforms a deformable volume

	The buffers affected by this operation can be obtained from the deformable volume using the methods getSimPositionInvMassBufferD() and getSimVelocityBufferD()

	Applies a transformation to the simulation mesh's positions an velocities. Velocities only get rotated and scaled (translation is not applicable to direction vectors).
	It does not modify the body's mass. 
	If the method is called multiple times, the transformation will compound with the ones previously applied.

	\param[in] deformableVolume The deformable volume which is transformed
	\param[in] transform The transform to apply
	\param[in] scale A scaling factor
	\param[in] simPositionsPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex of the simulation mesh.
	\param[in] simVelocitiesPinned A pointer to a pinned host memory buffer containing velocities for each vertex of the simulation mesh.
	\param[in] collPositionsPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex of the collision mesh.
	\param[in] restPositionsPinned A pointer to a pinned host memory buffer containing rest positions of the collision mesh.
	
	\see PxDeformableVolume
	*/
	static void transform(PxDeformableVolume& deformableVolume, const PxTransform& transform, const PxReal scale, PxVec4* simPositionsPinned, PxVec4* simVelocitiesPinned, PxVec4* collPositionsPinned, PxVec4* restPositionsPinned);

	/**
	\brief Updates the collision mesh's vertex positions to match the simulation mesh's transformation and scale.
	
	The buffer affected by this operation can be obtained from the deformable volume using the method getPositionInvMassBufferD()

	\param[in] deformableVolume The deformable volume which will get its collision mesh vertices updated
	\param[in] simPositionsPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex of the simulation mesh.
	\param[in] collPositionsPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex of the collision mesh.

	\see PxDeformableVolume
	*/
	static void updateEmbeddedCollisionMesh(PxDeformableVolume& deformableVolume, PxVec4* simPositionsPinned, PxVec4* collPositionsPinned);

	/**
	\brief Uploads prepared deformable volume data to the GPU. It ensures that the embedded collision mesh matches the simulation mesh's transformation and scale.

	\param[in] deformableVolume The deformable volume which will perform the data upload
	\param[in] flags Specifies which buffers the data transfer should include
	\param[in] simPositionsPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex of the simulation mesh.
	\param[in] simVelocitiesPinned A pointer to a pinned host memory buffer containing velocities for each vertex of the simulation mesh.
	\param[in] collPositionsPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex of the collision mesh.
	\param[in] restPositionsPinned A pointer to a pinned host memory buffer containing rest positions of the collision mesh.
	\param[in] stream A cuda stream to perform the copies.
	
	\see PxDeformableVolume
	*/
	static void copyToDevice(PxDeformableVolume& deformableVolume, PxDeformableVolumeDataFlags flags, PxVec4* simPositionsPinned, PxVec4* simVelocitiesPinned, PxVec4* collPositionsPinned, PxVec4* restPositionsPinned, CUstream stream = CUstream(0));

	/**
	\brief Creates a full deformable volume mesh matching the shape given as input. Uses a voxel mesh for FEM simulation and a surface-matching mesh for collision detection. 

	\param[in] params Cooking params instance required for mesh processing
	\param[in] surfaceMesh Input triangle mesh that represents the surface of the deformable volume
	\param[in] numVoxelsAlongLongestAABBAxis The number of voxels along the longest bounding box axis
	\param[in] insertionCallback The insertion interface from PxPhysics
	\param[in] validate If set to true the input triangle mesh will get analyzed to find possible deficiencies
	\return Deformable volume mesh if cooking was successful, NULL otherwise
	\see PxDeformableVolumeMesh
	*/
	static PxDeformableVolumeMesh* createDeformableVolumeMesh(const PxCookingParams& params, const PxSimpleTriangleMesh& surfaceMesh, PxU32 numVoxelsAlongLongestAABBAxis, PxInsertionCallback& insertionCallback, const bool validate = true);

	/**
	\brief Creates a full deformable volume mesh matching the shape given as input. Uses the same surface-matching mesh for collision detection and FEM simulation.

	\param[in] params Cooking params instance required for mesh processing
	\param[in] surfaceMesh Input triangle mesh that represents the surface of the deformable volume
	\param[in] insertionCallback The insertion interface from PxPhysics
	\param[in] maxWeightRatioInTet Upper limit for the ratio of node weights that are adjacent to the same tetrahedron. The closer to one (while remaining larger than one), the more stable the simulation. 
	\param[in] validate If set to true the input triangle mesh will get analyzed to find possible deficiencies
	\return Deformable volume mesh if cooking was successful, NULL otherwise
	\see PxDeformableVolumeMesh
	*/
	static PxDeformableVolumeMesh* createDeformableVolumeMeshNoVoxels(const PxCookingParams& params, const PxSimpleTriangleMesh& surfaceMesh,
		PxInsertionCallback& insertionCallback, PxReal maxWeightRatioInTet = 1.5f, const bool validate = true);

	/**
	\brief Creates a deformable volume instance from a deformable volume mesh

	\param[in] deformableVolumeMesh The deformable volume mesh
	\param[in] transform The transform that defines initial position and orientation of the deformable volume
	\param[in] material The material
	\param[in] cudaContextManager A cuda context manager
	\param[in] density The density used to compute the mass properties
	\param[in] scale The scaling of the deformable volume
	\return Deformable volume instance
	\see PxDeformableVolumeMesh, PxDeformableVolume
	*/
	static PxDeformableVolume* createDeformableVolumeFromMesh(PxDeformableVolumeMesh* deformableVolumeMesh, const PxTransform& transform,
		const PxDeformableVolumeMaterial& material, PxCudaContextManager& cudaContextManager, PxReal density = 100.0f, PxReal scale = 1.0f);

	/**
	\brief Creates a deformable volume instance with a box shape

	\param[in] transform The transform that defines initial position and orientation of the deformable volume
	\param[in] boxDimensions The dimensions (side lengths) of the box shape
	\param[in] material The material
	\param[in] cudaContextManager A cuda context manager
	\param[in] maxEdgeLength The maximal length of a triangle edge. Subdivision will get applied until the edge length criteria is matched. -1 means no subdivision is applied.
	\param[in] density The density used to compute the mass properties
	\param[in] numVoxelsAlongLongestAABBAxis The number of voxels to use for the simulation mesh along the longest bounding box dimension
	\param[in] scale The scaling of the deformable volume
	\return Deformable volume instance
	\see PxDeformableVolumeMesh, PxDeformableVolume
	*/
	static PxDeformableVolume* createDeformableVolumeBox(const PxTransform& transform, const PxVec3& boxDimensions, const PxDeformableVolumeMaterial& material,
		PxCudaContextManager& cudaContextManager, PxReal maxEdgeLength = -1.0f, PxReal density = 100.0f, PxU32 numVoxelsAlongLongestAABBAxis = 10, PxReal scale = 1.0f);

	/**
	\brief allocates and initializes pinned host memory buffers from an actor with shape.

	\param[in] deformableVolume A PxDeformableVolume that has a valid shape attached to it.
	\param[in] cudaContextManager The PxCudaContextManager of the scene this deformable volume will be simulated in
    \param[in] simPositionInvMassPinned A reference to a pointer for the return value of the simPositionInvMassPinned buffer, will be set by this function.
    \param[in] simVelocityPinned A reference to a pointer for the return value of the simVelocityPinned buffer, will be set by this function.
    \param[in] collPositionInvMassPinned A reference to a pointer for the return value of the collPositionInvMassPinned buffer, will be set by this function.
    \param[in] restPositionPinned A reference to a pointer for the return value of the restPositionPinned buffer, will be set by this function.

	\see PxDeformableVolume
	 */
	static void allocateAndInitializeHostMirror(PxDeformableVolume& deformableVolume, PxCudaContextManager* cudaContextManager, PxVec4*& simPositionInvMassPinned, PxVec4*& simVelocityPinned, PxVec4*& collPositionInvMassPinned, PxVec4*& restPositionPinned);
	
	/**
	\brief Given a set of points and a set of tetrahedra, it finds the equilibrium state of the deformable volume. Every input point is either fixed or can move freely.

	\param[in] verticesOriginal Mesh vertex positions in undeformed original state.
	\param[in] verticesDeformed Mesh vertex positions in new deformed state. Only fixed vertices must have their final location, all other locations will get updated by the method.
	\param[in] nbVertices The number of vertices.
	\param[in] tetrahedra The tetrahedra.
	\param[in] nbTetraheda The number of tetrahedra.
	\param[in] vertexIsFixed Optional input that specifies which vertex is fixed and which one can move to relax the tension. If not provided, vertices from verticesOriginal which have a .w value of 0 will be considered fixed.
	\param[in] numIterations The number of stress relaxation iterations to run.
	*/
	static void relaxDeformableVolumeMesh(const PxVec4* verticesOriginal, PxVec4* verticesDeformed, PxU32 nbVertices, 
		const PxU32* tetrahedra, PxU32 nbTetraheda, const bool* vertexIsFixed = NULL, PxU32 numIterations = 200);

	/**
	\brief Converts the tet id and barycentric from the collision mesh to the tet id and barycentric of the simulation mesh.

	\param[in] deformableVolume The deformable volume to perform the operation on.
	\param[in] tetId The tet id of the deformable volume's collision mesh.
	\param[in] tetBarycentric The barycentric coordinates of the tetrahedron specified with tetId.
	\param[out] outTetId The tet id of the deformable volume's simulation mesh.
	\param[out] outTetBarycentric The barycentric coordinates of the tetrahedron specified with outTetId.
	*/
	static void convertCollisionToSimulationTet(PxDeformableVolume& deformableVolume, PxU32 tetId, const PxVec4& tetBarycentric, PxU32& outTetId, PxVec4& outTetBarycentric);
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_DEFORMABLE_VOLUME_EXT_H

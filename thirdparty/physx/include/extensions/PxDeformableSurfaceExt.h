// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_SURFACE_EXT_H
#define PX_DEFORMABLE_SURFACE_EXT_H

#include "PxDeformableSurface.h"
#include "foundation/PxVec4.h"
#include "foundation/PxTransform.h"
#include "foundation/PxUserAllocated.h"
#include "cudamanager/PxCudaContextManager.h"
#include "cudamanager/PxCudaTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Utility functions for use with PxDeformableSurface
*/
class PxDeformableSurfaceExt
{
public:
    /**
    \brief Uploads prepared deformable surface data to the GPU.

    \param[in] deformableSurface The deformable surface to perform the data upload to.
    \param[in] flags Specifies which buffers the data transfer should include.
    \param[in] nbVertices The number of vertices in the surface deformable mesh.
    \param[in] positionsPinned A pointer to a pinned host memory buffer containing position and inverse mass for each vertex.
    \param[in] velocitiesPinned A pointer to a pinned host memory buffer containing the velocity for each vertex.
    \param[in] restPositionsPinned A pointer to a pinned host memory buffer containing the rest position for each vertex.
    \param[in] copyStream An optional cuda stream to schedule the copy on.

    \see PxDeformableSurface
    */
    static void copyToDevice(PxDeformableSurface& deformableSurface, PxDeformableSurfaceDataFlags flags, PxU32 nbVertices,
							 PxVec4* positionsPinned, PxVec4* velocitiesPinned, PxVec4* restPositionsPinned,
							 CUstream copyStream = CUstream(0));

    /**
    \brief Distributes a list of triangles masses to vertices.

    The mass for each triangle will be distributed in equal parts to the vertices of said triangle.

    \param[in] deformableSurface The deformable surface to perform the operation on.
    \param[in] triangleMasses A list of floats specifying the mass of each triangle.
    \param[in] positionInvMassPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex.

    \see PxDeformableSurface
    */
    static void distributeTriangleMassToVertices(PxDeformableSurface& deformableSurface, const PxReal* triangleMasses,
	                                             PxVec4* positionInvMassPinned);

    /**
    \brief Distributes a uniform density to the vertices of a deformable surface.

    This method distributes mass based on a specified mass per unit area. The mass for each vertex is calculated
    according to the area of the triangles connected to it, and the resulting mass is assigned to the vertex.

    \param[in] deformableSurface The deformable surface to perform the operation on.
    \param[in] massPerVolume The mass per unit volume (=density) to be distributed across the vertices.
    \param[in] clothThickness The cloth thickness to compute the mass
    \param[in] positionInvMassPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex.

    \see PxDeformableSurface
    */
    static void distributeDensityToVertices(PxDeformableSurface& deformableSurface, PxReal massPerVolume, PxReal clothThickness,
        PxVec4* positionInvMassPinned);

    /**
    \brief Distributes a total mass uniformly to the vertices of a deformable surface.

    This method calculates the total mass to be distributed across all vertices, and assigns a proportional mass to each
    vertex based on the geometry of the surface. The mass is distributed equally to ensure the total mass of the surface
    matches the specified value.

    \param[in] deformableSurface The deformable surface to perform the operation on.
    \param[in] totalMass The total mass to be distributed across the vertices.
    \param[in] positionInvMassPinned A pointer to a pinned host memory buffer containing positions and inverse masses for each vertex.

    \see PxDeformableSurface
    */
    static void distributeMassToVertices(PxDeformableSurface& deformableSurface, PxReal totalMass, 
        PxVec4* positionInvMassPinned);

    /**
    \brief Allocates and initializes a pinned host memory from a PxTriangleMesh attached to a PxDeformableSurface using a PxShape.

    \note The user is responsible for deallocation and lifetime management of the positionInvMassPinned, velocityPinned
    and restPositionsPinned buffers.

    This method fails if the deformable surface does not have a shape attached to it.

    \param[in] deformableSurface The deformable surface to perform the operation on.
    \param[in] positions The initial positions of the surface deformable vertices.
    \param[in] velocities The initial velocities of the surface deformable vertices.
    \param[in] restPositions The rest positions of the surface deformable vertices.
    \param[in] mass The mass of the deformable surface, will be distributed equally among vertices.
    \param[in] transform The world-space transform of the deformable surface.
    \param[in] cudaContextManager The PxCudaContextManager of the scene this deformable surface will be simulated in.
    \param[in] positionInvMassPinned A reference to a pointer for the return value of the positionInvMassPinned buffer, will be set by this function.
    \param[in] velocityPinned A reference to a pointer for the return value of the velocityPinned buffer, will be set by this function.
    \param[in] restPositionPinned A reference to a pointer for the return value of the restPositionPinned buffer, will be set by this function.
    
    \return The number of vertices in the surface deformable mesh.
    
    \see PxDeformableSurface
    */
	static PxU32 allocateAndInitializeHostMirror(PxDeformableSurface& deformableSurface, const PxVec3* positions,
	                                             const PxVec3* velocities, const PxVec3* restPositions, float mass,
	                                             const PxTransform& transform, PxCudaContextManager* cudaContextManager,
	                                             PxVec4*& positionInvMassPinned, PxVec4*& velocityPinned,
	                                             PxVec4*& restPositionPinned);

    /**
    \brief Allocates and initializes a pinned host memory from given positions, velocities, and rest positions.

    \note The user is responsible for deallocation and lifetime management of the positionInvMassPinned, velocityPinned
    and restPositionsPinned buffers.

    If the input 'restPositions' is a null pointer, positions are used in place of restPositions.
    If the input 'velocities' is a null pointer, zero velocities are assigned to velocityPinned.

    \param[in] positions The positions of the surface deformable vertices, will be used to assign positionInvMassPinned buffer.
    \param[in] velocities The velocities of the surface deformable vertices, will be used to assign velocityPinned buffer.
    \param[in] restPositions The rest positions of the surface deformable vertices, will be used to assign restPositionPinned buffer.
    \param[in] nbVertices The number of vertices in the surface deformable mesh.
    \param[in] mass The mass of the deformable surface, will be distributed equally among vertices.
    \param[in] transform The world-space transform of the deformable surface.
    \param[in] cudaContextManager The PxCudaContextManager of the scene this deformable surface will be simulated in.
    \param[in] positionInvMassPinned A reference to a pointer for the return value of the positionInvMassPinned buffer, will be set by this function.
    \param[in] velocityPinned A reference to a pointer for the return value of the velocityPinned buffer, will be set by this function.
    \param[in] restPositionPinned A reference to a pointer for the return value of the restPositionPinned buffer, will be set by this function.

    \return The number of vertices in the surface deformable mesh.

    \see PxDeformableSurface
    */
	static PxU32 allocateAndInitializeHostMirror(const PxVec3* positions, const PxVec3* velocities,
	                                             const PxVec3* restPositions, PxU32 nbVertices, float mass,
	                                             const PxTransform& transform, PxCudaContextManager* cudaContextManager,
	                                             PxVec4*& positionInvMassPinned, PxVec4*& velocityPinned,
	                                             PxVec4*& restPositionPinned);
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_DEFORMABLE_SURFACE_EXT_H

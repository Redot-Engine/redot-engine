// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0


#ifndef PX_PHYSICS_DEFORMABLE_SURFACE_H
#define PX_PHYSICS_DEFORMABLE_SURFACE_H

#include "PxDeformableBody.h"
#include "PxDeformableSurfaceFlag.h"
#include "foundation/PxArray.h"
#include "foundation/PxVec4.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

#if PX_VC
#pragma warning(push)
#pragma warning(disable : 4435)
#endif


class PxCudaContextManager;

/**
\brief The maximum number of triangles supported in a surface deformable mesh

The current limit is 1'048'575.
*/
#define	PX_MAX_NB_DEFORMABLE_SURFACE_TRI 0x000fffff

/**
\brief The maximum number of vertices supported in a surface deformable mesh

The current limit is 1'048'575.
*/
#define	PX_MAX_NB_DEFORMABLE_SURFACE_VTX 0x000fffff

/**
\brief The maximum number of deformable surfaces supported in a scene

The current limit is 4095.
*/
#define PX_MAX_NB_DEFORMABLE_SURFACE 0xfff

/**
\brief Represents a deformable surface

The deformable surface feature is exclusively supported on GPU. The full GPU pipeline needs to 
be enabled in order to make use of deformable bodies, see #PxSceneFlag::eENABLE_GPU_DYNAMICS,
#PxBroadPhaseType::eGPU.
*/
class PxDeformableSurface : public PxDeformableBody
{
public:

	/**
	\brief Raises or clears a particular deformable surface flag.

	See the list of flags #PxDeformableSurfaceFlag

	<b>Default:</b> No flags are set

	\param[in] flag The PxDeformableSurfaceFlag to raise(set) or clear. See #PxDeformableSurfaceFlag.
	\param[in] val The new boolean value for the flag.
	*/
	virtual	void							setDeformableSurfaceFlag(PxDeformableSurfaceFlag::Enum flag, bool val) = 0;
	
	/**
	\brief Sets deformable surface flags.

	See the list of flags #PxDeformableSurfaceFlag

	<b>Default:</b> No flags are set

	\param[in] flags The PxDeformableSurfaceFlags to set.
	*/
	virtual	void							setDeformableSurfaceFlags(PxDeformableSurfaceFlags flags) = 0;
	
	/**
	\brief Reads the deformable surface flags.

	See the list of flags #PxDeformableSurfaceFlag

	\return The values of the deformable surface flags.

	\see setDeformableSurfaceFlag()
	*/
	virtual	PxDeformableSurfaceFlags		getDeformableSurfaceFlags() const = 0;

	/**
	\brief Sets the number of collision pair updates per timestep. 

	Collision pair is updated at least once per timestep and increasing the frequency provides better collision pairs.
	<b>Default:</b> 1

	\param[in] frequency It sets the number of collision pair updates per timestep.

	\see getNbCollisionPairUpdatesPerTimestep()
	*/
	virtual	void							setNbCollisionPairUpdatesPerTimestep(const PxU32 frequency) = 0;

	/**
	\brief Retrieves number of collision pair updates per timestep. 

	\return The number of collision pair updates per timestep.
	\see setNbCollisionPairUpdatesPerTimestep()
	*/
	virtual	PxU32							getNbCollisionPairUpdatesPerTimestep() const = 0;

	/**
	\brief Sets the number of collision substeps in each sub-timestep.

	Collision constraints can be applied multiple times in each sub-timestep
	<b>Default:</b> 1

	\param[in] frequency It sets the number of collision substeps in each sub-timestep.

	\see getNbCollisionSubsteps()
	*/
	virtual	void							setNbCollisionSubsteps(const PxU32 frequency) = 0;

	/**
	\brief Retrieves the number of collision substeps in each sub-timestep.

	\return The number of collision substeps in each sub-timestep.

	\see setNbCollisionSubsteps()
	*/
	virtual	PxU32							getNbCollisionSubsteps() const = 0;

	/**
	\brief Gets a pointer to a device buffer containing positions and inverse masses of the 
	surface deformable.

	This function returns a pointer to device memory for the positions and inverse masses of
	the surface deformable. The device memory buffer is used to both initialize/update the vertices of the surface deformable and 
	read the simulation results.

	\note It is mandatory to call PxDeformableSurface::markDirty() with PxDeformableSurfaceDataFlag::ePOSITION_INVMASS when
	updating data in this buffer.

	The simulation expects 4 consecutive floats for each vertex, aligned to a 16B boundary. 
	The first 3 floats specify the positions and the last float specifies the inverse mass of the vertex.
	The size of the buffer is the number of vertices of the surface deformable mesh * sizeof(PxVec4).
	\see PxTriangleMesh::getNbVertices().

	The device memory pointed to by this pointer is allocated when a shape is attached to the 
	deformable surface. Calling PxDeformableSurface::detachShape() will deallocate the memory.

	It is not allowed to write to this buffer from the start of the PxScene::simulate() call
	until PxScene::fetchResults() returns. Reading the data is allowed once all the PhysX tasks
	have finished, reading the data during a completion task is explicitly allowed. The 
	simulation will read and write directly from/into this buffer.

	It is the users' responsibility to initialize this buffer with the initial positions of 
	the vertices of the surface deformable mesh.
	
	\return PxVec4* A pointer to a device buffer containing positions and inverse masses of
	the surface deformable mesh.
	 */
	virtual	PxVec4* 						getPositionInvMassBufferD() = 0;

	/**
	\brief Gets a pointer to a device buffer containing velocities of the deformable surface.

	This function returns a pointer to device memory for the velocities of the deformable surface. This buffer
	is used to both initialize/update the vertices of the surface deformable and read the simulation results. 
	
	\note It is mandatory to call PxDeformableSurface::markDirty() with PxDeformableSurfaceDataFlag::eVELOCITY when
	updating data in this buffer.


	The simulation expects 4 consecutive floats for each vertex, aligned to a 16B boundary. The 
	first 3 floats specify the velocity of the vertex. The final float is unused. The size of 
	the buffer is the number of vertices of the surface deformable mesh * sizeof(PxVec4). 
	\see PxTriangleMesh::getNbVertices().

	The device memory pointed to by this pointer is allocated when a shape is attached to the 
	deformable surface. Calling PxDeformableSurface::detachShape() will deallocate the memory.

	It is not allowed to write to this buffer from the start of the PxScene::simulate() call
	until PxScene::fetchResults() returns. Reading the data is allowed once all the PhysX tasks
	have finished, reading the data during a completion task is explicitly allowed. The 
	simulation will read and write directly from/into this buffer.

	It is the users' responsibility to initialize this buffer with the initial velocities of 
	the vertices of the surface deformable mesh.
	
	\return PxVec4* A pointer to a device buffer containing the velocities of the surface deformable mesh.
	 */
	virtual	PxVec4* 						getVelocityBufferD() = 0;

	/**
	\brief Gets a pointer to a device buffer containing the rest positions of the deformable surface.

	This function returns a pointer to device memory for the rest positions of the deformable surface.
	This buffer is used to initialize/update the rest positions of the vertices of the deformable surface.

	\note It is mandatory to call PxDeformableSurface::markDirty() with PxDeformableSurfaceDataFlag::eREST_POSITION when
	updating data in this buffer.

	The simulation expects 4 consecutive floats for each vertex, aligned to a 16B boundary.
	The first 3 specify the rest position. The last float is unused. The size of the buffer 
	is the number of vertices of the surface deformable mesh * sizeof(PxVec4). \see PxTriangleMesh::getNbVertices().

	The device memory pointed to by this pointer is allocated when a shape is attached to the 
	deformable surface. Calling PxDeformableSurface::detachShape() will deallocate the memory.

	It is not allowed to write to this buffer from the start of the PxScene::simulate() call
	until PxScene::fetchResults() returns. Reading the data is allowed once all the PhysX tasks
	have finished, reading the data during a completion task is explicitly allowed. The 
	simulation will read directly from this buffer.

	It is the users' responsibility to initialize this buffer with the initial rest positions of 
	the vertices of the surface deformable mesh.
	
	\return PxVec4* A pointer to a device buffer containing the rest positions of the surface deformable mesh.
	 */
	virtual	PxVec4* 						getRestPositionBufferD() = 0;

	/**
	\brief Marks per-vertex simulation state and configuration buffers dirty to signal to the simulation
	that changes have been made.

	Calling this function is required to notify the simulation of changes made in the positionInvMass, 
	velocity and rest position buffers.

	This function can be called multiple times, and dirty flags are accumulated internally until 
	PxScene::simulate() is called.

	\see getPositionInvMassBufferD, getVelocityBufferD, getRestPositionBufferD
	
	\param flags The buffers that have been updated.
	*/
	virtual	void							markDirty(PxDeformableSurfaceDataFlags flags) = 0;

	/**
	\brief Gets the concrete type name.
	\return The name of the concrete type.
	*/
	virtual	const char*						getConcreteTypeName() const PX_OVERRIDE	PX_FINAL { return "PxDeformableSurface"; }

protected:
	PX_INLINE								PxDeformableSurface(PxType concreteType, PxBaseFlags baseFlags) : PxDeformableBody(concreteType, baseFlags) {}
	PX_INLINE								PxDeformableSurface(PxBaseFlags baseFlags) : PxDeformableBody(baseFlags) {}
	virtual									~PxDeformableSurface() {}
	virtual	bool							isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxDeformableSurface", PxDeformableBody); }
};

#if PX_VC
#pragma warning(pop)
#endif


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_PHYSICS_DEFORMABLE_SURFACE_H


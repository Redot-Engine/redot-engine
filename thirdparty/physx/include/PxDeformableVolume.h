// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_VOLUME_H
#define PX_DEFORMABLE_VOLUME_H

#include "PxDeformableBody.h"
#include "PxDeformableVolumeFlag.h"
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
class PxTetrahedronMesh;
class PxDeformableVolumeAuxData;
class PxDeformableSurface;
class PxParticleBuffer;

/**
\brief The maximum number of tetrahedrons supported in a deformable volume tetrahedron mesh

The current limit is 1'048'575.
*/
#define PX_MAX_NB_DEFORMABLE_VOLUME_TET 0x000fffff

/**
\brief The maximum number of deformable volumes supported in a scene

The current limit is 4095.
*/
#define PX_MAX_NB_DEFORMABLE_VOLUME 0xfff

/**
\brief Represents a deformable volume

The deformable volume feature is exclusively supported on GPU. The full GPU pipeline needs to
be enabled in order to make use of deformable bodies, see #PxSceneFlag::eENABLE_GPU_DYNAMICS,
#PxBroadPhaseType::eGPU.
*/
class PxDeformableVolume : public PxDeformableBody
{
public:

	/**
	\brief Raises or clears a particular deformable volume flag.

	See the list of flags #PxDeformableVolumeFlag

	<b>Default:</b> No flags are set

	\param[in] flag The PxDeformableVolumeFlag to raise(set) or clear. See #PxDeformableVolumeFlag.
	\param[in] val  The new boolean value for the flag.
	*/
	virtual		void						setDeformableVolumeFlag(PxDeformableVolumeFlag::Enum flag, bool val) = 0;
	
	/**
	\brief Sets deformable volume flags.

	See the list of flags #PxDeformableVolumeFlag

	<b>Default:</b> No flags are set

	\param[in] flags The PxDeformableVolumeFlags to set.
	*/
	virtual		void						setDeformableVolumeFlags(PxDeformableVolumeFlags flags) = 0;

	/**
	\brief Reads the deformable volume flags.

	See the list of flags #PxDeformableVolumeFlag

	\return The values of the deformable volume flags.

	\see setDeformableVolumeFlag()
	*/
	virtual		PxDeformableVolumeFlags		getDeformableVolumeFlags() const = 0;

	/**
	\brief Sets the self collision stress tolerance.

	Stress threshold to deactivate collision contacts in case the local stress magnitude exceeds the threshold.
	<b>Default:</b> 0.9

	\param[in] selfCollisionStressTolerance The maximal depenetration velocity
	*/
	virtual		void						setSelfCollisionStressTolerance(const PxReal selfCollisionStressTolerance) = 0;

	/**
	\brief Retrieves the self collision stress tolerance.

	\return The self collision filter distance
	\see setSelfCollisionFilterDistance
	*/
	virtual		PxReal						getSelfCollisionStressTolerance() const = 0;

	/**
	\brief Gets a pointer to a device buffer containing positions and inverse masses of the 
	collision mesh.

	This function returns a pointer to device memory for the positions and inverse masses of
	the deformable volume. This buffer is used to both initialize/update the collision mesh vertices
	of the deformable volume and read the simulation results.

	\note It is mandatory to call PxDeformableVolume::markDirty() with PxDeformableVolumeDataFlag::ePOSITION_INVMASS
	when updating data in this buffer. 

	The simulation expects 4 consecutive floats for each vertex, aligned to a 16B boundary.
	The first 3 floats specify the vertex position and the last float contains the inverse mass of the
	vertex. The size of the buffer is the number of vertices of the collision mesh * sizeof(PxVec4).
	\see PxTetrahedronMesh::getNbVertices().

	The device memory pointed to by this pointer is allocated when a shape is attached to the 
	deformable volume. Calling PxDeformableVolume::detachShape() will deallocate the memory.

	It is not allowed to write to this buffer from the start of the PxScene::simulate() call
	until PxScene::fetchResults() returns. Reading the data is allowed once all the PhysX tasks
	have finished, reading the data during a completion task is explicitly allowed. The 
	simulation will read and write directly from/into this buffer.

	It is the users' responsibility to initialize this buffer with the initial positions of 
	the vertices of the collision mesh. See PxDeformableVolumeExt::allocateAndInitializeHostMirror(),
	PxDeformableVolumeExt::copyToDevice().
	
	\return PxVec4* A pointer to a device buffer containing positions and inverse masses of
	the collision mesh.
	*/
	virtual 	PxVec4*						getPositionInvMassBufferD() = 0;

	/**
	\brief Gets a pointer to a device buffer containing rest positions of the collision mesh vertices.

	This function returns a pointer to device memory for the rest positions of the deformable volume collision
	mesh. This buffer is used to initialize the rest positions of the collision mesh vertices.

	\note It is mandatory to call PxDeformableVolume::markDirty() with PxDeformableVolumeDataFlag::eREST_POSITION when
	updating data in this buffer.

	The simulation expects 4 floats per vertex, aligned to a 16B boundary. The first 3 specify the 
	rest position. The last float is unused. The size of the buffer is the number of vertices in 
	the collision mesh * sizeof(PxVec4). \see PxTetrahedronMesh::getNbVertices().

	The device memory pointed to by this pointer is allocated when a shape is attached to the deformable volume.
	Calling PxDeformableVolume::detachShape() will deallocate the memory.

	It is not allowed to write data into this buffer from the start of PxScene::simulate() until
	PxScene::fetchResults() returns.

	It is the users' responsibility to initialize this buffer with the initial rest positions of the 
	vertices of the collision mesh. See PxDeformableVolumeExt::allocateAndInitializeHostMirror(),
	PxDeformableVolumeExt::copyToDevice().
	
	\return PxVec4* A pointer to a device buffer containing the rest positions of the collision mesh. 
	 */
	virtual 	PxVec4*						getRestPositionBufferD() = 0;

	/**
	\brief Gets a pointer to a device buffer containing the vertex positions of the simulation mesh.

	This function returns a pointer to device memory for the positions and inverse masses of the deformable volume
	simulation mesh. This buffer is used to both initialize/update the simulation mesh vertices
	of the deformable volume and read the simulation results.

	\note It is mandatory to call PxDeformableVolume::markDirty() with PxDeformableVolumeDataFlag::eSIM_POSITION_INVMASS when
	updating data in this buffer.

	The simulation expects 4 consecutive floats for each vertex, aligned to a 16B boundary. The 
	first 3 floats specify the positions and the last float specifies the inverse mass of the vertex.
	The size of the buffer is the number of vertices of the simulation mesh * sizeof(PxVec4).
	\see PxTetrahedronMesh::getNbVertices().

	The device memory pointed to by this pointer is allocated when a simulation mesh is attached to the 
	deformable volume. Calling PxDeformableVolume::detachSimulationMesh() will deallocate the memory.

	It is not allowed to write to this buffer from the start of the PxScene::simulate() call
	until PxScene::fetchResults() returns. Reading the data is allowed once all the PhysX tasks
	have finished, reading the data during a completion task is explicitly allowed. The 
	simulation will read and write directly from/into this buffer.

	It is the users' responsibility to initialize this buffer with the initial positions of 
	the vertices of the simulation mesh. See PxDeformableVolumeExt::allocateAndInitializeHostMirror(),
	PxDeformableVolumeExt::copyToDevice().
	
	\return PxVec4* A pointer to a device buffer containing the vertex positions of the simulation mesh.
	*/
	virtual 	PxVec4*						getSimPositionInvMassBufferD() = 0;

	/**
	\brief Gets a pointer to a device buffer containing the vertex velocities of the simulation mesh.

	This function returns a pointer to device memory for the velocities of the deformable volume simulation mesh 
	vertices. This buffer is used to both initialize/update the simulation mesh vertex velocities
	of the deformable volume and read the simulation results.

	\note It is mandatory to call PxDeformableVolume::markDirty() with PxDeformableVolumeDataFlag::eSIM_VELOCITY when
	updating data in this buffer.

	The simulation expects 4 consecutive floats for each vertex, aligned to a 16B boundary. The 
	first 3 specify the velocities for each vertex. The final float is unused. The size of the 
	buffer is the number of vertices of the simulation mesh * sizeof(PxVec4).
	\see PxTetrahedronMesh::getNbVertices().

	The device memory pointed to by this pointer is allocated when a simulation mesh is attached to the 
	deformable volume. Calling PxDeformableVolume::detachSimulationMesh() will deallocate the memory.

	It is not allowed to write to this buffer from the start of the PxScene::simulate() call
	until PxScene::fetchResults() returns. Reading the data is allowed once all the PhysX tasks
	have finished, reading the data during a completion task is explicitly allowed. The 
	simulation will read and write directly from/into this buffer.

	It is the users' responsibility to initialize this buffer with the initial velocities of 
	the vertices of the simulation mesh. See PxDeformableVolumeExt::allocateAndInitializeHostMirror(),
	PxDeformableVolumeExt::copyToDevice().
	
	\return PxVec4*  A pointer to a device buffer containing the vertex velocities of the simulation mesh.
	*/
	virtual		PxVec4*						getSimVelocityBufferD() = 0;

	/**
	\brief Marks per-vertex simulation state and configuration buffers dirty to signal to the simulation
	that changes have been made.

	Calling this function is mandatory to notify the simulation of changes made in the positionInvMass,
	simPositionInvMass, simVelocity and rest position buffers.

	This function can be called multiple times, and dirty flags are accumulated internally until 
	PxScene::simulate() is called.

	\see getPositionInvMassBufferD, getSimVelocityBufferD, getRestPositionBufferD, getSimPositionInvMassBufferD
	
	\param flags The buffers that have been updated.
	*/
	virtual		void						markDirty(PxDeformableVolumeDataFlags flags) = 0;

	/**
	\brief Sets the device buffer containing the kinematic targets for this deformable volume.

	This function sets the kinematic targets for a deformable volume to a user-provided device buffer. This buffer is
	read by the simulation to obtain the target position for each vertex of the simulation mesh.

	The simulation expects 4 consecutive float for each vertex, aligned to a 16B boundary. The first 3
	floats specify the target positions. The last float determines (together with the flag argument)
	if the target is active or not.
	For a deformable volume with the flag PxDeformableBodyFlag::eKINEMATIC raised, all target positions are considered
	valid. In case a deformable volume has the PxDeformableVolumeFlag::ePARTIALLY_KINEMATIC raised, only target
	positions whose corresponding last float has been set to 0.f are considered valid target positions.
	\see PxConfigureDeformableVolumeKinematicTarget
	Setting the kinematic targets has no effect if neither PxDeformableBodyFlag::eKINEMATIC nor
	PxDeformableVolumeFlag::ePARTIALLY_KINEMATIC is set.

	The size of the buffer is the number of vertices of the simulation mesh * sizeof(PxVec4).
	\see PxTetrahedronMesh::getNbVertices().

	It is the users responsibility to manage the memory pointed to by the input to this function,
	as well as guaranteeing the integrity of the input data. In particular, this means that it is
	not allowed to write this data from from the start of PxScene::simulate() until PxScene::fetchResults()
	returns. The memory is not allowed to be deallocated until PxScene::fetchResults() returns.

	Calling this function with a null pointer for the positions will clear the input and resume normal
	simulation. PxDeformableBodyFlag::eKINEMATIC or PxDeformableVolumeFlag::ePARTIALLY_KINEMATIC are ignored
	if no targets are set.

	This call is persistent across calls to PxScene::simulate(). Once this function is called, the
	simulation will look up the target positions from the same buffer for every call to PxScene::simulate().
	The user is allowed to update the target positions without calling this function again, provided that
	the synchronization requirements are adhered to (no changes between start of PxScene::simulate() until
	PxScene::fetchResults() returns).

	\param positions A pointer to a device buffer containing the kinematic targets for this deformable volume.
	 */
	virtual		void						setKinematicTargetBufferD(const PxVec4* positions) = 0;

	/**
	\brief Attaches a simulation mesh

	Attaches the simulation mesh (geometry) and a state containing inverse mass, rest pose
	etc. required to compute the deformation.

	\param[in] simulationMesh The tetrahedral mesh used to compute the deformable's deformation
	\param[in] deformableVolumeAuxData A state that contain a mapping from simulation to collision mesh, volume information etc.

	\return Returns true if the operation was successful
	\see detachSimulationMesh, PxDeformableBody.attachShape
	*/
	virtual		bool						attachSimulationMesh(PxTetrahedronMesh& simulationMesh, PxDeformableVolumeAuxData& deformableVolumeAuxData) = 0;

	/**
	\brief Detaches the simulation mesh

	Detaches the simulation mesh and simulation state used to compute the deformation.

	\see attachSimulationMesh, PxDeformableBody.detachShape
	*/
	virtual		void						detachSimulationMesh() = 0;

	/**
	\brief Retrieves the simulation mesh pointer.

	Allows to access the geometry of the tetrahedral mesh used to compute the object's deformation

	\return Pointer to the simulation mesh
	*/
	virtual		PxTetrahedronMesh*			getSimulationMesh() = 0;

	//! \brief Const version of getSimulationMesh()
	virtual		const PxTetrahedronMesh*	getSimulationMesh() const = 0;

	/**
	\brief Retrieve the collision mesh pointer.

	Allows to access the geometry of the tetrahedral mesh used to perform collision detection

	\return Pointer to the collision mesh
	*/
	virtual		PxTetrahedronMesh*			getCollisionMesh() = 0;

	//! \brief Const version of getCollisionMesh()
	virtual		const PxTetrahedronMesh*	getCollisionMesh() const = 0;

	/**
	\brief Retrieves the simulation state pointer.

	Allows to access the additional data of the simulation mesh (inverse mass, rest state etc.).
	The geometry part of the data is stored in the simulation mesh.

	\return Pointer to the simulation state
	*/
	virtual		PxDeformableVolumeAuxData*	getDeformableVolumeAuxData() = 0;

	//! \brief const version of getDeformableVolumeAuxData()
	virtual		const PxDeformableVolumeAuxData*
											getDeformableVolumeAuxData() const = 0;

	/**
	\brief Returns the GPU deformable volume index.

	\return The GPU index, or 0xFFFFFFFF if the deformable volume is not in a scene.
	*/
	virtual		PxU32						getGpuDeformableVolumeIndex() = 0;

	/**
	\brief Gets the concrete type name.
	\return The name of the concrete type.
	*/
	virtual		const char*					getConcreteTypeName() const PX_OVERRIDE	PX_FINAL { return "PxDeformableVolume"; }

protected:
	PX_INLINE					PxDeformableVolume(PxType concreteType, PxBaseFlags baseFlags) : PxDeformableBody(concreteType, baseFlags) {}
	PX_INLINE					PxDeformableVolume(PxBaseFlags baseFlags) : PxDeformableBody(baseFlags) {}
	virtual						~PxDeformableVolume() {}
	virtual		bool			isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxDeformableVolume", PxDeformableBody); }
};

/**
\brief Adjusts a deformable volume kinematic target such that it is properly set as active or inactive. Inactive targets will not affect vertex position, they are ignored by the solver.

\param[in] target The kinematic target
\param[in] isActive A boolean indicating if the returned target should be marked as active or not
\return The target with adjusted w component
*/
PX_CUDA_CALLABLE PX_FORCE_INLINE PxVec4 PxConfigureDeformableVolumeKinematicTarget(const PxVec4& target, bool isActive)
{
	PxVec4 result = target;
	if (isActive)
		result.w = 0.0f;
	else
	{
		//Any non-zero value will mark the target as inactive
		if (result.w == 0.0f)
			result.w = 1.0f;
	}
	return result;
}

/**
\brief Sets up a deformable volume kinematic target such that it is properly set as active or inactive. Inactive targets will not affect vertex position, they are ignored by the solver.

\param[in] target The kinematic target
\param[in] isActive A boolean indicating if the returned target should be marked as active or not
\return The target with configured w component
*/
PX_CUDA_CALLABLE PX_FORCE_INLINE PxVec4 PxConfigureDeformableVolumeKinematicTarget(const PxVec3& target, bool isActive)
{
	return PxConfigureDeformableVolumeKinematicTarget(PxVec4(target, 0.0f), isActive);
}

#if PX_VC
#pragma warning(pop)
#endif


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_DEFORMABLE_VOLUME_H

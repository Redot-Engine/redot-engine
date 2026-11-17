// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DIRECT_GPU_API_H
#define PX_DIRECT_GPU_API_H

#include "cudamanager/PxCudaTypes.h"
#include "foundation/PxVec4.h"
#include "foundation/PxSimpleTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief This flag specifies the type of data to get when calling PxDirectGPUAPI::getRigidDynamicData().
*/
class PxRigidDynamicGPUAPIReadType
{
public:
	enum Enum
	{
		eGLOBAL_POSE = 0,		//!< Get the global poses. Type: 1 PxTransform per PxRigidDynamic.
		eLINEAR_VELOCITY,		//!< Get the linear velocities. Type: 1 PxVec3 per PxRigidDynamic.
		eANGULAR_VELOCITY,		//!< Get the angular velocities. Type: 1 PxVec3 per PxRigidDynamic.
		// eLINEAR_ACCELERATION and eANGULAR_ACCELERATION are only available if PxSceneFlag::eENABLE_BODY_ACCELERATIONS is enabled.
		eLINEAR_ACCELERATION,	//!< Get the linear accelerations. Type: 1 PxVec3 per PxRigidDynamic.
		eANGULAR_ACCELERATION	//!< Get the angular accelerations. Type: 1 PxVec3 per PxRigidDynamic.
	};
};

/**
\brief This flag specifies the type of data to set when calling PxDirectGPUAPI::setRigidDynamicData().
*/
class PxRigidDynamicGPUAPIWriteType
{
public:
	enum Enum
	{
		eGLOBAL_POSE = 0,	//!< Set the global poses. Type: 1 PxTransform per PxRigidDynamic.
		eLINEAR_VELOCITY,	//!< Set the linear velocities. Type: 1 PxVec3 per PxRigidDynamic.
		eANGULAR_VELOCITY,	//!< Set the angular velocities. Type: 1 PxVec3 per PxRigidDynamic.
		eFORCE,				//!< Set the forces. Will be applied at the center of gravity of the bodies. Type: 1 PxVec3 per PxRigidDynamic.
		eTORQUE				//!< Set the torques. Will be applied at the center of gravity of the bodies. Type: 1 PxVec3 per PxRigidDynamic.
	};
};

/**
\brief This flag specifies the type of data to get when calling PxDirectGPUAPI::getArticulationData().
*/
class PxArticulationGPUAPIReadType
{
  public:
	enum Enum
	{
		eJOINT_POSITION = 0,		//!< The joint positions. 1 PxReal per dof. Block size per articulation: maxDofs.
		eJOINT_VELOCITY,			//!< The joint velocities. 1 PxReal per dof. Block size per articulation: maxDofs.
		eJOINT_ACCELERATION,		//!< The joint accelerations. 1 PxReal per dof. Block size per articulation: maxDofs.
		eJOINT_FORCE,				//!< The joint forces or torques applied using setArticulationData. 1 PxReal per dof. Block size per articulation: maxDofs. Not updated by the simulation, will return the values set by PxDirectGPUAPI::setArticulationData().
		eJOINT_TARGET_VELOCITY,		//!< The velocity targets applied using setArticulationData. 1 PxReal per dof. Block size per articulation: maxDofs. Not updated by the simulation, will return the values set by PxDirectGPUAPI::setArticulationData().
		eJOINT_TARGET_POSITION,		//!< The position targets applied using setArticulationData. 1 PxReal per dof. Block size per articulation: maxDofs. Not updated by the simulation, will return the values set by PxDirectGPUAPI::setArticulationData().
		eROOT_GLOBAL_POSE,			//!< The root link global pose. 1 PxTransform per articulation. Block size per articulation: 1.
		eROOT_LINEAR_VELOCITY,		//!< The root link linear velocity. 1 PxVec3 per articulation. Block size per articulation: 1.
		eROOT_ANGULAR_VELOCITY,		//!< The root link angular velocity. 1 PxVec3 per articulation. Block size per articulation: 1.
		eLINK_GLOBAL_POSE,			//!< The link global pose including root link. 1 PxTransform per link. Block size per articulation: maxLinks.
		eLINK_LINEAR_VELOCITY,		//!< The link linear velocities including root link. 1 PxVec3 per link. Block size per articulation: maxLinks.
		eLINK_ANGULAR_VELOCITY,		//!< The link angular velocities including root link. 1 PxVec3 per link. Block size per articulation: maxLinks.
		eLINK_LINEAR_ACCELERATION,	//!< The link linear accelerations including root link. 1 PxVec3 per link. Block size per articulation: maxLinks.
		eLINK_ANGULAR_ACCELERATION,	//!< The link angular accelerations including root link. 1 PxVec3 per link. Block size per articulation: maxLinks.
		eLINK_INCOMING_JOINT_FORCE,	//!< The link incoming joint forces including root link. The force is reported in the child joint frame of the link's incoming joint. 2 PxVec3 per link. The first PxVec3 contains the force, and the second PxVec3 contains the torque. Block size per articulation: maxLinks.
		eFIXED_TENDON,				//!< Fixed tendon data. 1 PxGpuFixedTendonData per fixed tendon. Block size per articulation: maxFixedTendons. Not updated by the simulation, will return the values set by PxDirectGPUAPI::setArticulationData().
		eFIXED_TENDON_JOINT,		//!< Fixed tendon joint data. 1 PxGpuTendonJointCoefficientData per fixed tendon joint. Block size per articulation: maxFixedTendons * maxFixedTendonJoints. Not updated by the simulation, will return the values set by PxDirectGPUAPI::setArticulationData().
		eSPATIAL_TENDON,			//!< Spatial tendon data. 1 PxGpuSpatialTendonData per spatial tendon. Block size per articulation: maxSpatialTendons. Not updated by the simulation, will return the values set by PxDirectGPUAPI::setArticulationData().
		eSPATIAL_TENDON_ATTACHMENT  //!< Spatial tendon attachment data. 1 PxGpuTendonAttachmentData per spatial tendon attachment. Block size per articulation: maxSpatialTendons * maxSpatialTendonAttachments. Not updated by the simulation, will return the values set by PxDirectGPUAPI::setArticulationData().
	};
};

/**
\brief This flag specifies the type of data to set when calling PxDirectGPUAPI::setArticulationData().
*/
class PxArticulationGPUAPIWriteType
{
public:
	enum Enum
	{
		eJOINT_POSITION = 0,		//!< The joint positions. 1 PxReal per dof. Block size per articulation: maxDofs.
		eJOINT_VELOCITY,			//!< The joint velocities. 1 PxReal per dof. Block size per articulation: maxDofs.
		eJOINT_FORCE,				//!< The applied joint forces or torques. 1 PxReal per dof. Block size per articulation: maxDofs.
		eJOINT_TARGET_VELOCITY,		//!< The velocity targets for the joint drives. 1 PxReal per dof. Block size per articulation: maxDofs.
		eJOINT_TARGET_POSITION,		//!< The position targets for the joint drives. 1 PxReal per dof. Block size per articulation: maxDofs.
		eROOT_GLOBAL_POSE,			//!< The root link transform. 1 PxTransform per articulation. Block size per articulation: 1.
		eROOT_LINEAR_VELOCITY,		//!< The root link linear velocity. 1 PxVec3 per articulation. Block size per articulation: 1.
		eROOT_ANGULAR_VELOCITY,		//!< The root link angular velocity. 1 PxVec3 per articulation. Block size per articulation: 1.
		eLINK_FORCE,				//!< The forces to apply to links. 1 PxVec3 per link. Block size per articulation: maxLinks.
		eLINK_TORQUE,				//!< The torques to apply to links. 1 PxVec3 per link. Block size per articulation: maxLinks.
		eFIXED_TENDON,				//!< Fixed tendon data. 1 PxGpuFixedTendonData per fixed tendon. Block size per articulation: maxFixedTendons.
		eFIXED_TENDON_JOINT,		//!< Fixed tendon joint data. 1 PxGpuTendonJointCoefficientData per fixed tendon joint. Block size per articulation: maxFixedTendons * maxFixedTendonJoints.
		eSPATIAL_TENDON,			//!< Spatial tendon data. 1 PxGpuSpatialTendonData per spatial tendon. Block size per articulation: maxSpatialTendons.
		eSPATIAL_TENDON_ATTACHMENT  //!< Spatial tendon attachment data. 1 PxGpuTendonAttachmentData per spatial tendon attachment. Block size per articulation: maxSpatialTendons * maxSpatialTendonAttachments.
	};
};

/**
\brief This flag specifies the type of operation to perform when calling PxDirectGPUAPI::computeArticulationData.
*/
class PxArticulationGPUAPIComputeType
{
public:
	enum Enum
	{
		eUPDATE_KINEMATIC = 0,					//!< Updates the link state for all the articulations specified in the index list. This operation can be performed
												//!< by the user to propagate changes made to root transform/root velocities/joint positions/joint velocities to
												//!< be reflected in the link transforms/velocities. Performing this operation will clear output values calculated by
												//!< the simulation, specifically link accelerations, link incoming joint forces, and joint accelerations. Note
												//!< that this is only necessary if the user wants to query link state, otherwise it will be performed automatically
												//!< at the start of the next call to simulate(). The data input parameter will be ignored and can be set to NULL for
												//!< this operation.
		eDENSE_JACOBIANS,						//!< Computes the dense Jacobian for the articulation in world space, including the dofs of a potentially floating base.
												//!< This is the batched, direct-GPU equivalent of PxArticulationReducedCoordinate::computeDenseJacobian. The output data
												//!< buffer is laid out into sequential blocks per articulation, where each block has the size
												//!< (6 + maxDofs) * (6 + (maxLinks - 1) * 6) * sizeof(float). maxLinks and maxDofs are the maximum link and dof counts
												//!< across all the articulations in the scene, and can be queried by calling PxDirectGPUAPI::getArticulationGPUAPIMaxCounts().
												//!< The size of the jacobian can vary by articulation, and will be determined using these formulas:
												//!< nCols = (fixedBase ? 0 : 6) + dofCount, nRows = (fixedBase ? 0 : 6) + (linkCount - 1) * 6. The matrix is indexed [nCols * row + column].
		eMASS_MATRICES,							//!< Computes the mass matrices that maps accelerations to forces: forces = M * accelerations on the GPU.
												//!< This is the batched, direct-GPU equivalent of PxArticulationReducedCoordinate::computeMassMatrix(). The output buffer is laid
												//!< out into sequential blocks per articulation, where each block has the size (maxDofs + 6) * (maxDofs + 6) * sizeof(float).
												//!< maxDofs is the maximum dof count across all the articulations in the scene, and can be queried by calling
												//!< PxDirectGPUAPI::getArticulationGPUAPIMaxCounts(), The size of the matrix can vary by articulation, and will be dofCount * dofCount
												//!< for fixed-base articulations and (dofCount + 6) * (dofCount + 6) for floating-base articulations.
												//!< We refer to the documentation of PxArticulationCache and PxArticulationReducedCoordinate::computeMassMatrix() for a more detailed explanation.
		eCORIOLIS_AND_CENTRIFUGAL_COMPENSATION,	//!< Computes the joint dof forces (and root force) required to counteract Coriolis and centrifugal forces for the given articulation pose.
												//!< This is the batched, direct-GPU equivalent to PxArticulationReducedCoordinate::computeCoriolisCompensation(). The output data
												//!< buffer is laid out into sequential blocks per articulation, where each block has the size (maxDofs + 6) * sizeof(float). maxDofs
												//!< is the maximum dof count across all the articulations in the scene, and can be queried by calling
												//!< PxDirectGPUAPI::getArticulationGPUAPIMaxCounts(). The size of the output can vary by articulation, and will be dofCount
												//!< for fixed-base articulations and (dofCount + 6) for floating-base articulations. We refer to the documentation of
												//!< PxArticulationCache and PxArticulationReducedCoordinate::computeCoriolisCompensation() for a more detailed explanation.
		eGRAVITY_COMPENSATION,					//!< Computes the forces required to counteract gravitational forces for the given articulation pose. This is the batched,
												//!< direct-GPU equivalent of PxArticulationReducedCoordinate::computeGravityCompensation(). The output data buffer is laid out
												//!< into sequential blocks per articulation, where each block has the size (maxDofs + 6) * sizeof(float). maxDofs
												//!< is the maximum dof count across all the articulations in the scene, and can be queried by calling
												//!< PxDirectGPUAPI::getArticulationGPUAPIMaxCounts(). The size of the output can vary by articulation, and will be dofCount
												//!< for fixed-base articulations and (dofCount + 6) for floating-base articulations. We refer to the documentation of
												//!< PxArticulationCache and PxArticulationReducedCoordinate::computeGravityCompensation() for a more detailed explanation.
		eARTICULATION_COMS_WORLD_FRAME,			//!< Computes the articulation's center of mass in the world frame for the given articulation pose.
												//!< This is the batched, direct-GPU equivalent to PxArticulationReducedCoordinate::computeArticulationCOM(). The output data
												//!< buffer is laid out into sequential blocks per articulation, where each block has the size sizeof(float) * 3.
        eARTICULATION_COMS_ROOT_FRAME,			//!< Computes the articulation's center of mass in the root frame for the given articulation pose.
												//!< This is the batched, direct-GPU equivalent to PxArticulationReducedCoordinate::computeArticulationCOM(). The output data
												//!< buffer is laid out into sequential blocks per articulation, where each block has the size sizeof(float) * 3.
		eCENTROIDAL_MOMENTUM_MATRICES			//!< Computes the centroidal momentum matrix and bias force for a floating-base articulation.
												//!< This is the batched, direct-GPU equivalent to PxArticulationReducedCoordinate::computeCentroidalMomentumMatrix(). The data buffer is laid
												//!< out into four main blocks. The two first blocks correspond to the input (mass matrix, Coriolis and Centrifugal compensation force),
												//!< and the two last blocks correspond to the output (centroidal momentum matrix, bias force). Each block must be organized into sequential
												//!< subblocks per articulation. The size of the subblock is (maxDofs + 6) * (maxDofs + 6) * sizeof(float) for the mass matrix,
												//!< (maxDofs + 6) * sizeof(float) for the Coriolis and Centrifugal compensation force, 6 * (maxDofs + 6) * sizeof(float) for the centroidal
												//!< momentum matrix, and 6 * sizeof(float) for the bias force. maxDofs is the maximum dof count across all the articulations in the scene,
												//!< and can be queried by calling PxDirectGPUAPI::getArticulationGPUAPIMaxCounts(). The size of the actual data in each subblock can vary by
												//!< articulation, and will depend on the value of dofCount. The dof indices will be according to the low-level indexing, we refer to
												//!< the documentation of PxArticulationCache for an explanation.
	};
};

/**
\brief Container to hold the results of PxDirectGPUAPI::getArticulationGPUAPIMaxCounts(). All the quantities are the maximum values
for the PxScene associated with this instance of PxDirectGPUAPI.
*/
struct PxArticulationGPUAPIMaxCounts
{
	PxU32 maxDofs;
	PxU32 maxLinks;
	PxU32 maxFixedTendons;
	PxU32 maxFixedTendonJoints;
	PxU32 maxSpatialTendons;
	PxU32 maxSpatialTendonAttachments;

	PxArticulationGPUAPIMaxCounts() :
		maxDofs(0),
		maxLinks(0),
		maxFixedTendons(0),
		maxFixedTendonJoints(0),
		maxSpatialTendons(0),
		maxSpatialTendonAttachments(0)
	{	}
};

/**
\brief This flag specifies the type of data to get when calling #PxDirectGPUAPI::getD6JointData().
*/
class PxD6JointGPUAPIReadType
{
public:
	enum Enum
	{
		/**
		\brief The joint forces applied by the solver.

		The forces are in world space. 1 PxVec3 per joint.
		
		\note Replaces calls to PxConstraint::getForce() which will not work properly anymore if direct GPU API is used.
		*/
		eJOINT_FORCE,

		/**
		\brief The joint torques applied by the solver.

		The torques are in world space. 1 PxVec3 per joint.
		
		\note Replaces calls to PxConstraint::getForce() which will not work properly anymore if direct GPU API is used.
		*/
		eJOINT_TORQUE
	};
};

/**
\brief PxDirectGPUAPI exposes an API that enables batched direct access to GPU data for a PxScene.

The functions in this class allow batched direct access to GPU data for PxRigidDynamic, PxArticulationReducedCoordinate, 
PxShape and PxD6Joint types. This allows interoperation with GPU post- and preprocessing for users and allows the user 
to implement more efficient CPU-GPU data copies based on the specific needs of the application.

Using this direct-API will disable the existing CPU-based API for all the data exposed in the direct-API. For any API function
that does not have a counterpart in this direct-API, the existing API will continue to work.

To use this API, PxSceneFlag::eENABLE_DIRECT_GPU_API needs to be raised, in combination with PxSceneFlag::eENABLE_GPU_DYNAMICS
and PxBroadphaseType::eGPU. Note that these options are immutable and cannot be changed after the scene has been created.

Due to the internal architecture of the GPU-accelerated parts of PhysX, using this API comes with caveats:

1) All GPU-CPU copies for data exposed in this API will be disabled. This means that the existing CPU-based API will 
return outdated data, and any setters for data exposed in the interface will not work. On the other hand, significant
speedups can be achieved because of the reduced amount of GPU-CPU memory copies.

2) Due to the internal architecture of the GPU-accelerated PhysX, this API will only work after a first simulation step has been
taken. The reason for this is that the PxScene first needs to know all the actors it will have to simulate, and setup the 
sizes of the GPU-based structures. For setup, the existing CPU API should be used.

\note Due to the fact that this API is exposing low-level data, we do reserve the right to change this API without deprecation
in case of changes in the internal implementations.
*/

class PxDirectGPUAPI
{
protected:
				PxDirectGPUAPI()  {}
	virtual		~PxDirectGPUAPI() {}

public:

	/**
	\brief Copies the simulation state for a set of PxRigidDynamic actors into a user-provided GPU data buffer.

	\param[out] data User-provided GPU data buffer which has size nbElements * sizeof(type). For the types, see the dataType options in PxRigidDynamicGPUAPIReadType.
	\param[in] gpuIndices User-provided GPU index buffer containing elements of PxRigidDynamicGPUIndex. This buffer contains the GPU indices of the PxRigidDynamic objects that are part of this get operation. See #PxRigidDynamic::getGPUIndex(). The size of this buffer needs to be nbElements * sizeof(PxRigidDynamicGPUIndex). The data requested for the PxRigidDynamic with its GPU index at position x in the gpuIndices array will be located at position x in the data array.
	\param[in] dataType The type of data to get. See #PxRigidDynamicGPUAPIReadType.
	\param[in] nbElements The number of rigid bodies to be copied.
	\param[in] startEvent User-provided CUDA event that is awaited at the start of this function. Defaults to NULL which means the function will dispatch the copy immediately.
	\param[in] finishEvent User-provided CUDA event that is recorded at the end of this function. Defaults to NULL which means the function will wait for the copy to finish before returning.

	\return bool Whether the operation was successful. Note that this might not include asynchronous CUDA errors.
	
	*/
	virtual bool getRigidDynamicData(void* data, const PxRigidDynamicGPUIndex* gpuIndices, PxRigidDynamicGPUAPIReadType::Enum dataType, PxU32 nbElements, CUevent startEvent = NULL, CUevent finishEvent = NULL) const = 0;
	
	/**
	\brief Sets the simulation state for a set of PxRigidDynamic actors from a user-provided GPU data buffer.

	\param[in] data User-provided GPU data buffer which has size nbElements * sizeof(type). For the types, see the dataType options in PxRigidDynamicGPUAPIWriteType.
	\param[in] gpuIndices User-provided GPU index buffer containing elements of PxRigidDynamicGPUIndex. This buffer contains the GPU indices of the PxRigidDynamic objects that are part of this set operation. See #PxRigidDynamic::getGPUIndex(). The size of this buffer needs to be nbElements * sizeof(PxRigidDynamicGPUIndex). The data for the PxRigidDynamic with its GPU index at position x in the gpuIndices array needs to be located at position x in the data array.
	\param[in] dataType The type of data to set. See #PxRigidDynamicGPUAPIWriteType.
	\param[in] nbElements The number of rigid bodies to be set.
	\param[in] startEvent User-provided CUDA event that is awaited at the start of this function. Defaults to NULL which means the function will dispatch the copy immediately.
	\param[in] finishEvent User-provided CUDA event that is recorded at the end of this function. Defaults to NULL which means the function will wait for the copy to finish before returning.

	\return bool Whether the operation was successful. Note that this might not include asynchronous CUDA errors.
	*/
	virtual bool setRigidDynamicData(const void* data, const PxRigidDynamicGPUIndex* gpuIndices, PxRigidDynamicGPUAPIWriteType::Enum dataType, PxU32 nbElements, CUevent startEvent = NULL, CUevent finishEvent = NULL) = 0;

	/**
	\brief Gets the simulation state for a set of articulations, i.e. PxArticulationReducedCoordinate objects and copies into a user-provided GPU data buffer.

	\param[out] data User-provided GPU data buffer that is appropriately sized for the data being requested. The sizing is explained in detail below.
	\param[in] gpuIndices User-provided GPU index buffer containing elements of PxArticulationGPUIndex. This buffer contains the GPU indices of the PxArticulationReducedCoordinate objects that are part of this get operation. See #PxArticulationReducedCoordinate::getGPUIndex(). The size of this buffer needs to be nbElements * sizeof(PxArticulationGPUIndex). The data for the PxArticulationReducedCoordinate with its GPU index at position x in the gpuIndices array will have its data block located at position x in the data array.
	\param[in] dataType The type of data to get. See #PxArticulationGPUAPIReadType.
	\param[in] nbElements The number of articulations to copy data from.
	\param[in] startEvent User-provided CUDA event that is awaited at the start of this function. Defaults to NULL which means the function will dispatch the copy immediately.
	\param[in] finishEvent User-provided CUDA event that is recorded at the end of this function. Defaults to NULL which means the function will wait for the copy to finish before returning.

	\return bool Whether the operation was successful. Note that this might not include asynchronous CUDA errors.

	The data buffer must be sized according to the maximum component counts across all articulations in the PxScene, as summarised in PxArticulationGPUAPIMaxCounts. The data buffer is split into sequential
	blocks that are of equal size and can hold the data for all components of an articulation. For example, for a link-centric data type (PxArticulationGPUAPIReadType::eLINK_GLOBAL_POSE, for example)
	each of these blocks has to be maxLinks * sizeof(dataType). The size of the complete buffer would then be nbElements * maxLinks * sizeof(dataType). For a dof-centric data type, 
	the block size would be maxDofs * sizeof(dataType). The specific layout for each dataType is detailed in the API documentation of PxArticulationGPUAPIReadType.
	The max counts for a scene can be obtained by calling PxDirectGPUAPI::getArticulationGPUAPIMaxCounts().

	The link and dof indexing of these blocks then follows the same pattern as the PxArticulationCache API. We refer to the user guide for an explanation.
	*/
	virtual bool getArticulationData(void* data, const PxArticulationGPUIndex* gpuIndices, PxArticulationGPUAPIReadType::Enum dataType, PxU32 nbElements, CUevent startEvent = NULL, CUevent finishEvent = NULL) const = 0;
	
	/**
	\brief Sets the simulation state for a set of articulations, i.e. PxArticulationReducedCoordinate objects from a user-provided GPU data buffer.

	\param[in] data User-provided GPU data buffer that is appropriately sized for the data to be set. The sizing is explained in detail below.
	\param[in] gpuIndices User-provided GPU index buffer containing elements of PxArticulationGPUIndex. This buffer contains the GPU indices of the PxArticulationReducedCoordinate objects that are part of this set operation. See #PxArticulationReducedCoordinate::getGPUIndex(). The size of this buffer needs to be nbElements * sizeof(PxArticulationGPUIndex). The data for the PxArticulationReducedCoordinate with its GPU index at position x in the gpuIndices array needs to have its data block located at position x in the data array.
	\param[in] dataType The type of data to set. See #PxArticulationGPUAPIWriteType.
	\param[in] nbElements The number of articulations to set data for.
	\param[in] startEvent User-provided CUDA event that is awaited at the start of this function. Defaults to NULL which means the function will dispatch the copy immediately.
	\param[in] finishEvent User-provided CUDA event that is recorded at the end of this function. Defaults to NULL which means the function will wait for the copy to finish before returning.

	\return bool Whether the operation was successful. Note that this might not include asynchronous CUDA errors.

	The data buffer must be sized according to the maximum component counts across all articulations in the PxScene, as summarised in PxArticulationGPUAPIMaxCounts. The data buffer is split into sequential
	blocks that are of equal size and can hold the data for all components of an articulation. For example, for a link-centric data type (PxArticulationGPUAPIWriteType::eLINK_FORCE, for example)
	each of these blocks has to be maxLinks * sizeof(dataType). The size of the complete buffer would then be nbElements * maxLinks * sizeof(dataType). For a dof-centric data type, 
	the block size would be maxDofs * sizeof(dataType). The specific layout for each dataType is detailed in the API documentation of PxArticulationGPUAPIWriteType.
	The max counts for a scene can be obtained by calling PxDirectGPUAPI::getArticulationGPUAPIMaxCounts().

	The internal indexing of these blocks then follows the same pattern as the PxArticulationCache API. We refer to the user guide for an explanation.
	*/
	virtual bool setArticulationData(const void* data, const PxArticulationGPUIndex* gpuIndices, PxArticulationGPUAPIWriteType::Enum dataType, PxU32 nbElements, CUevent startEvent = NULL, CUevent finishEvent = NULL) = 0;
	
	/**
	\brief performs a compute operation on a set of articulations, i.e. PxArticulationReducedCoordinate objects.

	\param[in,out] data User-provided GPU data buffer that is appropriately sized for the operation to be performed. Depending on the operation, can be input or output data.
	\param[in] gpuIndices User-provided GPU index buffer containing elements of PxArticulationGPUIndex. This buffer contains the GPU indices of the PxArticulationReducedCoordinate objects that are part of this compute operation. See #PxArticulationReducedCoordinate::getGPUIndex(). The size of this buffer needs to be nbElements * sizeof(PxArticulationGPUIndex).
	\param[in] operation The operation to perform. See PxArticulationGPUAPIComputeType::Enum.
	\param[in] nbElements The number of articulations to perform this compute operation on.
	\param[in] startEvent User-provided CUDA event that is awaited at the start of this function. Defaults to NULL which means the function will dispatch the computation immediately.
	\param[in] finishEvent User-provided CUDA event that is recorded at the end of this function. Defaults to NULL which means the function will wait for the computation to finish before returning.

	\return bool Whether the operation was successful. Note that this might not include asynchronous CUDA errors.

	The appropriate sizing of the data buffer as well as the data layout is documented alongside the compute operations in the API documentation of PxArticulationGPUAPIComputeType.
	*/
	virtual bool computeArticulationData(void* data, const PxArticulationGPUIndex* gpuIndices, PxArticulationGPUAPIComputeType::Enum operation, PxU32 nbElements, CUevent startEvent = NULL, CUevent finishEvent = NULL) = 0;

	/**
	\brief Copy rigid body (PxRigidBody) and articulation (PxArticulationReducedCoordinate) contact data to a user-provided GPU data buffer.

	\note This function only reports contact data for actor pairs where both actors are either rigid bodies or articulations.
	\note The contact data contains pointers to internal state and is only valid until the next call to simulate().

	\param[out] data User-provided GPU data buffer, which should be the size of PxGpuContactPair * maxPairs
	\param[out] nbContactPairs User-provided GPU data buffer of 1 * sizeof(PxU32) that contains the actual number of pairs that was written.
	\param[in] maxPairs  The maximum number of pairs that the buffer can contain.
	\param[in] startEvent User-provided CUDA event that is awaited at the start of this function. Defaults to NULL which means the function will dispatch the copy immediately.
	\param[in] finishEvent User-provided CUDA event that is recorded at the end of this function. Defaults to NULL which means the function will wait for the copy to finish before returning.

	\return bool Whether the operation was successful. Note that this might not include asynchronous CUDA errors.
	*/
	virtual bool copyContactData(void* data, PxU32* nbContactPairs, PxU32 maxPairs, CUevent startEvent = NULL, CUevent finishEvent = NULL) const = 0;
	
	/**
	\brief Evaluate sample point distances and gradients on SDF shapes in local space. Local space is the space in which the mesh's raw vertex positions are represented.
	\param[out] localGradientAndSignedDistanceConcatenated User-provided GPU buffer where the evaluated gradients and distances in SDF local space get stored. It has the same structure as localSamplePointsConcatenated. The PxVec4 elements contain the gradient and the distance (gradX, gradY, gradZ, distance).
	\param[in] shapeIndices User-provided GPU index buffer containing elements of PxShapeGPUIndex. This buffer contains the GPU indices of the PxShape objects that are part of this operation. See #PxShape::getGPUIndex(). The size of this buffer (in bytes) needs to be nbElements * sizeof(PxShapeGPUIndex). The shapes must be triangle mesh shapes with SDFs.
	\param[in] localSamplePointsConcatenated User-provided GPU buffer containing the sample point locations for every shape in the shapes' local space. The buffer stride is maxPointCount.
	\param[in] samplePointCountPerShape User-provided GPU buffer containing the number of sample points for every shape.
	\param[in] nbElements The number of shapes to be queried.
	\param[in] maxPointCount The maximum value in the array samplePointCountPerShape. Note that the arrays localGradientAndSignedDistanceConcatenated and localSamplePointsConcatenated must have the size (in bytes) nbElements * maxPointCount * sizeof(PxVec4).
	\param[in] startEvent User-provided CUDA event that is awaited at the start of this function. Defaults to NULL which means the function will dispatch the computation immediately.
	\param[in] finishEvent User-provided CUDA event that is recorded at the end of this function. Defaults to NULL which means the function will wait for the computation to finish before returning.

	Example: Ten shapes are part of the simulation. Three of them have an SDF (shapeIndices of the SDF meshes are 2, 4 and 6). For the first shape, the SDF distance of 10 sample points should be queried. 20 sample 
	points for the second mesh and 30 sample points for the third mesh. The slice size (=maxPointCount) is the maximum of sample points required for any shape participating in the query, 30 = max(10, 20, 30) for this example.
	The buffers required for the method evaluateSDFDistances are constructed as follows (not including optional parameters):
		* localGradientAndSignedDistanceConcatenated[length: 3 * 30]: 
			* No initialization needed. It will hold the result after the finishEvent occurred. It has the same structure as localSamplePointsConcatenated, see below.
			* The format of the written PxVec4 is as follows (gradX, gradY, gradZ, sdfDistance)
		* shapeIndices[length: 3]
			* The content is {2, 4, 6} which are the shape indices for this example
		* localSamplePointsConcatenated[length: 3 * 30]:			
			* Slice 0...29 has only the first 10 elements set to local sample points (w component is unused) with respect to the coordinate frame of the first shape to be queried
			* Slice 30...59 has only the first 20 elements set to local sample points (w component is unused) with respect to the coordinate frame of the second shape to be queried
			* Slice 60...89 has all 30 elements set to local sample points (w component is unused) with respect to the coordinate frame of the third shape to be queried
		* samplePointCountPerShape[length: 3]
			* The content is {10, 20, 30} which are the number of samples to evaluate per shape used in this example. Note that the slice size (=maxPointCount) is the maximum value in this list.
		* nbElements: 3 for this example since 3 shapes are participating in the query
		* maxPointCount: 30 for this example since 30 is the slice size (= maxPointCount = 30 = max(10, 20, 30))

	\return bool Whether the operation was successful. Note that this might not include asynchronous CUDA errors.
	*/
	virtual bool evaluateSDFDistances(PxVec4* localGradientAndSignedDistanceConcatenated, const PxShapeGPUIndex* shapeIndices, const PxVec4* localSamplePointsConcatenated, const PxU32* samplePointCountPerShape, PxU32 nbElements, PxU32 maxPointCount, CUevent startEvent = NULL, CUevent finishEvent = NULL) const = 0;

	/**
	\brief Get the maximal articulation index and component counts for a PxScene.

	Get the maximal articulation index and component counts for a PxScene. This is a helper function to ease the derivation of the correct data layout
	for the articulation functions in PxDirectGPUAPI. Specifically, this function will return maxLinks, maxDofs, maxFixedTendons, maxFixedTendonJoints,
	maxSpatialTendons and maxSpatialTendonAttachments for a scene. See #PxArticulationGPUAPIMaxCounts.

	\see PxDirectGPUAPI::getArticulationData, PxDirectGPUAPI::setArticulationData, PxDirectGPUAPI::computeArticulationData

	\return PxArticulationGPUAPIMaxCounts the max counts across the scene for all articulation indices and components.
	*/
	virtual PxArticulationGPUAPIMaxCounts getArticulationGPUAPIMaxCounts()	const = 0;

	/**
	\brief Copies the simulation state for a set of PxD6Joint instances into a user-provided GPU data buffer.

	\param[out] data User-provided GPU data buffer which has size nbElements * sizeof(type). For the types, see the options in #PxD6JointGPUAPIReadType.
	\param[in] gpuIndices User-provided GPU index buffer containing elements of PxD6JointGPUIndex. This buffer contains the GPU indices of the PxD6Joint objects that are part of this get operation
	           (see #PxD6Joint::getGPUIndex()). The size of this buffer needs to be nbElements * sizeof(PxD6JointGPUIndex). The data requested for the PxD6Joint with its GPU index at position x in the
			   gpuIndices array will be located at position x in the data array.
	\param[in] dataType The type of data to get (see #PxD6JointGPUAPIReadType).
	\param[in] nbElements The number of provided GPU indices and as such the number of data entries that will be written to the provided GPU data buffer.
	\param[in] startEvent User-provided CUDA event that is awaited at the start of this function. Defaults to NULL which means the function will dispatch the copy immediately.
	\param[in] finishEvent User-provided CUDA event that is recorded at the end of this function. Defaults to NULL which means the function will wait for the copy to finish before returning.

	\return bool Whether the operation was successful. Note that this might not include asynchronous CUDA errors.
	
	*/
	virtual bool getD6JointData(void* data, const PxD6JointGPUIndex* gpuIndices, PxD6JointGPUAPIReadType::Enum dataType, PxU32 nbElements, CUevent startEvent = NULL, CUevent finishEvent = NULL) const = 0;
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

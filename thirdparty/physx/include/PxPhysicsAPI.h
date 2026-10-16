// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PHYSICS_API_H
#define PX_PHYSICS_API_H

/**
This is the main include header for the Physics SDK, for users who
want to use a single #include file.

Alternatively, one can instead directly #include a subset of the below files.
*/

// Foundation SDK 
#include "foundation/PxAlignedMalloc.h"
#include "foundation/PxAlloca.h"
#include "foundation/PxAllocatorCallback.h"
#include "foundation/PxArray.h"
#include "foundation/PxAssert.h"
#include "foundation/PxAtomic.h"
#include "foundation/PxBasicTemplates.h"
#include "foundation/PxBitAndData.h"
#include "foundation/PxBitMap.h"
#include "foundation/PxBitUtils.h"
#include "foundation/PxBounds3.h"
#include "foundation/PxBroadcast.h"
#include "foundation/PxConstructor.h"
#include "foundation/PxErrorCallback.h"
#include "foundation/PxErrors.h"
#include "foundation/PxFlags.h"
#include "foundation/PxFoundation.h"
#include "foundation/PxFoundationConfig.h"
#include "foundation/PxFPU.h"
#include "foundation/PxHash.h"
#include "foundation/PxHashMap.h"
#include "foundation/PxHashSet.h"
#include "foundation/PxInlineAllocator.h"
#include "foundation/PxInlineArray.h"
#include "foundation/PxIntrinsics.h"
#include "foundation/PxIO.h"
#include "foundation/PxMat33.h"
#include "foundation/PxMat44.h"
#include "foundation/PxMath.h"
#include "foundation/PxMathIntrinsics.h"
#include "foundation/PxMathUtils.h"
#include "foundation/PxMemory.h"
#include "foundation/PxMutex.h"
#include "foundation/PxPhysicsVersion.h"
#include "foundation/PxPlane.h"
#include "foundation/PxPool.h"
#include "foundation/PxPreprocessor.h"
#include "foundation/PxProfiler.h"
#include "foundation/PxQuat.h"
#include "foundation/PxSimpleTypes.h"
#include "foundation/PxSList.h"
#include "foundation/PxSocket.h"
#include "foundation/PxSort.h"
#include "foundation/PxStrideIterator.h"
#include "foundation/PxString.h"
#include "foundation/PxSync.h"
#include "foundation/PxTempAllocator.h"
#include "foundation/PxThread.h"
#include "foundation/PxTime.h"
#include "foundation/PxTransform.h"
#include "foundation/PxUnionCast.h"
#include "foundation/PxUserAllocated.h"
#include "foundation/PxUtilities.h"
#include "foundation/PxVec2.h"
#include "foundation/PxVec3.h"
#include "foundation/PxVec4.h"
#include "foundation/PxVecMath.h"
#include "foundation/PxVecQuat.h"
#include "foundation/PxVecTransform.h"


//Not physics specific utilities and common code
#include "common/PxCoreUtilityTypes.h"
#include "common/PxPhysXCommonConfig.h"
#include "common/PxRenderBuffer.h"
#include "common/PxBase.h"
#include "common/PxTolerancesScale.h"
#include "common/PxTypeInfo.h"
#include "common/PxStringTable.h"
#include "common/PxSerializer.h"
#include "common/PxSerialFramework.h"
#include "common/PxInsertionCallback.h"

//Task Manager
#include "task/PxTask.h"

// Cuda Mananger
#if PX_SUPPORT_GPU_PHYSX
#include "gpu/PxGpu.h"
#endif

//Geometry Library
#include "geometry/PxBoxGeometry.h"
#include "geometry/PxBVH.h"
#include "geometry/PxBVHBuildStrategy.h"
#include "geometry/PxCapsuleGeometry.h"
#include "geometry/PxConvexMesh.h"
#include "geometry/PxConvexMeshGeometry.h"
#include "geometry/PxGeometry.h"
#include "geometry/PxGeometryHelpers.h"
#include "geometry/PxGeometryQuery.h"
#include "geometry/PxHeightField.h"
#include "geometry/PxHeightFieldDesc.h"
#include "geometry/PxHeightFieldFlag.h"
#include "geometry/PxHeightFieldGeometry.h"
#include "geometry/PxHeightFieldSample.h"
#include "geometry/PxMeshQuery.h"
#include "geometry/PxMeshScale.h"
#include "geometry/PxPlaneGeometry.h"
#include "geometry/PxSimpleTriangleMesh.h"
#include "geometry/PxSphereGeometry.h"
#include "geometry/PxTriangle.h"
#include "geometry/PxTriangleMesh.h"
#include "geometry/PxTriangleMeshGeometry.h"
#include "geometry/PxTetrahedron.h"
#include "geometry/PxTetrahedronMesh.h"
#include "geometry/PxTetrahedronMeshGeometry.h"

// PhysX Core SDK
#include "PxActor.h"
#include "PxAggregate.h"
#include "PxArticulationReducedCoordinate.h"
#include "PxArticulationJointReducedCoordinate.h"
#include "PxArticulationLink.h"
#include "PxClient.h"
#include "PxConstraint.h"
#include "PxConstraintDesc.h"
#include "PxContact.h"
#include "PxContactModifyCallback.h"
#include "PxDeformableSurface.h"
#include "PxDeformableSurfaceMaterial.h"
#include "PxDeformableVolume.h"
#include "PxDeformableVolumeMaterial.h"
#include "PxDeletionListener.h"
#include "PxFiltering.h"
#include "PxMaterial.h"
#include "PxParticleBuffer.h"
#include "PxParticleSystem.h"
#include "PxPBDParticleSystem.h"
#include "PxPBDMaterial.h"
#include "PxPhysics.h"
#include "PxPhysXConfig.h"
#include "PxQueryFiltering.h"
#include "PxQueryReport.h"
#include "PxRigidActor.h"
#include "PxRigidBody.h"
#include "PxRigidDynamic.h"
#include "PxRigidStatic.h"
#include "PxScene.h"
#include "PxSceneDesc.h"
#include "PxSceneLock.h"
#include "PxShape.h"
#include "PxSimulationEventCallback.h"
#include "PxSimulationStatistics.h"
#include "PxVisualizationParameter.h"
#include "PxPruningStructure.h"

//Character Controller
#include "characterkinematic/PxBoxController.h"
#include "characterkinematic/PxCapsuleController.h"
#include "characterkinematic/PxController.h"
#include "characterkinematic/PxControllerBehavior.h"
#include "characterkinematic/PxControllerManager.h"
#include "characterkinematic/PxControllerObstacles.h"
#include "characterkinematic/PxExtended.h"

//Cooking (data preprocessing)
#include "cooking/Pxc.h"
#include "cooking/PxConvexMeshDesc.h"
#include "cooking/PxCooking.h"
#include "cooking/PxTriangleMeshDesc.h"
#include "cooking/PxBVH33MidphaseDesc.h"
#include "cooking/PxBVH34MidphaseDesc.h"
#include "cooking/PxMidphaseDesc.h"

//Extensions to the SDK
#include "extensions/PxDefaultStreams.h"
#include "extensions/PxExtensionsAPI.h"

//Serialization
#include "extensions/PxSerialization.h"
#include "extensions/PxRepXSerializer.h"

//Vehicle Simulation
#include "vehicle/PxVehicleAPI.h"

//Connecting the SDK to Visual Debugger
#include "pvd/PxPvdSceneClient.h"
#include "pvd/PxPvd.h"
#include "pvd/PxPvdTransport.h"
#endif

// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_EXTENSIONS_API_H
#define PX_EXTENSIONS_API_H

#include "foundation/PxErrorCallback.h"
#include "extensions/PxDefaultAllocator.h"
#include "extensions/PxConstraintExt.h"
#include "extensions/PxDistanceJoint.h"
#include "extensions/PxFixedJoint.h"
#include "extensions/PxPrismaticJoint.h"
#include "extensions/PxRevoluteJoint.h"
#include "extensions/PxSphericalJoint.h"
#include "extensions/PxD6Joint.h"
#include "extensions/PxGearJoint.h"
#include "extensions/PxRackAndPinionJoint.h"
#include "extensions/PxDefaultSimulationFilterShader.h"
#include "extensions/PxDefaultErrorCallback.h"
#include "extensions/PxDefaultStreams.h"
#include "extensions/PxRigidActorExt.h"
#include "extensions/PxRigidBodyExt.h"
#include "extensions/PxShapeExt.h"
#include "extensions/PxTriangleMeshExt.h"
#include "extensions/PxSerialization.h"
#include "extensions/PxDefaultCpuDispatcher.h"
#include "extensions/PxSmoothNormals.h"
#include "extensions/PxSimpleFactory.h"
#include "extensions/PxStringTableExt.h"
#include "extensions/PxBroadPhaseExt.h"
#include "extensions/PxMassProperties.h"
#include "extensions/PxSceneQueryExt.h"
#include "extensions/PxSceneQuerySystemExt.h"
#include "extensions/PxCustomSceneQuerySystem.h"
#include "extensions/PxConvexMeshExt.h"
#include "extensions/PxSamplingExt.h"
#include "extensions/PxTetrahedronMeshExt.h"
#include "extensions/PxCustomGeometryExt.h"
#include "extensions/PxDeformableSurfaceExt.h"

/** \brief Initialize the PhysXExtensions library. 

This should be called before calling any functions or methods in extensions which may require allocation. 
\note This function does not need to be called before creating a PxDefaultAllocator object.

\param physics a PxPhysics object
\param pvd an PxPvd (PhysX Visual Debugger) object

\see PxCloseExtensions PxFoundation PxPhysics
*/
PX_C_EXPORT bool PX_CALL_CONV PxInitExtensions(physx::PxPhysics& physics, physx::PxPvd* pvd);

/** \brief Shut down the PhysXExtensions library. 

This function should be called to cleanly shut down the PhysXExtensions library before application exit. 

\note This function is required to be called to release foundation usage.

\see PxInitExtensions
*/
PX_C_EXPORT void PX_CALL_CONV PxCloseExtensions();

#endif


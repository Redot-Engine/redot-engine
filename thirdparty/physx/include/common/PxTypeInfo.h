// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TYPE_INFO_H
#define PX_TYPE_INFO_H


#include "common/PxPhysXCommonConfig.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief an enumeration of concrete classes inheriting from PxBase

Enumeration space is reserved for future PhysX core types, PhysXExtensions, 
PhysXVehicle and Custom application types.

\see PxBase, PxTypeInfo
*/

struct PxConcreteType
{
	enum Enum
	{
		eUNDEFINED,

		eHEIGHTFIELD,
		eCONVEX_MESH,
		eTRIANGLE_MESH_BVH33 PX_DEPRECATED,	//!< \deprecated Will be removed together with deprecated BVH33.
		eTRIANGLE_MESH_BVH34,
		eTETRAHEDRON_MESH,
		eDEFORMABLE_VOLUME_MESH,

		eRIGID_DYNAMIC,
		eRIGID_STATIC,
		eSHAPE,
		eMATERIAL,
		eDEFORMABLE_SURFACE_MATERIAL,
		eDEFORMABLE_VOLUME_MATERIAL,
		ePBD_MATERIAL,
		eCONSTRAINT,
		eAGGREGATE,
		eARTICULATION_REDUCED_COORDINATE,
		eARTICULATION_LINK,
		eARTICULATION_JOINT_REDUCED_COORDINATE,
		eARTICULATION_SPATIAL_TENDON,
		eARTICULATION_FIXED_TENDON,
		eARTICULATION_ATTACHMENT,
		eARTICULATION_TENDON_JOINT,
		eARTICULATION_MIMIC_JOINT,
		ePRUNING_STRUCTURE,
		eBVH,
		eDEFORMABLE_VOLUME,
		eDEFORMABLE_VOLUME_STATE,
		ePBD_PARTICLESYSTEM,
		eDEFORMABLE_SURFACE,
		eDEFORMABLE_ATTACHMENT,
		eDEFORMABLE_ELEMENT_FILTER,
		ePARTICLE_BUFFER,
		ePARTICLE_DIFFUSE_BUFFER,
		
		ePHYSX_CORE_COUNT,
        eFIRST_PHYSX_EXTENSION = 256,
		eFIRST_VEHICLE_EXTENSION = 512,
        eFIRST_USER_EXTENSION = 1024
	};
};

/** 
\brief a structure containing per-type information for types inheriting from PxBase

\see PxBase, PxConcreteType
*/

template<typename T> struct PxTypeInfo {};

#define PX_DEFINE_TYPEINFO(_name, _fastType) \
	class _name; \
	template <> struct PxTypeInfo<_name>	{	static const char* name() { return #_name;	}	enum { eFastTypeId = _fastType };	};

/* the semantics of the fastType are as follows: an object A can be cast to a type B if B's fastType is defined, and A has the same fastType.
 * This implies that B has no concrete subclasses or superclasses.
 */

PX_DEFINE_TYPEINFO(PxBase,									PxConcreteType::eUNDEFINED)
PX_DEFINE_TYPEINFO(PxMaterial,								PxConcreteType::eMATERIAL)
PX_DEFINE_TYPEINFO(PxDeformableSurfaceMaterial,				PxConcreteType::eDEFORMABLE_SURFACE_MATERIAL)
PX_DEFINE_TYPEINFO(PxDeformableVolumeMaterial,				PxConcreteType::eDEFORMABLE_VOLUME_MATERIAL)
PX_DEFINE_TYPEINFO(PxPBDMaterial,							PxConcreteType::ePBD_MATERIAL)
PX_DEFINE_TYPEINFO(PxConvexMesh,							PxConcreteType::eCONVEX_MESH)
PX_DEFINE_TYPEINFO(PxTriangleMesh,							PxConcreteType::eUNDEFINED)
PX_DEFINE_TYPEINFO(PxBVH33TriangleMesh,						PxConcreteType::eTRIANGLE_MESH_BVH33)
PX_DEFINE_TYPEINFO(PxBVH34TriangleMesh,						PxConcreteType::eTRIANGLE_MESH_BVH34)
PX_DEFINE_TYPEINFO(PxTetrahedronMesh,						PxConcreteType::eTETRAHEDRON_MESH)
PX_DEFINE_TYPEINFO(PxHeightField,							PxConcreteType::eHEIGHTFIELD)
PX_DEFINE_TYPEINFO(PxActor,									PxConcreteType::eUNDEFINED)
PX_DEFINE_TYPEINFO(PxRigidActor,							PxConcreteType::eUNDEFINED)
PX_DEFINE_TYPEINFO(PxRigidBody,								PxConcreteType::eUNDEFINED)
PX_DEFINE_TYPEINFO(PxRigidDynamic,							PxConcreteType::eRIGID_DYNAMIC)
PX_DEFINE_TYPEINFO(PxRigidStatic,							PxConcreteType::eRIGID_STATIC)
PX_DEFINE_TYPEINFO(PxArticulationLink,						PxConcreteType::eARTICULATION_LINK)
PX_DEFINE_TYPEINFO(PxArticulationJointReducedCoordinate,	PxConcreteType::eARTICULATION_JOINT_REDUCED_COORDINATE)
PX_DEFINE_TYPEINFO(PxArticulationReducedCoordinate,			PxConcreteType::eARTICULATION_REDUCED_COORDINATE)
PX_DEFINE_TYPEINFO(PxAggregate,								PxConcreteType::eAGGREGATE)
PX_DEFINE_TYPEINFO(PxConstraint,							PxConcreteType::eCONSTRAINT)
PX_DEFINE_TYPEINFO(PxShape,									PxConcreteType::eSHAPE)
PX_DEFINE_TYPEINFO(PxPruningStructure,						PxConcreteType::ePRUNING_STRUCTURE)
PX_DEFINE_TYPEINFO(PxPBDParticleSystem,						PxConcreteType::ePBD_PARTICLESYSTEM)
PX_DEFINE_TYPEINFO(PxDeformableSurface,						PxConcreteType::eDEFORMABLE_SURFACE)
PX_DEFINE_TYPEINFO(PxDeformableVolume,						PxConcreteType::eDEFORMABLE_VOLUME)
PX_DEFINE_TYPEINFO(PxDeformableAttachment,					PxConcreteType::eDEFORMABLE_ATTACHMENT)
PX_DEFINE_TYPEINFO(PxDeformableElementFilter,				PxConcreteType::eDEFORMABLE_ELEMENT_FILTER)
PX_DEFINE_TYPEINFO(PxParticleBuffer,						PxConcreteType::ePARTICLE_BUFFER)
PX_DEFINE_TYPEINFO(PxParticleAndDiffuseBuffer,				PxConcreteType::ePARTICLE_DIFFUSE_BUFFER)

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

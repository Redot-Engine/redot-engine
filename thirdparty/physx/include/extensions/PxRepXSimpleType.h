// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_REPX_SIMPLE_TYPE_H
#define PX_REPX_SIMPLE_TYPE_H


#include "foundation/PxSimpleTypes.h"
#include "cooking/PxCooking.h"
#include "common/PxStringTable.h"
#include "common/PxSerialFramework.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
	
	/**
	\brief Helper class containing the mapping of id to object, and type name.

	\deprecated Xml serialization is deprecated. An alternative serialization system is provided through USD Physics.
	*/
	struct PX_DEPRECATED PxRepXObject
	{
		/**
		\brief Identifies the extension meant to handle this object.
		\see PxTypeInfo, PX_DEFINE_TYPEINFO, PxRepXSerializer
		*/
		const char*			typeName;

		/**
		\brief Pointer to the serializable this was created from
		*/
		const void*			serializable;

		/**
		\brief Id given to this object at some point
		*/
		PxSerialObjectId 	id;
		PxRepXObject( const char* inTypeName = "", const void* inSerializable = NULL, const PxSerialObjectId inId = 0 )
			: typeName( inTypeName )
			, serializable( inSerializable )
			, id( inId )
		{
		}
		bool isValid() const { return serializable != NULL; }
	};

	/**
	\brief Arguments required to instantiate a serializable object from RepX.

	\deprecated Xml serialization is deprecated. An alternative serialization system is provided through USD Physics.

	Extra arguments can be added to the object map under special ids.

	\see PxRepXSerializer::objectToFile, PxRepXSerializer::fileToObject
	*/
	struct PX_DEPRECATED PxRepXInstantiationArgs
	{
		PxPhysics&				physics;
		const PxCookingParams*	cooker;
		PxStringTable*			stringTable;
		PxRepXInstantiationArgs( PxPhysics& inPhysics, const PxCookingParams* inCooking = NULL , PxStringTable* inStringTable = NULL ) 
			: physics( inPhysics )
			, cooker( inCooking )
			, stringTable( inStringTable )
		{
		}

		PxRepXInstantiationArgs& operator=(const PxRepXInstantiationArgs&);
	};


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

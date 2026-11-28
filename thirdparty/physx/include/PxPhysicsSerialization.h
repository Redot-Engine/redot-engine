// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PHYSICS_SERIALIZATION_H
#define PX_PHYSICS_SERIALIZATION_H

#include "common/PxSerialFramework.h"
#include "PxPhysXConfig.h"

#if !PX_DOXYGEN

/**
\brief Registers physics classes for serialization.
This function is used to implement PxSerialization.createSerializationRegistry() and is not intended to be needed otherwise.
\see PxSerializationRegistry
*/
PX_C_EXPORT PX_PHYSX_CORE_API void PX_CALL_CONV PxRegisterPhysicsSerializers(physx::PxSerializationRegistry& sr);

/**
\brief Unregisters physics classes for serialization.
This function is used in the release implementation of PxSerializationRegistry and in not intended to be used otherwise.
\see PxSerializationRegistry
*/
PX_C_EXPORT PX_PHYSX_CORE_API void PX_CALL_CONV PxUnregisterPhysicsSerializers(physx::PxSerializationRegistry& sr);


/**
\brief Adds collected objects to PxPhysics.

This function adds all objects contained in the input collection to the PxPhysics instance. This is used after deserializing
the collection, to populate the physics with inplace deserialized objects. This function is used in the implementation of 
PxSerialization.createCollectionFromBinary and is not intended to be needed otherwise.
\param[in] collection Objects to add to the PxPhysics instance.

\see PxCollection, PxSerialization.createCollectionFromBinary
*/
PX_C_EXPORT PX_PHYSX_CORE_API void PX_CALL_CONV PxAddCollectionToPhysics(const physx::PxCollection& collection);

#endif // !PX_DOXYGEN

#endif


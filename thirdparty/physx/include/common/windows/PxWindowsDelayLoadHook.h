// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_WINDOWS_DELAY_LOAD_HOOK_H
#define PX_WINDOWS_DELAY_LOAD_HOOK_H

#include "foundation/PxPreprocessor.h"
#include "common/PxPhysXCommonConfig.h"


#if !PX_DOXYGEN
namespace physx
{
#endif
	/**
 	\brief PxDelayLoadHook

	This is a helper class for delay loading the PhysXCommon dll and PhysXFoundation dll. 
	If a PhysXCommon dll or PhysXFoundation dll with a non-default file name needs to be loaded, 
	PxDelayLoadHook can be sub-classed to provide the custom filenames.

	Once the names are set, the instance must be set for use by PhysX.dll using PxSetPhysXDelayLoadHook(), 
	PhysXCooking.dll using PxSetPhysXCookingDelayLoadHook()	or by PhysXCommon.dll using PxSetPhysXCommonDelayLoadHook().

	\see PxSetPhysXDelayLoadHook(), PxSetPhysXCookingDelayLoadHook(), PxSetPhysXCommonDelayLoadHook()
 	*/
	class PxDelayLoadHook
	{
	public:
		PxDelayLoadHook() {}
		virtual ~PxDelayLoadHook() {}

		virtual const char* getPhysXFoundationDllName() const = 0;
		
		virtual const char* getPhysXCommonDllName() const = 0;

	protected:
	private:
	};

	/**
	\brief Sets delay load hook instance for PhysX dll.

	\param[in] hook Delay load hook.

	\see PxDelayLoadHook
	*/
	PX_C_EXPORT PX_PHYSX_CORE_API void PX_CALL_CONV PxSetPhysXDelayLoadHook(const physx::PxDelayLoadHook* hook);

	/**
	\brief Sets delay load hook instance for PhysXCooking dll.

	\param[in] hook Delay load hook.

	\see PxDelayLoadHook
	*/
	PX_C_EXPORT PX_PHYSX_CORE_API void PX_CALL_CONV PxSetPhysXCookingDelayLoadHook(const physx::PxDelayLoadHook* hook);

	/**
	\brief Sets delay load hook instance for PhysXCommon dll.

	\param[in] hook Delay load hook.

	\see PxDelayLoadHook
	*/
	PX_C_EXPORT PX_PHYSX_COMMON_API void PX_CALL_CONV PxSetPhysXCommonDelayLoadHook(const physx::PxDelayLoadHook* hook);

#if !PX_DOXYGEN
} // namespace physx
#endif
#endif

// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_GPU_H
#define PX_GPU_H

#include "PxPhysXConfig.h"


#if PX_SUPPORT_GPU_PHYSX

#include "cudamanager/PxCudaContextManager.h"
#include "foundation/PxPreprocessor.h"
#include "foundation/PxFoundation.h"
#include "common/PxPhysXCommonConfig.h"

/**
\brief PxGpuLoadHook

This is a helper class for loading the PhysXGpu dll. 
If a PhysXGpu dll with a non-default file name needs to be loaded, 
PxGpuLoadHook can be sub-classed to provide the custom filenames.

Once the names are set, the instance must be set for use by PhysX.dll using PxSetPhysXGpuLoadHook(), 

\see PxSetPhysXGpuLoadHook()
*/
class PxGpuLoadHook
{
public:
	PxGpuLoadHook() {}
	virtual ~PxGpuLoadHook() {}

	virtual const char* getPhysXGpuDllName() const = 0;

protected:
private:
};

/**
\brief Sets GPU load hook instance for PhysX dll.

\param[in] hook GPU load hook.

\see PxGpuLoadHook
*/
PX_C_EXPORT PX_PHYSX_CORE_API void PX_CALL_CONV PxSetPhysXGpuLoadHook(const PxGpuLoadHook* hook);

/**
 * \brief Ask the NVIDIA control panel which GPU has been selected for use by
 * PhysX.  Returns -1 if no PhysX capable GPU is found or GPU PhysX has
 * been disabled.
 */
PX_C_EXPORT PX_PHYSX_CORE_API int PX_CALL_CONV PxGetSuggestedCudaDeviceOrdinal(physx::PxErrorCallback& errc);

/**
 * \brief Allocate a CUDA Context manager, complete with heaps.
 * You only need one CUDA context manager per GPU device you intend to use for
 * CUDA tasks. 
 \param[in] foundation PhysXFoundation instance.
 \param[in] desc Cuda context manager desc.
 \param[in] profilerCallback PhysX profiler callback instance.
 \param[in] launchSynchronous Set launchSynchronous to true for CUDA to report the actual point of failure.

 \see PxGetProfilerCallback()
 */
PX_C_EXPORT PX_PHYSX_CORE_API physx::PxCudaContextManager* PX_CALL_CONV PxCreateCudaContextManager(physx::PxFoundation& foundation, const physx::PxCudaContextManagerDesc& desc, physx::PxProfilerCallback* profilerCallback = NULL, bool launchSynchronous = false);

/**
 * \brief Sets profiler callback to PhysX GPU
 \param[in] profilerCallback PhysX profiler callback instance.

 \see PxGetProfilerCallback()
 */
PX_C_EXPORT PX_PHYSX_CORE_API void PX_CALL_CONV PxSetPhysXGpuProfilerCallback(physx::PxProfilerCallback* profilerCallback);

/**
 * \brief Sets PhysXFoundation instance
 \param[in] foundation PhysXFoundation instance.

 \see PxGetFoundation()
 */
PX_C_EXPORT PX_PHYSX_CORE_API void PX_CALL_CONV PxSetPhysXGpuFoundationInstance(physx::PxFoundation& foundation);

/**
\brief Internally used callback to register function names of cuda kernels
*/
PX_C_EXPORT PX_PHYSX_CORE_API void PX_CALL_CONV PxCudaRegisterFunction(int moduleIndex, const char* functionName);

/**
\brief Internally used callback to register cuda modules at load time
*/
PX_C_EXPORT PX_PHYSX_CORE_API void** PX_CALL_CONV PxCudaRegisterFatBinary(void*);

/**
\brief Access to the registered cuda modules
*/
PX_C_EXPORT PX_PHYSX_CORE_API void** PX_CALL_CONV PxGetCudaModuleTable();

/**
\brief Number of registered cuda modules
*/
PX_C_EXPORT PX_PHYSX_CORE_API physx::PxU32 PX_CALL_CONV PxGetCudaModuleTableSize();

/**
\brief Access to the loaded cuda functions (kernels)
*/
PX_C_EXPORT PX_PHYSX_CORE_API physx::PxKernelIndex* PX_CALL_CONV PxGetCudaFunctionTable();

/**
\brief Number of loaded cuda functions (kernels)
*/
PX_C_EXPORT PX_PHYSX_CORE_API physx::PxU32 PX_CALL_CONV  PxGetCudaFunctionTableSize();


namespace physx
{
	class PxPhysicsGpu;
}

PX_C_EXPORT PX_PHYSX_CORE_API physx::PxPhysicsGpu* PX_CALL_CONV  PxGetPhysicsGpu();


#endif // PX_SUPPORT_GPU_PHYSX

#endif

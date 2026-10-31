// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFAULT_CPU_DISPATCHER_H
#define PX_DEFAULT_CPU_DISPATCHER_H

#include "common/PxPhysXCommonConfig.h"
#include "task/PxCpuDispatcher.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief A default implementation for a CPU task dispatcher.

\see PxDefaultCpuDispatcherCreate() PxCpuDispatcher
*/
class PxDefaultCpuDispatcher : public PxCpuDispatcher
{
public:
	/**
	\brief Deletes the dispatcher.
	
	Do not keep a reference to the deleted instance.

	\see PxDefaultCpuDispatcherCreate()
	*/
	virtual void release() = 0;

	/**
	\brief Enables profiling at task level.

	\note By default enabled only in profiling builds.
	
	\param[in] runProfiled True if tasks should be profiled.
	*/
	virtual void setRunProfiled(bool runProfiled) = 0;

	/**
	\brief Checks if profiling is enabled at task level.

	\return True if tasks should be profiled.
	*/
	virtual bool getRunProfiled() const = 0;
};


/**
\brief If a thread ends up waiting for work it will find itself in a spin-wait loop until work becomes available.
Three strategies are available to limit wasted cycles.
The strategies are as follows: 
a) wait until a work task signals the end of the spin-wait period.
b) yield the thread by providing a hint to reschedule thread execution, thereby allowing other threads to run.
c) yield the processor by informing it that it is waiting for work and requesting it to more efficiently use compute resources.
*/
struct PxDefaultCpuDispatcherWaitForWorkMode
{
	enum Enum
	{
		eWAIT_FOR_WORK,
		eYIELD_THREAD,
		eYIELD_PROCESSOR
	};
};


/**
\brief Create default dispatcher, extensions SDK needs to be initialized first.

\param[in] numThreads Number of worker threads the dispatcher should use.
\param[in] affinityMasks Array with affinity mask for each thread. If not defined, default masks will be used.
\param[in] mode is the strategy employed when a busy-wait is encountered. 
\param[in] yieldProcessorCount specifies the number of times a OS-specific yield processor command will be executed
during each cycle of a busy-wait in the event that the specified mode is eYIELD_PROCESSOR

\note numThreads may be zero in which case no worker thread are initialized and
simulation tasks will be executed on the thread that calls PxScene::simulate()

\note yieldProcessorCount must be greater than zero if eYIELD_PROCESSOR is the chosen mode and equal to zero for all other modes.

\note eYIELD_THREAD and eYIELD_PROCESSOR modes will use compute resources even if the simulation is not running.
It is left to users to keep threads inactive, if so desired, when no simulation is running.

\see PxDefaultCpuDispatcher
*/
PxDefaultCpuDispatcher* PxDefaultCpuDispatcherCreate(PxU32 numThreads, PxU32* affinityMasks = NULL, PxDefaultCpuDispatcherWaitForWorkMode::Enum mode = PxDefaultCpuDispatcherWaitForWorkMode::eWAIT_FOR_WORK, PxU32 yieldProcessorCount = 0);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CPU_DISPATCHER_H
#define PX_CPU_DISPATCHER_H

#include "foundation/PxSimpleTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxBaseTask;

/** 
 \brief A CpuDispatcher is responsible for scheduling the execution of tasks passed to it by the SDK.

 A typical implementation would for example use a thread pool with the dispatcher
 pushing tasks onto worker thread queues or a global queue.

 \see PxBaseTask
 \see PxTask
 \see PxTaskManager
*/
class PxCpuDispatcher
{
public:
	/**
	\brief Called by the TaskManager when a task is to be queued for execution.
	
	Upon receiving a task, the dispatcher should schedule the task to run.
	After the task has been run, it should call the release() method and 
	discard its pointer.

	\param[in] task The task to be run.

	\see PxBaseTask
	*/
    virtual void submitTask(PxBaseTask& task) = 0;

	/**
	\brief Returns the number of available worker threads for this dispatcher.
	
	The SDK will use this count to control how many tasks are submitted. By
	matching the number of tasks with the number of execution units task
	overhead can be reduced.
	*/
	virtual uint32_t getWorkerCount() const = 0;

	virtual ~PxCpuDispatcher() {}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


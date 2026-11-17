// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Copyright (c) 2016-2024 NVIDIA Corporation. All rights reserved.

//! @file
//!
//! @brief Defines a task manager API for multithreading Tk operations

#ifndef NVBLASTTKGROUPTASKMANAGER_H
#define NVBLASTTKGROUPTASKMANAGER_H

#include "NvBlastTypes.h"


// Forward declarations
namespace nvidia
{
namespace task
{
class NvTaskManager;
}
}

namespace Nv
{
namespace Blast
{


// Forward declarations
class TkGroup;


/**
Uses a nvidia::task::NvTaskManager to process a TkGroup concurrently.
*/
class NV_DLL_EXPORT TkGroupTaskManager
{
protected:
    virtual ~TkGroupTaskManager() {}

public:
    /**
    Construct using existing nvidia::task::NvTaskManager and TkGroup. The TkGroup can be set later with setGroup().
    */
    static TkGroupTaskManager* create(nvidia::task::NvTaskManager&, TkGroup* = nullptr);

    /**
    Set the group to process. Cannot be changed while a group being processed.
    */
    virtual void setGroup(TkGroup*) = 0;

    /**
    Start processing the group.
    The parallelizing strategy is to have all worker tasks running concurrently.
    The number of started tasks may be smaller than the requested value,
    when the task manager's dispatcher thread count or the number of group jobs are
    smaller.

    \param[in]  workerCount     The number of worker tasks to start, 
                                0 uses the dispatcher's worker thread count.

    \return                     The number of worker tasks started.
                                If 0, processing did not start and wait() will never return true.
    */
    virtual uint32_t process(uint32_t workerCount = 0) = 0;

    /**
    Wait for the group to end processing. When processing has finished, TkGroup::endProcess is executed.

    \param[in]  block           true:   does not return until the group has been processed.
                                false:  return immediately if workers are still processing the group.

    \return                     true if group processing was completed (and the group was actually processing)
    */
    virtual bool wait(bool block = true) = 0;

    /**
    Release this object.
    */
    virtual void release() = 0;
};


} // namespace Blast
} // namespace Nv

#endif // NVBLASTTKGROUPTASKMANAGER_H

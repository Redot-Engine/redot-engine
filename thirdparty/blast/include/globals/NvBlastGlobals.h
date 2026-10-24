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


#ifndef NVBLASTGLOBALS_H
#define NVBLASTGLOBALS_H

#include <new>
#include "NvBlastTypes.h"
#include "NvAllocatorCallback.h"
#include "NvErrorCallback.h"
#include "NvProfiler.h"


//! @file
//!
//! @brief API for the NvBlastGlobals library

//////// Global API to Access Global nvidia::NvAllocatorCallback, nvidia::NvErrorCallback, and nvidia::NvProfilerCallback ////////

/**
Retrieve a pointer to the global nvidia::NvAllocatorCallback. Default implementation with std allocator is used if user didn't provide
their own. It always exists, 'nullptr' will never be returned.

\return the pointer to the global nvidia::NvAllocatorCallback.
*/
NV_C_API nvidia::NvAllocatorCallback* NvBlastGlobalGetAllocatorCallback();

/**
Set global nvidia::NvAllocatorCallback.  If 'nullptr' is passed the default nvidia::NvAllocatorCallback with std allocator is set.
*/
NV_C_API void NvBlastGlobalSetAllocatorCallback(nvidia::NvAllocatorCallback* allocatorCallback);

/**
Retrieve a pointer to the global nvidia::NvErrorCallback. Default implementation which writes messages to stdout is used if user didn't provide
their own. It always exists, 'nullptr' will never be returned.

\return the pointer to the global nvidia::NvErrorCallback.
*/
NV_C_API nvidia::NvErrorCallback* NvBlastGlobalGetErrorCallback();

/**
Set global nvidia::NvErrorCallback.  If 'nullptr' is passed the default nvidia::NvErrorCallback that writes messages to stdout is set.
*/
NV_C_API void NvBlastGlobalSetErrorCallback(nvidia::NvErrorCallback* errorCallback);

/**
Retrieve a pointer to the global nvidia::NvProfilerCallback.  Returns nullptr if none is set.

\return the pointer to the global nvidia::NvProfilerCallback.
*/
NV_C_API nvidia::NvProfilerCallback* NvBlastGlobalGetProfilerCallback();

/**
Set a custom profiler callback. May be nullptr (the default).
*/
NV_C_API void NvBlastGlobalSetProfilerCallback(nvidia::NvProfilerCallback* profilerCallback);


//////// Helper Global Functions ////////

namespace Nv
{
namespace Blast
{

/**
Logging wrapper compatible with NvBlastLog. @see NvBlastLog.

Pass this function to LowLevel function calls in order to get logging into global nvidia::NvErrorCallback.
*/
NV_INLINE void logLL(int type, const char* msg, const char* file, int line)
{
    nvidia::NvErrorCode::Enum errorCode = nvidia::NvErrorCode::eNO_ERROR;
    switch (type)
    {
    case NvBlastMessage::Error:     errorCode = nvidia::NvErrorCode::eINVALID_OPERATION;    break;
    case NvBlastMessage::Warning:   errorCode = nvidia::NvErrorCode::eDEBUG_WARNING;        break;
    case NvBlastMessage::Info:      errorCode = nvidia::NvErrorCode::eDEBUG_INFO;           break;
    case NvBlastMessage::Debug:     errorCode = nvidia::NvErrorCode::eNO_ERROR;             break;
    }

    NvBlastGlobalGetErrorCallback()->reportError(errorCode, msg, file, line);
}


} // namespace Blast
} // namespace Nv



//////// Allocator macros ////////

/**
Alloc/Free macros that use global nvidia::NvAllocatorCallback.  Thus allocated memory is 16-byte aligned.
*/
#define NVBLAST_ALLOC(_size)                NvBlastGlobalGetAllocatorCallback()->allocate(_size, nullptr, __FILE__, __LINE__)
#define NVBLAST_ALLOC_NAMED(_size, _name)   NvBlastGlobalGetAllocatorCallback()->allocate(_size, _name, __FILE__, __LINE__)
#define NVBLAST_FREE(_mem)                  NvBlastGlobalGetAllocatorCallback()->deallocate(_mem)

/**
Placement new.
Example: Foo* foo = NVBLAST_NEW(Foo) (params);
*/
#define NVBLAST_NEW(T) new (NvBlastGlobalGetAllocatorCallback()->allocate(sizeof(T), #T, __FILE__, __LINE__)) T

/**
Respective delete to NVBLAST_NEW
The obj pointer may be NULL (to match the behavior of standard C++ delete)
Example: NVBLAST_DELETE(foo, Foo);
*/
#define NVBLAST_DELETE(obj, T)                                      \
    do                                                              \
    {                                                               \
        if ((obj) != nullptr)                                       \
        {                                                           \
            (obj)->~T();                                            \
            NvBlastGlobalGetAllocatorCallback()->deallocate(obj);   \
        }                                                           \
    } while (false)



//////// Log macros ////////

/**
Logging macros that use global nvidia::NvAllocatorCallback.
*/
#define NVBLAST_LOG(_code, _msg)    NvBlastGlobalGetErrorCallback()->reportError(_code, _msg, __FILE__, __LINE__)
#define NVBLAST_LOG_ERROR(_msg)     NVBLAST_LOG(nvidia::NvErrorCode::eINVALID_OPERATION, _msg)
#define NVBLAST_LOG_WARNING(_msg)   NVBLAST_LOG(nvidia::NvErrorCode::eDEBUG_WARNING, _msg)
#define NVBLAST_LOG_INFO(_msg)      NVBLAST_LOG(nvidia::NvErrorCode::eDEBUG_INFO, _msg)
#define NVBLAST_LOG_DEBUG(_msg)     NVBLAST_LOG(nvidia::NvErrorCode::eNO_ERROR, _msg)

/**
Check macros that use global nvidia::NvAllocatorCallback. The idea is that you pass an expression to check, if it fails
it logs and calls '_onFail' code you passed.
*/
#define NVBLAST_CHECK(_code, _expr, _msg, _onFail)                                                                      \
    {                                                                                                                   \
        if(!(_expr))                                                                                                    \
        {                                                                                                               \
            NVBLAST_LOG(_code, _msg);                                                                                   \
            { _onFail; };                                                                                               \
        }                                                                                                               \
    }                                                                                                                   

#define NVBLAST_CHECK_ERROR(_expr, _msg, _onFail)   NVBLAST_CHECK(nvidia::NvErrorCode::eINVALID_OPERATION, _expr, _msg, _onFail)
#define NVBLAST_CHECK_WARNING(_expr, _msg, _onFail) NVBLAST_CHECK(nvidia::NvErrorCode::eDEBUG_WARNING, _expr, _msg, _onFail)
#define NVBLAST_CHECK_INFO(_expr, _msg, _onFail)    NVBLAST_CHECK(nvidia::NvErrorCode::eDEBUG_INFO, _expr, _msg, _onFail)
#define NVBLAST_CHECK_DEBUG(_expr, _msg, _onFail)   NVBLAST_CHECK(nvidia::NvErrorCode::eNO_ERROR, _expr, _msg, _onFail)


//////// Misc ////////


// Macro to load a uint32_t (or larger) with four characters
#define NVBLAST_FOURCC(_a, _b, _c, _d)  ( (uint32_t)(_a) | (uint32_t)(_b)<<8 | (uint32_t)(_c)<<16 | (uint32_t)(_d)<<24 )


#endif // ifndef NVBLASTGLOBALS_H

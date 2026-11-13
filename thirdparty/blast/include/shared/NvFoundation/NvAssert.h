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
// Copyright (c) 2008-2024 NVIDIA Corporation. All rights reserved.

#ifndef NV_NVFOUNDATION_NVASSERT_H
#define NV_NVFOUNDATION_NVASSERT_H

/** \addtogroup foundation
@{ */

#include "Nv.h"

#if !NV_DOXYGEN
namespace nvidia
{
#endif

/* Base class to handle assert failures */
class NvAssertHandler
{
  public:
    virtual ~NvAssertHandler()
    {
    }
    virtual void operator()(const char* exp, const char* file, int line, bool& ignore) = 0;
};

NV_FOUNDATION_API NvAssertHandler& NvGetAssertHandler();
NV_FOUNDATION_API void NvSetAssertHandler(NvAssertHandler& handler);

#if !NV_DOXYGEN
} // namespace nvidia
#endif

#if !NV_ENABLE_ASSERTS
#define NV_ASSERT(exp) ((void)0)
#define NV_ALWAYS_ASSERT_MESSAGE(exp) ((void)0)
#define NV_ASSERT_WITH_MESSAGE(condition, message) ((void)0)
#elif NV_SPU
#include "ps3/NvPS3Assert.h"
#else
#if NV_VC
#define NV_CODE_ANALYSIS_ASSUME(exp)                                                                                   \
    __analysis_assume(!!(exp)) // This macro will be used to get rid of analysis warning messages if a NV_ASSERT is used
// to "guard" illegal mem access, for example.
#else
#define NV_CODE_ANALYSIS_ASSUME(exp)
#endif
#define NV_ASSERT(exp)                                                                                                 \
    {                                                                                                                  \
        static bool _ignore = false;                                                                                   \
        ((void)((!!(exp)) || (!_ignore && (nvidia::NvGetAssertHandler()(#exp, __FILE__, __LINE__, _ignore), false)))); \
        NV_CODE_ANALYSIS_ASSUME(exp);                                                                                  \
    }
#define NV_ALWAYS_ASSERT_MESSAGE(exp)                                                                                  \
    {                                                                                                                  \
        static bool _ignore = false;                                                                                   \
        if(!_ignore)                                                                                                   \
            nvidia::NvGetAssertHandler()(exp, __FILE__, __LINE__, _ignore);                                            \
    }
#define NV_ASSERT_WITH_MESSAGE(exp, message)                                                                              \
    {                                                                                                                     \
        static bool _ignore = false;                                                                                      \
        ((void)((!!(exp)) || (!_ignore && (nvidia::NvGetAssertHandler()(message, __FILE__, __LINE__, _ignore), false)))); \
        NV_CODE_ANALYSIS_ASSUME(exp);                                                                                     \
    }
#endif

#define NV_ALWAYS_ASSERT() NV_ASSERT(0)

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVASSERT_H

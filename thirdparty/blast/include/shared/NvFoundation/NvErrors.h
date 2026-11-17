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

#ifndef NV_NVFOUNDATION_NVERRORS_H
#define NV_NVFOUNDATION_NVERRORS_H
/** \addtogroup foundation
@{
*/

#include "Nv.h"

#if !NV_DOXYGEN
namespace nvidia
{
#endif

/**
\brief Error codes

These error codes are passed to #NvErrorCallback

@see NvErrorCallback
*/

struct NvErrorCode
{
    enum Enum
    {
        eNO_ERROR          = 0,

        //! \brief An informational message.
        eDEBUG_INFO        = 1,

        //! \brief a warning message for the user to help with debugging
        eDEBUG_WARNING     = 2,

        //! \brief method called with invalid parameter(s)
        eINVALID_PARAMETER = 4,

        //! \brief method was called at a time when an operation is not possible
        eINVALID_OPERATION = 8,

        //! \brief method failed to allocate some memory
        eOUT_OF_MEMORY     = 16,

        /** \brief The library failed for some reason.
        Possibly you have passed invalid values like NaNs, which are not checked for.
        */
        eINTERNAL_ERROR    = 32,

        //! \brief An unrecoverable error, execution should be halted and log output flushed
        eABORT             = 64,

        //! \brief The SDK has determined that an operation may result in poor performance.
        ePERF_WARNING      = 128,

        //! \brief A bit mask for including all errors
        eMASK_ALL          = -1
    };
};

#if !NV_DOXYGEN
} // namespace nvidia
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVERRORS_H

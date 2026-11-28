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

#ifndef NV_NVFOUNDATION_NV_H
#define NV_NVFOUNDATION_NV_H

/** \addtogroup foundation
@{
*/

#include "NvSimpleTypes.h"

/** files to always include */
#include <string.h>
#include <stdlib.h>

#if !NV_DOXYGEN
namespace nvidia
{
#endif
class NvAllocatorCallback;
class NvErrorCallback;
struct NvErrorCode;
class NvAssertHandler;

class NvInputStream;
class NvInputData;
class NvOutputStream;

class NvVec2;
class NvVec3;
class NvVec4;
class NvMat33;
class NvMat44;
class NvPlane;
class NvQuat;
class NvTransform;
class NvBounds3;

/** enum for empty constructor tag*/
enum NvEMPTY
{
    NvEmpty
};

/** enum for zero constructor tag for vectors and matrices */
enum NvZERO
{
    NvZero
};

/** enum for identity constructor flag for quaternions, transforms, and matrices */
enum NvIDENTITY
{
    NvIdentity
};

#if !NV_DOXYGEN
} // namespace nvidia
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NV_H

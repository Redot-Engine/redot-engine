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

#ifndef NV_NVFOUNDATION_NVINTRINSICS_H
#define NV_NVFOUNDATION_NVINTRINSICS_H

#include "NvPreprocessor.h"

#if NV_WINDOWS_FAMILY
#include "platform/windows/NvWindowsIntrinsics.h"
#elif NV_X360
#include "xbox360/NvXbox360Intrinsics.h"
#elif(NV_LINUX || NV_ANDROID || NV_APPLE_FAMILY || NV_PS4)
#include "platform/unix/NvUnixIntrinsics.h"
#elif NV_PS3
#include "ps3/NvPS3Intrinsics.h"
#elif NV_PSP2
#include "psp2/NvPSP2Intrinsics.h"
#elif NV_WIIU
#include "wiiu/NvWiiUIntrinsics.h"
#elif NV_XBOXONE
#include "XboxOne/NvXboxOneIntrinsics.h"
#else
#error "Platform not supported!"
#endif

#endif // #ifndef NV_NVFOUNDATION_NVINTRINSICS_H

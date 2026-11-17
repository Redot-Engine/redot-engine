// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CUDA_TYPES_H
#define PX_CUDA_TYPES_H

//type definitions to avoid forced inclusion of cuda.h
//if cuda.h is needed anyway, please include it before PxCudaContextManager.h, PxCudaContext.h or PxCudaTypes.h

#include "foundation/PxPreprocessor.h"

#if PX_SUPPORT_GPU_PHYSX
#ifndef CUDA_VERSION

#include "foundation/PxSimpleTypes.h"

#if PX_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#endif

#if PX_P64_FAMILY
typedef unsigned long long CUdeviceptr;
#else
typedef unsigned int CUdeviceptr;
#endif

#if PX_CLANG
#pragma clang diagnostic pop
#endif

typedef int CUdevice;

typedef struct CUctx_st* CUcontext;
typedef struct CUmod_st* CUmodule;
typedef struct CUfunc_st* CUfunction;
typedef struct CUstream_st* CUstream;
typedef struct CUevent_st* CUevent;
typedef struct CUgraphicsResource_st* CUgraphicsResource;

#define CU_MEMHOSTALLOC_PORTABLE 0x01
#define CU_MEMHOSTALLOC_DEVICEMAP 0x02
#define CU_MEMHOSTALLOC_WRITECOMBINED 0x04

#else

PX_COMPILE_TIME_ASSERT(CU_MEMHOSTALLOC_PORTABLE == 0x01);
PX_COMPILE_TIME_ASSERT(CU_MEMHOSTALLOC_DEVICEMAP == 0x02);
PX_COMPILE_TIME_ASSERT(CU_MEMHOSTALLOC_WRITECOMBINED == 0x04);

#endif

#else
typedef struct CUstream_st* CUstream; // We declare some callbacks taking CUstream as an argument even when building with PX_SUPPORT_GPU_PHYSX = 0.
typedef struct CUevent_st* CUevent;
#endif // PX_SUPPORT_GPU_PHYSX
#endif


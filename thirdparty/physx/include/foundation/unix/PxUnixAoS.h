// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PXFOUNDATION_PXUNIXAOS_H
#define PXFOUNDATION_PXUNIXAOS_H

// no includes here! this file should be included from PxcVecMath.h only!!!

#if !COMPILE_VECTOR_INTRINSICS
#error Vector intrinsics should not be included when using scalar implementation.
#endif

#if PX_INTEL_FAMILY
#include "foundation/unix/sse2/PxUnixSse2AoS.h"
#elif PX_NEON
#include "foundation/unix/neon/PxUnixNeonAoS.h"
#else
#error No SIMD implementation for this unix platform.
#endif

#endif // PXFOUNDATION_PXUNIXAOS_H

// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_MATH_INTRINSICS_H
#define PX_MATH_INTRINSICS_H

#include "foundation/PxPreprocessor.h"
#include "foundation/PxSimpleTypes.h"
#include <string.h>

#if PX_WINDOWS_FAMILY
#include "foundation/windows/PxWindowsMathIntrinsics.h"
#elif (PX_LINUX || PX_APPLE_FAMILY)
#include "foundation/unix/PxUnixMathIntrinsics.h"
#elif PX_SWITCH
#include "foundation/switch/PxSwitchMathIntrinsics.h"
#else
#error "Platform not supported!"
#endif

/**
Platform specific defines
*/
#if PX_WINDOWS_FAMILY && defined(_MSC_VER) // Godot patch: MSVC-only pragma
#pragma intrinsic(abs)
#pragma intrinsic(labs)
#endif

#endif

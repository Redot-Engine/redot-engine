// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_INTRINSICS_H
#define PX_INTRINSICS_H

#include "foundation/PxPreprocessor.h"
#if PX_WINDOWS_FAMILY
#include "windows/PxWindowsIntrinsics.h"
#elif(PX_LINUX || PX_APPLE_FAMILY)
#include "unix/PxUnixIntrinsics.h"
#elif PX_SWITCH
#include "switch/PxSwitchIntrinsics.h"
#else
#error "Platform not supported!"
#endif


#endif

// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_INLINE_AOS_H
#define PX_INLINE_AOS_H

#include "foundation/PxPreprocessor.h"

#if PX_WINDOWS
#include "windows/PxWindowsTrigConstants.h"
#include "windows/PxWindowsInlineAoS.h"
#elif(PX_UNIX_FAMILY || PX_SWITCH)
#include "unix/PxUnixTrigConstants.h"
#include "unix/PxUnixInlineAoS.h"
#else
#error "Platform not supported!"
#endif

#endif


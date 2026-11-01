// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_AOS_H
#define PX_AOS_H


#if PX_WINDOWS && !PX_NEON
#include "windows/PxWindowsAoS.h"
#elif(PX_UNIX_FAMILY || PX_SWITCH)
#include "unix/PxUnixAoS.h"
#else
#error "Platform not supported!"
#endif

#endif

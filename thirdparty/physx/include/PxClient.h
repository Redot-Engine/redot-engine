// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CLIENT_H
#define PX_CLIENT_H

#include "foundation/PxFlags.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief An ID to identify different clients for multiclient support.

\see PxScene::createClient() 
*/
typedef PxU8 PxClientID;

/**
\brief The predefined default PxClientID value.

\see PxClientID PxScene::createClient() 
*/
static const PxClientID PX_DEFAULT_CLIENT = 0;

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

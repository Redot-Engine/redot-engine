// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_ATOMIC_H
#define PX_ATOMIC_H

#include "foundation/PxFoundationConfig.h"
#include "foundation/PxSimpleTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
/* set *dest equal to val. Return the old value of *dest */
PX_FOUNDATION_API PxI32 PxAtomicExchange(volatile PxI32* dest, PxI32 val);
PX_FOUNDATION_API PxI64 PxAtomicExchange(volatile PxI64* dest, PxI64 val);

/* if *dest == comp, replace with exch. Return original value of *dest */
PX_FOUNDATION_API PxI32 PxAtomicCompareExchange(volatile PxI32* dest, PxI32 exch, PxI32 comp);
PX_FOUNDATION_API PxI64 PxAtomicCompareExchange(volatile PxI64* dest, PxI64 exch, PxI64 comp);

/* if *dest == comp, replace with exch. Return original value of *dest */
PX_FOUNDATION_API void* PxAtomicCompareExchangePointer(volatile void** dest, void* exch, void* comp);

/* increment the specified location. Return the incremented value */
PX_FOUNDATION_API PxI32 PxAtomicIncrement(volatile PxI32* val);
PX_FOUNDATION_API PxI64 PxAtomicIncrement(volatile PxI64* val);

/* decrement the specified location. Return the decremented value */
PX_FOUNDATION_API PxI32 PxAtomicDecrement(volatile PxI32* val);
PX_FOUNDATION_API PxI64 PxAtomicDecrement(volatile PxI64* val);

/* add delta to *val. Return the new value */
PX_FOUNDATION_API PxI32 PxAtomicAdd(volatile PxI32* val, PxI32 delta);
PX_FOUNDATION_API PxI64 PxAtomicAdd(volatile PxI64* val, PxI64 delta);

/* compute the maximum of dest and val. Return the new value */
PX_FOUNDATION_API PxI32 PxAtomicMax(volatile PxI32* val, PxI32 val2);
PX_FOUNDATION_API PxI64 PxAtomicMax(volatile PxI64* val, PxI64 val2);

/* or mask to *val. Return the old value */
PX_FOUNDATION_API PxI32 PxAtomicOr(volatile PxI32* val, PxI32 mask);
PX_FOUNDATION_API PxI64 PxAtomicOr(volatile PxI64* val, PxI64 mask);

/* and mask to *val. Return the old value */
PX_FOUNDATION_API PxI32 PxAtomicAnd(volatile PxI32* val, PxI32 mask);
PX_FOUNDATION_API PxI64 PxAtomicAnd(volatile PxI64* val, PxI64 mask);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


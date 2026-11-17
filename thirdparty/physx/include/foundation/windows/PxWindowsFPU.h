// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_WINDOWS_FPU_H
#define PX_WINDOWS_FPU_H

PX_INLINE physx::PxSIMDGuard::PxSIMDGuard(bool enable) : mEnabled(enable)
{
#if !PX_ARM && !PX_A64
	if (enable)
	{
		mControlWord = _mm_getcsr();
		// set default (disable exceptions: _MM_MASK_MASK) and FTZ (_MM_FLUSH_ZERO_ON), DAZ (_MM_DENORMALS_ZERO_ON: (1<<6))
		_mm_setcsr(_MM_MASK_MASK | _MM_FLUSH_ZERO_ON | (1 << 6));
	}
	else
	{
		PX_ASSERT(_mm_getcsr() & _MM_FLUSH_ZERO_ON);
		PX_ASSERT(_mm_getcsr() & (1 << 6));
		PX_ASSERT(_mm_getcsr() & _MM_MASK_MASK);
	}
#endif
}

PX_INLINE physx::PxSIMDGuard::~PxSIMDGuard()
{
#if !PX_ARM && !PX_A64
	if (mEnabled)
	{
		// restore control word and clear any exception flags
		// (setting exception state flags cause exceptions on the first following fp operation)
		_mm_setcsr(mControlWord & ~_MM_EXCEPT_MASK);
	}
#endif
}

#endif


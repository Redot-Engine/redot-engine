// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PXFOUNDATION_PXUNIXFPU_H
#define PXFOUNDATION_PXUNIXFPU_H

#include "foundation/PxPreprocessor.h"

#if PX_LINUX || PX_OSX

#if PX_X86 || PX_X64
#if PX_EMSCRIPTEN
#include <emmintrin.h>
#endif
#include <xmmintrin.h>
#elif PX_NEON
#include <arm_neon.h>
#endif

PX_INLINE physx::PxSIMDGuard::PxSIMDGuard(bool enable) 
#if !PX_EMSCRIPTEN && (PX_X86 || PX_X64)
	: mEnabled(enable)
#endif
{
#if !PX_EMSCRIPTEN && (PX_X86 || PX_X64)
	if(enable)
	{
		mControlWord = _mm_getcsr();
		// set default (disable exceptions: _MM_MASK_MASK) and FTZ (_MM_FLUSH_ZERO_ON), DAZ (_MM_DENORMALS_ZERO_ON: (1<<6))
		_mm_setcsr(_MM_MASK_MASK | _MM_FLUSH_ZERO_ON | (1 << 6));
	}
	else
	{
		PX_UNUSED(enable);
		PX_ASSERT(_mm_getcsr() & _MM_FLUSH_ZERO_ON);
		PX_ASSERT(_mm_getcsr() & (1 << 6));
		PX_ASSERT(_mm_getcsr() & _MM_MASK_MASK);
	}
#endif
}

PX_INLINE physx::PxSIMDGuard::~PxSIMDGuard()
{
#if !PX_EMSCRIPTEN && (PX_X86 || PX_X64)
	if(mEnabled)
	{
		// restore control word and clear exception flags
		// (setting exception state flags cause exceptions on the first following fp operation)
		_mm_setcsr(mControlWord & PxU32(~_MM_EXCEPT_MASK));
	}
#endif
}

#else
	#error No SIMD implementation for this unix platform.
#endif // PX_LINUX || PX_OSX

#endif

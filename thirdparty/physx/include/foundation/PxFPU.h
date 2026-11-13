// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_FPU_H
#define PX_FPU_H

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxIntrinsics.h"
#include "foundation/PxAssert.h"
#include "foundation/PxFoundationConfig.h"

#define PX_IR(x) ((PxU32&)(x))	// integer representation of a floating-point value.
#define PX_SIR(x) ((PxI32&)(x))	// signed integer representation of a floating-point value.
#define PX_FR(x) ((PxReal&)(x))		// floating-point representation of a integer value.

#define PX_FPU_GUARD PxFPUGuard scopedFpGuard;
#define PX_SIMD_GUARD PxSIMDGuard scopedFpGuard;
#define PX_SIMD_GUARD_CNDT(x) PxSIMDGuard scopedFpGuard(x);

#if !PX_DOXYGEN
namespace physx
{
#endif
// sets the default SDK state for scalar and SIMD units
class PX_FOUNDATION_API PxFPUGuard
{
  public:
	PxFPUGuard();  // set fpu control word for PhysX
	~PxFPUGuard(); // restore fpu control word
  private:
	PxU32 mControlWords[8];
};

// sets default SDK state for simd unit only, lighter weight than FPUGuard
class PxSIMDGuard
{
  public:
	PX_INLINE PxSIMDGuard(bool enable = true);  // set simd control word for PhysX
	PX_INLINE ~PxSIMDGuard(); // restore simd control word
  private:
#if !(PX_LINUX || PX_OSX) || (!PX_EMSCRIPTEN && PX_INTEL_FAMILY)
  PxU32			mControlWord;
  bool			mEnabled;
#endif
};

/**
\brief Enables floating point exceptions for the scalar and SIMD unit
*/
PX_FOUNDATION_API void PxEnableFPExceptions();

/**
\brief Disables floating point exceptions for the scalar and SIMD unit
*/
PX_FOUNDATION_API void PxDisableFPExceptions();

#if !PX_DOXYGEN
} // namespace physx
#endif

#if PX_WINDOWS_FAMILY
#include "foundation/windows/PxWindowsFPU.h"
#elif (PX_LINUX && PX_SSE2) || PX_OSX
#include "foundation/unix/PxUnixFPU.h"
#else
PX_INLINE physx::PxSIMDGuard::PxSIMDGuard(bool)
{
}
PX_INLINE physx::PxSIMDGuard::~PxSIMDGuard()
{
}
#endif

#endif


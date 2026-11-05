// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TIME_H
#define PX_TIME_H

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxFoundationConfig.h"

#if PX_LINUX
#include <time.h>
#endif

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxCounterFrequencyToTensOfNanos
{
	PxU64 mNumerator;
	PxU64 mDenominator;
	PxCounterFrequencyToTensOfNanos(PxU64 inNum, PxU64 inDenom) : mNumerator(inNum), mDenominator(inDenom)
	{
	}

	// quite slow.
	PxU64 toTensOfNanos(PxU64 inCounter) const
	{
		return (inCounter * mNumerator) / mDenominator;
	}
};

class PX_FOUNDATION_API PxTime
{
  public:
	typedef PxF64 Second;
	static const PxU64 sNumTensOfNanoSecondsInASecond = 100000000;
	// This is supposedly guaranteed to not change after system boot
	// regardless of processors, speedstep, etc.
	static const PxCounterFrequencyToTensOfNanos& getBootCounterFrequency();

	static PxCounterFrequencyToTensOfNanos getCounterFrequency();

	static PxU64 getCurrentCounterValue();

	// SLOW!!
	// Thar be a 64 bit divide in thar!
	static PxU64 getCurrentTimeInTensOfNanoSeconds()
	{
		PxU64 ticks = getCurrentCounterValue();
		return getBootCounterFrequency().toTensOfNanos(ticks);
	}

	PxTime();
	Second getElapsedSeconds();
	Second peekElapsedSeconds();
	Second getLastTime() const;

  private:
#if PX_LINUX || PX_APPLE_FAMILY
	Second mLastTime;
#else
	PxI64 mTickCount;
#endif
};
#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BIT_AND_DATA_H
#define PX_BIT_AND_DATA_H

#include "foundation/PxIO.h"
#include "foundation/PxSimpleTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

template <typename storageType, storageType bitMask>
class PxBitAndDataT
{
  public:
	PX_FORCE_INLINE PxBitAndDataT(const PxEMPTY)
	{
	}
	PX_FORCE_INLINE PxBitAndDataT() : mData(0)
	{
	}
	PX_FORCE_INLINE PxBitAndDataT(storageType data, bool bit = false)
	{
		mData = bit ? storageType(data | bitMask) : data;
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE operator storageType() const
	{
		return storageType(mData & ~bitMask);
	}
	PX_CUDA_CALLABLE PX_FORCE_INLINE void setBit()
	{
		mData |= bitMask;
	}
	PX_CUDA_CALLABLE PX_FORCE_INLINE void clearBit()
	{
		mData &= ~bitMask;
	}
	PX_CUDA_CALLABLE PX_FORCE_INLINE storageType isBitSet() const
	{
		return storageType(mData & bitMask);
	}

  protected:
	storageType mData;
};
typedef PxBitAndDataT<PxU8, 0x80> PxBitAndByte;
typedef PxBitAndDataT<PxU16, 0x8000> PxBitAndWord;
typedef PxBitAndDataT<PxU32, 0x80000000> PxBitAndDword;

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


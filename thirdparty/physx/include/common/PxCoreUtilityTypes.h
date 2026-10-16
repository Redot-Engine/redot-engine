// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CORE_UTILITY_TYPES_H
#define PX_CORE_UTILITY_TYPES_H

#include "foundation/PxAssert.h"
#include "foundation/PxMemory.h"
#include "foundation/PxIO.h"

#if !PX_DOXYGEN
namespace physx
{
#endif


struct PxBoundedData
{
	/**
	\brief The offset in bytes between consecutive samples in the data.

	<b>Default:</b> 0
	*/
	const void* data;
	PxU32 stride;
	PxU32 count;

	PxBoundedData() : data( NULL ), stride(0), count(0) {}

	PxBoundedData(void* data_, PxU32 stride_ = 0, PxU32 count_ = 0)
		: data(data_)
		, stride(stride_)
		, count(count_)
	{
	}

	template<typename TDataType>
	PX_INLINE const TDataType& at( PxU32 idx ) const
	{
		PxU32 theStride( stride );
		if ( theStride == 0 )
			theStride = sizeof( TDataType );
		PxU32 offset( theStride * idx );
		return *(reinterpret_cast<const TDataType*>( reinterpret_cast< const PxU8* >( data ) + offset ));
	}
};

typedef PX_DEPRECATED PxBoundedData PxStridedData;

template<typename TDataType>
struct PxTypedBoundedData
{
	TDataType* data;
	PxU32 stride;
	PxU32 count;

	PxTypedBoundedData()
		: data(NULL)
		, stride(0)
		, count(0)
	{
	}

	PxTypedBoundedData(TDataType* data_, PxU32 stride_ = 0, PxU32 count_ = 0)
		: data(data_)
		, stride(stride_)
		, count(count_)
	{
	}
	
	PX_CUDA_CALLABLE PX_INLINE const TDataType& at(PxU32 idx) const
	{
		PxU32 theStride(stride);
		if (theStride == 0)
			theStride = sizeof(TDataType);
		PxU32 offset(theStride * idx);
		return *(reinterpret_cast<const TDataType*>(reinterpret_cast<const PxU8*>(data) + offset));
	}
	
	PX_CUDA_CALLABLE PX_INLINE TDataType& atRef(PxU32 idx) 
	{
		PxU32 theStride(stride);
		if (theStride == 0)
			theStride = sizeof(TDataType);
		PxU32 offset(theStride * idx);
		return *(reinterpret_cast<TDataType*>(reinterpret_cast<PxU8*>(data) + offset));
	}
};

template<typename TDataType>
PX_DEPRECATED struct PxTypedStridedData : public PxTypedBoundedData<TDataType>
{
	PxTypedStridedData() : PxTypedBoundedData<TDataType>()
	{
	}

	PxTypedStridedData(TDataType* data_, PxU32 stride_ = 0) : PxTypedBoundedData<TDataType>(data_, stride_)
	{
	}
};

template<PxU8 TNumBytes>
struct PxPadding
{
	PxU8 mPadding[TNumBytes];
	PxPadding()
	{
		for ( PxU8 idx =0; idx < TNumBytes; ++idx )
			mPadding[idx] = 0;
	}
};

template <PxU32 NB_ELEMENTS> class PxFixedSizeLookupTable
{
public:
	
	PxFixedSizeLookupTable() 
		: mNbDataPairs(0)
	{
	}

	PxFixedSizeLookupTable(const PxEMPTY) {}

	PxFixedSizeLookupTable(const PxReal* dataPairs, const PxU32 numDataPairs)
	{
		PxMemCopy(mDataPairs,dataPairs,sizeof(PxReal)*2*numDataPairs);
		mNbDataPairs=numDataPairs;
	}

	PxFixedSizeLookupTable(const PxFixedSizeLookupTable& src)
	{
		PxMemCopy(mDataPairs,src.mDataPairs,sizeof(PxReal)*2*src.mNbDataPairs);
		mNbDataPairs=src.mNbDataPairs;
	}

	~PxFixedSizeLookupTable()
	{
	}

	PxFixedSizeLookupTable& operator=(const PxFixedSizeLookupTable& src)
	{
		PxMemCopy(mDataPairs,src.mDataPairs,sizeof(PxReal)*2*src.mNbDataPairs);
		mNbDataPairs=src.mNbDataPairs;
		return *this;
	}

	PX_FORCE_INLINE void addPair(const PxReal x, const PxReal y)
	{
		PX_ASSERT(mNbDataPairs<NB_ELEMENTS);
		mDataPairs[2*mNbDataPairs+0]=x;
		mDataPairs[2*mNbDataPairs+1]=y;
		mNbDataPairs++;
	}

	PX_FORCE_INLINE PxReal getYVal(const PxReal x) const
	{
		if(0==mNbDataPairs)
		{
			PX_ASSERT(false);
			return 0;
		}

		if(1==mNbDataPairs || x<getX(0))
		{
			return getY(0);
		}

		PxReal x0=getX(0);
		PxReal y0=getY(0);

		for(PxU32 i=1;i<mNbDataPairs;i++)
		{
			const PxReal x1=getX(i);
			const PxReal y1=getY(i);

			if((x>=x0)&&(x<x1))
			{
				return (y0+(y1-y0)*(x-x0)/(x1-x0));
			}

			x0=x1;
			y0=y1;
		}

		PX_ASSERT(x>=getX(mNbDataPairs-1));
		return getY(mNbDataPairs-1);
	}

	PxU32 getNbDataPairs() const {return mNbDataPairs;}
	
	void clear()
	{
		PxMemSet(mDataPairs, 0, NB_ELEMENTS*2*sizeof(PxReal));
		mNbDataPairs = 0;
	}

	PX_FORCE_INLINE PxReal getX(const PxU32 i) const
	{
		return mDataPairs[2*i];
	}
	PX_FORCE_INLINE PxReal getY(const PxU32 i) const
	{
		return mDataPairs[2*i+1];
	}

	PxReal mDataPairs[2*NB_ELEMENTS];
	PxU32 mNbDataPairs;
	PxU32 mPad[3];

	
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

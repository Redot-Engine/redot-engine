// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_FLAGS_H
#define PX_FLAGS_H


#include "foundation/PxSimpleTypes.h"
#include "foundation/PxPreprocessor.h"
#include "foundation/PxIO.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
/**
\brief Container for bitfield flag variables associated with a specific enum type.

This allows for type safe manipulation for bitfields.

<h3>Example</h3>
    // enum that defines each bit...
    struct MyEnum
    {
        enum Enum
        {
            eMAN  = 1,
            eBEAR = 2,
            ePIG  = 4,
        };
    };

    // implements some convenient global operators.
    PX_FLAGS_OPERATORS(MyEnum::Enum, PxU8);

    PxFlags<MyEnum::Enum, PxU8> myFlags;
    myFlags |= MyEnum::eMAN;
    myFlags |= MyEnum::eBEAR | MyEnum::ePIG;
    if(myFlags & MyEnum::eBEAR)
    {
        doSomething();
    }
*/

template <typename enumtype, typename storagetype = PxU32>
class PxFlags
{
  public:
	typedef storagetype InternalType;

	PX_CUDA_CALLABLE PX_INLINE explicit PxFlags(const PxEMPTY)
	{
	}
	PX_CUDA_CALLABLE PX_INLINE PxFlags();
	PX_CUDA_CALLABLE PX_INLINE PxFlags(enumtype e);
	PX_CUDA_CALLABLE PX_INLINE PxFlags(const PxFlags<enumtype, storagetype>& f);
	PX_CUDA_CALLABLE PX_INLINE explicit PxFlags(storagetype b);

	PX_CUDA_CALLABLE PX_INLINE bool operator==(enumtype e) const;
	PX_CUDA_CALLABLE PX_INLINE bool operator==(const PxFlags<enumtype, storagetype>& f) const;
	PX_CUDA_CALLABLE PX_INLINE bool operator==(bool b) const;
	PX_CUDA_CALLABLE PX_INLINE bool operator!=(enumtype e) const;
	PX_CUDA_CALLABLE PX_INLINE bool operator!=(const PxFlags<enumtype, storagetype>& f) const;

	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& operator=(const PxFlags<enumtype, storagetype>& f);
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& operator=(enumtype e);

	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& operator|=(enumtype e);
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& operator|=(const PxFlags<enumtype, storagetype>& f);
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> operator|(enumtype e) const;
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> operator|(const PxFlags<enumtype, storagetype>& f) const;

	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& operator&=(enumtype e);
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& operator&=(const PxFlags<enumtype, storagetype>& f);
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> operator&(enumtype e) const;
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> operator&(const PxFlags<enumtype, storagetype>& f) const;

	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& operator^=(enumtype e);
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& operator^=(const PxFlags<enumtype, storagetype>& f);
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> operator^(enumtype e) const;
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> operator^(const PxFlags<enumtype, storagetype>& f) const;

	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> operator~() const;

	PX_CUDA_CALLABLE PX_INLINE operator bool() const;
	PX_CUDA_CALLABLE PX_INLINE operator PxU8() const;
	PX_CUDA_CALLABLE PX_INLINE operator PxU16() const;
	PX_CUDA_CALLABLE PX_INLINE operator PxU32() const;

	PX_CUDA_CALLABLE PX_INLINE void clear(enumtype e);
	PX_CUDA_CALLABLE PX_INLINE void raise(enumtype e);
	PX_CUDA_CALLABLE PX_INLINE bool isSet(enumtype e) const;
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& setAll(enumtype e);

  public:
	friend PX_INLINE PxFlags<enumtype, storagetype> operator&(enumtype a, PxFlags<enumtype, storagetype>& b)
	{
		PxFlags<enumtype, storagetype> out;
		out.mBits = a & b.mBits;
		return out;
	}

  private:
	storagetype mBits;
};

#if !PX_DOXYGEN

#define PX_FLAGS_OPERATORS(enumtype, storagetype)												\
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> operator|(enumtype a, enumtype b)	\
	{																							\
		PxFlags<enumtype, storagetype> r(a);													\
		r |= b;																					\
		return r;																				\
	}																							\
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> operator&(enumtype a, enumtype b)	\
	{																							\
		PxFlags<enumtype, storagetype> r(a);													\
		r &= b;																					\
		return r;																				\
	}																							\
	PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> operator~(enumtype a)				\
	{																							\
		return ~PxFlags<enumtype, storagetype>(a);												\
	}

#define PX_FLAGS_TYPEDEF(x, y)																	\
	typedef PxFlags<x::Enum, y> x##s;															\
	PX_FLAGS_OPERATORS(x::Enum, y)

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>::PxFlags()
{
	mBits = 0;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>::PxFlags(enumtype e)
{
	mBits = static_cast<storagetype>(e);
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>::PxFlags(const PxFlags<enumtype, storagetype>& f)
{
	mBits = f.mBits;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>::PxFlags(storagetype b)
{
	mBits = b;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE bool PxFlags<enumtype, storagetype>::operator==(enumtype e) const
{
	return mBits == static_cast<storagetype>(e);
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE bool PxFlags<enumtype, storagetype>::operator==(const PxFlags<enumtype, storagetype>& f) const
{
	return mBits == f.mBits;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE bool PxFlags<enumtype, storagetype>::operator==(bool b) const
{
	return bool(*this) == b;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE bool PxFlags<enumtype, storagetype>::operator!=(enumtype e) const
{
	return mBits != static_cast<storagetype>(e);
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE bool PxFlags<enumtype, storagetype>::operator!=(const PxFlags<enumtype, storagetype>& f) const
{
	return mBits != f.mBits;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& PxFlags<enumtype, storagetype>::operator=(enumtype e)
{
	mBits = static_cast<storagetype>(e);
	return *this;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& PxFlags<enumtype, storagetype>::operator=(const PxFlags<enumtype, storagetype>& f)
{
	mBits = f.mBits;
	return *this;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& PxFlags<enumtype, storagetype>::operator|=(enumtype e)
{
	mBits |= static_cast<storagetype>(e);
	return *this;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& PxFlags<enumtype, storagetype>::
operator|=(const PxFlags<enumtype, storagetype>& f)
{
	mBits |= f.mBits;
	return *this;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> PxFlags<enumtype, storagetype>::operator|(enumtype e) const
{
	PxFlags<enumtype, storagetype> out(*this);
	out |= e;
	return out;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> PxFlags<enumtype, storagetype>::
operator|(const PxFlags<enumtype, storagetype>& f) const
{
	PxFlags<enumtype, storagetype> out(*this);
	out |= f;
	return out;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& PxFlags<enumtype, storagetype>::operator&=(enumtype e)
{
	mBits &= static_cast<storagetype>(e);
	return *this;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& PxFlags<enumtype, storagetype>::
operator&=(const PxFlags<enumtype, storagetype>& f)
{
	mBits &= f.mBits;
	return *this;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> PxFlags<enumtype, storagetype>::operator&(enumtype e) const
{
	PxFlags<enumtype, storagetype> out = *this;
	out.mBits &= static_cast<storagetype>(e);
	return out;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> PxFlags<enumtype, storagetype>::
operator&(const PxFlags<enumtype, storagetype>& f) const
{
	PxFlags<enumtype, storagetype> out = *this;
	out.mBits &= f.mBits;
	return out;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& PxFlags<enumtype, storagetype>::operator^=(enumtype e)
{
	mBits ^= static_cast<storagetype>(e);
	return *this;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& PxFlags<enumtype, storagetype>::
operator^=(const PxFlags<enumtype, storagetype>& f)
{
	mBits ^= f.mBits;
	return *this;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> PxFlags<enumtype, storagetype>::operator^(enumtype e) const
{
	PxFlags<enumtype, storagetype> out = *this;
	out.mBits ^= static_cast<storagetype>(e);
	return out;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> PxFlags<enumtype, storagetype>::
operator^(const PxFlags<enumtype, storagetype>& f) const
{
	PxFlags<enumtype, storagetype> out = *this;
	out.mBits ^= f.mBits;
	return out;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype> PxFlags<enumtype, storagetype>::operator~() const
{
	PxFlags<enumtype, storagetype> out;
	out.mBits = storagetype(~mBits);
	return out;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>::operator bool() const
{
	return mBits ? true : false;
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>::operator PxU8() const
{
	return static_cast<PxU8>(mBits);
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>::operator PxU16() const
{
	return static_cast<PxU16>(mBits);
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>::operator PxU32() const
{
	return static_cast<PxU32>(mBits);
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE void PxFlags<enumtype, storagetype>::clear(enumtype e)
{
	mBits &= ~static_cast<storagetype>(e);
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE void PxFlags<enumtype, storagetype>::raise(enumtype e)
{
	mBits |= static_cast<storagetype>(e);
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE bool PxFlags<enumtype, storagetype>::isSet(enumtype e) const
{
	return (mBits & static_cast<storagetype>(e)) == static_cast<storagetype>(e);
}

template <typename enumtype, typename storagetype>
PX_CUDA_CALLABLE PX_INLINE PxFlags<enumtype, storagetype>& PxFlags<enumtype, storagetype>::setAll(enumtype e)
{
	mBits = static_cast<storagetype>(e);
	return *this;
}

} // namespace physx
#endif //!PX_DOXYGEN

#endif


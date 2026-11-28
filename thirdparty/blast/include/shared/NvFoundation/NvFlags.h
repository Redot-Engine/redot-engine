// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Copyright (c) 2008-2024 NVIDIA Corporation. All rights reserved.

#ifndef NV_NVFOUNDATION_NVFLAGS_H
#define NV_NVFOUNDATION_NVFLAGS_H

/** \addtogroup foundation
  @{
*/

#include "Nv.h"

#if !NV_DOXYGEN
namespace nvidia
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
    NV_FLAGS_OPERATORS(MyEnum::Enum, uint8_t);

    NvFlags<MyEnum::Enum, uint8_t> myFlags;
    myFlags |= MyEnum::eMAN;
    myFlags |= MyEnum::eBEAR | MyEnum::ePIG;
    if(myFlags & MyEnum::eBEAR)
    {
        doSomething();
    }
*/

template <typename enumtype, typename storagetype = uint32_t>
class NvFlags
{
  public:
    typedef storagetype InternalType;

    NV_INLINE explicit NvFlags(const NvEMPTY)
    {
    }
    NV_INLINE NvFlags(void);
    NV_INLINE NvFlags(enumtype e);
    NV_INLINE NvFlags(const NvFlags<enumtype, storagetype>& f);
    NV_INLINE explicit NvFlags(storagetype b);

    NV_INLINE bool isSet(enumtype e) const;
    NV_INLINE NvFlags<enumtype, storagetype>& set(enumtype e);
    NV_INLINE bool operator==(enumtype e) const;
    NV_INLINE bool operator==(const NvFlags<enumtype, storagetype>& f) const;
    NV_INLINE bool operator==(bool b) const;
    NV_INLINE bool operator!=(enumtype e) const;
    NV_INLINE bool operator!=(const NvFlags<enumtype, storagetype>& f) const;

    NV_INLINE NvFlags<enumtype, storagetype>& operator=(const NvFlags<enumtype, storagetype>& f);
    NV_INLINE NvFlags<enumtype, storagetype>& operator=(enumtype e);

    NV_INLINE NvFlags<enumtype, storagetype>& operator|=(enumtype e);
    NV_INLINE NvFlags<enumtype, storagetype>& operator|=(const NvFlags<enumtype, storagetype>& f);
    NV_INLINE NvFlags<enumtype, storagetype> operator|(enumtype e) const;
    NV_INLINE NvFlags<enumtype, storagetype> operator|(const NvFlags<enumtype, storagetype>& f) const;

    NV_INLINE NvFlags<enumtype, storagetype>& operator&=(enumtype e);
    NV_INLINE NvFlags<enumtype, storagetype>& operator&=(const NvFlags<enumtype, storagetype>& f);
    NV_INLINE NvFlags<enumtype, storagetype> operator&(enumtype e) const;
    NV_INLINE NvFlags<enumtype, storagetype> operator&(const NvFlags<enumtype, storagetype>& f) const;

    NV_INLINE NvFlags<enumtype, storagetype>& operator^=(enumtype e);
    NV_INLINE NvFlags<enumtype, storagetype>& operator^=(const NvFlags<enumtype, storagetype>& f);
    NV_INLINE NvFlags<enumtype, storagetype> operator^(enumtype e) const;
    NV_INLINE NvFlags<enumtype, storagetype> operator^(const NvFlags<enumtype, storagetype>& f) const;

    NV_INLINE NvFlags<enumtype, storagetype> operator~(void) const;

    NV_INLINE operator bool(void) const;
    NV_INLINE operator uint8_t(void) const;
    NV_INLINE operator uint16_t(void) const;
    NV_INLINE operator uint32_t(void) const;

    NV_INLINE void clear(enumtype e);

  public:
    friend NV_INLINE NvFlags<enumtype, storagetype> operator&(enumtype a, NvFlags<enumtype, storagetype>& b)
    {
        NvFlags<enumtype, storagetype> out;
        out.mBits = a & b.mBits;
        return out;
    }

  private:
    storagetype mBits;
};

#define NV_FLAGS_OPERATORS(enumtype, storagetype)                                                                      \
    NV_INLINE NvFlags<enumtype, storagetype> operator|(enumtype a, enumtype b)                                         \
    {                                                                                                                  \
        NvFlags<enumtype, storagetype> r(a);                                                                           \
        r |= b;                                                                                                        \
        return r;                                                                                                      \
    }                                                                                                                  \
    NV_INLINE NvFlags<enumtype, storagetype> operator&(enumtype a, enumtype b)                                         \
    {                                                                                                                  \
        NvFlags<enumtype, storagetype> r(a);                                                                           \
        r &= b;                                                                                                        \
        return r;                                                                                                      \
    }                                                                                                                  \
    NV_INLINE NvFlags<enumtype, storagetype> operator~(enumtype a)                                                     \
    {                                                                                                                  \
        return ~NvFlags<enumtype, storagetype>(a);                                                                     \
    }

#define NV_FLAGS_TYPEDEF(x, y)                                                                                         \
    typedef NvFlags<x::Enum, y> x##s;                                                                                  \
    NV_FLAGS_OPERATORS(x::Enum, y)

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>::NvFlags(void)
{
    mBits = 0;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>::NvFlags(enumtype e)
{
    mBits = static_cast<storagetype>(e);
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>::NvFlags(const NvFlags<enumtype, storagetype>& f)
{
    mBits = f.mBits;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>::NvFlags(storagetype b)
{
    mBits = b;
}

template <typename enumtype, typename storagetype>
NV_INLINE bool NvFlags<enumtype, storagetype>::isSet(enumtype e) const
{
    return (mBits & static_cast<storagetype>(e)) == static_cast<storagetype>(e);
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>& NvFlags<enumtype, storagetype>::set(enumtype e)
{
    mBits = static_cast<storagetype>(e);
    return *this;
}

template <typename enumtype, typename storagetype>
NV_INLINE bool NvFlags<enumtype, storagetype>::operator==(enumtype e) const
{
    return mBits == static_cast<storagetype>(e);
}

template <typename enumtype, typename storagetype>
NV_INLINE bool NvFlags<enumtype, storagetype>::operator==(const NvFlags<enumtype, storagetype>& f) const
{
    return mBits == f.mBits;
}

template <typename enumtype, typename storagetype>
NV_INLINE bool NvFlags<enumtype, storagetype>::operator==(bool b) const
{
    return bool(*this) == b;
}

template <typename enumtype, typename storagetype>
NV_INLINE bool NvFlags<enumtype, storagetype>::operator!=(enumtype e) const
{
    return mBits != static_cast<storagetype>(e);
}

template <typename enumtype, typename storagetype>
NV_INLINE bool NvFlags<enumtype, storagetype>::operator!=(const NvFlags<enumtype, storagetype>& f) const
{
    return mBits != f.mBits;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>& NvFlags<enumtype, storagetype>::operator=(enumtype e)
{
    mBits = static_cast<storagetype>(e);
    return *this;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>& NvFlags<enumtype, storagetype>::operator=(const NvFlags<enumtype, storagetype>& f)
{
    mBits = f.mBits;
    return *this;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>& NvFlags<enumtype, storagetype>::operator|=(enumtype e)
{
    mBits |= static_cast<storagetype>(e);
    return *this;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>& NvFlags<enumtype, storagetype>::
operator|=(const NvFlags<enumtype, storagetype>& f)
{
    mBits |= f.mBits;
    return *this;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype> NvFlags<enumtype, storagetype>::operator|(enumtype e) const
{
    NvFlags<enumtype, storagetype> out(*this);
    out |= e;
    return out;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype> NvFlags<enumtype, storagetype>::
operator|(const NvFlags<enumtype, storagetype>& f) const
{
    NvFlags<enumtype, storagetype> out(*this);
    out |= f;
    return out;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>& NvFlags<enumtype, storagetype>::operator&=(enumtype e)
{
    mBits &= static_cast<storagetype>(e);
    return *this;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>& NvFlags<enumtype, storagetype>::
operator&=(const NvFlags<enumtype, storagetype>& f)
{
    mBits &= f.mBits;
    return *this;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype> NvFlags<enumtype, storagetype>::operator&(enumtype e) const
{
    NvFlags<enumtype, storagetype> out = *this;
    out.mBits &= static_cast<storagetype>(e);
    return out;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype> NvFlags<enumtype, storagetype>::
operator&(const NvFlags<enumtype, storagetype>& f) const
{
    NvFlags<enumtype, storagetype> out = *this;
    out.mBits &= f.mBits;
    return out;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>& NvFlags<enumtype, storagetype>::operator^=(enumtype e)
{
    mBits ^= static_cast<storagetype>(e);
    return *this;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>& NvFlags<enumtype, storagetype>::
operator^=(const NvFlags<enumtype, storagetype>& f)
{
    mBits ^= f.mBits;
    return *this;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype> NvFlags<enumtype, storagetype>::operator^(enumtype e) const
{
    NvFlags<enumtype, storagetype> out = *this;
    out.mBits ^= static_cast<storagetype>(e);
    return out;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype> NvFlags<enumtype, storagetype>::
operator^(const NvFlags<enumtype, storagetype>& f) const
{
    NvFlags<enumtype, storagetype> out = *this;
    out.mBits ^= f.mBits;
    return out;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype> NvFlags<enumtype, storagetype>::operator~(void) const
{
    NvFlags<enumtype, storagetype> out;
    out.mBits = storagetype(~mBits);
    return out;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>::operator bool(void) const
{
    return mBits ? true : false;
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>::operator uint8_t(void) const
{
    return static_cast<uint8_t>(mBits);
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>::operator uint16_t(void) const
{
    return static_cast<uint16_t>(mBits);
}

template <typename enumtype, typename storagetype>
NV_INLINE NvFlags<enumtype, storagetype>::operator uint32_t(void) const
{
    return static_cast<uint32_t>(mBits);
}

template <typename enumtype, typename storagetype>
NV_INLINE void NvFlags<enumtype, storagetype>::clear(enumtype e)
{
    mBits &= ~static_cast<storagetype>(e);
}

#if !NV_DOXYGEN
} // namespace nvidia
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVFLAGS_H

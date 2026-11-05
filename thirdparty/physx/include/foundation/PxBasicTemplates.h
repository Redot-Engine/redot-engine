// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BASIC_TEMPLATES_H
#define PX_BASIC_TEMPLATES_H

#include "foundation/PxPreprocessor.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
	template <typename A>
	struct PxEqual
	{
		bool operator()(const A& a, const A& b) const
		{
			return a == b;
		}
	};

	template <typename A>
	struct PxLess
	{
		bool operator()(const A& a, const A& b) const
		{
			return a < b;
		}
	};

	template <typename A>
	struct PxGreater
	{
		bool operator()(const A& a, const A& b) const
		{
			return a > b;
		}
	};

	template <class F, class S>
	class PxPair
	{
	public:
		F first;
		S second;
		PX_CUDA_CALLABLE PX_INLINE PxPair() : first(F()), second(S())
		{
		}
		PX_CUDA_CALLABLE PX_INLINE PxPair(const F& f, const S& s) : first(f), second(s)
		{
		}
		PX_CUDA_CALLABLE PX_INLINE PxPair(const PxPair& p) : first(p.first), second(p.second)
		{
		}
		PX_CUDA_CALLABLE PX_INLINE PxPair& operator=(const PxPair& p)
		{
			first = p.first;
			second = p.second;
			return *this;
		}
		PX_CUDA_CALLABLE PX_INLINE bool operator==(const PxPair& p) const
		{
			return first == p.first && second == p.second;
		}
		PX_CUDA_CALLABLE PX_INLINE bool operator<(const PxPair& p) const
		{
			if (first < p.first)
				return true;
			else
				return !(p.first < first) && (second < p.second);
		}
	};

	template <unsigned int A>
	struct PxLogTwo
	{
		static const unsigned int value = PxLogTwo<(A >> 1)>::value + 1;
	};
	template <>
	struct PxLogTwo<1>
	{
		static const unsigned int value = 0;
	};

	template <typename T>
	struct PxUnConst
	{
		typedef T Type;
	};
	template <typename T>
	struct PxUnConst<const T>
	{
		typedef T Type;
	};

	template <typename T>
	T PxPointerOffset(void* p, ptrdiff_t offset)
	{
		return reinterpret_cast<T>(reinterpret_cast<char*>(p) + offset);
	}
	template <typename T>
	T PxPointerOffset(const void* p, ptrdiff_t offset)
	{
		return reinterpret_cast<T>(reinterpret_cast<const char*>(p) + offset);
	}

	template <class T>
	PX_CUDA_CALLABLE PX_INLINE void PxSwap(T& x, T& y)
	{
		const T tmp = x;
		x = y;
		y = tmp;
	}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


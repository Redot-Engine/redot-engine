// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BITMAP_H
#define PX_BITMAP_H

#include "foundation/PxAssert.h"
#include "foundation/PxMath.h"
#include "foundation/PxMemory.h"
#include "foundation/PxAllocator.h"
#include "foundation/PxUserAllocated.h"
#include "foundation/PxIntrinsics.h"
#include "foundation/PxBitUtils.h"
#include "foundation/PxConstructor.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/*!
	Holds a bitmap with operations to set, reset or test individual bits.

	We inhibit copy to prevent unintentional copies. If a copy is desired copy() should be used or
	alternatively a copy constructor implemented.
	*/
	template <class Alloc = PxAllocatorTraits<PxU32>::Type>
	class PxBitMapBase : protected Alloc
	{
		PX_NOCOPY(PxBitMapBase)

	public:

		/*!
		Deserialization constructor
		*/
		explicit PxBitMapBase(const PxEMPTY)
		{
			if(mMap)
				mWordCount |= PX_SIGN_BITMASK;
		}

		PX_INLINE PxBitMapBase(const Alloc& alloc = Alloc()) : Alloc(alloc), mMap(0), mWordCount(0) {}

		/*!
		Bitmap destructor
		*/
		PX_INLINE ~PxBitMapBase()
		{
			release();
		}

		/*!
		Deallocates bitmap and sets word count to 0.
		*/
		PX_INLINE void release()
		{
			if(mMap && !isInUserMemory())
				Alloc::deallocate(mMap, NULL);
			mMap = NULL;
			// also resets the isInUserMemory bit
			mWordCount = 0;
		}

		/*!
		Gets allocator used for bitmap alloction.
		\return
		Reference to allocator
		*/
		PX_INLINE Alloc& getAllocator() { return *this; }

		/*!
		Extends the bitmap if necessary and sets bit at the given index.
		\param index
		Index of the bit to be set.
		\return
		True if operation succeeds, false otherwise.
		*/
		PX_INLINE bool growAndSet(PxU32 index)
		{
			if (!extend(index + 1))
				return false;
			mMap[index >> 5] |= 1 << (index & 31);
			return true;
		}

		/*!
		Extends the bitmap if necessary and clears bit at the given index.
		\param index
		Index of the bit to be cleared.
		\return
		True if operation succeeds, false otherwise.
		*/
		PX_INLINE bool growAndReset(PxU32 index)
		{
			if (!extend(index + 1))
				return false;
			mMap[index >> 5] &= ~(1 << (index & 31));
			return true;
		}

		/*!
		Tests value of bit at the given index, if the index is in bounds.
		\param index
		Index of the bit to test.
		\return
		A value > 0 if the bit at the index is set and index is in bounds, 0 if the bit is not set or if index is out of bounds.
		*/
		PX_INLINE PxIntBool boundedTest(PxU32 index) const
		{
			return PxIntBool(index >> 5 >= getWordCount() ? PxIntFalse : (mMap[index >> 5] & (1 << (index & 31))));
		}

		/*!
		Clears the bit at the given index, if the index is in bounds.
		\param index
		Index of the bit to clear.
		*/
		PX_INLINE void boundedReset(PxU32 index)
		{
			if((index >> 5) < getWordCount())
				mMap[index >> 5] &= ~(1 << (index & 31));
		}

		/*!
		Sets the bit at the given index.
		
		The index must be in bounds (i.e. index < getWordCount() * 32) or behavior is undefined.
		
		\param index
		Index of the bit to set.
		*/
		PX_INLINE void set(PxU32 index)
		{
			PX_ASSERT(index<getWordCount() * 32);
			mMap[index >> 5] |= 1 << (index & 31);
		}

		/*!
		Clears bit at the given index.

		The index must be in bounds (i.e. index < getWordCount() * 32) or behavior is undefined.

		\param index
		Index of the bit to clear.
		*/
		PX_INLINE void reset(PxU32 index)
		{
			PX_ASSERT(index<getWordCount() * 32);
			mMap[index >> 5] &= ~(1 << (index & 31));
		}

		/*!
		Tests value of the bit at the given index.

		The index must be in bounds (i.e. index < getWordCount() * 32) or behavior is undefined.

		\param index
		Index of the bit to test.
		\return
		A value > 0 if the bit at the index is set, 0 if the bit is not set.
		*/
		PX_INLINE PxIntBool test(PxU32 index) const
		{
			PX_ASSERT(index<getWordCount() * 32);
			return PxIntBool(mMap[index >> 5] & (1 << (index & 31)));
		}

		/*!
		Returns the 4-bit value (nibble) at the given nibble index.

		The nibble index must be in bounds (i.e. \p nibIndex < getWordCount() * 8) or behavior is undefined.

		\param nibIndex
		Index of the nibble to retrieve.
		\return
		The 4-bit value at the nibble index.
		*/
		PX_INLINE PxU32 getNibbleFast(PxU32 nibIndex) const
		{
			const PxU32 bitIndex = nibIndex << 2;
			PX_ASSERT(bitIndex < getWordCount() * 32);
			return (mMap[bitIndex >> 5] >> (bitIndex & 31)) & 0xf;
		}

		/*!
		Performs a bitwise AND of the 4-bit value (nibble) at the given nibble index
		with the specified mask.

		The nibble index must be in bounds (i.e. \p nibIndex < getWordCount() * 8)
		or behavior is undefined. Only the lower 4 bits of \p mask are relevant.

		\param nibIndex
		Index of the nibble to modify.
		\param mask
		4-bit mask to AND with the nibble.
		*/
		PX_INLINE void andNibbleFast(PxU32 nibIndex, PxU32 mask)
		{
			//TODO: there has to be a faster way...
			const PxU32 bitIndex = nibIndex << 2;
			const PxU32 shift = (bitIndex & 31);
			const PxU32 nibMask = (0xfu << shift);

			PX_ASSERT(bitIndex < getWordCount() * 32);

			mMap[bitIndex >> 5] &= ((mask << shift) | ~nibMask);
		}

		/*!
		Performs a bitwise OR of the 4-bit value (nibble) at the given nibble index
		with the specified mask.

		The nibble index must be in bounds (i.e. \p nibIndex < getWordCount() * 8)
		or behavior is undefined. Only the lower 4 bits of \p mask are relevant.

		\param nibIndex
		Index of the nibble to modify.
		\param mask
		4-bit mask to OR with the nibble.
		*/
		PX_INLINE void orNibbleFast(PxU32 nibIndex, PxU32 mask)
		{
			PX_ASSERT(!(mask & ~0xfu)); //check extra bits are not set

			const PxU32 bitIndex = nibIndex << 2;
			const PxU32 shift = bitIndex & 31;

			PX_ASSERT(bitIndex < getWordCount() * 32);

			mMap[bitIndex >> 5] |= (mask << shift);
		}

		/*!
		Clears bitmap.
		*/
		void clear()
		{
			PxMemSet(mMap, 0, getWordCount() * sizeof(PxU32));
		}

		/*!
		Resizes the bitmap to hold at least the given number of bits.

		\param newBitCount
		The required bit capacity of the bitmap.
		\return
		True if the bitmap was successfully resized; false otherwise.
		*/
		bool resize(PxU32 newBitCount)
		{
			return extend(newBitCount);
		}

		/*!
		Resizes the bitmap to hold at least the given number of bits and clears all bits.
		\param newBitCount
		The required bit capacity of the bitmap.
		\return
		True if operation succeeds, false otherwise.
		*/
		bool resizeAndClear(PxU32 newBitCount)
		{
			if (!extendUninitialized(newBitCount))
				return false;
			PxMemSet(mMap, 0, getWordCount() * sizeof(PxU32));
			return true;
		}

		/*!
		Sets bitmap empty by deallocating the bitmap data and setting word count to 0.
		*/
		void setEmpty()
		{
			release();
		}

		/*!
		Sets bitmap to external 32-bit word data, and marks the data as user owned.

		\param map
		Pointer to external 32-bit word data.
		\param wordCount
		Number of 32-bit words in the external data.
		*/
		void setWords(PxU32* map, PxU32 wordCount)
		{
			mMap = map;
			mWordCount = wordCount | PX_SIGN_BITMASK;
		}

		/*!
		Returns the number of bits represented by the bitmap.
		\return
		Number of bits stored in the bitmap.
		*/
		PX_FORCE_INLINE	PxU32 size() const { return getWordCount() * 32; }

		/*!
		Copies the contents of another bitmap, extending this bitmap if necessary.

		After the call, this bitmap will hold at least as many bits as \p a.
		Bits beyond the size of \p a (if any) are cleared.

		\param a
		Source bitmap to copy from.
		\return
		True if the bitmap could be extended (if needed) and copied, false otherwise.
		*/
		bool copy(const PxBitMapBase& a)
		{
			if (!extendUninitialized(a.getWordCount() << 5))
				return false;
			PxMemCopy(mMap, a.mMap, a.getWordCount() * sizeof(PxU32));
			if(getWordCount() > a.getWordCount())
				PxMemSet(mMap + a.getWordCount(), 0, (getWordCount() - a.getWordCount()) * sizeof(PxU32));
			return true;
		}

		/*!
		Counts the number of set bits in the bitmap.
		\return
		The number of bits set to 1.
		*/
		PX_INLINE PxU32 count()		const
		{
			// NOTE: we can probably do this faster, since the last steps in PxBitCount can be defered to
			// the end of the seq. + 64/128bits at a time + native bit counting instructions.
			PxU32 count = 0;
			const PxU32 wordCount = getWordCount();
			for(PxU32 i = 0; i<wordCount; i++)
				count += PxBitCount(mMap[i]);

			return count;
		}

		/*!
		Counts the number of set bits in a range of the bitmap.

		The range starts at \p start and spans \p length bits. Bits outside the
		bounds of the bitmap are ignored.

		\param start
		Index of the first bit to test.
		\param length
		Number of bits to include in the count.
		\return
		The number of bits set to 1 in the specified range.
		*/
		PX_INLINE PxU32 count(PxU32 start, PxU32 length) const
		{
			const PxU32 end = PxMin(getWordCount() << 5, start + length);
			PxU32 count = 0;
			for(PxU32 i = start; i<end; i++)
				count += (test(i) != 0);
			return count;
		}

		/*!
		Returns the index of the highest set bit in the bitmap.

		If no bits are set, this function returns 0! As a result, a return value
		of 0 is ambiguous and may also indicate that bit 0 is set.

		\return
		Index of the highest set bit, or 0 if no bits are set.
		*/
		PxU32 findLast() const
		{
			const PxU32 wordCount = getWordCount();
			for(PxU32 i = wordCount; i-- > 0;)
			{
				if(mMap[i])
					return (i << 5) + PxHighestSetBit(mMap[i]);
			}
			return PxU32(0);
		}

		/*!
		Returns whether any bit is set in the bitmap.

		\return
		True if at least one bit is set; false otherwise.
		*/
		bool hasAnyBitSet() const
		{
			const PxU32 wordCount = getWordCount();
			for(PxU32 i = 0; i<wordCount; i++)
			{
				if (mMap[i])
					return true;
			}
			return false;
		}

		// the obvious combiners and some used in the SDK

		struct OR { PX_INLINE PxU32 operator()(PxU32 a, PxU32 b) { return a | b; } };
		struct AND { PX_INLINE PxU32 operator()(PxU32 a, PxU32 b) { return a&b; } };
		struct XOR { PX_INLINE PxU32 operator()(PxU32 a, PxU32 b) { return a^b; } };

		// we use auxiliary functions here so as not to generate combiners for every combination
		// of allocators

		template<class Combiner, class _>
		PX_INLINE bool combineInPlace(const PxBitMapBase<_>& b)
		{
			return combine1<Combiner>(b.mMap, b.getWordCount());
		}

		template<class Combiner, class _1, class _2>
		PX_INLINE bool combine(const PxBitMapBase<_1>& a, const PxBitMapBase<_2>& b)
		{
			return combine2<Combiner>(a.mMap, a.getWordCount(), b.mMap, b.getWordCount());
		}

		PX_FORCE_INLINE const PxU32*	getWords()			const	{ return mMap; }
		PX_FORCE_INLINE PxU32*			getWords()					{ return mMap; }

		// PX_SERIALIZATION
		PX_FORCE_INLINE PxU32			getWordCount()		const	{ return mWordCount & ~PX_SIGN_BITMASK; }

		// We need one bit to mark arrays that have been deserialized from a user-provided memory block.
		PX_FORCE_INLINE	PxU32			isInUserMemory()	const	{ return mWordCount & PX_SIGN_BITMASK; }
		//~PX_SERIALIZATION

		/*!
		Iterate over indices in a bitmap

		This iterator is good because it finds the set bit without looping over the cached bits upto 31 times.
		However it does require a variable shift.
		*/
		class Iterator
		{
		public:
			static const PxU32 DONE = 0xffffffff;

			PX_INLINE Iterator(const PxBitMapBase &map) : mBitMap(map)
			{
				reset();
			}

			PX_INLINE Iterator& operator=(const Iterator& other)
			{
				PX_ASSERT(&mBitMap == &other.mBitMap);
				mBlock = other.mBlock;
				mIndex = other.mIndex;
				return *this;
			}

			PX_INLINE PxU32	getNext()
			{
				if(mBlock)
				{
					PxU32 block = mBlock;
					PxU32 index = mIndex;

					const PxU32 bitIndex = index << 5 | PxLowestSetBit(block);
					block &= block - 1;
					PxU32 wordCount = mBitMap.getWordCount();
					while(!block && ++index < wordCount)
						block = mBitMap.mMap[index];

					mBlock = block;
					mIndex = index;

					return bitIndex;
				}
				return DONE;
			}

			PX_INLINE void reset()
			{
				PxU32 index = 0;
				PxU32 block = 0;

				PxU32 wordCount = mBitMap.getWordCount();
				while(index < wordCount && ((block = mBitMap.mMap[index]) == 0))
					++index;

				mBlock = block;
				mIndex = index;
			}
		private:
			PxU32 mBlock, mIndex;
			const PxBitMapBase& mBitMap;
		};

		// DS: faster but less general: hasBits() must be true or getNext() is illegal so it is the calling code's responsibility to ensure that getNext() is not called illegally.
		class PxLoopIterator
		{
			PX_NOCOPY(PxLoopIterator)

		public:
			PX_FORCE_INLINE PxLoopIterator(const PxBitMapBase &map) : mMap(map.getWords()), mBlock(0), mIndex(-1), mWordCount(PxI32(map.getWordCount())) {}

			PX_FORCE_INLINE bool hasBits()
			{
				PX_ASSERT(mIndex<mWordCount);
				while (mBlock == 0)
				{
					if (++mIndex == mWordCount)
						return false;
					mBlock = mMap[mIndex];
				}
				return true;
			}

			PX_FORCE_INLINE PxU32 getNext()
			{
				PX_ASSERT(mIndex<mWordCount && mBlock != 0);
				PxU32 result = PxU32(mIndex) << 5 | PxLowestSetBit(mBlock);	// will assert if mask is zero
				mBlock &= (mBlock - 1);
				return result;
			}

		private:
			const PxU32*const mMap;
			PxU32 mBlock;		// the word we're currently scanning
			PxI32 mIndex;		// the index of the word we're currently looking at
			PxI32 mWordCount;
		};

		/*!
		Class to iterate over the bitmap from a particular start location rather than the beginning of
		the list. Once the end of the bitmap is reached the iteration wraps around and resumes from the
		first word, stopping when it gets back to the word it started from.

		Note that the iteration is word-granular: it starts at the beginning of the word containing
		\p startIndex, so bits between that word boundary and \p startIndex are visited first.
		*/
		class PxCircularIterator
		{
		public:
			static const PxU32 DONE = 0xffffffff;

			/*!
			\param map
			The bitmap to iterate over.
			\param startIndex
			Index of the bit to start from (see the word-granularity note above).
			*/
			PX_INLINE PxCircularIterator(const PxBitMapBase &map, PxU32 startIndex) : mBitMap(map)
			{
				const PxU32 wordCount = mBitMap.getWordCount();

				PxU32 localIndex = 0;
				const PxU32 startWord = startIndex >> 5;
				if(startWord < wordCount)
					localIndex = startWord;

				const PxU32 firstIndex = localIndex;

				PxU32 block = 0;
				if(localIndex < wordCount)
				{
					block = mBitMap.mMap[localIndex];
					if(block == 0)
					{
						localIndex = (localIndex + 1) % wordCount;
						while(localIndex != firstIndex && (block = mBitMap.mMap[localIndex]) == 0)
							localIndex = (localIndex + 1) % wordCount;
					}
				}

				mIndex = localIndex;
				mBlock = block;
				mStartIndex = firstIndex;
			}

			PX_INLINE PxU32	getNext()
			{
				if(mBlock)
				{
					PxU32 index = mIndex;
					PxU32 block = mBlock;
					const PxU32 startIndex = mStartIndex;

					PxU32 bitIndex = index << 5 | PxLowestSetBit(block);
					block &= block - 1;
					PxU32 wordCount = mBitMap.getWordCount();
					while (!block && (index = ((index + 1) % wordCount)) != startIndex)
						block = mBitMap.mMap[index];

					mIndex = index;
					mBlock = block;

					return bitIndex;
				}
				return DONE;
			}

		private:
			PxU32 mBlock, mIndex;
			PxU32 mStartIndex;
			const PxBitMapBase& mBitMap;

			PX_NOCOPY(PxCircularIterator)
		};

	protected:
		PxU32*		mMap;			//one bit per index
		PxU32		mWordCount;
		PxAllocator	mAllocator;
		PxU8		mPadding[3];	// PT: "mAllocator" is empty but consumes 1 byte

		bool extend(PxU32 size)
		{
			const PxU32 newWordCount = (size + 31) >> 5;
			if (newWordCount > getWordCount())
			{
				uint32_t cookie = 0;
				void* newMap = Alloc::allocate(newWordCount * sizeof(PxU32), PX_FL, &cookie);
				if (!newMap)
					return false;

				if (mMap)
				{
					PxMemCopy(newMap, mMap, getWordCount() * sizeof(PxU32));
					if (!isInUserMemory())
						Alloc::deallocate(mMap, &cookie);
				}
				mMap = reinterpret_cast<PxU32*>(newMap);
				PxMemSet(mMap + getWordCount(), 0, (newWordCount - getWordCount()) * sizeof(PxU32));
				// also resets the isInUserMemory bit
				mWordCount = newWordCount;
			}
			return true;
		}

		bool extendUninitialized(PxU32 size)
		{
			PxU32 newWordCount = (size + 31) >> 5;
			if (newWordCount > getWordCount())
			{
				uint32_t cookie = 0;
				void* newMap = Alloc::allocate(newWordCount * sizeof(PxU32), PX_FL, &cookie);
				if(!newMap)
					return false;

				if(mMap && !isInUserMemory())
					Alloc::deallocate(mMap, &cookie);

				mMap = reinterpret_cast<PxU32*>(newMap);

				// also resets the isInUserMemory bit
				mWordCount = newWordCount;
			}
			return true;
		}

		template<class Combiner>
		bool combine1(const PxU32* words, PxU32 length)
		{
			if (!extend(length << 5))
				return false;

			PxU32 combineLength = PxMin(getWordCount(), length);
			for (PxU32 i = 0; i<combineLength; i++)
				mMap[i] = Combiner()(mMap[i], words[i]);

			return true;
		}

		template<class Combiner>
		bool combine2(const PxU32* words1, PxU32 length1,
			const PxU32* words2, PxU32 length2)
		{
			if(!extendUninitialized(PxMax(length1, length2) << 5))
				return false;

			PxU32 commonSize = PxMin(length1, length2);

			for (PxU32 i = 0; i<commonSize; i++)
				mMap[i] = Combiner()(words1[i], words2[i]);

			for (PxU32 i = commonSize; i<length1; i++)
				mMap[i] = Combiner()(words1[i], 0);

			for (PxU32 i = commonSize; i<length2; i++)
				mMap[i] = Combiner()(0, words2[i]);

			return true;
		}

		friend class Iterator;
	};

	/*!
	Convenience alias for PxBitMapBase using the default allocator.

	Use PxBitMapBase directly if you need a custom allocator type.
	*/
	using PxBitMap = PxBitMapBase<>;

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

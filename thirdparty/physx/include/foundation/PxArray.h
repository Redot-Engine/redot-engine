// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_ARRAY_H
#define PX_ARRAY_H

#include "foundation/PxAssert.h"
#include "foundation/PxMathIntrinsics.h"
#include "foundation/PxAllocator.h"
#include "foundation/PxBasicTemplates.h"
#include "foundation/PxMemory.h"
#include "foundation/PxIO.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/*!
\brief An array is a sequential container.

Implementation note:
- entries between 0 and size are valid objects
- we use inheritance to build this because the array is included inline in a lot
  of objects and we want the allocator to take no space if it's not stateful, which
  aggregation doesn't allow. Also, we want the metadata at the front for the inline
  case where the allocator contains some inline storage space

\tparam T Element type
\tparam Alloc Allocator type (defaults to PxAllocatorTraits<T>::Type)
*/
#if !PX_DOXYGEN
template <class T, class Alloc = typename PxAllocatorTraits<T>::Type>
class PxArray : protected Alloc
#else
template <class T, class Alloc> class PxArray
#endif
{
  public:
	typedef T* Iterator;
	typedef const T* ConstIterator;

	/*!
	\brief Deserialization constructor.
	\param v Empty tag for deserialization
	*/
	explicit PxArray(const PxEMPTY v) : Alloc(v)
	{
		if(mData)
			mCapacity |= PX_SIGN_BITMASK;
	}

	/*!
	\brief Default array constructor. Initialize an empty array
	\param alloc Allocator to use
	*/
	PX_INLINE explicit PxArray(const Alloc& alloc = Alloc()) : Alloc(alloc), mData(0), mSize(0), mCapacity(0)
	{
	}

	/*!
	\brief Initialize array with given size
	\param size Initial size (and capacity) of the array
	\param a Default value for elements
	\param alloc Allocator to use
	*/
	PX_INLINE explicit PxArray(uint32_t size, const T& a = T(), const Alloc& alloc = Alloc())
	: Alloc(alloc), mData(0), mSize(0), mCapacity(0)
	{
		resize(size, a);
	}

	/*!
	\brief Copy-constructor. Copy all entries from other array
	\param other Source array to copy from
	\param alloc Allocator to use
	*/
	template <class A>
	PX_INLINE explicit PxArray(const PxArray<T, A>& other, const Alloc& alloc = Alloc())
	: Alloc(alloc)
	{
		copy(other);
	}

	// This is necessary else the basic default copy constructor is used in the case of both arrays being of the same
	// template instance
	// The C++ standard clearly states that a template constructor is never a copy constructor [2]. In other words,
	// the presence of a template constructor does not suppress the implicit declaration of the copy constructor.
	// Also never make a copy constructor explicit, or copy-initialization* will no longer work. This is because
	// 'binding an rvalue to a const reference requires an accessible copy constructor' (http://gcc.gnu.org/bugs/)
	// *http://stackoverflow.com/questions/1051379/is-there-a-difference-in-c-between-copy-initialization-and-assignment-initializ
	PX_INLINE PxArray(const PxArray& other, const Alloc& alloc = Alloc()) : Alloc(alloc)
	{
		copy(other);
	}

	/*!
	\brief Initialize array with given length
	\param first Pointer to the first element to copy
	\param last Pointer past the last element to copy
	\param alloc Allocator to use
	*/
	PX_INLINE explicit PxArray(const T* first, const T* last, const Alloc& alloc = Alloc())
	: Alloc(alloc), mSize(last < first ? 0 : uint32_t(last - first)), mCapacity(mSize)
	{
		mData = allocate(mSize);
		if(mData)
			copy(mData, mData + mSize, first);
	}

	/*!
	\brief Destructor
	*/
	PX_INLINE ~PxArray()
	{
		destroy(mData, mData + mSize);

		if(capacity() && !isInUserMemory())
			deallocate(mData);
	}

	/*!
	\brief Assignment operator. Copy content (deep-copy)
	\param rhs Source array to copy from
	\return Reference to this array
	*/
	template <class A>
	PX_INLINE PxArray& operator=(const PxArray<T, A>& rhs)
	{
		if(&rhs == this)
			return *this;

		clear();
		reserve(rhs.mSize);
		copy(mData, mData + rhs.mSize, rhs.mData);

		mSize = rhs.mSize;
		return *this;
	}

	PX_INLINE PxArray& operator=(const PxArray& t) // Needs to be declared, see comment at copy-constructor
	{
		return operator=<Alloc>(t);
	}

	/*!
	\brief Array indexing operator.
	\param i The index of the element that will be returned.
	\return The element i in the array.
	*/
	PX_FORCE_INLINE const T& operator[](uint32_t i) const
	{
		PX_ASSERT(i < mSize);
		return mData[i];
	}

	/*!
	\brief Array indexing operator.
	\param i The index of the element that will be returned.
	\return The element i in the array.
	*/
	PX_FORCE_INLINE T& operator[](uint32_t i)
	{
		PX_ASSERT(i < mSize);
		return mData[i];
	}

	/*!
	\brief Returns a pointer to the first element of the array.
	\return Pointer to the first element of the array.
	*/
	PX_FORCE_INLINE ConstIterator begin() const
	{
		return mData;
	}

	PX_FORCE_INLINE Iterator begin()
	{
		return mData;
	}

	/*!
	\brief Returns a pointer beyond the last element of the array. Do not dereference.
	\return Pointer to the element beyond the last element of the array.
	*/
	PX_FORCE_INLINE ConstIterator end() const
	{
		return mData + mSize;
	}

	PX_FORCE_INLINE Iterator end()
	{
		return mData + mSize;
	}

	/*!
	\brief Returns a reference to the first element of the array. Undefined if the array is empty.
	\return Reference to the first element of the array
	*/
	PX_FORCE_INLINE const T& front() const
	{
		PX_ASSERT(mSize);
		return mData[0];
	}

	PX_FORCE_INLINE T& front()
	{
		PX_ASSERT(mSize);
		return mData[0];
	}

	/*!
	\brief Returns a reference to the last element of the array. Undefined if the array is empty.
	\return Reference to the last element of the array
	*/
	PX_FORCE_INLINE const T& back() const
	{
		PX_ASSERT(mSize);
		return mData[mSize - 1];
	}

	PX_FORCE_INLINE T& back()
	{
		PX_ASSERT(mSize);
		return mData[mSize - 1];
	}

	/*!
	\brief Returns the number of entries in the array. This can, and probably will,
	differ from the array capacity.
	\return The number of entries in the array.
	*/
	PX_FORCE_INLINE uint32_t size() const
	{
		return mSize;
	}

	/*!
	\brief Clears the array.
	Calls destructor on all elements.
	*/
	PX_INLINE void clear()
	{
		destroy(mData, mData + mSize);
		mSize = 0;
	}

	/*!
	\brief Returns whether the array is empty (i.e. whether its size is 0).
	\return True if the array is empty
	*/
	PX_FORCE_INLINE bool empty() const
	{
		return mSize == 0;
	}

	/*!
	\brief Finds the first occurrence of an element in the array.
	\param a The element to find.
	\return Pointer to the found element or end() if not found
	*/
	PX_INLINE Iterator find(const T& a)
	{
		uint32_t index;
		for(index = 0; index < mSize && mData[index] != a; index++)
			;
		return mData + index;
	}

	PX_INLINE ConstIterator find(const T& a) const
	{
		uint32_t index;
		for(index = 0; index < mSize && mData[index] != a; index++)
			;
		return mData + index;
	}

	/////////////////////////////////////////////////////////////////////////
	/*!
	\brief Adds one element to the end of the array. Operation is O(1).
	\param a The element that will be added to this array.
	\return Pointer to the element that has been added, NULL if operation failed.
	*/
	/////////////////////////////////////////////////////////////////////////
	PX_FORCE_INLINE T* pushBack(const T& a)
	{
		if(capacity() <= mSize)
			return growAndPushBack(a);

		PX_PLACEMENT_NEW(reinterpret_cast<void*>(mData + mSize), T)(a);

		return &mData[mSize++];
	}

	/////////////////////////////////////////////////////////////////////////
	/*!
	\brief Removes and returns the element at the end of the array. Only legal if the array is non-empty.
	\return The last element of the array
	*/
	/////////////////////////////////////////////////////////////////////////
	PX_INLINE T popBack()
	{
		PX_ASSERT(mSize);
		T t = mData[mSize - 1];

		mData[--mSize].~T();

		return t;
	}

	/////////////////////////////////////////////////////////////////////////
	/*!
	\brief Construct one element at the end of the array. Operation is O(1).
	\return Pointer to the element that has been inserted, NULL if operation failed.
	*/
	/////////////////////////////////////////////////////////////////////////
	PX_INLINE T* insert()
	{
		if(capacity() <= mSize)
		{
			if (!grow(capacityIncrement()))
			{
				return NULL;
			}
		}

		T* ptr = mData + mSize++;
		PX_PLACEMENT_NEW(ptr, T); // not 'T()' because PODs should not get default-initialized.
		return ptr;
	}

	/////////////////////////////////////////////////////////////////////////
	/*!
	\brief Removes the element at position i from the array and replaces it with
	the last element. Operation is O(1)
	\param i The position of the element that will be removed from this array.
	*/
	/////////////////////////////////////////////////////////////////////////
	PX_INLINE void replaceWithLast(uint32_t i)
	{
		PX_ASSERT(i < mSize);
		mData[i] = mData[--mSize];

		mData[mSize].~T();
	}

	PX_INLINE void replaceWithLast(Iterator i)
	{
		replaceWithLast(static_cast<uint32_t>(i - mData));
	}

	/////////////////////////////////////////////////////////////////////////
	/*!
	\brief Replaces the first occurrence of the element a with the last element.
	Operation is O(n)
	\param a The element to find and remove from this array.
	\return True if the element has been removed.
	*/
	/////////////////////////////////////////////////////////////////////////
	PX_INLINE bool findAndReplaceWithLast(const T& a)
	{
		uint32_t index = 0;
		while(index < mSize && mData[index] != a)
			++index;
		if(index == mSize)
			return false;
		replaceWithLast(index);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////
	/*!
	\brief Subtracts the element on position i from the array. Shift the entire
	array one step. Operation is O(n)
	\param i The position of the element that will be subtracted from this array.
	*/
	/////////////////////////////////////////////////////////////////////////
	PX_INLINE void remove(uint32_t i)
	{
		PX_ASSERT(i < mSize);

		T* it = mData + i;
		it->~T();
		while (++i < mSize)
		{
			PX_PLACEMENT_NEW(it, T(mData[i]));
			++it;
			it->~T();
		} 
		--mSize;
	}

	/////////////////////////////////////////////////////////////////////////
	/*!
	\brief Removes a range from the array.  Shifts the array so order is maintained.
	Operation is O(n)
	\param begin The starting position of the element that will be subtracted from this array.
	\param count The number of elements that will be subtracted from this array.
	*/
	/////////////////////////////////////////////////////////////////////////
	PX_INLINE void removeRange(uint32_t begin, uint32_t count)
	{
		PX_ASSERT(begin < mSize);
		PX_ASSERT((begin + count) <= mSize);

		for(uint32_t i = 0; i < count; i++)
			mData[begin + i].~T(); // call the destructor on the ones being removed first.

		T* dest = &mData[begin];                       // location we are copying the tail end objects to
		T* src = &mData[begin + count];                // start of tail objects
		uint32_t move_count = mSize - (begin + count); // compute remainder that needs to be copied down

		for(uint32_t i = 0; i < move_count; i++)
		{
			PX_PLACEMENT_NEW(dest, T(*src));	// copy the old one to the new location
			src->~T();							// call the destructor on the old location
			dest++;
			src++;
		}
		mSize -= count;
	}

	//////////////////////////////////////////////////////////////////////////
	/*!
	\brief Resize array
	\param size New size of the array
	\param a Default value for new elements
	\return True if operation succeeded
	*/
	//////////////////////////////////////////////////////////////////////////
	PX_NOINLINE bool resize(const uint32_t size, const T& a = T());

	/*!
	\brief Resize array without initializing new elements
	\param size New size of the array
	\return True if operation succeeded
	*/
	PX_NOINLINE bool resizeUninitialized(const uint32_t size);

	//////////////////////////////////////////////////////////////////////////
	/*!
	\brief Resize array such that only as much memory is allocated to hold the
	existing elements
	\return True if operation succeeds, false otherwise.
	*/
	//////////////////////////////////////////////////////////////////////////
	PX_INLINE bool shrink()
	{
		return recreate(mSize);
	}

	//////////////////////////////////////////////////////////////////////////
	/*!
	\brief Deletes all array elements and frees memory.
	*/
	//////////////////////////////////////////////////////////////////////////
	PX_INLINE void reset()
	{
		resize(0);
		shrink();
	}

	//////////////////////////////////////////////////////////////////////////
	/*!
	\brief Resets or clears the array depending on occupancy.
	*/
	//////////////////////////////////////////////////////////////////////////
	PX_INLINE void resetOrClear()
	{
		const PxU32 c = capacity();
		const PxU32 s = size();
		if(s>=c/2)
			clear();
		else
			reset();
	}

	//////////////////////////////////////////////////////////////////////////
	/*!
	\brief Ensure that the array has at least the requested capacity.
	\param capacity Minimum capacity to reserve
	\return True if operation succeeds, false otherwise.
	*/
	//////////////////////////////////////////////////////////////////////////
	PX_INLINE bool reserve(const uint32_t capacity)
	{
		if(capacity > this->capacity())
			return grow(capacity);
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	/*!
	\brief Query the capacity(allocated mem) for the array.
	\return The capacity of the array
	*/
	//////////////////////////////////////////////////////////////////////////
	PX_FORCE_INLINE uint32_t capacity() const
	{
		return mCapacity & ~PX_SIGN_BITMASK;
	}

	//////////////////////////////////////////////////////////////////////////
	/*!
	\brief Unsafe function to force the size of the array
	\param size New size to force
	*/
	//////////////////////////////////////////////////////////////////////////
	PX_FORCE_INLINE void forceSize_Unsafe(uint32_t size)
	{
		PX_ASSERT(size <= capacity());
		mSize = size;
	}

	//////////////////////////////////////////////////////////////////////////
	/*!
	\brief Swap contents of an array without allocating temporary storage
	\param other Array to swap with
	*/
	//////////////////////////////////////////////////////////////////////////
	PX_INLINE void swap(PxArray<T, Alloc>& other)
	{
		PxSwap(mData, other.mData);
		PxSwap(mSize, other.mSize);
		PxSwap(mCapacity, other.mCapacity);
	}

	//////////////////////////////////////////////////////////////////////////
	/*!
	\brief Assign a range of values to this array (resizes to length of range)
	\param first Pointer to the first element to copy
	\param last Pointer past the last element to copy
	\return True if operation succeeds, false otherwise.
	*/
	//////////////////////////////////////////////////////////////////////////
	PX_INLINE bool assign(const T* first, const T* last)
	{
		if(!resizeUninitialized(uint32_t(last - first)))
			return false;

		copy(begin(), end(), first);
		return true;
	}

	// We need one bit to mark arrays that have been deserialized from a user-provided memory block.
	// For alignment & memory saving purpose we store that bit in the rarely used capacity member.
	PX_FORCE_INLINE uint32_t isInUserMemory() const
	{
		return mCapacity & PX_SIGN_BITMASK;
	}

	/// return reference to allocator
	PX_INLINE Alloc& getAllocator()
	{
		return *this;
	}

  protected:
	// constructor for where we don't own the memory
	PxArray(T* memory, uint32_t size, uint32_t capacity, const Alloc& alloc = Alloc())
	: Alloc(alloc), mData(memory), mSize(size), mCapacity(capacity | PX_SIGN_BITMASK)
	{
	}

	template <class A>
	PX_NOINLINE void copy(const PxArray<T, A>& other);

	PX_INLINE T* allocate(uint32_t size, uint32_t* cookie=NULL)
	{
		// always call Alloc::allocate to respect cookie logic even if size is 0
		T* p = reinterpret_cast<T*>(Alloc::allocate(sizeof(T) * size, PX_FL, cookie));
		if(size > 0)
		{
			if(p)
			{
				PxMarkSerializedMemory(p, sizeof(T) * size);
			}
			else
			{
				PxGetFoundation().error(PxErrorCode::eOUT_OF_MEMORY, PX_FL, "PxArray::allocate: allocator returned null pointer.");
			}
		}
		return p;
	}

	PX_INLINE void deallocate(void* mem, uint32_t* cookie=NULL)
	{
		Alloc::deallocate(mem, cookie);
	}

	static PX_INLINE void create(T* first, T* last, const T& a)
	{
		for(; first < last; ++first)
			::PX_PLACEMENT_NEW(first, T(a));
	}

	static PX_INLINE void copy(T* first, T* last, const T* src)
	{
		if(last <= first)
			return;

		for(; first < last; ++first, ++src)
			::PX_PLACEMENT_NEW(first, T(*src));
	}

	static PX_INLINE void destroy(T* first, T* last)
	{
		for(; first < last; ++first)
			first->~T();
	}

	/*!
	\brief Called when pushBack() needs to grow the array.
	\param a The element that will be added to this array.
	\return Pointer to the pushed element or NULL if the operation failed
	*/
	PX_NOINLINE T* growAndPushBack(const T& a);

	/*!
	\brief Resizes the available memory for the array.
	\param capacity The number of entries that the array should be able to hold.
	\return True if operation succeeds, false otherwise.
	*/
	PX_INLINE bool grow(uint32_t capacity)
	{
		PX_ASSERT(this->capacity() < capacity);
		return recreate(capacity);
	}

	/*!
	\brief Creates a new memory block, copies all entries to the new block and destroys old entries.
	\param capacity The number of entries that the array should be able to hold.
	\return True if operation succeeds, false otherwise.
	*/
	PX_NOINLINE bool recreate(uint32_t capacity);

	// The idea here is to prevent accidental bugs with pushBack or insert. Unfortunately
	// it interacts badly with InlineArrays with smaller inline allocations.
	// TODO(dsequeira): policy template arg, this is exactly what they're for.
	PX_INLINE uint32_t capacityIncrement() const
	{
		const uint32_t capacity = this->capacity();
		return capacity == 0 ? 1 : capacity * 2;
	}

	T* mData;
	uint32_t mSize;
	uint32_t mCapacity;
};

template <class T, class Alloc>
PX_NOINLINE bool PxArray<T, Alloc>::resize(const uint32_t size, const T& a)
{
	if(!reserve(size))
		return false;
	create(mData + mSize, mData + size, a);
	destroy(mData + size, mData + mSize);
	mSize = size;
	return true;
}

template <class T, class Alloc>
template <class A>
PX_NOINLINE void PxArray<T, Alloc>::copy(const PxArray<T, A>& other)
{
	if(!other.empty())
	{
		mData = allocate(mSize = mCapacity = other.size());
		if(mData)
			copy(mData, mData + mSize, other.begin());
	}
	else
	{
		mData = NULL;
		mSize = 0;
		mCapacity = 0;
	}

	// mData = allocate(other.mSize);
	// mSize = other.mSize;
	// mCapacity = other.mSize;
	// copy(mData, mData + mSize, other.mData);
}

template <class T, class Alloc>
PX_NOINLINE bool PxArray<T, Alloc>::resizeUninitialized(const uint32_t size)
{
	if(!reserve(size))
		return false;
	mSize = size;
	return true;
}

template <class T, class Alloc>
PX_NOINLINE T* PxArray<T, Alloc>::growAndPushBack(const T& a)
{
	const uint32_t capacity = capacityIncrement();

	uint32_t cookie = 0;
	T* newData = allocate(capacity, &cookie);
	if(!newData)
		return NULL;
	PX_ASSERT((!capacity) || (newData && (newData != mData)));

	copy(newData, newData + mSize, mData);

	// inserting element before destroying old array
	// avoids referencing destroyed object when duplicating array element.
	PX_PLACEMENT_NEW(reinterpret_cast<void*>(newData + mSize), T)(a);

	destroy(mData, mData + mSize);
	if(!isInUserMemory())
		deallocate(mData, &cookie);

	mData = newData;
	mCapacity = capacity;
	return &mData[mSize++];
}

template <class T, class Alloc>
PX_NOINLINE bool PxArray<T, Alloc>::recreate(uint32_t capacity)
{
	uint32_t cookie = 0;
	T* newData = allocate(capacity, &cookie);
	if(capacity && !newData)
		return false;
	PX_ASSERT((!capacity) || (newData && (newData != mData)));

	copy(newData, newData + mSize, mData);
	destroy(mData, mData + mSize);
	if(!isInUserMemory())
		deallocate(mData, &cookie);

	mData = newData;
	mCapacity = capacity;
	return true;
}

template <class T, class Alloc>
PX_INLINE void swap(PxArray<T, Alloc>& x, PxArray<T, Alloc>& y)
{
	x.swap(y);
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


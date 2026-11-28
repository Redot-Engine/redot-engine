// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_REPORT_CALLBACK_H
#define PX_REPORT_CALLBACK_H

#include "common/PxPhysXCommonConfig.h"
#include "foundation/PxArray.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief Base class for all report callbacks.

	\see PxRegularReportCallback PxLocalStorageReportCallback PxExternalStorageReportCallback PxDynamicArrayReportCallback
	*/
	class PxReportCallbackBase
	{
		public:
						PxReportCallbackBase(PxU32 capacity=0) : mCapacity(capacity), mSize(0)	{}
		virtual			~PxReportCallbackBase()													{}
		
				PxU32	mCapacity;	// Capacity of mBuffer. If mBuffer is NULL, this controls how many items are reported to users at the same time (with a limit of 256).
				PxU32	mSize;		// Current number of items in the buffer. This is entirely managed by the system.
	};

	/**
	\brief Base class for callback reporting an unknown number of items to users.

	This can be used as-is and customized by users, or several pre-designed callbacks can be used instead (see below).

	This design lets users decide how to retrieve the results of a query:
	- either one by one via a regular callback
	- or one batch at a time via a callback
	- or written out directly to their own C-style buffer
	- or pushed back to their own PxArray
	- etc

	\see PxRegularReportCallback PxLocalStorageReportCallback PxExternalStorageReportCallback PxDynamicArrayReportCallback
	*/
	template<class T>
	class PxReportCallback : public PxReportCallbackBase
	{
		public:
						PxReportCallback(T* buffer=NULL, PxU32 capacity=0) : PxReportCallbackBase(capacity), mBuffer(buffer)	{}
		virtual			~PxReportCallback()																						{}
		
				T*		mBuffer;	// Destination buffer for writing results. if NULL, the system will use its internal buffer and set that pointer as it sees fit.
									// Otherwise users can set it to where they want the results to be written.	

		/**
		\brief Reports query results to users.
		
		This will be called by the system as many times as necessary to report all results.

		\param[in] nbItems	Number of reported items
		\param[in] items	array of reported items

		\return	true to continue the query, false to abort the query
		*/
		virtual	bool	flushResults(PxU32 nbItems, const T* items) = 0;
	};

	/**
	\brief Regular report callback

	This reports results like a regular callback would:
	- without explicit buffer management from users
	- by default, one item at a time

	This customized callback sends results to users via the processResults() function.

	The capacity parameter dictates how many items can be reported at a time,
	i.e. how many times the flushResults/processResults function will be called by the system.

	\see PxReportCallback
	*/
	template<class T>
	class PxRegularReportCallback : public PxReportCallback<T>
	{
		public:
						PxRegularReportCallback(const PxU32 capacity=1)
						{
							PX_ASSERT(capacity<=256);
							this->mCapacity = capacity;
						}

		virtual	bool	flushResults(PxU32 nbItems, const T* items)
						{
							PX_ASSERT(nbItems<=this->mCapacity);
							PX_ASSERT(items==this->mBuffer);
							return processResults(nbItems, items);
						}

		/**
		\brief Reports query results to users.
		
		\param[in] nbItems	Number of reported items
		\param[in] items	array of reported items

		\return	true to continue the query, false to abort the query
		*/
		virtual	bool	processResults(PxU32 nbItems, const T* items)	= 0;
	};

	/**
	\brief Local storage report callback

	This is the same as a regular callback, except the destination buffer is a local buffer within the class.

	This customized callback sends results to users via the processResults() function.

	The capacity of the embedded buffer (determined by a template parameter) dictates how many items can be reported at a time,
	i.e. how many times the flushResults/processResults function will be called by the system.

	\see PxReportCallback
	*/
	template<class T, const PxU32 capacityT>
	class PxLocalStorageReportCallback : public PxReportCallback<T>
	{
		T		mLocalStorage[capacityT];

		public:
						PxLocalStorageReportCallback()
						{
							this->mBuffer = mLocalStorage;
							this->mCapacity = capacityT;
						}

		virtual	bool	flushResults(PxU32 nbItems, const T* items)
						{
							PX_ASSERT(items==mLocalStorage);
							PX_ASSERT(nbItems<=this->mCapacity);
							return processResults(nbItems, items);
						}

		/**
		\brief Reports query results to users.
		
		\param[in] nbItems	Number of reported items
		\param[in] items	array of reported items

		\return	true to continue the query, false to abort the query
		*/
		virtual	bool	processResults(PxU32 nbItems, const T* items)	= 0;
	};

	/**
	\brief External storage report callback

	This is the same as a regular callback, except the destination buffer is a user-provided external buffer.

	Typically the provided buffer can be larger here than for PxLocalStorageReportCallback, and it could
	even be a scratchpad-kind of memory shared by multiple sub-systems.

	This would be the same as having a C-style buffer to write out results in the query interface.

	This customized callback sends results to users via the processResults() function.

	The capacity parameter dictates how many items can be reported at a time,
	i.e. how many times the flushResults/processResults function will be called by the system.

	\see PxReportCallback
	*/
	template<class T>
	class PxExternalStorageReportCallback : public PxReportCallback<T>
	{
		public:
						PxExternalStorageReportCallback(T* buffer, PxU32 capacity)
						{
							this->mBuffer = buffer;
							this->mCapacity = capacity;
						}

		virtual	bool	flushResults(PxU32 nbItems, const T* items)
						{
							PX_ASSERT(items==this->mBuffer);
							PX_ASSERT(nbItems<=this->mCapacity);
							return processResults(nbItems, items);
						}

		/**
		\brief Reports query results to users.
		
		\param[in] nbItems	Number of reported items
		\param[in] items	array of reported items

		\return	true to continue the query, false to abort the query
		*/
		virtual	bool	processResults(PxU32 nbItems, const T* items)	= 0;
	};

	/**
	\brief Dynamic array report callback

	This callback emulates the behavior of pushing results to a (user-provided) dynamic array.

	This customized callback does not actually call users back during the query, results are
	available afterwards in the provided dynamic array. This would be the same as having a PxArray
	directly in the query interface.

	\see PxReportCallback
	*/
	template<class T>
	class PxDynamicArrayReportCallback : public PxReportCallback<T>
	{
		public:
						PxDynamicArrayReportCallback(PxArray<T>& results) : mResults(results)
						{
							mResults.reserve(32);
							this->mBuffer = mResults.begin();
							this->mCapacity = mResults.capacity();
						}

		virtual	bool	flushResults(PxU32 nbItems, const T* /*items*/)
						{
							const PxU32 size = mResults.size();
							const PxU32 capa = mResults.capacity();
							const PxU32 newSize = size+nbItems;
							PX_ASSERT(newSize<=capa);
							mResults.forceSize_Unsafe(newSize);
							if(newSize==capa)
							{
								const PxU32 newCapa = capa*2;
								mResults.reserve(newCapa);
								this->mBuffer = mResults.begin() + newSize;
								this->mCapacity = mResults.capacity() - newSize;
							}
							return true;
						}

		PxArray<T>&	mResults;
	};

#if !PX_DOXYGEN
}
#endif

#endif

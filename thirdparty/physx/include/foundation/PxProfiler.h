// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PROFILER_H
#define PX_PROFILER_H

#include "foundation/PxSimpleTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief The pure virtual callback interface for general purpose instrumentation and profiling of GameWorks modules as
well as applications
*/
class PxProfilerCallback
{
protected:
	virtual ~PxProfilerCallback()	{}

public:
	/**************************************************************************************************************************
	Instrumented profiling events
	***************************************************************************************************************************/

	/**
	\brief Mark the beginning of a nested profile block
	\param[in] eventName	Event name. Must be a persistent const char* that is the same pointer passed to zoneEnd such that the pointer can be used to pair the calls.
	\param[in] detached		True for cross thread events
	\param[in] contextId	the context id of this zone. Zones with the same id belong to the same group. 0 is used for no specific group.
	\return Returns implementation-specific profiler data for this event
	*/
	virtual void* zoneStart(const char* eventName, bool detached, uint64_t contextId) = 0;

	/**
	\brief Mark the end of a nested profile block
	\param[in] profilerData	The data returned by the corresponding zoneStart call (or NULL if not available)
	\param[in] eventName	Event name. Must be a persistent const char* that is the same pointer passed to zoneStart such that the pointer can be used to pair the calls.
	\param[in] detached		True for cross thread events. Should match the value passed to zoneStart.
	\param[in] contextId	The context of this zone. Should match the value passed to zoneStart.

	\note eventName plus contextId can be used to uniquely match up start and end of a zone.
	*/
	virtual void zoneEnd(void* profilerData, const char* eventName, bool detached, uint64_t contextId) = 0;

    /**
    \brief Record integer data to be displayed in the profiler.
    \param[in] value	    The integer data point to be recorded.
    \param[in] valueName	The name of the data being recorded. Must be a persistent const char *
    \param[in] contextId	The context of this data.
    */
    virtual void recordData(int32_t value, const char* valueName, uint64_t contextId) 
	{ 
		PX_UNUSED(value);
		PX_UNUSED(valueName);
		PX_UNUSED(contextId);
	}

	/**
	\brief Record float data to be displayed in the profiler.
	\param[in] value	    The floating point data to be recorded.
	\param[in] valueName	The name of the data being recorded. Must be a persistent const char *
	\param[in] contextId	The context of this data.
	*/
	virtual void recordData(float value, const char* valueName, uint64_t contextId)
	{
		PX_UNUSED(value);
		PX_UNUSED(valueName);
		PX_UNUSED(contextId);
	}

	/**
	\brief Record a frame marker to be displayed in the profiler. 

	Markers that have identical names will be displayed in the profiler
	along with the time between each of the markers. A frame counter will display the frame marker count.
	\param[in] name			The name of the frame. Must be a persistent const char *
	\param[in] contextId	The context of the frame.
	*/
	virtual void recordFrame(const char* name, uint64_t contextId)
	{
		PX_UNUSED(name);
		PX_UNUSED(contextId);
	}
};

class PxProfileScoped
{
  public:
	PX_FORCE_INLINE PxProfileScoped(PxProfilerCallback* callback, const char* eventName, bool detached, uint64_t contextId) : mCallback(callback), mProfilerData(NULL)
	{
		if(mCallback)
		{
			mEventName		= eventName;
			mContextId		= contextId;
			mDetached		= detached;
			mProfilerData	= mCallback->zoneStart(eventName, detached, contextId);
		}
	}

	PX_FORCE_INLINE ~PxProfileScoped()
	{
		if(mCallback)
			mCallback->zoneEnd(mProfilerData, mEventName, mDetached, mContextId);
	}
	PxProfilerCallback*			mCallback;
	const char*					mEventName;
	void*						mProfilerData;
	uint64_t					mContextId;
	bool						mDetached;
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


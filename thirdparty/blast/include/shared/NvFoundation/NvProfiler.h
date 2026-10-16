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

#ifndef NV_PROFILER_H
#define NV_PROFILER_H

#include <NvSimpleTypes.h>

namespace nvidia
{
    class NvProfilerCallback;
    namespace shdfnd
    {
        NV_FOUNDATION_API NvProfilerCallback *getProfilerCallback();
        NV_FOUNDATION_API void setProfilerCallback(NvProfilerCallback *profiler);
    }
}


namespace nvidia
{

struct NvProfileContext
{
    enum Enum
    {
        eNONE = 0 //!< value for no specific profile context. \see NvProfilerCallback::zoneAt
    };
};


/**
\brief The pure virtual callback interface for general purpose instrumentation and profiling of GameWorks modules as well as applications
*/
class NvProfilerCallback
{
protected:
	virtual ~NvProfilerCallback()	{}

public:
	/**************************************************************************************************************************
	Instrumented profiling events
	***************************************************************************************************************************/

	/**
	\brief Mark the beginning of a nested profile block
	\param[in] eventName	Event name. Must be a persistent const char *
	\param[in] detached		True for cross thread events
	\param[in] contextId	the context id of this zone. Zones with the same id belong to the same group. 0 is used for no specific group.
	\return Returns implementation-specific profiler data for this event
	*/
	virtual void* zoneStart(const char* eventName, bool detached, uint64_t contextId) = 0;

	/**
	\brief Mark the end of a nested profile block
	\param[in] profilerData	The data returned by the corresponding zoneStart call (or NULL if not available)
	\param[in] eventName	The name of the zone ending, must match the corresponding name passed with 'zoneStart'. Must be a persistent const char *.
	\param[in] detached		True for cross thread events. Should match the value passed to zoneStart.
	\param[in] contextId	The context of this zone. Should match the value passed to zoneStart.

	\note eventName plus contextId can be used to uniquely match up start and end of a zone.
	*/
	virtual void zoneEnd(void* profilerData, const char* eventName, bool detached, uint64_t contextId) = 0;
};

class NvProfileScoped
{
public:
    NV_FORCE_INLINE NvProfileScoped(const char* eventName, bool detached, uint64_t contextId)
        : mCallback(nvidia::shdfnd::getProfilerCallback())
    {
        if (mCallback)
        {
            mEventName = eventName;
            mDetached = detached;
            mContextId = contextId;
            mProfilerData = mCallback->zoneStart(mEventName, mDetached, mContextId);
        }
    }
    ~NvProfileScoped(void)
    {
        if (mCallback)
        {
            mCallback->zoneEnd(mProfilerData, mEventName, mDetached, mContextId);
        }
    }
    nvidia::NvProfilerCallback* mCallback;
    void*                       mProfilerData;
    const char*                 mEventName;
    bool                        mDetached;
    uint64_t                    mContextId;
};



} // end of NVIDIA namespace



#if NV_DEBUG || NV_CHECKED || NV_PROFILE

#define NV_PROFILE_ZONE(name,context_id) nvidia::NvProfileScoped NV_CONCAT(_scoped,__LINE__)(name,false,context_id)
#define NV_PROFILE_START_CROSSTHREAD(name,context_id) if ( nvidia::shdfnd::getProfilerCallback() ) nvidia::shdfnd::getProfilerCallback()->zoneStart(name,true,context_id)
#define NV_PROFILE_STOP_CROSSTHREAD(name,context_id) if ( nvidia::shdfnd::getProfilerCallback() ) nvidia::shdfnd::getProfilerCallback()->zoneEnd(nullptr,name,true,context_id)

#else

#define NV_PROFILE_ZONE(name,context_id) 
#define NV_PROFILE_START_CROSSTHREAD(name,context_id)
#define NV_PROFILE_STOP_CROSSTHREAD(name,context_id)

#endif

#define NV_PROFILE_POINTER_TO_U64( pointer ) static_cast<uint64_t>(reinterpret_cast<size_t>(pointer))

#endif

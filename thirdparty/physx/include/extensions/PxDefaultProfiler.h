// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFAULT_PROFILER_H
#define PX_DEFAULT_PROFILER_H

#include "foundation/PxProfiler.h"


#if !PX_DOXYGEN
namespace physx
{
#endif




/**
\brief The major and minor version of the current Default Profiler file format.
*/
#define PX_PROFILER_VERSION_MAJOR				0
#define PX_PROFILER_VERSION_MINOR				1


/**
\brief The various profiler data types that describe what data follows in the stream.
*/
struct PxDefaultProfilerDataType
{
	enum Enum : PxU64
	{
		eTHREAD_BLOCK = 0,				//!< A thread ID follows a thread tag. All data following a thread block are from this thread.

		/*!
		A key, size and character string follows a name registration tag. Event names are only written once per thread.

		Ensure names are global or static so the character pointer is static and does not change.
		Pointers values are used as keys to look up names. It is especially important for cross thread
		zones so the zone names can be found across threads.
		*/
		eNAME_REGISTRATION,

		eZONE_START,					//!< A PxProfilerCallback::zoneStart callback. A time tag, context and name key follows a zone start tag.
		eZONE_END,						//!< A PxProfilerCallback::zoneEnd callback. A time tag, context and name key follows a zone end tag.
		eZONE_START_CROSS_THREAD,		//!< A PxProfilerCallback::zoneStart callback with detached set to true indicating a cross thread zone.
		eZONE_END_CROSS_THREAD,			//!< A PxProfilerCallback::zoneEnd callback with detached set to true indicating a cross thread zone.
		eVALUE_INT,						//!< A PxProfilerCallback::recordData callback with an integer value. 
		eVALUE_FLOAT,					//!< A PxProfilerCallback::recordData callback with a floating point value.
		eFRAME							//!< A PxProfilerCallback::recordFrame callback. Frames are simple markers that are identifable in the profiling tools.
	};
};

struct PxDefaultProfilerVersionInfo
{
	PxU32 major;		//!< The major version of the current file format.
	PxU32 minor;		//!< The minor version of the current file format.
};

struct PxDefaultProfilerHeader
{
	PxDefaultProfilerDataType::Enum type;		//!< The type of event or data that follows this header. See PxDefaultProfilerDataType.
};

struct PxDefaultProfilerThread
{
	PxU64 threadId;		//!< The thread identifier for all of the data that follows.
};

struct PxDefaultProfilerName
{
	PxU64 key;			//!< The key to the profiler name. Used to store the name in a map for quick look ups. It is also the pointer to the character string.
	PxU32 size;			//!< Byte size of the null terminated string that follows the PxDefaultProfilerName entry in the stream. Note that due to padding the size can be larger than the string size itself.
	
	/// @cond
	PxU32 padding;
	/// @endcond
};

struct PxDefaultProfilerEvent
{
	PxU64 time;			//!< The time the event was received, in tens of nano seconds.
	PxU64 contextId;	//!< The context sent with the event. User controlled data to associate with the event.
	PxU64 nameKey;		//!< The key to look up the name at a later time so strings are not stored with every event.
};

struct PxDefaultProfilerValueEvent : public PxDefaultProfilerEvent
{
	union
	{
		PxI32 intValue;			//!< Either integer or float data that is to be graphed.
		PxF32 floatValue;		//!< Either integer or float data that is to be graphed.
	};

	/// @cond
	PxU32 padding;
	/// @endcond
};

class PxOutputStream;

/**

\brief Default implementation of PxProfilerCallback to record profiling events.

The profiling events are stored in a memory buffer which gets flushed to a user provided stream when the 
buffer is full, or when flush or release is called.

\see PxDefaultProfilerCreate()
*/
class PxDefaultProfiler : public PxProfilerCallback
{
public:

	/**
	\brief Deletes the profiler.

	Do not keep a reference to the deleted instance.

	\see PxDefaultProfilerCreate()
	*/
	virtual void release() = 0;

	/**
	\brief Write all of the collected profiling data to the output stream.

	This call is not thread safe, so it should only be called during a sync point or when all threads have yielded.
	Calling this regularly, at appropriate times will prevent the buffers from filling up and forcing a write
	to the stream at unexpected times because writes will affect performance.
	*/
	virtual void flush() = 0;
};

/**
\brief Create default profiler.

\note The PhysXExtensions SDK needs to be initialized first before using this method (see #PxInitExtensions)

\param[in] outputStream A PxOutputStream used to write all of the recorded 
profiler events received. Writing to the stream occurs when the 
buffer is full or when flush or release is called.
\param[in] numberOfBuffers The number of buffers to pre-allocate. One buffer is 
used per thread, so a number larger than the anticipated number of threads is best.
If more buffers are needed, additional memory is allocated as needed, but this may
affect performance.
\param[in] bufferSize The number of bytes to allocate per thread for recording all
profiler events received on that thread. Once the buffer is full, the profiling data 
is written to the stream, which will cause a slight delay. Use a larger buffer size 
to prevent this, if memory permits. The minimum buffer size is 32,767 bytes, which is also
the default setting.
*/
PxDefaultProfiler* PxDefaultProfilerCreate(PxOutputStream& outputStream, PxU32 numberOfBuffers = 16, PxU32 bufferSize = 32767);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

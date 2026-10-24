// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_FOUNDATION_H
#define PX_FOUNDATION_H


#include "foundation/PxSimpleTypes.h"
#include "foundation/PxErrors.h"
#include "foundation/PxFoundationConfig.h"

#include <stdarg.h>

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxAllocationListener;
class PxErrorCallback;
class PxAllocatorCallback;

/**
\brief Foundation SDK singleton class.

You need to have an instance of this class to instance the higher level SDKs.
*/
class PX_FOUNDATION_API PxFoundation
{
  public:
	/**
	\brief Destroys the instance it is called on.

	The operation will fail, if there are still modules referencing the foundation object. Release all dependent modules
	prior to calling this method.

	\see PxCreateFoundation()
	*/
	virtual void release() = 0;

	/**
	retrieves error callback
	*/
	virtual PxErrorCallback& getErrorCallback() = 0;

	/**
	Sets mask of errors to report.
	*/
	virtual void setErrorLevel(PxErrorCode::Enum mask = PxErrorCode::eMASK_ALL) = 0;

	/**
	Retrieves mask of errors to be reported.
	*/
	virtual PxErrorCode::Enum getErrorLevel() const = 0;

	/**
	Retrieves the allocator this object was created with.
	*/
	virtual PxAllocatorCallback& getAllocatorCallback() = 0;

	/**
	Retrieves if allocation names are being passed to allocator callback.
	*/
	virtual bool getReportAllocationNames() const = 0;

	/**
	Set if allocation names are being passed to allocator callback.
	\details Enabled by default in debug and checked build, disabled by default in profile and release build.
	*/
	virtual void setReportAllocationNames(bool value) = 0;

	virtual void registerAllocationListener(PxAllocationListener& listener) = 0;

	virtual void deregisterAllocationListener(PxAllocationListener& listener) = 0;

	virtual void registerErrorCallback(PxErrorCallback& callback) = 0;

	virtual void deregisterErrorCallback(PxErrorCallback& callback) = 0;

	virtual bool error(PxErrorCode::Enum c, const char* file, int line, const char* messageFmt, ...) = 0;

	virtual bool error(PxErrorCode::Enum, const char* file, int line, const char* messageFmt, va_list) = 0;

  protected:
	virtual ~PxFoundation()
	{
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

// PT: use this to make generated code shorter (e.g. from 52 to 24 bytes of assembly (10 to 4 instructions))
// We must use a macro here to let __FILE__ expand to the proper filename (it doesn't work with an inlined function).
#define PX_IMPLEMENT_OUTPUT_ERROR															\
template<const int errorCode>																\
static PX_NOINLINE bool outputError(int line, const char* message)							\
{																							\
	return PxGetFoundation().error(PxErrorCode::Enum(errorCode), __FILE__, line, message);	\
}

/**
\brief Creates an instance of the foundation class

The foundation class is needed to initialize higher level SDKs. There may be only one instance per process.
Calling this method after an instance has been created already will result in an error message and NULL will be
returned.

\param version Version number we are expecting (should be #PX_PHYSICS_VERSION)
\param allocator User supplied interface for allocating memory(see #PxAllocatorCallback)
\param errorCallback User supplied interface for reporting errors and displaying messages(see #PxErrorCallback)
\return Foundation instance on success, NULL if operation failed

\see PxFoundation
*/
PX_C_EXPORT PX_FOUNDATION_API physx::PxFoundation* PX_CALL_CONV PxCreateFoundation(physx::PxU32 version, physx::PxAllocatorCallback& allocator, physx::PxErrorCallback& errorCallback);


PX_C_EXPORT PX_FOUNDATION_API void PX_CALL_CONV PxSetFoundationInstance(physx::PxFoundation& foundation);


/**
\brief Retrieves the Foundation SDK after it has been created.

\note The behavior of this method is undefined if the foundation instance has not been created already.

\see PxCreateFoundation(), PxIsFoundationValid()
*/
#if PX_CLANG
#if PX_LINUX
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif // PX_LINUX
#endif // PX_CLANG
PX_C_EXPORT PX_FOUNDATION_API physx::PxFoundation& PX_CALL_CONV PxGetFoundation();
#if PX_CLANG
#if PX_LINUX
#pragma clang diagnostic pop
#endif // PX_LINUX
#endif // PX_CLANG

/**
\brief Similar to PxGetFoundation() except it handles the case if the foundation was not created already.
\return Pointer to the foundation if an instance is currently available, otherwise null.

\see PxCreateFoundation(), PxGetFoundation()
*/
PX_C_EXPORT PX_FOUNDATION_API physx::PxFoundation* PX_CALL_CONV PxIsFoundationValid();

#if !PX_DOXYGEN
namespace physx
{
#endif
class PxProfilerCallback;
class PxAllocatorCallback;
class PxErrorCallback;
#if !PX_DOXYGEN
} // namespace physx
#endif

/**
\brief Get the callback that will be used for all profiling.
*/
PX_C_EXPORT PX_FOUNDATION_API physx::PxProfilerCallback* PX_CALL_CONV PxGetProfilerCallback();

/**
\brief Set the callback that will be used for all profiling.
*/
PX_C_EXPORT PX_FOUNDATION_API void PX_CALL_CONV PxSetProfilerCallback(physx::PxProfilerCallback* profiler);

/**
\brief Get the allocator callback
*/
PX_C_EXPORT PX_FOUNDATION_API physx::PxAllocatorCallback* PX_CALL_CONV PxGetAllocatorCallback();

/**
\brief Get the broadcasting allocator callback
*/
PX_C_EXPORT PX_FOUNDATION_API physx::PxAllocatorCallback* PX_CALL_CONV PxGetBroadcastAllocator(bool* reportAllocationNames = NULL);

/**
\brief Get the error callback
*/
PX_C_EXPORT PX_FOUNDATION_API physx::PxErrorCallback* PX_CALL_CONV PxGetErrorCallback();

/**
\brief Get the broadcasting error callback
*/
PX_C_EXPORT PX_FOUNDATION_API physx::PxErrorCallback* PX_CALL_CONV PxGetBroadcastError();

/**
\brief Get the warn once timestamp
*/
PX_C_EXPORT PX_FOUNDATION_API physx::PxU32 PX_CALL_CONV PxGetWarnOnceTimeStamp();

/**
\brief Decrement the ref count of PxFoundation
*/
PX_C_EXPORT PX_FOUNDATION_API void PX_CALL_CONV PxDecFoundationRefCount();

/**
\brief Increment the ref count of PxFoundation
*/
PX_C_EXPORT PX_FOUNDATION_API void PX_CALL_CONV PxIncFoundationRefCount();

#endif


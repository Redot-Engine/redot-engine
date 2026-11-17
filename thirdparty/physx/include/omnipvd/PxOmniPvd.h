// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_OMNI_PVD_H
#define PX_OMNI_PVD_H

#include "PxPhysXConfig.h"

class OmniPvdWriter;

// The OVD integration version:
// 
//   Major version indicates breaking changes in how PhysX SDK objects
//   are streamed using the OmniPVD API, or if certain attributes changed
//   name/type or set size, or was removed, hence subtractive changes.
// 
//   Minor version version indicates non-breaking changes such as the
//   addition of a class or attribute on top of those already existing,
//   hence additive changes.
#define PX_PHYSICS_OVD_INTEGRATION_VERSION_MAJOR 3
#define PX_PHYSICS_OVD_INTEGRATION_VERSION_MINOR 1

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxFoundation;
class PxOmniPvd;

/**
\brief Callback that lets a module add its own objects to the recording started by startSampling().

A module (such as PhysXExtensions) implements this callback and registers it with
PxOmniPvd::addEventCallback(). startSampling() calls onStartSampling() after it has recorded
the core PhysX objects, so the module can record its own objects onto the bound write stream.
*/
class PxOmniPvdEventCallback
{
public:
	/**
	\brief Called by startSampling() after the core PhysX objects have been recorded.

	The module records its own objects onto the bound write stream here, so they can refer to
	the core objects already recorded.

	\param omniPvd The PxOmniPvd taking the snapshot (the same instance the callback was registered with).
	*/
	virtual void onStartSampling(PxOmniPvd& omniPvd) = 0;
	virtual ~PxOmniPvdEventCallback() {}
};

class PxOmniPvd
{
public:
	class ScopedExclusiveWriter
	{
	  public:
		PX_FORCE_INLINE ScopedExclusiveWriter(PxOmniPvd* omniPvd)
		{
			mOmniPvd = omniPvd;
			mWriter = NULL;
			if (mOmniPvd) {
				mWriter = mOmniPvd->acquireExclusiveWriterAccess();
			}
		}

		PX_FORCE_INLINE ~ScopedExclusiveWriter()
		{
			if (mOmniPvd && mWriter) {
				mOmniPvd->releaseExclusiveWriterAccess();
			}
		}

		PX_FORCE_INLINE OmniPvdWriter* operator-> ()
		{
			return mWriter;
		}
		
		PX_FORCE_INLINE OmniPvdWriter* getWriter()
		{
			return mWriter;
		}
	private:
		OmniPvdWriter* mWriter;
		PxOmniPvd* mOmniPvd;
	};

	virtual ~PxOmniPvd()
	{
	}
	/**
	\brief Get the OmniPvd writer.
	
	Gets an instance of the OmniPvd writer. The writer access will not be thread safe since the OmniPVD API is not thread safe itself. Writing concurrently and simultaneously using the OmniPVD API is undefined.
	The returned writer is owned by this PxOmniPvd instance. Do not pass it to destroyOmniPvdWriter().
	
	For thread safe exlcusive access use the mechanism acquireExclusiveWriterAccess/releaseExclusiveWriterAccess.

	\return OmniPvdWriter instance on succes, NULL otherwise.
	*/
	virtual OmniPvdWriter* getWriter() = 0;
	
	/**
	\brief Acquires an exclusive writer access.
	
	This call blocks until exclusive access to the writer can be acquired. Once access has been granted, it is guaranteed that no other caller can access the writer through this method until releaseExclusiveWriterAccess() has been called.
	
	This allows to safely write PVD data in environments with concurrent processing workflows.

	\return OmniPvdWriter instance on succes, NULL otherwise.
	*/
	virtual OmniPvdWriter* acquireExclusiveWriterAccess() = 0;

	/**
	\brief Releases the exclusive writer access
	
	Releases the access to the writer that was previously acquired using acquireExclusiveWriterAccess.

	*/
	virtual void releaseExclusiveWriterAccess() = 0;

	/**
	\brief Starts recording to the bound write stream.

	First records the current state of the whole scene, then records every change from this
	point on. Because the current state is recorded first, the newly written versioned segment
	contains the state needed to decode it from its starting boundary whether the scene is empty
	or has been running for a while. This is what makes a late attach work: you can bind a stream
	after the simulation has been running and the new segment still contains the full scene.

	Bind the write stream first with OmniPvdWriter::setWriteStream(), then call startSampling().
	To take a fresh recording later, call stopSampling(), bind a stream with setWriteStream(), then
	call startSampling() again. Rebinding resets the writer session but does not itself reset the
	transport: an already-open stream appends a versioned segment at its current position, so retain
	that boundary and position a fresh reader there. For one standalone recording decodable from
	byte zero, use a new or explicitly reset transport, or close and reopen a file writer so its
	truncating reopen policy applies. Calling startSampling() while already sampling is not allowed:
	it does not start a new recording and returns false with an error message. Call stopSampling()
	before starting another recording. For final teardown, sampling may remain active through
	PxPhysics::release() so object-removal notifications are recorded; releasing the associated
	PxPhysics implicitly ends sampling.

	\note Recording the current state reads the whole scene, so do not call this while the scene
	is stepping. Call it before PxScene::simulate()/collide() or after fetchResults()/fetchCollision(),
	or while holding the scene write lock.

	\return True if recording started. False if recording was already on, if no writer is
	available, or if a write failed while recording the current state.

	\see OmniPvdWriter::setWriteStream()
	\see PxOmniPvd::addEventCallback()
	\see PxOmniPvd::stopSampling()
	\see PxOmniPvd::isSampling()
	*/
	virtual bool startSampling() = 0;

	/**
	\brief Stops recording.

	After this call, no further object or per-frame data is written. The write stream is not
	flushed or closed; you own the stream and decide when to flush or close it. Call stopSampling()
	before starting another recording: a later startSampling() then records the current state again
	onto the (re-)bound stream. It is not a resume. Calling stopSampling() is optional during final
	teardown: leaving sampling active through PxPhysics::release() records object-removal
	notifications, and releasing the associated PxPhysics implicitly ends sampling. It is safe to
	call stopSampling() after that PxPhysics has been released; the call returns false because its
	associated sampler state is no longer available.

	\return True if the sampling state was set to false, false if no associated sampler state was
	available.

	\see PxOmniPvd::startSampling()
	*/
	virtual bool stopSampling() = 0;

	/**
	\brief Whether recording is currently on.

	True after a successful startSampling() until stopSampling() is called or the associated
	PxPhysics is released.

	\return True if sampling, false otherwise.

	\see PxOmniPvd::startSampling()
	\see PxOmniPvd::stopSampling()
	*/
	virtual bool isSampling() const = 0;

	/**
	\brief Registers a callback that contributes its objects to the recording.

	A module (such as PhysXExtensions) implements PxOmniPvdEventCallback to record its own
	objects when startSampling() records the current state. startSampling() calls each
	registered callback's onStartSampling() after it has recorded the core PhysX objects.
	Registering the same callback twice has no effect.

	\note addEventCallback() and removeEventCallback() are not thread-safe; call them from a
	single thread (for example at setup), not concurrently with each other or with sampling.

	\param callback The callback to register.

	\see PxOmniPvd::removeEventCallback()
	*/
	virtual void addEventCallback(PxOmniPvdEventCallback& callback) = 0;

	/**
	\brief Unregisters a previously registered callback.

	Has no effect if the callback was not registered.

	\note Not thread-safe; see addEventCallback().

	\param callback The callback to unregister.

	\see PxOmniPvd::addEventCallback()
	*/
	virtual void removeEventCallback(PxOmniPvdEventCallback& callback) = 0;

	/**
	\brief Releases the PxOmniPvd object

	*/
	virtual void release() = 0;

};
#if !PX_DOXYGEN
} // namespace physx
#endif
/**
\brief Creates an instance of the OmniPvd object

Creates an instance of the OmniPvd class. There may be only one instance of this class per process. Calling this method after an instance
has been created already will return the same instance over and over.

\param foundation Foundation instance (see PxFoundation)

\return PxOmniPvd instance on succes, NULL otherwise.

*/
PX_C_EXPORT PX_PHYSX_CORE_API physx::PxOmniPvd* PX_CALL_CONV PxCreateOmniPvd(physx::PxFoundation& foundation);


#endif

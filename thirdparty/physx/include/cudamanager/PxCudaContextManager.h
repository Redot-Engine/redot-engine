// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CUDA_CONTEXT_MANAGER_H
#define PX_CUDA_CONTEXT_MANAGER_H

#include "foundation/PxPreprocessor.h"

#if PX_SUPPORT_GPU_PHYSX

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxErrorCallback.h"
#include "foundation/PxFlags.h"

#include "PxCudaTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxCudaContext;

struct PxCudaInteropRegisterFlag
{
	enum Enum
	{
		eNONE           = 0x00,
		eREAD_ONLY      = 0x01,
		eWRITE_DISCARD  = 0x02,
		eSURFACE_LDST   = 0x04,
		eTEXTURE_GATHER = 0x08
	};
};

/**
\brief An interface class that the user can implement in order for PhysX to use a user-defined device memory allocator.
*/
class PxDeviceAllocatorCallback
{
public:

	/**
	\brief Allocated device memory.
	\param[out] ptr Output pointer to the allocated memory. The returned address must be 256 bytes aligned.
	\param[in] size The amount of memory to be allocated.
	\return A boolean indicating whether the allocation succeeded or not.
	*/
	virtual bool memAlloc(void** ptr, size_t size) = 0;

	/**
	\brief Frees device memory.
	\param[in] ptr The memory to free.
	\return A boolean indicating whether the deallocation succeeded or not.
	*/
	virtual bool memFree(void* ptr) = 0;

protected:
	virtual ~PxDeviceAllocatorCallback() {}
};

/**
\brief An interface class that the user can implement in order for PhysX to use a user-defined host pinned memory allocator.
*/
class PxPinnedHostAllocatorCallback
{
public:

	/**
	\brief Allocates host pinned memory.
	\param[in] ptr Output pointer to the allocated memory. The returned address must be 256 bytes aligned.
	\param[in] size The amount of memory to be allocated.
	\param[in] flags CUDA allocation flags (e.g., CU_MEMHOSTALLOC_DEVICEMAP, CU_MEMHOSTALLOC_PORTABLE)
	\return A boolean indicating whether the allocation succeeded or not.
	*/
	virtual bool memAlloc(void** ptr, size_t size, PxU32 flags) = 0;

	/**
	\brief Frees host pinned memory.
	\param[in] ptr The memory to free.
	\return A boolean indicating whether the deallocation succeeded or not.
	*/
	virtual bool memFree(void* ptr) = 0;

protected:
	virtual ~PxPinnedHostAllocatorCallback() {}
};
/**
\brief collection of set bits defined in NxCudaInteropRegisterFlag.

\see NxCudaInteropRegisterFlag
*/
typedef PxFlags<PxCudaInteropRegisterFlag::Enum, uint32_t> PxCudaInteropRegisterFlags;
PX_FLAGS_OPERATORS(PxCudaInteropRegisterFlag::Enum, uint32_t)

//! \brief Descriptor used to create a PxCudaContextManager
class PxCudaContextManagerDesc
{
public:
    /**
     * \brief The CUDA context to manage
     *
     * If left NULL, the PxCudaContextManager will create a new context.  If
     * graphicsDevice is also not NULL, this new CUDA context will be bound to
     * that graphics device, enabling the use of CUDA/Graphics interop features.
     *
     * If ctx is not NULL, the specified context must be applied to the thread
     * that is allocating the PxCudaContextManager at creation time (aka, it
     * cannot be popped).  The PxCudaContextManager will take ownership of the
     * context until the manager is released.  All access to the context must be
     * gated by lock acquisition.
     *
     * If the user provides a context for the PxCudaContextManager, the context
     * _must_ have either been created on the GPU ordinal returned by
     * PxGetSuggestedCudaDeviceOrdinal() or on your graphics device.
     */
	CUcontext*	ctx;

    /**
     * \brief D3D device pointer or OpenGl context handle
     *
     * Only applicable when ctx is NULL, thus forcing a new context to be
     * created.  In that case, the created context will be bound to this
     * graphics device.
     */
	void*	graphicsDevice;

	/**
	 * \brief CUDA device ordinal
	 *
	 * Only applicable when ctx is NULL, thus forcing a new context to be created based on the CUDA device ordinal.
	 * The first CUDA device will have an ordinal value of 0 and so on.
	 * If the CUDA device ordinal is -1, the device selected will be queried from the environment variable PHYSX_GPU_DEVICE.
	 * \note If the environment variable PHYSX_GPU_DEVICE is not found, the CUDA device ordinal will default to 0.
	 */
	PxI32 deviceOrdinal;

	/**
	  * \brief Application-specific GUID
	  *
	  * If your application employs PhysX modules that use CUDA you need to use a GUID 
	  * so that patches for new architectures can be released for your game.You can obtain a GUID for your 
	  * application from Nvidia.
	  */
	const char*	appGUID;

	/**
	  * \brief The application-specific device memory allocator
	  *
	  * The application can implement a device memory allocator, which inherits from PxDeviceAllocatorCallback, and
	  * pass it to the SDK via PxCudaContextManagerDesc. The SDK will use that allocator to allocate device memory instead of
	  * using the default CUDA device memory allocator.
	  */
	PxDeviceAllocatorCallback*		deviceAllocator;

	/**
	  * \brief The application-specific host pinned memory allocator
	  *
	  * The application can implement a host pinned memory allocator, which inherits from PxPinnedHostAllocatorCallback, and
	  * pass it to the SDK via PxCudaContextManagerDesc. The SDK will use that allocator to allocate host pinned memory instead of
	  * using the default CUDA pinned host memory allocator.
	  */
	PxPinnedHostAllocatorCallback*	pinnedHostAllocator;

	PX_INLINE PxCudaContextManagerDesc() :
		ctx					(NULL),
		graphicsDevice		(NULL),
		deviceOrdinal		(-1),
		appGUID				(NULL),
		deviceAllocator		(NULL),
		pinnedHostAllocator	(NULL)
	{
	}
};

/**
\brief A cuda kernel index providing an index to the cuda module and the function name
*/
struct PxKernelIndex
{
    PxU32 moduleIndex;
    const char* functionName;
};

/**
 * \brief Manages thread locks, and task scheduling for a CUDA context
 *
 * A PxCudaContextManager manages access to a single CUDA context, allowing it to
 * be shared between multiple scenes.
 * The context must be acquired from the manager before using any CUDA APIs unless stated differently.
 *
 * The PxCudaContextManager is based on the CUDA driver API and explicitly does not
 * support the CUDA runtime API (aka, CUDART).
 */
class PxCudaContextManager
{
public:
	/**
	* \brief Schedules clear operation for a device memory buffer on the specified stream
	*
	* The cuda context will get acquired automatically
	* \deprecated The replacement is PxCudaHelpersExt::memsetAsync.
	*/
	template<typename T>
	PX_DEPRECATED void clearDeviceBufferAsync(T* deviceBuffer, PxU32 numElements, CUstream stream, PxI32 value = 0)
	{
		clearDeviceBufferAsyncInternal(deviceBuffer, numElements * sizeof(T), stream, value);
	}

	/**
	* \brief Copies a device buffer to the host
	*
	* The cuda context will get acquired automatically
	* \deprecated The replacement is PxCudaHelpersExt::copyDToH.
	*/
	template<typename T>
	PX_DEPRECATED void copyDToH(T* hostBuffer, const T* deviceBuffer, PxU32 numElements)
	{
		copyDToHInternal(hostBuffer, deviceBuffer, numElements * sizeof(T));
	}

	/**
	* \brief Copies a host buffer to the device
	*
	* The cuda context will get acquired automatically
	* \deprecated The replacement is PxCudaHelpersExt::copyHtoD
	*/
	template<typename T>
	PX_DEPRECATED void copyHToD(T* deviceBuffer, const T* hostBuffer, PxU32 numElements)
	{
		copyHToDInternal(deviceBuffer, hostBuffer, numElements * sizeof(T));
	}

	/**
	* \brief Schedules device to host copy operation on the specified stream
	*
	* The cuda context will get acquired automatically
	* \deprecated Use PxCudaHelpersExt::copyDToHAsync instead.
	*/
	template<typename T>
	PX_DEPRECATED void copyDToHAsync(T* hostBuffer, const T* deviceBuffer, PxU32 numElements, CUstream stream)
	{
		copyDToHAsyncInternal(hostBuffer, deviceBuffer, numElements * sizeof(T), stream);
	}

	/**
	* \brief Schedules host to device copy operation on the specified stream
	*
	* The cuda context will get acquired automatically
	* \deprecated Use PxCudaHelpersExt::copyHToDAsync instead.
	*/
	template<typename T>
	PX_DEPRECATED void copyHToDAsync(T* deviceBuffer, const T* hostBuffer, PxU32 numElements, CUstream stream)
	{
		copyHToDAsyncInternal(deviceBuffer, hostBuffer, numElements * sizeof(T), stream);
	}

	/**
	* \brief Schedules device to device copy operation on the specified stream
	*
	* The cuda context will get acquired automatically
	* \deprecated Use PxCudaHelpersExt::copyDToDAsync instead.
	*/
	template<typename T>
	PX_DEPRECATED void copyDToDAsync(T* dstDeviceBuffer, const T* srcDeviceBuffer, PxU32 numElements, CUstream stream)
	{
		copyDToDAsyncInternal(dstDeviceBuffer, srcDeviceBuffer, numElements * sizeof(T), stream);
	}

	/**
	* \brief Schedules a memset operation on the device on the specified stream. Only supported for 1 byte or 4 byte data types.
	*
	* The cuda context will get acquired automatically
	* \deprecated Use PxCudaHelpersExt::memsetAsync instead.
	*/
	template<typename T>
	PX_DEPRECATED void memsetAsync(T* dstDeviceBuffer, const T& value, PxU32 numElements, CUstream stream)
	{
		PX_COMPILE_TIME_ASSERT(sizeof(value) == sizeof(PxU32) || sizeof(value) == sizeof(PxU8));		

		if (sizeof(value) == sizeof(PxU32))		
			memsetD32AsyncInternal(dstDeviceBuffer, reinterpret_cast<const PxU32&>(value), numElements, stream);
		else
			memsetD8AsyncInternal(dstDeviceBuffer, reinterpret_cast<const PxU8&>(value), numElements, stream);		
	}

	/**
	* \brief Allocates a device buffer
	*
	* The cuda context will get acquired automatically
	* \deprecated - use PxCudaHelpersExt::allocDeviceBuffer instead.
	*/
	template<typename T>
	PX_DEPRECATED void allocDeviceBuffer(T*& deviceBuffer, PxU32 numElements, const char* filename = __FILE__, PxI32 line = __LINE__)
	{
		void* ptr = allocDeviceBufferInternal(PxU64(numElements) * sizeof(T), filename, line);
		deviceBuffer = reinterpret_cast<T*>(ptr);
	}

	/**
	* \brief Allocates a device buffer and returns the pointer to the memory
	*
	* The cuda context will get acquired automatically
	* \deprecated - use PxCudaHelpersExt::allocDeviceBuffer instead.
	*/
	template<typename T>
	PX_DEPRECATED T* allocDeviceBuffer(PxU32 numElements, const char* filename = __FILE__, PxI32 line = __LINE__)
	{
		void* ptr = allocDeviceBufferInternal(PxU64(numElements) * sizeof(T), filename, line);
		return reinterpret_cast<T*>(ptr);
	}

	/**
	* \brief Frees a device buffer
	*
	* The cuda context will get acquired automatically
	* \deprecated - use PxCudaHelpersExt::freeDeviceBuffer instead.
	*/
	template<typename T>
	PX_DEPRECATED void freeDeviceBuffer(T*& deviceBuffer)
	{
		freeDeviceBufferInternal(deviceBuffer);
		deviceBuffer = NULL;
	}

	/**
	* \brief Allocates a pinned host buffer
	*
	* A pinned host buffer can be used on the gpu after getting a mapped device pointer from the pinned host buffer pointer, see getMappedDevicePtr
	* The cuda context will get acquired automatically
	* \see getMappedDevicePtr
	* \deprecated - use PxCudaHelpersExt::allocPinnedHostBuffer instead.
	*/
	template<typename T>
	PX_DEPRECATED void allocPinnedHostBuffer(T*& pinnedHostBuffer, PxU32 numElements, const char* filename = __FILE__, PxI32 line = __LINE__)
	{
		void* ptr = allocPinnedHostBufferInternal(PxU64(numElements) * sizeof(T), filename, line);
		pinnedHostBuffer = reinterpret_cast<T*>(ptr);
	}

	/**
	* \brief Allocates a pinned host buffer and returns the pointer to the memory
	*
	* A pinned host buffer can be used on the gpu after getting a mapped device pointer from the pinned host buffer pointer, see getMappedDevicePtr
	* The cuda context will get acquired automatically
	* \deprecated - use PxCudaHelpersExt::allocPinnedHostBuffer instead.
	* \see getMappedDevicePtr
	*/
	template<typename T>
	PX_DEPRECATED T* allocPinnedHostBuffer(PxU32 numElements, const char* filename = __FILE__, PxI32 line = __LINE__)
	{
		void* ptr = allocPinnedHostBufferInternal(PxU64(numElements) * sizeof(T), filename, line);
		return reinterpret_cast<T*>(ptr);
	}

	/**
	* \brief Frees a pinned host buffer
	*
	* The cuda context will get acquired automatically
	* \deprecated - use PxCudaHelpersExt::freePinnedHostBuffer instead.
	*/
	template<typename T>
	PX_DEPRECATED void freePinnedHostBuffer(T*& pinnedHostBuffer)
	{
		freePinnedHostBufferInternal(pinnedHostBuffer);
		pinnedHostBuffer = NULL;
	}

	/**
	* \brief Gets a mapped pointer from a pinned host buffer that can be used in cuda kernels directly
	*
	* Data access performance with a mapped pinned host pointer will be slower than using a device pointer directly 
	* but the changes done in the kernel will be available on the host immediately.
	* The cuda context will get acquired automatically
	*/
	virtual CUdeviceptr getMappedDevicePtr(void* pinnedHostBuffer) = 0;

    /**
     * \brief Acquire the CUDA context for the current thread
     *
     * Acquisitions are allowed to be recursive within a single thread.
     * You can acquire the context multiple times so long as you release
     * it the same count.
     *
     * The context must be acquired before using most CUDA functions.
     */
    virtual void acquireContext() = 0;

	/**
	 * \brief Acquire the CUDA context for the current thread
	 *
	 * Acquisitions are allowed to be recursive within a single thread.
	 * You can acquire the context multiple times so long as you release
	 * it the same count.
	 *
	 * The context must be acquired before using most CUDA functions.
	 * The function will return false if context aquisition fails for some reason
	 */
	virtual bool tryAcquireContext() = 0;

    /**
     * \brief Release the CUDA context from the current thread
     *
     * The CUDA context should be released as soon as practically
     * possible, to allow other CPU threads to work efficiently.
     */
    virtual void releaseContext() = 0;

	/**
	* \brief Return the CUcontext
	*/
	virtual CUcontext getContext() = 0;

	/**
	* \brief Return the CudaContext
	*/
	virtual PxCudaContext* getCudaContext() = 0;

    /**
     * \brief Context manager has a valid CUDA context
     *
     * This method should be called after creating a PxCudaContextManager,
     * especially if the manager was responsible for allocating its own
     * CUDA context (desc.ctx == NULL).
     */
    virtual bool contextIsValid() const = 0;

	/* Query CUDA context and device properties, without acquiring context */

    virtual bool supportsArchSM10() const = 0;  //!< G80
    virtual bool supportsArchSM11() const = 0;  //!< G92
    virtual bool supportsArchSM12() const = 0;  //!< GT200
    virtual bool supportsArchSM13() const = 0;  //!< GT260
    virtual bool supportsArchSM20() const = 0;  //!< GF100
    virtual bool supportsArchSM30() const = 0;  //!< GK100
	virtual bool supportsArchSM35() const = 0;  //!< GK110
	virtual bool supportsArchSM50() const = 0;  //!< GM100
	virtual bool supportsArchSM52() const = 0;  //!< GM200
	virtual bool supportsArchSM60() const = 0;  //!< GP100
	virtual bool isIntegrated() const = 0;      //!< true if GPU is an integrated (MCP) part
	virtual bool canMapHostMemory() const = 0;  //!< true if GPU map host memory to GPU (0-copy)
	virtual int  getDriverVersion() const = 0;  //!< returns cached value of cuGetDriverVersion()
	virtual size_t getDeviceTotalMemBytes() const = 0; //!< returns cached value of device memory size
	virtual int	getMultiprocessorCount() const = 0; //!< returns cache value of SM unit count
    virtual unsigned int getClockRate() const = 0; //!< returns cached value of SM clock frequency
    virtual int  getSharedMemPerBlock() const = 0; //!< returns total amount of shared memory available per block in bytes
	virtual int  getSharedMemPerMultiprocessor() const = 0; //!< returns total amount of shared memory available per multiprocessor in bytes
	virtual unsigned int getMaxThreadsPerBlock() const = 0; //!< returns the maximum number of threads per block
    virtual const char *getDeviceName() const = 0; //!< returns device name retrieved from driver
	virtual CUdevice getDevice() const = 0; //!< returns device handle retrieved from driver

	virtual void setUsingConcurrentStreams(bool) = 0; //!< turn on/off using concurrent streams for GPU work
	virtual bool getUsingConcurrentStreams() const = 0; //!< true if GPU work can run in concurrent streams
    /* End query methods that don't require context to be acquired */

	virtual void getDeviceMemoryInfo(size_t& free, size_t& total) const = 0; //!< get currently available and total memory

    /**
     * \brief Get the cuda modules that have been loaded into this context on construction
     * \return Pointer to the cuda modules
     */
	virtual CUmodule* getCuModules() = 0;

    /**
     * \brief Release the PxCudaContextManager
     *
     * If the PxCudaContextManager created the CUDA context it was 
	 * responsible for, it also frees that context.
     */
	virtual void release() = 0;

	/**
	 * \brief Acquire a reference to the PxCudaContextManager, incrementing its reference count.
	 *
	 * The context manager is destroyed once its reference count reaches zero (via release()). Holding a
	 * reference guarantees it stays alive; every acquireReference() must be matched by a release().
	 * Used e.g. by a scene to ensure the context manager cannot be destroyed while the scene still uses it.
	 */
	virtual void acquireReference() = 0;

protected:

    /**
     * \brief protected destructor, use release() method
     */
    virtual ~PxCudaContextManager() {}
	
	PX_DEPRECATED virtual void* allocDeviceBufferInternal(PxU64 numBytes, const char* filename = NULL, PxI32 line = -1) = 0;	
	PX_DEPRECATED virtual void* allocPinnedHostBufferInternal(PxU64 numBytes, const char* filename = NULL, PxI32 line = -1) = 0;
	
	PX_DEPRECATED virtual void freeDeviceBufferInternal(void* deviceBuffer) = 0;	
	PX_DEPRECATED virtual void freePinnedHostBufferInternal(void* pinnedHostBuffer) = 0;	 

	PX_DEPRECATED virtual void clearDeviceBufferAsyncInternal(void* deviceBuffer, PxU32 numBytes, CUstream stream, PxI32 value) = 0;
	 
	PX_DEPRECATED virtual void copyDToHAsyncInternal(void* hostBuffer, const void* deviceBuffer, PxU32 numBytes, CUstream stream) = 0;
	PX_DEPRECATED virtual void copyHToDAsyncInternal(void* deviceBuffer, const void* hostBuffer, PxU32 numBytes, CUstream stream) = 0;
	PX_DEPRECATED virtual void copyDToDAsyncInternal(void* dstDeviceBuffer, const void* srcDeviceBuffer, PxU32 numBytes, CUstream stream) = 0;
	 
	PX_DEPRECATED virtual void copyDToHInternal(void* hostBuffer, const void* deviceBuffer, PxU32 numBytes) = 0;
	PX_DEPRECATED virtual void copyHToDInternal(void* deviceBuffer, const void* hostBuffer, PxU32 numBytes) = 0;

	PX_DEPRECATED virtual void memsetD8AsyncInternal(void* dstDeviceBuffer, const PxU8& value, PxU32 numBytes, CUstream stream) = 0;
	PX_DEPRECATED virtual void memsetD32AsyncInternal(void* dstDeviceBuffer, const PxU32& value, PxU32 numIntegers, CUstream stream) = 0;
};

// These macros are deprecated. Please use the functions in extensions/PxCudaHelpersExt.h
/**
 \deprecated Please use the functions in extensions/PxCudaHelpersExt.h
 */
#define PX_DEVICE_ALLOC(cudaContextManager, deviceBuffer, numElements) cudaContextManager->allocDeviceBuffer(deviceBuffer, numElements, PX_FL)

/**
 \deprecated Please use the functions in extensions/PxCudaHelpersExt.h
 */
#define PX_DEVICE_ALLOC_T(T, cudaContextManager, numElements) cudaContextManager->allocDeviceBuffer<T>(numElements, PX_FL)

/**
 \deprecated Please use the functions in extensions/PxCudaHelpersExt.h
 */
#define PX_DEVICE_FREE(cudaContextManager, deviceBuffer) cudaContextManager->freeDeviceBuffer(deviceBuffer);

/**
 \deprecated Please use the functions in extensions/PxCudaHelpersExt.h
 */
#define PX_PINNED_HOST_ALLOC(cudaContextManager, pinnedHostBuffer, numElements) cudaContextManager->allocPinnedHostBuffer(pinnedHostBuffer, numElements, PX_FL)

/**
 \deprecated Please use the functions in extensions/PxCudaHelpersExt.h
 */
#define PX_PINNED_HOST_ALLOC_T(T, cudaContextManager, numElements) cudaContextManager->allocPinnedHostBuffer<T>(numElements, PX_FL)

/**
 \deprecated Please use the functions in extensions/PxCudaHelpersExt.h
 */
#define PX_PINNED_HOST_FREE(cudaContextManager, pinnedHostBuffer) cudaContextManager->freePinnedHostBuffer(pinnedHostBuffer);

/**
 * \brief Convenience class for holding CUDA lock within a scope
 */
class PxScopedCudaLock
{
public:
    /**
     * \brief ScopedCudaLock constructor
     */
	PxScopedCudaLock(PxCudaContextManager& ctx) : mCtx(&ctx)
	{
		mCtx->acquireContext();
	}

    /**
     * \brief ScopedCudaLock destructor
     */
	~PxScopedCudaLock()
	{
		mCtx->releaseContext();
	}

protected:

    /**
     * \brief CUDA context manager pointer (initialized in the constructor)
     */
    PxCudaContextManager* mCtx;
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_SUPPORT_GPU_PHYSX
#endif

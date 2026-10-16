// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CUDA_HELPERS_EXT_H
#define PX_CUDA_HELPERS_EXT_H

#include "foundation/PxPreprocessor.h"

#if PX_SUPPORT_GPU_PHYSX
#include "cudamanager/PxCudaContext.h"
#include "cudamanager/PxCudaContextManager.h"
#include "cudamanager/PxCudaTypes.h"
#include "foundation/PxAssert.h"
#include "foundation/PxFoundation.h"
#include "foundation/PxSimpleTypes.h"

namespace physx {
namespace Ext {

class PxCudaHelpersExt {
public:
	/**
	 * \brief Allocates a device buffer and returns the pointer to the memory
	 *
	 * The cuda context will get acquired automatically
	 *
	 * \param[in] cudaContextManager the PxCudaContextManager managing the CUDA context the allocation should be attributed to.
	 * \param[in] numElements the number of elements of type T to allocate.
	 *
	 * \return a pointer to the allocated memory.
	 */
	template <typename T>
	static inline T *allocDeviceBuffer(PxCudaContextManager &cudaContextManager, PxU64 numElements) {
		PxScopedCudaLock lock(cudaContextManager);

		CUdeviceptr ptr = 0;
		PxCUresult result = cudaContextManager.getCudaContext()->memAlloc(&ptr, numElements * sizeof(T));
		if (result != 0) {
			PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "allocDeviceBuffer failed with error code %i!\n", PxI32(result));
		}

		return reinterpret_cast<T *>(ptr);
	}

	/**
	 * \brief Frees a device buffer
	 *
	 * The cuda context will get acquired automatically
	 *
	 * \param[in] cudaContextManager the PxCudaContextManager managing the CUDA context the device buffer is attributed to.
	 * \param[in] deviceBuffer reference to a pointer pointing to the device memory buffer.
	 */
	template <typename T>
	static inline void freeDeviceBuffer(PxCudaContextManager &cudaContextManager, T *&deviceBuffer) {
		if (deviceBuffer) {
			PxScopedCudaLock lock(cudaContextManager);
			PxCUresult result = cudaContextManager.getCudaContext()->memFree(reinterpret_cast<CUdeviceptr>(deviceBuffer));
			if (result != 0) {
				PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "freeDeviceBuffer failed with error code %i!\n", PxI32(result));
			}

			deviceBuffer = NULL;
		}
	}

	/**
	 * \brief Allocates a pinned host buffer and returns the pointer to the memory
	 *
	 * A pinned host buffer can be used on the gpu after getting a mapped device pointer from the pinned host buffer pointer, see getMappedDevicePtr
	 * The cuda context will get acquired automatically
	 *
	 * \param[in] cudaContextManager the PxCudaContextManager managing the CUDA context the allocation should be attributed to.
	 * \param[in] numElements the number of elements of type T to allocate.
	 *
	 * \return a pointer to the allocated memory.
	 */
	template <typename T>
	static inline T *allocPinnedHostBuffer(PxCudaContextManager &cudaContextManager, PxU64 numElements) {
		PxScopedCudaLock lock(cudaContextManager);

		void *ptr = NULL;
		const unsigned int cuMemhostallocDevicemap = 0x02;
		const unsigned int cuMemhostallocPortable = 0x01;
		PxCUresult result = cudaContextManager.getCudaContext()->memHostAlloc(&ptr, numElements * sizeof(T), cuMemhostallocDevicemap | cuMemhostallocPortable);
		if (result != 0) {
			PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "allocPinnedHostBuffer failed with error code %i!\n", PxI32(result));
		}

		return reinterpret_cast<T *>(ptr);
	}

	/**
	 * \brief Frees a pinned host buffer
	 *
	 * The cuda context will get acquired automatically
	 *
	 * \param[in] cudaContextManager the PxCudaContextManager managing the CUDA context the pinned memory buffer is attributed to.
	 * \param[in] pinnedHostBuffer reference to a pointer pointing to the pinned memory buffer.
	 */
	template <typename T>
	static inline void freePinnedHostBuffer(PxCudaContextManager &cudaContextManager, T *&pinnedHostBuffer) {
		if (pinnedHostBuffer) {
			PxScopedCudaLock lock(cudaContextManager);
			PxCUresult result = cudaContextManager.getCudaContext()->memFreeHost(pinnedHostBuffer);
			if (result != 0) {
				PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "freePinnedHostBuffer failed with error code %i!\n", PxI32(result));
			}

			pinnedHostBuffer = NULL;
		}
	}

	/**
	 * \brief Copies a device buffer to the host
	 *
	 * The cuda context will get acquired automatically
	 *
	 * \param[in] cudaContextManager the PxCudaContextManager managing the CUDA context executing the operation.
	 * \param[in] hostBuffer pointer to the destination (pinned) host memory buffer.
	 * \param[in] deviceBuffer pointer to the source device buffer.
	 * \param[in] numElements the number of elements of type T to copy.
	 */
	template <typename T>
	static inline void copyDToH(PxCudaContextManager &cudaContextManager, T *hostBuffer, const T *deviceBuffer, PxU64 numElements) {
		if (!deviceBuffer || !hostBuffer) {
			return;
		}

		PxScopedCudaLock lock(cudaContextManager);
		PxU64 numBytes = numElements * sizeof(T);

		PxCUresult result = cudaContextManager.getCudaContext()->memcpyDtoH(hostBuffer, CUdeviceptr(deviceBuffer), numBytes);
		if (result != 0) {
			PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "copyDToH failed with error code %i!\n", PxI32(result));
		}
	}

	/**
	 * \brief Copies a host buffer to the device
	 *
	 * The cuda context will get acquired automatically
	 *
	 * \param[in] cudaContextManager the PxCudaContextManager managing the CUDA context executing the operation.
	 * \param[in] deviceBuffer pointer to the destination device buffer.
	 * \param[in] hostBuffer pointer to the source (pinned) host memory buffer.
	 * \param[in] numElements the number of elements of type T to copy.
	 */
	template <typename T>
	static inline void copyHToD(PxCudaContextManager &cudaContextManager, T *deviceBuffer, const T *hostBuffer, PxU64 numElements) {
		if (!deviceBuffer || !hostBuffer) {
			return;
		}

		PxScopedCudaLock lock(cudaContextManager);
		PxU64 numBytes = numElements * sizeof(T);

		PxCUresult result = cudaContextManager.getCudaContext()->memcpyHtoD(CUdeviceptr(deviceBuffer), hostBuffer, numBytes);
		if (result != 0) {
			PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "copyHtoD failed with error code %i!\n", PxI32(result));
		}
	}

	/**
	 * \brief Schedules device to host copy operation on the specified stream
	 *
	 * The cuda context will get acquired automatically
	 *
	 * \param[in] cudaContextManager the PxCudaContextManager managing the CUDA context executing the operation.
	 * \param[in] hostBuffer pointer to the destination (pinned) host memory buffer.
	 * \param[in] deviceBuffer pointer to the source device buffer.
	 * \param[in] numElements the number of elements of type T to copy.
	 * \param[in] stream the CUDA stream to perform the operation on.
	 */
	template <typename T>
	static inline void copyDToHAsync(PxCudaContextManager &cudaContextManager, T *hostBuffer, const T *deviceBuffer, PxU64 numElements, CUstream stream) {
		if (!deviceBuffer || !hostBuffer) {
			return;
		}

		PxScopedCudaLock lock(cudaContextManager);
		PxU64 numBytes = numElements * sizeof(T);

		PxCUresult result = cudaContextManager.getCudaContext()->memcpyDtoHAsync(hostBuffer, CUdeviceptr(deviceBuffer), numBytes, stream);
		if (result != 0) {
			PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "copyDtoHAsync failed with error code %i!\n", PxI32(result));
		}
	}

	/**
	 * \brief Schedules host to device copy operation on the specified stream
	 *
	 * The cuda context will get acquired automatically
	 *
	 * \param[in] cudaContextManager the PxCudaContextManager managing the CUDA context executing the operation.
	 * \param[in] deviceBuffer pointer to the destination device buffer.
	 * \param[in] hostBuffer pointer to the source (pinned) host memory buffer.
	 * \param[in] numElements the number of elements of type T to copy.
	 * \param[in] stream the CUDA stream to perform the operation on.
	 */
	template <typename T>
	static inline void copyHToDAsync(PxCudaContextManager &cudaContextManager, T *deviceBuffer, const T *hostBuffer, PxU64 numElements, CUstream stream) {
		if (!deviceBuffer || !hostBuffer) {
			return;
		}

		PxScopedCudaLock lock(cudaContextManager);
		PxU64 numBytes = numElements * sizeof(T);

		PxCUresult result = cudaContextManager.getCudaContext()->memcpyHtoDAsync(CUdeviceptr(deviceBuffer), hostBuffer, numBytes, stream);
		if (result != 0) {
			PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "copyHtoDAsync failed with error code %i!\n", PxI32(result));
		}
	}

	/**
	 * \brief Schedules device to device copy operation on the specified stream
	 *
	 * The cuda context will get acquired automatically
	 *
	 * \param[in] cudaContextManager the PxCudaContextManager managing the CUDA context executing the operation.
	 * \param[in] dstDeviceBuffer pointer to the destination device buffer.
	 * \param[in] srcDeviceBuffer pointer to the source device buffer.
	 * \param[in] numElements the number of elements of type T to copy.
	 * \param[in] stream the CUDA stream to perform the operation on.
	 */
	template <typename T>
	static inline void copyDToDAsync(PxCudaContextManager &cudaContextManager, T *dstDeviceBuffer, const T *srcDeviceBuffer, PxU64 numElements, CUstream stream) {
		if (!srcDeviceBuffer || !dstDeviceBuffer) {
			return;
		}

		PxScopedCudaLock lock(cudaContextManager);
		PxU64 numBytes = numElements * sizeof(T);

		PxCUresult result = cudaContextManager.getCudaContext()->memcpyDtoDAsync(CUdeviceptr(dstDeviceBuffer), CUdeviceptr(srcDeviceBuffer), numBytes, stream);
		if (result != 0) {
			PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "copyDtoDAsync failed with error code %i!\n", PxI32(result));
		}
	}

	/**
	 * \brief Schedules a memset operation on the device on the specified stream. Only supported for 1 bytes or 4 byte data types.
	 *
	 * The cuda context will get acquired automatically
	 *
	 * \param[in] cudaContextManager the PxCudaContextManager managing the CUDA context executing the operation.
	 * \param[in] dstDeviceBuffer pointer to the destination device buffer.
	 * \param[in] value the value to set the memory to.
	 * \param[in] numElements the number of elements of type T to set.
	 * \param[in] stream the CUDA stream to perform the operation on.
	 */
	template <typename T>
	static inline void memsetAsync(PxCudaContextManager &cudaContextManager, T *dstDeviceBuffer, const T &value, PxU64 numElements, CUstream stream) {
		PX_COMPILE_TIME_ASSERT(sizeof(T) == sizeof(PxU32) || sizeof(T) == sizeof(PxU8));

		if (!dstDeviceBuffer) {
			return;
		}

		PxScopedCudaLock lock(cudaContextManager);
		PxU64 numBytes = numElements * sizeof(T);

		if (sizeof(T) == sizeof(PxU32)) {
			PxCUresult result = cudaContextManager.getCudaContext()->memsetD32Async(CUdeviceptr(dstDeviceBuffer), reinterpret_cast<const PxU32 &>(value), numBytes >> 2, stream);
			if (result != 0) {
				PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "memsetAsync failed with error code %i!\n", PxI32(result));
			}
		} else {
			PxCUresult result = cudaContextManager.getCudaContext()->memsetD8Async(CUdeviceptr(dstDeviceBuffer), reinterpret_cast<const PxU8 &>(value), numBytes, stream);
			if (result != 0) {
				PxGetFoundation().error(PxErrorCode::eINTERNAL_ERROR, PX_FL, "memsetAsync failed with error code %i!\n", PxI32(result));
			}
		}
	}
};

#define PX_EXT_DEVICE_MEMORY_ALLOC(T, cudaContextManager, numElements) physx::Ext::PxCudaHelpersExt::allocDeviceBuffer<T>(cudaContextManager, numElements)
#define PX_EXT_DEVICE_MEMORY_FREE(cudaContextManager, deviceBuffer) physx::Ext::PxCudaHelpersExt::freeDeviceBuffer(cudaContextManager, deviceBuffer);

#define PX_EXT_PINNED_MEMORY_ALLOC(T, cudaContextManager, numElements) physx::Ext::PxCudaHelpersExt::allocPinnedHostBuffer<T>(cudaContextManager, numElements)
#define PX_EXT_PINNED_MEMORY_FREE(cudaContextManager, pinnedHostBuffer) physx::Ext::PxCudaHelpersExt::freePinnedHostBuffer(cudaContextManager, pinnedHostBuffer);

} //namespace Ext
} //namespace physx

#endif

#endif

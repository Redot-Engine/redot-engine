// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CUDA_CONTEX_H
#define PX_CUDA_CONTEX_H

#include "foundation/PxPreprocessor.h"

#if PX_SUPPORT_GPU_PHYSX

#include "PxCudaTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
	struct PxCudaKernelParam
	{
		void* data;
		size_t size;
	};

	// workaround for not being able to forward declare enums in PxCudaTypes.h. 
	// provides different automatic casting depending on whether cuda.h was included beforehand or not.
	template<typename CUenum>
	struct PxCUenum
	{
		PxU32 value;

		PxCUenum(CUenum e) { value = PxU32(e); }
		operator CUenum() const { return CUenum(value); }
	};

#ifdef CUDA_VERSION
	typedef PxCUenum<CUjit_option> PxCUjit_option;
	typedef PxCUenum<CUresult> PxCUresult;
#else
	typedef PxCUenum<PxU32> PxCUjit_option;
	typedef PxCUenum<PxU32> PxCUresult;
#endif

#define PX_CUDA_KERNEL_PARAM(X)		{ (void*)&X, sizeof(X) }
#define PX_CUDA_KERNEL_PARAM2(X)	(void*)&X

	class PxDeviceAllocatorCallback;
	class PxPinnedHostAllocatorCallback;
	/**
	Cuda Context
	*/
	class PxCudaContext
	{
	protected:
		virtual ~PxCudaContext() {}

		PxDeviceAllocatorCallback* mDeviceAllocatorCallback;
		PxPinnedHostAllocatorCallback* mPinnedHostAllocatorCallback;

	public:
		virtual void release() = 0;

		virtual PxCUresult memAlloc(CUdeviceptr *dptr, size_t bytesize) = 0;

		virtual PxCUresult memFree(CUdeviceptr dptr) = 0;

		virtual PxCUresult memHostAlloc(void **pp, size_t bytesize, unsigned int Flags) = 0;

		virtual PxCUresult memFreeHost(void *p) = 0;

		virtual PxCUresult memHostGetDevicePointer(CUdeviceptr *pdptr, void *p, unsigned int Flags) = 0;

		virtual PxCUresult moduleLoadDataEx(CUmodule *module, const void *image, unsigned int numOptions, PxCUjit_option *options, void **optionValues) = 0;

		virtual PxCUresult moduleGetFunction(CUfunction *hfunc, CUmodule hmod, const char *name) = 0;

		virtual PxCUresult moduleUnload(CUmodule hmod) = 0;

		virtual PxCUresult streamCreate(CUstream *phStream, unsigned int Flags) = 0;

		virtual PxCUresult streamCreateWithPriority(CUstream *phStream, unsigned int flags, int priority) = 0;

		virtual PxCUresult streamFlush(CUstream hStream) = 0;

		virtual PxCUresult streamWaitEvent(CUstream hStream, CUevent hEvent, unsigned int Flags) = 0;

		virtual PxCUresult streamWaitEvent(CUstream hStream, CUevent hEvent) = 0;

		virtual PxCUresult streamDestroy(CUstream hStream) = 0;

		virtual PxCUresult streamSynchronize(CUstream hStream) = 0;

		virtual PxCUresult eventCreate(CUevent *phEvent, unsigned int Flags) = 0;

		virtual PxCUresult eventRecord(CUevent hEvent, CUstream hStream) = 0;

		virtual PxCUresult eventQuery(CUevent hEvent) = 0;

		virtual PxCUresult eventSynchronize(CUevent hEvent) = 0;

		virtual PxCUresult eventDestroy(CUevent hEvent) = 0;

		virtual PxCUresult launchKernel(
			CUfunction f,
			unsigned int gridDimX,
			unsigned int gridDimY,
			unsigned int gridDimZ,
			unsigned int blockDimX,
			unsigned int blockDimY,
			unsigned int blockDimZ,
			unsigned int sharedMemBytes,
			CUstream hStream,
			PxCudaKernelParam* kernelParams,
			size_t kernelParamsSizeInBytes,
			void** extra,
			const char* file,
			int line
		) = 0;

		// PT: same as above but without copying the kernel params to a local stack before the launch
		// i.e. the kernelParams data is passed directly to the kernel.
		virtual PxCUresult launchKernel(
			CUfunction f,
			PxU32 gridDimX, PxU32 gridDimY, PxU32 gridDimZ,
			PxU32 blockDimX, PxU32 blockDimY, PxU32 blockDimZ,
			PxU32 sharedMemBytes,
			CUstream hStream,
			void** kernelParams,
			void** extra,
			const char* file,
			int line
		) = 0;

		virtual PxCUresult memcpyDtoH(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount) = 0;

		virtual PxCUresult memcpyDtoHAsync(void* dstHost, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream) = 0;

		virtual PxCUresult memcpyHtoD(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount) = 0;

		virtual PxCUresult memcpyHtoDAsync(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount, CUstream hStream) = 0;

		virtual PxCUresult memcpyDtoDAsync(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream) = 0;

		virtual PxCUresult memcpyDtoD(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount) = 0;

		virtual PxCUresult memcpyPeerAsync(CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount, CUstream hStream) = 0;

		virtual PxCUresult memsetD32Async(CUdeviceptr dstDevice, unsigned int ui, size_t N, CUstream hStream) = 0;

		virtual PxCUresult memsetD8Async(CUdeviceptr dstDevice, unsigned char uc, size_t N, CUstream hStream) = 0;

		virtual PxCUresult memsetD32(CUdeviceptr dstDevice, unsigned int ui, size_t N) = 0;

		virtual PxCUresult memsetD16(CUdeviceptr dstDevice, unsigned short uh, size_t N) = 0;

		virtual PxCUresult memsetD8(CUdeviceptr dstDevice, unsigned char uc, size_t N) = 0;

		virtual PxCUresult getLastError() = 0;

		PX_DEPRECATED PxDeviceAllocatorCallback* getAllocatorCallback() { return mDeviceAllocatorCallback; }
		PxDeviceAllocatorCallback* getDeviceAllocatorCallback() { return mDeviceAllocatorCallback; }
		PxPinnedHostAllocatorCallback* getPinnedHostAllocatorCallback() { return mPinnedHostAllocatorCallback; }

		virtual void setAbortMode(bool abort) = 0;

		virtual bool isInAbortMode() = 0;
	};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_SUPPORT_GPU_PHYSX
#endif


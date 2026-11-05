// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_ARRAY_CONVERTER_H
#define PX_ARRAY_CONVERTER_H

#include "cudamanager/PxCudaContext.h"
#include "cudamanager/PxCudaContextManager.h"

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxVec4.h"


#if !PX_DOXYGEN
namespace physx
{
#endif

#if PX_SUPPORT_GPU_PHYSX

	/**
	\brief Utility class to convert gpu arrays to a different memory layout
	*/
	class PxArrayConverter
	{
	public:
		/**
		\brief Helper function to merge two separate PxVec4 arrays into one interleaved PxVec3 array
		\param[in] verticesD The vertices device memory buffer
		\param[in] normalsD The normals device memory buffer
		\param[in] length The number of vertices and normals
		\param[out] interleavedResultBufferD The resulting interleaved buffer containing 2*length elements with the format vertex0, normal0, vertex1, normal1...
		\param[in] stream The cuda stream on which the conversion is processed
		*/		
		virtual void interleaveGpuBuffers(const PxVec4* verticesD, const PxVec4* normalsD, PxU32 length, PxVec3* interleavedResultBufferD, CUstream stream) = 0;

		/**
		\brief Destructor
		*/
		virtual ~PxArrayConverter() {}
	};

#endif	

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

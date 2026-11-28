// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_IO_H
#define PX_IO_H


#include "foundation/PxSimpleTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/** enum for empty constructor tag*/
enum PxEMPTY
{
	PxEmpty
};

/**
\brief Input stream class for I/O.

The user needs to supply a PxInputStream implementation to a number of methods to allow the SDK to read data.
*/

class PxInputStream
{
  public:
	/**
	\brief read from the stream. The number of bytes read may be less than the number requested.

	\param[in] dest the destination address to which the data will be read
	\param[in] count the number of bytes requested

	\return the number of bytes read from the stream.
	*/

	virtual uint64_t read(void* dest, uint64_t count) = 0;

	virtual ~PxInputStream()
	{
	}
};

/**
\brief Input data class for I/O which provides random read access.

The user needs to supply a PxInputData implementation to a number of methods to allow the SDK to read data.
*/

class PxInputData : public PxInputStream
{
  public:
	/**
	\brief return the length of the input data

	\return size in bytes of the input data
	*/

	virtual uint64_t getLength() const = 0;

	/**
	\brief seek to the given offset from the start of the data.

	\param[in] offset the offset to seek to. 	If greater than the length of the data, this call is equivalent to
	seek(length);
	*/

	virtual void seek(uint64_t offset) = 0;

	/**
	\brief return the current offset from the start of the data

	\return the offset to seek to.
	*/

	virtual uint64_t tell() const = 0;

	virtual ~PxInputData()
	{
	}
};

/**
\brief Output stream class for I/O.

The user needs to supply a PxOutputStream implementation to a number of methods to allow the SDK to write data.
*/

class PxOutputStream
{
  public:
	/**
	\brief write to the stream. The number of bytes written may be less than the number sent.

	\param[in] src the destination address from which the data will be written
	\param[in] count the number of bytes to be written

	\return the number of bytes written to the stream by this call.
	*/

	virtual uint64_t write(const void* src, uint64_t count) = 0;

	virtual ~PxOutputStream()
	{
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


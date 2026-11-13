// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PVD_TRANSPORT_H
#define PX_PVD_TRANSPORT_H

#include "foundation/PxErrors.h"
#include "foundation/PxFlags.h"
#include "pvd/PxPvd.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief	PxPvdTransport is an interface representing the data transport mechanism.
This class defines all services associated with the transport: configuration, connection, reading, writing etc.
It is owned by the application, and can be realized as a file or a socket (using one-line PxDefault<...> methods in
PhysXExtensions) or in a custom implementation. This is a class that is intended for use by PVD, not by the
application, the application entry points are PxPvd and PvdClient.
*/

class PxPvdTransport
{
  public:
	// connect, isConnected, disconnect, read, write, flush

	/**
	Connects to the Visual Debugger application.
	return True if success
	*/
	virtual bool connect() = 0;

	/**
	Disconnects from the Visual Debugger application.
	If we are still connected, this will kill the entire debugger connection.
	*/
	virtual void disconnect() = 0;

	/**
	 *	Return if connection to PVD is created.
	 */
	virtual bool isConnected() = 0;

	/**
	 *	write bytes to the other endpoint of the connection. should lock before witre. If an error occurs
	 *	this connection will assume to be dead.
	 */
	virtual bool write(const uint8_t* inBytes, uint32_t inLength) = 0;

	/*
	    lock this transport and return it
	*/
	virtual PxPvdTransport& lock() = 0;

	/*
	    unlock this transport
	*/
	virtual void unlock() = 0;

	/**
	 *	send any data and block until we know it is at least on the wire.
	 */
	virtual void flush() = 0;

	/**
	 *	Return size of written data.
	 */
	virtual uint64_t getWrittenDataSize() = 0;

	virtual void release() = 0;

  protected:
	virtual ~PxPvdTransport()
	{
	}
};

/**
	\brief Create a default socket transport.
	\param host host address of the pvd application.
	\param port ip port used for pvd, should same as the port setting in pvd application.
	\param timeoutInMilliseconds timeout when connect to pvd host.
*/
PX_C_EXPORT PxPvdTransport* PX_CALL_CONV
PxDefaultPvdSocketTransportCreate(const char* host, int port, unsigned int timeoutInMilliseconds);

/**
	\brief Create a default file transport.
	\param name full path filename used save captured pvd data, or NULL for a fake/test file transport.
*/
PX_C_EXPORT PxPvdTransport* PX_CALL_CONV PxDefaultPvdFileTransportCreate(const char* name);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif


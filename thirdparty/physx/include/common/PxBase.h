// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BASE_H
#define PX_BASE_H

#include "foundation/PxFlags.h"
#include "foundation/PxString.h"
#include "foundation/PxFoundation.h"
#include "common/PxSerialFramework.h"
#include "common/PxCollection.h"
#include "common/PxTypeInfo.h"
#include "foundation/PxAssert.h"

#define PX_IS_KIND_OF(query, classname, baseclass)                                                                     \
	PX_ASSERT(query != NULL);                                                                                          \
	if(query == NULL)                                                                                                  \
	{                                                                                                                  \
		PxGetFoundation().error(PxErrorCode::eINVALID_PARAMETER, PX_FL, "isKindOf called with invalid string");        \
		return false;                                                                                                  \
	}                                                                                                                  \
	return !Pxstrcmp(classname, query) || baseclass::isKindOf(query)

#if !PX_DOXYGEN
namespace physx
{
#endif

typedef PxU16 PxType;

/**
\brief Flags for PxBase.
*/
struct PxBaseFlag
{
	enum Enum
	{
		eOWNS_MEMORY	= (1<<0),
		eIS_RELEASABLE	= (1<<1)
	};
};

typedef PxFlags<PxBaseFlag::Enum, PxU16> PxBaseFlags;
PX_FLAGS_OPERATORS(PxBaseFlag::Enum, PxU16)

/**
\brief Base class for objects that can be members of a PxCollection.

All PxBase sub-classes can be serialized.

\see PxCollection 
*/
class PxBase
{
public:
	/**
	\brief Releases the PxBase instance, please check documentation of release in derived class.
	*/
	virtual     void				release()										= 0;

	/**
	\brief Returns string name of dynamic type.
	\return	Class name of most derived type of this object.
	*/
	virtual		const char*			getConcreteTypeName() const						= 0;

	/* brief Implements dynamic cast functionality. 

	Example use:
	
	if(actor->is<PxRigidDynamic>()) {...}

	\return A pointer to the specified type if object matches, otherwise NULL
	*/
	template<class T> T*			is()											{ return typeMatch<T>() ? static_cast<T*>(this) : NULL; }

	/* brief Implements dynamic cast functionality for const objects. 

	Example use:
	
	if(actor->is<PxRigidDynamic>()) {...}

	\return A pointer to the specified type if object matches, otherwise NULL
	*/
	template<class T> const T*		is() const										{ return typeMatch<T>() ? static_cast<const T*>(this) : NULL; }

	/**
	\brief	Returns concrete type of object.
	\return	PxConcreteType::Enum of serialized object

	\see PxConcreteType
	*/
	PX_FORCE_INLINE	PxType			getConcreteType() const							{ return mConcreteType;	}
				
	/**
	\brief Set PxBaseFlag	

	\param[in] flag The flag to be set
	\param[in] value The flags new value
	*/
	PX_FORCE_INLINE	void			setBaseFlag(PxBaseFlag::Enum flag, bool value)	{ mBaseFlags = value ? mBaseFlags|flag : mBaseFlags&~flag; }
	
	/**
	\brief Set PxBaseFlags	

	\param[in] inFlags The flags to be set

	\see PxBaseFlags
	*/
	PX_FORCE_INLINE	void			setBaseFlags(PxBaseFlags inFlags)				{ mBaseFlags = inFlags; }
	
	/**
	\brief Returns PxBaseFlags 

	\return	PxBaseFlags

	\see PxBaseFlags
	*/
	PX_FORCE_INLINE	PxBaseFlags		getBaseFlags() const							{ return mBaseFlags; }

	/**
	\brief Whether the object is subordinate.
	
	A class is subordinate, if it can only be instantiated in the context of another class.

	\return	Whether the class is subordinate
	
	\see PxSerialization::isSerializable
	*/
	virtual		bool				isReleasable() const							{ return mBaseFlags & PxBaseFlag::eIS_RELEASABLE; }

protected:
	/**
	\brief Constructor setting concrete type and base flags.
	*/
	PX_INLINE						PxBase(PxType concreteType, PxBaseFlags baseFlags)
										: mConcreteType(concreteType), mBaseFlags(baseFlags), mBuiltInRefCount(1)	{}

	/**
	\brief Deserialization constructor setting base flags.
	*/
	PX_INLINE						PxBase(PxBaseFlags baseFlags) : mBaseFlags(baseFlags)
									{
										PX_ASSERT(mBuiltInRefCount == 1);
									}
	/**
	\brief Destructor.
	*/
	virtual							~PxBase()										{}

	/**
	\brief Returns whether a given type name matches with the type of this instance
	*/	
	virtual				bool		isKindOf(const char* superClass) const { return !Pxstrcmp(superClass, "PxBase"); }

	template<class T>	bool		typeMatch() const
									{
										return PxU32(PxTypeInfo<T>::eFastTypeId)!=PxU32(PxConcreteType::eUNDEFINED) ? 
											PxU32(getConcreteType()) == PxU32(PxTypeInfo<T>::eFastTypeId) : isKindOf(PxTypeInfo<T>::name());
									}

protected:
						PxType		mConcreteType;			// concrete type identifier - see PxConcreteType.
						PxBaseFlags	mBaseFlags;				// internal flags
						PxU32		mBuiltInRefCount;
};

/**
\brief Base class for ref-counted objects.
*/
class PxRefCounted : public PxBase
{
public:

	/**
	\brief Decrements the reference count of the object and releases it if the new reference count is zero.		
	*/
	virtual     void				release()					= 0;

	/**
	\brief Returns the reference count of the object.

	At creation, the reference count of the object is 1. Every other object referencing this object increments the
	count by 1.	When the reference count reaches 0, and only then, the object gets destroyed automatically.

	\return the current reference count.
	*/
	virtual				PxU32		getReferenceCount()	const	= 0;

	/**
	\brief Acquires a counted reference to this object.

	This method increases the reference count of the object by 1. Decrement the reference count by calling release()
	*/
	virtual				void		acquireReference()			= 0;

protected:
	virtual				void		onRefCountZero()	{ delete this;	}

	PX_INLINE						PxRefCounted(PxType concreteType, PxBaseFlags baseFlags) : PxBase(concreteType, baseFlags)	{}
	PX_INLINE						PxRefCounted(PxBaseFlags baseFlags) : PxBase(baseFlags)										{}
	virtual							~PxRefCounted()																				{}
	virtual				bool		isKindOf(const char* name) const { PX_IS_KIND_OF(name, "PxRefCounted", PxBase); }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

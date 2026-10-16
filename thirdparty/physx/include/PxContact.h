// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONTACT_H
#define PX_CONTACT_H


#include "foundation/PxVec3.h"
#include "foundation/PxAssert.h"
#include "PxConstraintDesc.h"
#include "PxNodeIndex.h"
#include "PxMaterial.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

#if PX_VC
#pragma warning(push)
#pragma warning(disable: 4324)	// Padding was added at the end of a structure because of a __declspec(align) value.
#endif

#define PXC_CONTACT_NO_FACE_INDEX 0xffffffff

class PxActor;

/**
\brief Header for a contact patch where all points share same material and normal
*/
PX_ALIGN_PREFIX(16)
struct PxContactPatch
{
	enum PxContactPatchFlags
	{
		eHAS_FACE_INDICES = 1,				//!< Indicates this contact stream has face indices.
		eMODIFIABLE = 2,					//!< Indicates this contact stream is modifiable.
		eFORCE_NO_RESPONSE = 4,				//!< Indicates this contact stream is notify-only (no contact response).
		eHAS_MODIFIED_MASS_RATIOS = 8,		//!< Indicates this contact stream has modified mass ratios
		eHAS_TARGET_VELOCITY = 16,			//!< Indicates this contact stream has target velocities set
		eHAS_MAX_IMPULSE = 32,				//!< Indicates this contact stream has max impulses set
		eREGENERATE_PATCHES = 64,			//!< Indicates this contact stream needs patches re-generated. This is required if the application modified either the contact normal or the material properties
		eCOMPRESSED_MODIFIED_CONTACT = 128
	};

	/**
	\brief Modifiers for scaling the inertia of the involved bodies
	*/
	PX_ALIGN(16, PxConstraintInvMassScale mMassModification);

	/**
	\brief Contact normal
	*/
	PX_ALIGN(16, PxVec3	normal);

	/**
	\brief Restitution coefficient
	*/
	PxReal	restitution;

	/**
	\brief Dynamic friction coefficient
	*/
	PxReal	dynamicFriction;

	/**
	\brief Static friction coefficient
	*/
	PxReal	staticFriction;

	/**
	\brief Damping coefficient (for compliant contacts)
	*/
	PxReal	damping;

	/**
	\brief Index of the first contact in the patch
	*/
	PxU16	startContactIndex;
	
	/**
	\brief The number of contacts in this patch
	*/
	PxU8	nbContacts;

	/**
	\brief The combined material flag of two actors that come in contact
	\see PxMaterialFlag, PxCombineMode
	*/
	PxU8	materialFlags;

	/**
	\brief The PxContactPatchFlags for this patch
	*/
	PxU16	internalFlags;

	/**
	\brief Material index of first body
	*/
	PxU16	materialIndex0;

	/**
	\brief Material index of second body
	*/
	PxU16	materialIndex1;

	PxU16	pad[5];
}
PX_ALIGN_SUFFIX(16);

/**
\brief Contact point data
*/
PX_ALIGN_PREFIX(16)
struct PxContact
{
	/**
	\brief Contact point in world space
	*/
	PxVec3	contact;
	/**
	\brief Separation value (negative implies penetration).
	*/
	PxReal	separation;
}
PX_ALIGN_SUFFIX(16);

/**
\brief Contact point data with additional target and max impulse values
*/
PX_ALIGN_PREFIX(16)
struct PxExtendedContact : public PxContact
{
	/**
	\brief Target velocity
	*/
	PX_ALIGN(16, PxVec3 targetVelocity);
	/**
	\brief Maximum impulse
	*/
	PxReal	maxImpulse;
}
PX_ALIGN_SUFFIX(16);

/**
\brief A modifiable contact point. This has additional fields per-contact to permit modification by user.
\note Not all fields are currently exposed to the user.
*/
PX_ALIGN_PREFIX(16)
struct PxModifiableContact : public PxExtendedContact
{
	/**
	\brief Contact normal
	*/
	PX_ALIGN(16, PxVec3	normal);
	/**
	\brief Restitution coefficient
	*/
	PxReal	restitution;
	
	/**
	\brief Material Flags
	*/
	PxU32	materialFlags;
	
	/**
	\brief Shape A's material index
	*/
	PxU16	materialIndex0;
	/**
	\brief Shape B's material index
	*/
	PxU16	materialIndex1;
	/**
	\brief static friction coefficient
	*/
	PxReal	staticFriction;
	/**
	\brief dynamic friction coefficient
	*/	
	PxReal dynamicFriction;
}
PX_ALIGN_SUFFIX(16);

/**
\brief A class to iterate over a compressed contact stream. This supports read-only access to the various contact formats.
*/
struct PxContactStreamIterator
{
	enum StreamFormat
	{
		eSIMPLE_STREAM,
		eMODIFIABLE_STREAM,
		eCOMPRESSED_MODIFIABLE_STREAM
	};
	/**
	\brief Utility zero vector to optimize functions returning zero vectors when a certain flag isn't set.
	\note This allows us to return by reference instead of having to return by value. Returning by value will go via memory (registers -> stack -> registers), which can 
	cause performance issues on certain platforms.
	*/
	PxVec3 zero;

	/**
	\brief The patch headers.
	*/
	const PxContactPatch* patch;

	/**
	\brief The contacts
	*/
	const PxContact* contact;

	/**
	\brief The contact triangle face index
	*/
	const PxU32* faceIndice;

	/**
	\brief The total number of patches in this contact stream
	*/
	PxU32 totalPatches;
	
	/**
	\brief The total number of contact points in this stream
	*/
	PxU32 totalContacts;

	/**
	\brief The current contact index
	*/
	PxU32 nextContactIndex;
	
	/**
	\brief The current patch Index
	*/
	PxU32 nextPatchIndex;

	/**
	\brief Size of contact patch header 
	\note This varies whether the patch is modifiable or not.
	*/
	PxU32 contactPatchHeaderSize;

	/**
	\brief Contact point size
	\note This varies whether the patch has feature indices or is modifiable.
	*/
	PxU32 contactPointSize;

	/**
	\brief The stream format
	*/
	StreamFormat mStreamFormat;

	/**
	\brief Indicates whether this stream is notify-only or not.
	*/
	PxU32 forceNoResponse;

	/**
	\brief Internal helper for stepping the contact stream iterator
	*/
	bool pointStepped;

	/**
	\brief Specifies if this contactPatch has face indices (handled as bool)
	\see faceIndice
	*/
	PxU32 hasFaceIndices;

	/**
	\brief Constructor
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxContactStreamIterator(const PxU8* contactPatches, const PxU8* contactPoints, const PxU32* contactFaceIndices, PxU32 nbPatches, PxU32 nbContacts) 
		: zero(0.f)
	{
		bool modify = false;
		bool compressedModify = false;
		bool response = false;
		bool indices = false; 
		
		PxU32 pointSize = 0;
		PxU32 patchHeaderSize = sizeof(PxContactPatch);

		const PxContactPatch* patches = reinterpret_cast<const PxContactPatch*>(contactPatches);

		if(patches)
		{
			modify = (patches->internalFlags & PxContactPatch::eMODIFIABLE) != 0;
			compressedModify = (patches->internalFlags & PxContactPatch::eCOMPRESSED_MODIFIED_CONTACT) != 0;
			indices = (patches->internalFlags & PxContactPatch::eHAS_FACE_INDICES) != 0;

			patch = patches;

			contact = reinterpret_cast<const PxContact*>(contactPoints);

			faceIndice = contactFaceIndices;

			pointSize = compressedModify ?  sizeof(PxExtendedContact) : modify ? sizeof(PxModifiableContact) : sizeof(PxContact);

			response = (patch->internalFlags & PxContactPatch::eFORCE_NO_RESPONSE) == 0;
		}


		mStreamFormat = compressedModify ? eCOMPRESSED_MODIFIABLE_STREAM : modify ? eMODIFIABLE_STREAM : eSIMPLE_STREAM;
		hasFaceIndices = PxU32(indices);
		forceNoResponse = PxU32(!response);

		contactPatchHeaderSize = patchHeaderSize;
		contactPointSize = pointSize;
		nextPatchIndex = 0;
		nextContactIndex = 0;
		totalContacts = nbContacts;
		totalPatches = nbPatches;
		
		pointStepped = false;
	}

	/**
	\brief Returns whether there are more patches in this stream.
	\return Whether there are more patches in this stream.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool hasNextPatch() const
	{
		return nextPatchIndex < totalPatches;
	}

	/**
	\brief Returns the total contact count.
	\return Total contact count.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxU32 getTotalContactCount() const
	{
		return totalContacts;
	}

	/**
	\brief Returns the total patch count.
	\return Total patch count.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxU32 getTotalPatchCount() const
	{
		return totalPatches;
	}

	/**
	\brief Advances iterator to next contact patch.
	*/
	PX_CUDA_CALLABLE PX_INLINE void nextPatch()
	{
		PX_ASSERT(nextPatchIndex < totalPatches);
		if(nextPatchIndex)
		{
			if(nextContactIndex < patch->nbContacts)
			{
				PxU32 nbToStep = patch->nbContacts - this->nextContactIndex;
				contact = reinterpret_cast<const PxContact*>(reinterpret_cast<const PxU8*>(contact) + contactPointSize * nbToStep);
			}
			patch = reinterpret_cast<const PxContactPatch*>(reinterpret_cast<const PxU8*>(patch) + contactPatchHeaderSize);
		}
		nextPatchIndex++;
		nextContactIndex = 0;
	}

	/**
	\brief Returns if the current patch has more contacts.
	\return If there are more contacts in the current patch.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool hasNextContact() const
	{
		return nextContactIndex < (patch->nbContacts);
	}

	/**
	\brief Advances to the next contact in the patch.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE void nextContact()
	{
		PX_ASSERT(nextContactIndex < patch->nbContacts);
		if(pointStepped)
		{
			contact = reinterpret_cast<const PxContact*>(reinterpret_cast<const PxU8*>(contact) + contactPointSize);
			faceIndice++;
		}
		nextContactIndex++;
		pointStepped = true;
	}

	/**
	\brief Gets the current contact's normal
	\return The current contact's normal.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE const PxVec3& getContactNormal() const
	{
		return getContactPatch().normal;
	}

	/**
	\brief Gets the inverse mass scale for body 0.
	\return The inverse mass scale for body 0.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getInvMassScale0() const
	{
		return patch->mMassModification.linear0;
	}

	/**
	\brief Gets the inverse mass scale for body 1.
	\return The inverse mass scale for body 1.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getInvMassScale1() const
	{
		return patch->mMassModification.linear1;
	}

	/**
	\brief Gets the inverse inertia scale for body 0.
	\return The inverse inertia scale for body 0.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getInvInertiaScale0() const
	{
		return patch->mMassModification.angular0;
	}

	/**
	\brief Gets the inverse inertia scale for body 1.
	\return The inverse inertia scale for body 1.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getInvInertiaScale1() const
	{
		return patch->mMassModification.angular1;
	}

	/**
	\brief Gets the contact's max impulse.
	\return The contact's max impulse.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getMaxImpulse() const
	{
		return mStreamFormat != eSIMPLE_STREAM ? getExtendedContact().maxImpulse : PX_MAX_REAL;
	}

	/**
	\brief Gets the contact's target velocity.
	\return The contact's target velocity.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE const PxVec3& getTargetVel() const
	{
		return mStreamFormat != eSIMPLE_STREAM ? getExtendedContact().targetVelocity : zero;
	}

	/**
	\brief Gets the contact's contact point.
	\return The contact's contact point.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE const PxVec3& getContactPoint() const
	{
		return contact->contact;
	}

	/**
	\brief Gets the contact's separation.
	\return The contact's separation.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getSeparation() const
	{
		return contact->separation;
	}

	/**
	\brief Gets the contact's face index for shape 0.
	\return The contact's face index for shape 0.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxU32 getFaceIndex0() const
	{
		return PXC_CONTACT_NO_FACE_INDEX;
	}

	/**
	\brief Gets the contact's face index for shape 1.
	\return The contact's face index for shape 1.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxU32 getFaceIndex1() const
	{
		return hasFaceIndices ? *faceIndice : PXC_CONTACT_NO_FACE_INDEX;
	}

	/**
	\brief Gets the contact's static friction coefficient.
	\return The contact's static friction coefficient.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getStaticFriction() const
	{
		return getContactPatch().staticFriction;
	}

	/**
	\brief Gets the contact's dynamic friction coefficient.
	\return The contact's dynamic friction coefficient.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getDynamicFriction() const
	{
		return getContactPatch().dynamicFriction;
	}

	/**
	\brief Gets the contact's restitution coefficient.
	\return The contact's restitution coefficient.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getRestitution() const
	{
		return getContactPatch().restitution;
	}

	/**
	\brief Gets the contact's damping value.
	\return The contact's damping value.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getDamping() const
	{
		return getContactPatch().damping;
	}

	/**
	\brief Gets the contact's material flags.
	\return The contact's material flags.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxU32 getMaterialFlags() const
	{
		return getContactPatch().materialFlags;
	}

	/**
	\brief Gets the contact's material index for shape 0.
	\return The contact's material index for shape 0.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxU16 getMaterialIndex0() const
	{
		return PxU16(getContactPatch().materialIndex0);
	}

	/**
	\brief Gets the contact's material index for shape 1.
	\return The contact's material index for shape 1.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxU16 getMaterialIndex1() const
	{
		return PxU16(getContactPatch().materialIndex1);
	}

	/**
	\brief Advances the contact stream iterator to a specific contact index.
	\return True if advancing was possible
	*/
	bool advanceToIndex(const PxU32 initialIndex)
	{
		PX_ASSERT(this->nextPatchIndex == 0 && this->nextContactIndex == 0);
	
		PxU32 numToAdvance = initialIndex;

		if(numToAdvance == 0)
		{
			PX_ASSERT(hasNextPatch());
			nextPatch();
			return true;
		}
		
		while(numToAdvance)
		{
			while(hasNextPatch())
			{
				nextPatch();
				PxU32 patchSize = patch->nbContacts;
				if(numToAdvance <= patchSize)
				{
					contact = reinterpret_cast<const PxContact*>(reinterpret_cast<const PxU8*>(contact) + contactPointSize * numToAdvance);
					nextContactIndex += numToAdvance;
					return true;
				}
				else
				{
					numToAdvance -= patchSize;
				}
			}
		}
		return false;
	}

private:

	/**
	\brief Internal helper
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE const PxContactPatch& getContactPatch() const
	{
		return *static_cast<const PxContactPatch*>(patch);
	}

	PX_CUDA_CALLABLE PX_FORCE_INLINE const PxExtendedContact& getExtendedContact() const
	{
		PX_ASSERT(mStreamFormat == eMODIFIABLE_STREAM || mStreamFormat == eCOMPRESSED_MODIFIABLE_STREAM);
		return *static_cast<const PxExtendedContact*>(contact);
	}

};

/**
\brief Contact patch friction information.
*/
struct PxFrictionPatch
{
	/**
	\brief Max anchors per patch
	*/
	static const PxU32 MAX_ANCHOR_COUNT = 2;

	/**
	\brief Friction anchors' positions
	*/
	PxVec3 anchorPositions[MAX_ANCHOR_COUNT];

	/**
	\brief Friction anchors' impulses
	*/
	PxVec3 anchorImpulses[MAX_ANCHOR_COUNT];

	/**
	\brief Friction anchor count
	*/
	PxU32 anchorCount;
};

/**
\brief A class to iterate over a friction anchor stream.
*/
class PxFrictionAnchorStreamIterator
{
public:
	/**
	\brief Constructor
	\param contactPatches Pointer to first patch header in contact stream containing contact patch data
	\param frictionPatches Buffer containing contact patches friction information.
	\param patchCount Number of contact patches stored in the contact stream
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxFrictionAnchorStreamIterator(const PxU8* contactPatches, const PxU8* frictionPatches, PxU32 patchCount)
		:
		mContactPatches(reinterpret_cast<const PxContactPatch*>(contactPatches)),
		mFrictionPatches(reinterpret_cast<const PxFrictionPatch*>(frictionPatches)),
		mPatchCount(patchCount),
		mFrictionAnchorIndex(-1),
		mPatchIndex(-1)
	{}

	/**
	\brief Check if there are more patches.
	\return true if there are more patches.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool hasNextPatch() const
	{
		return isValid() && mPatchIndex < PxI32(mPatchCount) - 1;
	}

	/**
	\brief Advance to the next patch.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE void nextPatch()
	{
		PX_ASSERT(hasNextPatch());
		++mPatchIndex;
		mFrictionAnchorIndex = -1;
	}

	/**
	\brief Check if current patch has more friction anchors.
	\return true if there are more friction anchors in current patch.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool hasNextFrictionAnchor() const
	{
		return patchIsValid() && mFrictionAnchorIndex < PxI32(mFrictionPatches[mPatchIndex].anchorCount) - 1;
	}

	/**
	\brief Advance to the next friction anchor in the patch.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE void nextFrictionAnchor()
	{
		PX_ASSERT(hasNextFrictionAnchor());
		mFrictionAnchorIndex++;
	}

	/**
	\brief Get the friction anchor's position.
	\return The friction anchor's position.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE const PxVec3& getPosition() const
	{
		PX_ASSERT(frictionAnchorIsValid());
		return mFrictionPatches[mPatchIndex].anchorPositions[mFrictionAnchorIndex];
	}

	/**
	\brief Get the friction anchor's impulse.
	\return The friction anchor's impulse.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE const PxVec3& getImpulse() const
	{
		PX_ASSERT(frictionAnchorIsValid());
		return mFrictionPatches[mPatchIndex].anchorImpulses[mFrictionAnchorIndex];
	}

	/**
	\brief Get the friction anchor's normal.
	\return The friction anchor's normal.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE const PxVec3& getNormal() const
	{
		PX_ASSERT(patchIsValid());
		return mContactPatches[mPatchIndex].normal;
	}

	/**
	\brief Get current patch's static friction coefficient.
	\return The patch's static friction coefficient.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getStaticFriction() const
	{
		PX_ASSERT(patchIsValid());
		return mContactPatches[mPatchIndex].staticFriction;
	}

	/**
	\brief Get current patch's dynamic friction coefficient.
	\return The patch's dynamic friction coefficient.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxReal getDynamicFriction() const
	{
		PX_ASSERT(patchIsValid());
		return mContactPatches[mPatchIndex].dynamicFriction;
	}

	/**
	\brief Get current patch's combined material flags.
	\return The patch's combined material flags.
	\see PxMaterialFlag, PxCombineMode
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxMaterialFlags getMaterialFlags() const
	{
		PX_ASSERT(patchIsValid());
		return PxMaterialFlags(mContactPatches[mPatchIndex].materialFlags);
	}

private:

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxFrictionAnchorStreamIterator();

	/**
	\brief Check if valid.
	\return true if valid.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool isValid() const
	{
		return mContactPatches && mFrictionPatches;
	}

	/**
	\brief Check if current patch is valid.
	\return true if current patch is valid.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool patchIsValid() const
	{
		return isValid() && mPatchIndex >= 0 && mPatchIndex < PxI32(mPatchCount);
	}

	/**
	\brief Check if current friction anchor is valid.
	\return true if current friction anchor is valid.
	*/
	PX_CUDA_CALLABLE PX_FORCE_INLINE bool frictionAnchorIsValid() const
	{
		return patchIsValid() && mFrictionAnchorIndex >= 0 && mFrictionAnchorIndex < PxI32(mFrictionPatches[mPatchIndex].anchorCount);
	}

	const PxContactPatch* mContactPatches;
	const PxFrictionPatch* mFrictionPatches;
	PxU32 mPatchCount;
	PxI32 mFrictionAnchorIndex;
	PxI32 mPatchIndex;
};

/**
\brief Contains contact information for a contact reported by the direct-GPU contact report API. See PxDirectGPUAPI::copyContactData().
*/
struct PxGpuContactPair
{
	PxU8* contactPatches;				//!< Ptr to contact patches. Type: PxContactPatch*, size: nbPatches.
	PxU8* contactPoints;				//!< Ptr to contact points. Type: PxContact*, size: nbContacts.
	PxReal* contactForces;				//!< Ptr to contact forces. Size: nbContacts.
	PxU8* frictionPatches;				//!< Ptr to friction patch information. Type: PxFrictionPatch*, size: nbPatches.
	PxU32 transformCacheRef0;			//!< Ref to shape0's transform in transform cache.
	PxU32 transformCacheRef1;			//!< Ref to shape1's transform in transform cache.
	PxNodeIndex nodeIndex0;				//!< Unique Id for actor0 if the actor is dynamic.
	PxNodeIndex nodeIndex1;				//!< Unique Id for actor1 if the actor is dynamic.
	PxActor* actor0;					//!< Ptr to PxActor for actor0.
	PxActor* actor1;					//!< Ptr to PxActor for actor1.

	PxU16 nbContacts;					//!< Num contacts.
	PxU16 nbPatches;					//!< Num patches.
};


#if PX_VC
#pragma warning(pop)
#endif

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

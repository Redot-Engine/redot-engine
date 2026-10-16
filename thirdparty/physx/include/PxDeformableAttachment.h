// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_ATTACHMENT_H
#define PX_DEFORMABLE_ATTACHMENT_H

#include "foundation/PxTransform.h"
#include "foundation/PxVec4.h"
#include "common/PxCoreUtilityTypes.h"
#include "common/PxBase.h"


#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Identifies the attachment target type for an actor involved in an attachment.

The target type provides actor related information about what kind of attachment should be created.
\see PxDeformableAttachmentData
*/
struct PxDeformableAttachmentTargetType
{
	enum Enum
	{
		eVERTEX,		//!< Attachment to vertex points of deformable mesh.
		eTRIANGLE,		//!< Attachment to points on triangles of deformable mesh.
		eTETRAHEDRON,	//!< Attachment to points in tetrahedrons of deformable mesh.
		eRIGID,			//!< Attachment to points in rigid actor local frame.
		eWORLD,			//!< Attachment to points in global frame.
		eUNDEFINED		//!< Internal use only.
	};
};

/**
\brief Attachment data for a pair of actors where one of the actors must be a deformable. For attaching rigids to rigids or rigids to the world, use joints instead.

An attachment is created based on a collection of attachment points. The attachment 
points are specified relatively to each of the two actors. They can be defined on the basis of 
deformable mesh elements, such as vertices, triangles or tetrahedrons. Depending on the deformable mesh element type, 
a baricentric coordinate further specifies the location on the element. For rigid or world attachments, the points are 
specified using cartesion coordinates.

The points are specified by:
- Two actor instances and their types
- Two attachment target types
- Two sets of attachment data related to the target types

<a name="attachment_table1"></a>
Table 1) The type of an actor limits which target types can be used:

	PxDeformableSurface:	eVERTEX, eTRIANGLE
	PxDeformableVolume:		eVERTEX, eTETRAHEDRON (simulation mesh)
	PxRigidActor:			eRIGID
	NULL:					eWORLD

<a name="attachment_table2"></a>
Table 2) On the other hand, the target type dictates which per-actor attachment data is needed:

	eVERTEX:		indices
	eTRIANGLE:		indices, coords (barycentric: x, y, z)
	eTETRAHEDRON:	indices, coords (barycentric: x, y, z, w)
	eRIGID:			pose, coords (cartesian, local space: x, y, z)
	eWORLD:			pose, coords (cartesion, world space: x, y, z)

Each entry pair in (indices, coords) defines an attachment point. Therefore, the size of indices and coords need to match up, if both are required. 

\note The topology of an attachment is fixed once it's been created. To change the attachment points,
the application will need to release the attachment and create a new attachment with the updated attachment points.
The pose for attachments to eRIGID or eWORLD, however, can be updated without re-creating the attachment.

\see PxDeformableAttachment, PxPhysics::createDeformableAttachment()
*/
struct PxDeformableAttachmentData
{
	PxDeformableAttachmentData()
	{
		for (PxU32 i = 0; i < 2; i++)
		{
			actor[i] = NULL;
			type[i] = PxDeformableAttachmentTargetType::eUNDEFINED;
			pose[i] = PxTransform(PxIdentity);
		}
	}

	/**
	\brief Actor 0 and Actor 1. At least one of the actors must be a deformable. For attaching statically to the world, one actor is allowed to be NULL while the other is a deformable.
	*/
	PxActor* actor[2];

	/**
	\brief One target type per actor.
	The target type must be supported by the corresponding actor type, see [table 1](#attachment_table1).
	*/
	PxDeformableAttachmentTargetType::Enum type[2];

	/**
	\brief Indices data per actor.
	The content of the two index arrays depends on the corresponding target types 'type[0]' and 'type[1]'
	as well as the number of attachments:
	For PxDeformableAttachmentTargetType::eVERTEX, eTRIANGLE and eTETRAHEDRON, the corresponding array describes vertex,
	triangle or tetrahedon indices, and the size of the array needs to match the number of attachments.
	For PxDeformableAttachmentTargetType::eRIGID and eWORLD, the corresponding array needs to be empty.
	See [table 2](#attachment_table2).
	*/
	PxTypedBoundedData<const PxU32> indices[2];

	/**
	\brief Coordinate data per actor.
	The content of the two coords arrays depends on the corresponding target types 'type[0]' and 'type[1]'
	as well as the number of attachments:
	For PxDeformableAttachmentTargetType::eVERTEX, the corresponding array needs to be empty.
	For PxDeformableAttachmentTargetType::eTRIANGLE and eTETRAHEDRON, the corresponding array descibes barycentric coordinates,
	and the size of the array needs to match the number of attachments.
	For PxDeformableAttachmentTargetType::eRIGID and eWORLD, the corresponding array describes cartesian coordinates and the size
	of the array needs to match the number of attachments.
	See [table 2](#attachment_table2).
	*/
	PxTypedBoundedData<const PxVec4> coords[2];

	/**
	\brief Pose per actor.
	Global pose for PxDeformableAttachmentTargetType::eWORLD or local pose for PxDeformableAttachmentTargetType::eRIGID, see [table 2](#attachment_table2).
	The pose represents a coordinate frame for all attachment points specified by the array of euclidean coords in the case of PxDeformableAttachmentTargetType::eRIGID and eWORLD attachments.
	It can be updated after the attachment has been created.

	\see PxDeformableAttachment::updatePose()
	*/
	PxTransform pose[2];
};

/**
\brief PxDeformableAttachment class representing an attachment for deformable actors.

An attachment is a collection of one or more positional constraints between a point on one actor and a point on another actor.

\see PxDeformableAttachmentData, PxPhysics::createDeformableAttachment()
*/
class PxDeformableAttachment : public PxBase
{
public:
	/**
	\brief Gets the two actors for this attachment.

	\param[out] actor0 The first actor, may be NULL
	\param[out] actor1 The second actor, may be NULL
	*/
	virtual void	getActors(PxActor*& actor0, PxActor*& actor1) const = 0;

	/**
	\brief Updates the pose of the attachment.

	\param[in] pose Pose relative to world or rigid actor transform. Valid only for attachment against world or rigid actor.
	*/
	virtual void	updatePose(const PxTransform& pose) = 0;

	/**
	\brief Returns string name of PxDeformableAttachment, used for serialization
	*/
	virtual	const char*			getConcreteTypeName() const PX_OVERRIDE { return "PxDeformableAttachment"; }

	void* userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.

protected:
	virtual						~PxDeformableAttachment() {}

	//serialization

	/**
	\brief Constructor
	*/
	PX_INLINE					PxDeformableAttachment(PxType concreteType, PxBaseFlags baseFlags) : PxBase(concreteType, baseFlags), userData(NULL) {}

	/**
	\brief Deserialization constructor
	*/
	PX_INLINE					PxDeformableAttachment(PxBaseFlags baseFlags) : PxBase(baseFlags) {}

	/**
	\brief Returns whether a given type name matches with the type of this instance
	*/
	virtual	bool				isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxDeformableAttachment", PxBase); }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

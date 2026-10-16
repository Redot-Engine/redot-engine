// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_ELEMENT_FILTER_H
#define PX_DEFORMABLE_ELEMENT_FILTER_H

#include "common/PxCoreUtilityTypes.h"
#include "common/PxBase.h"

/** \addtogroup physics
@{
*/

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Element filter data for a pair of actors where one of the actors must be a deformable.

An element filter defines mesh element wise collision filtering between two deformable actors, or one deformable actor and
a rigid actor. 

The element types used for collision filtering are implicitely given by the deformable actor type:

	PxDeformableSurface: PxTriangleMesh triangle indices.
	PxDeformableVolume: PxTetrahedronMesh tetrahedron indices (collision mesh)

If the actor is rigid, then filtering always relates to the actor as a whole.

In order to effectively specify which elements shouldn't collide against which other elements, the following representation is used.
A pair of element groups specifies that none of the elements in the first group collides against any elements of the second group. One group relates 
to one actor, and the other group relates to the other actor. The whole collision filter consists of a set of element group pairs.

In the following we use "A" to denote one of the two actors, so either actor with index 0 or index 1 in the actor array, and "B" for the other actor.

The element groups are specified for each actor separately:
The groups for actor A are specified by groupElemCounts[A] and groupElemIndices[A]

The size of groupElemCounts[A] and groupElemCounts[B] specifies the number of group pairs. They need to have the same number of entries (there is an exception, see further below).
Each entry in the groupElemCounts[A] specifies the size of each group of elements in the mesh of actor A.

The entries in groupElemIndices[A] represent all elements referenced by the groupElemCounts[A], in the order of the groups specified in groupElemCounts[A].

Below are some examples to clarify the concept.

Example 1: Two groups for each actor.
groupElemCounts[0] = [2, 1], groupElemIndices[0] = [3, 4, 6]
groupElemCounts[1] = [2, 3], groupElemIndices[1] = [9, 7, 2, 5, 6]
For the first group, the count is 2 for both actors. So element 3 and 4 of actor[0] is filtered against element 9 and 7 of actor[1].
For the second group, the element count is 1 for actor[0] and 3 for actor[1]. So element 6 of actor[0] is filtered against element 2, 5 and 6 of actor[1].

Example 2: Pairwise filtering.
groupElemCounts[0] = [1, 1], groupElemIndices[0] = [3, 4]
groupElemCounts[1] = [1, 1], groupElemIndices[1] = [9, 7]
For the first group, element 3 of actor[0] is filtered against element 9 of actor[1]
For the second group, element 4 of actor[0] is filtered against element 7 of actor[1]

There are two special cases that are supported by the element filter.
- groupElemCounts[A] entries that are 0, indicate that elements of the corresponding group of actor B are filtered against all elements of actor A.
- empty groupElemCounts[A], indicates that all groups of actor B are filtered against all elements of actor A. This is always the case if actor A is a rigid.

\see PxDeformableElementFilter, PxPhysics::createDeformableElementFilter()
*/
struct PxDeformableElementFilterData
{
	PxDeformableElementFilterData()
	{
		for (PxU32 i = 0; i < 2; i++)
		{
			actor[i] = NULL;
		}
	}

	/**
	\brief Actor 0 and Actor 1. NULL actors are not allowed.
	*/
	PxActor* actor[2];

	/**
	\brief Element counts for all filter groups, per actor.
	*/
	PxTypedBoundedData<const PxU32> groupElementCounts[2];

	/**
	\brief Element indices for all filter groups, per actor.
	*/
	PxTypedBoundedData<const PxU32> groupElementIndices[2];
};

/**
\brief PxDeformableElementFilter class representing an element level collision filter for deformable actors.

Element filters define how parts of deformable actors are excluded from collisions.
They are usually added to avoid conflicting attachment and contact constraints.

\see PxDeformableElementFilterData, PxPhysics::createDeformableElementFilter()
*/
class PxDeformableElementFilter : public PxBase
{
public:
	/**
	\brief Gets the actors for this element filter.

	\param[out] actor0 The first actor.
	\param[out] actor1 The second actor.
	*/
	virtual void				getActors(PxActor*& actor0, PxActor*& actor1) const = 0;

	/**
	\brief Returns string name of PxDeformableElementFilter, used for serialization
	*/
	virtual	const char*			getConcreteTypeName() const PX_OVERRIDE { return "PxDeformableElementFilter"; }

	void* userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.

protected:
	virtual						~PxDeformableElementFilter() {}

	//serialization

	/**
	\brief Constructor
	*/
	PX_INLINE					PxDeformableElementFilter(PxType concreteType, PxBaseFlags baseFlags) : PxBase(concreteType, baseFlags), userData(NULL) {}

	/**
	\brief Deserialization constructor
	*/
	PX_INLINE					PxDeformableElementFilter(PxBaseFlags baseFlags) : PxBase(baseFlags) {}

	/**
	\brief Returns whether a given type name matches with the type of this instance
	*/
	virtual	bool				isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxDeformableElementFilter", PxBase); }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

/** @} */
#endif

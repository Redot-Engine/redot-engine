// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SERIALIZATION_H
#define PX_SERIALIZATION_H

#include "PxPhysXConfig.h"
#include "common/PxBase.h"
#include "cooking/PxCooking.h"
#include "foundation/PxIO.h"
#include "common/PxTolerancesScale.h"
#include "common/PxTypeInfo.h"
#include "common/PxStringTable.h"

/**
PX_BINARY_SERIAL_VERSION is used to version the PhysX binary data and meta data. The global unique identifier of the PhysX SDK needs to match 
the one in the data and meta data, otherwise they are considered incompatible. A 32 character wide GUID can be generated with https://www.guidgenerator.com/ for example. 
*/
#define PX_BINARY_SERIAL_VERSION "EDB4E4FBAE27E728C721DDDD619FD191"


#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Utility functions for serialization

\see PxCollection, PxSerializationRegistry
*/
class PxSerialization
{
public:
	/**
	\brief Additional PxScene and PxPhysics options stored in XML serialized data.

	\deprecated Xml serialization is deprecated. An alternative serialization system is provided through USD Physics.

	The PxXmlMiscParameter parameter can be serialized and deserialized along with PxCollection instances (XML only).
	This is for application use only and has no impact on how objects are serialized or deserialized. 

	\see PxSerialization::createCollectionFromXml, PxSerialization::serializeCollectionToXml
	*/
	struct PX_DEPRECATED PxXmlMiscParameter
	{
		/**
		\brief Up vector for the scene reference coordinate system.
		*/
		PxVec3				upVector;

		/**
		\brief Tolerances scale to be used for the scene.
		*/
		PxTolerancesScale	scale;
		
		PxXmlMiscParameter() : upVector(0) {}
		PxXmlMiscParameter(PxVec3& inUpVector, PxTolerancesScale inScale) : upVector(inUpVector), scale(inScale) {}
	};

	/**
	\brief Returns whether the collection is serializable with the externalReferences collection.

	Some definitions to explain whether a collection can be serialized or not:

	For definitions of <b>requires</b> and <b>complete</b> see #PxSerialization::complete

	A serializable object is <b>subordinate</b> if it cannot be serialized on its own
	The following objects are subordinate:
	- articulation links
	- articulation joints
	- joints

	A collection C can be serialized with external references collection D iff
	- C is complete relative to D (no dangling references)
	- Every object in D required by an object in C has a valid ID (no unnamed references)
	- Every subordinate object in C is required by another object in C (no orphans)

	\param[in] collection Collection to be checked
	\param[in] sr PxSerializationRegistry instance with information about registered classes.
	\param[in] externalReferences the external References collection
	\return  Whether the collection is serializable
	\see PxSerialization::complete, PxSerialization::serializeCollectionToBinary, PxSerialization::serializeCollectionToXml, PxSerializationRegistry
	*/
	static	bool			isSerializable(PxCollection& collection, PxSerializationRegistry& sr, const PxCollection* externalReferences = NULL);

	/**
	\brief Adds to a collection all objects such that it can be successfully serialized.
	
	A collection C is complete relative to an other collection D if every object required by C is either in C or D.
	This function adds objects to a collection, such that it becomes complete with respect to the exceptFor collection.
	Completeness is needed for serialization. See #PxSerialization::serializeCollectionToBinary, 
	#PxSerialization::serializeCollectionToXml.

	Sdk objects require other sdk object according to the following rules: 
	 - joints require their actors and constraint
	 - rigid actors require their shapes
	 - shapes require their material(s) and mesh (triangle mesh, convex mesh or height field), if any
	 - articulations require their links and joints
	 - aggregates require their actors

	If followJoints is specified another rule is added:
	 - actors require their joints
	
	Specifying followJoints will make whole jointed actor chains being added to the collection. Following chains 
	is interrupted whenever a object in exceptFor is encountered.

	\param[in,out] collection Collection which is completed
	\param[in] sr PxSerializationRegistry instance with information about registered classes.
	\param[in] exceptFor Optional exemption collection
	\param[in] followJoints Specifies whether joints should be added for jointed actors
	\see PxCollection, PxSerialization::serializeCollectionToBinary, PxSerialization::serializeCollectionToXml, PxSerializationRegistry
	*/
	static	void			complete(PxCollection& collection, PxSerializationRegistry& sr, const PxCollection* exceptFor = NULL, bool followJoints = false);
	
	/**
	\brief Creates PxSerialObjectId values for unnamed objects in a collection.

	Creates PxSerialObjectId names for unnamed objects in a collection starting at a base value and incrementing, 
	skipping values that are already assigned to objects in the collection.

	\param[in,out] collection Collection for which names are created
	\param[in] base Start address for PxSerialObjectId names
	\see PxCollection
	*/
	static	void			createSerialObjectIds(PxCollection& collection, const PxSerialObjectId base);
			
	/**
	\brief Creates a PxCollection from XML data.
	
	\deprecated Xml serialization is deprecated. An alternative serialization system is provided through USD Physics.

	\param inputData The input data containing the XML collection.
	\param params Cooking parameters used for sdk object instantiation.
	\param sr PxSerializationRegistry instance with information about registered classes.
	\param externalRefs PxCollection used to resolve external references.
	\param stringTable PxStringTable instance used for storing object names.
	\param outArgs Optional parameters of physics and scene deserialized from XML. See #PxSerialization::PxXmlMiscParameter
	\return a pointer to a PxCollection if successful or NULL if it failed.

	\see PxCollection, PxSerializationRegistry, PxInputData, PxStringTable, PxCooking, PxSerialization::PxXmlMiscParameter
	*/
	PX_DEPRECATED static PxCollection* createCollectionFromXml(PxInputData& inputData, const PxCookingParams& params, PxSerializationRegistry& sr, const PxCollection* externalRefs = NULL, PxStringTable* stringTable = NULL, PxXmlMiscParameter* outArgs = NULL);
	
	/**
	\brief Deserializes a PxCollection from memory.

	Creates a collection from memory. If the collection has external dependencies another collection 
	can be provided to resolve these.

	The memory block provided has to be 128 bytes aligned and contain a contiguous serialized collection as written
	by PxSerialization::serializeCollectionToBinary. The contained binary data needs to be compatible with the current binary format version
	which is defined by "PX_PHYSICS_VERSION_MAJOR.PX_PHYSICS_VERSION_MINOR.PX_PHYSICS_VERSION_BUGFIX-PX_BINARY_SERIAL_VERSION".
	For a list of compatible sdk releases refer to the documentation of PX_BINARY_SERIAL_VERSION.

	\param[in] memBlock Pointer to memory block containing the serialized collection
	\param[in] sr PxSerializationRegistry instance with information about registered classes.
	\param[in] externalRefs Collection to resolve external dependencies

	\see PxCollection, PxSerialization::complete, PxSerialization::serializeCollectionToBinary, PxSerializationRegistry, PX_BINARY_SERIAL_VERSION
	*/
	static	PxCollection*	createCollectionFromBinary(void* memBlock, PxSerializationRegistry& sr, const PxCollection* externalRefs = NULL);

	/**
	\brief Serializes a physics collection to an XML output stream.

	\deprecated Xml serialization is deprecated. An alternative serialization system is provided through USD Physics.

	The collection to be serialized needs to be complete \see PxSerialization.complete.
	Optionally the XML may contain meshes in binary cooked format for fast loading. It does this when providing a valid non-null PxCooking pointer.

	\note Serialization of objects in a scene that is simultaneously being simulated is not supported and leads to undefined behavior. 

	\param outputStream Stream to save collection to.
	\param collection PxCollection instance which is serialized. The collection needs to be complete with respect to the externalRefs collection.
	\param sr PxSerializationRegistry instance with information about registered classes.
	\param params Optional pointer to cooking params. If provided, cooked mesh data is cached for fast loading.
	\param externalRefs Collection containing external references.
	\param inArgs Optional parameters of physics and scene serialized to XML along with the collection. See #PxSerialization::PxXmlMiscParameter
	\return true if the collection is successfully serialized.

	\see PxCollection, PxOutputStream, PxSerializationRegistry, PxCooking, PxSerialization::PxXmlMiscParameter
	*/
	PX_DEPRECATED static bool serializeCollectionToXml(PxOutputStream& outputStream, PxCollection& collection, PxSerializationRegistry& sr, const PxCookingParams* params = NULL, const PxCollection* externalRefs = NULL, PxXmlMiscParameter* inArgs = NULL);
	
	/**
	\brief Serializes a collection to a binary stream.

	Serializes a collection to a stream. In order to resolve external dependencies the externalReferences collection has to be provided. 
	Optionally names of objects that where set for example with #PxActor::setName are serialized along with the objects.

	The collection can be successfully serialized if isSerializable(collection) returns true. See #isSerializable.

	The implementation of the output stream needs to fulfill the requirements on the memory block input taken by
	PxSerialization::createCollectionFromBinary.

	\note Serialization of objects in a scene that is simultaneously being simulated is not supported and leads to undefined behavior. 

	\param[out] outputStream into which the collection is serialized
	\param[in] collection Collection to be serialized
	\param[in] sr PxSerializationRegistry instance with information about registered classes.
	\param[in] externalRefs Collection used to resolve external dependencies
	\param[in] exportNames Specifies whether object names are serialized
	\return Whether serialization was successful

	\see PxCollection, PxOutputStream, PxSerialization::complete, PxSerialization::createCollectionFromBinary, PxSerializationRegistry
	*/
	static	bool			serializeCollectionToBinary(PxOutputStream& outputStream, PxCollection& collection, PxSerializationRegistry& sr, const PxCollection* externalRefs = NULL, bool exportNames = false );

	/**
	\brief Creates an application managed registry for serialization.
	
	\param[in] physics Physics SDK to generate create serialization registry
	
	\return PxSerializationRegistry instance.

	\see PxSerializationRegistry
	*/
	static PxSerializationRegistry* createSerializationRegistry(PxPhysics& physics);	
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

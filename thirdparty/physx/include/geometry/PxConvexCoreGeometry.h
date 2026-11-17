// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONVEX_CORE_GEOMETRY_H
#define PX_CONVEX_CORE_GEOMETRY_H

#include "foundation/PxVec3.h"
#include "foundation/PxMemory.h"
#include "geometry/PxGeometry.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief Pre-authored cores for convex core geometry
	*/
	class PxConvexCore
	{
	public:
		/**
		\brief Enumeration of core types for convex core geometries.

		This enum defines the various cores that can be used as the basis
		for creating convex core geometries. Each type represents a different
		fundamental shape that can be extended with a margin to create more
		complex convex shapes.
		*/
		enum Type
		{
			ePOINT,
			eSEGMENT,
			eBOX,
			eELLIPSOID,
			eCYLINDER,
			eCONE,

			eCOUNT
		};

		/**
		\brief Point core data

		The point core has no additional data. The point is located
		at the origin of the geometry's local space
		*/
		struct Point
		{
			static const Type TYPE = ePOINT;
		};

		/**
		\brief Segment core data

		The segment is defined by its length and goes along the geometry's
		local space X axis, from -length/2 to length/2
		*/
		struct Segment
		{
			static const Type TYPE = eSEGMENT;

			/// Segment length
			PxReal length;

			PX_INLINE Segment() {}

			/**
			 \brief Constructs a SegmentCore with a specified length
			 \param _length Segment length
			 */
			PX_INLINE Segment(PxReal _length) : length(_length) {}
		};

		/**
		\brief Box core data

		The box is defined by its extents, centered at the origin of the
		geometry's local space, and aligned with the local space's axes
		*/
		struct Box
		{
			static const Type TYPE = eBOX;

			/// Box extents
			PxVec3 extents;

			PX_INLINE Box() {}

			/**
			\brief Constructs a BoxCore with specified extents
			\param eX Extent in the x direction
			\param eY Extent in the y direction
			\param eZ Extent in the z direction
			*/
			PX_INLINE Box(PxReal eX, PxReal eY, PxReal eZ) : extents(eX, eY, eZ) {}

			/**
			\brief Constructs a BoxCore with specified extents
			\param _extents Vector containing extents in x, y, and z directions
			*/
			PX_INLINE Box(const PxVec3& _extents) : extents(_extents) {}
		};

		/**
		\brief Ellipsoid core data

		The ellipsoid is defined by its radii and is centered at the origin
		of the geometry's local space
		*/
		struct Ellipsoid
		{
			static const Type TYPE = eELLIPSOID;

			/// Ellipsoid radii
			PxVec3 radii;

			PX_INLINE Ellipsoid() {}

			/**
			\brief Constructs an EllipsoidCore with specified radii
			\param rX Radius in the x direction
			\param rY Radius in the y direction
			\param rZ Radius in the z direction
			*/
			PX_INLINE Ellipsoid(PxReal rX, PxReal rY, PxReal rZ) : radii(rX, rY, rZ) {}

			/**
			\brief Constructs an EllipsoidCore with specified radii
			\param _radii Vector containing radii in x, y, and z directions
			*/
			PX_INLINE Ellipsoid(const PxVec3& _radii) : radii(_radii) {}
		};

		/**
		\brief Cylinder core data

		The cylinder is defined by its height and radius. It is centered at the origin
		of the geometry's local space with its axis along the X axis
		*/
		struct Cylinder
		{
			static const Type TYPE = eCYLINDER;

			/// Cylinder height
			PxReal height;

			/// Cylinder radius
			PxReal radius;

			PX_INLINE Cylinder() {}

			/**
			\brief Constructs a CylinderCore with specified height and radius
			\param _height Cylinder height
			\param _radius Cylinder radius
			*/
			PX_INLINE Cylinder(PxReal _height, PxReal _radius) : height(_height), radius(_radius) {}
		};

		/**
		\brief Cone core data

		The cone is defined by its height and base radius. It is centered at the origin
		of the geometry's local space with its axis along the X axis and the base
		at x = -height/2
		*/
		struct Cone
		{
			static const Type TYPE = eCONE;

			/// Cone height
			PxReal height;

			/// Cone base radius
			PxReal radius;

			PX_INLINE Cone() {}

			/**
			\brief Constructs a ConeCore with specified height and radius
			\param _height Cone height
			\param _radius Cone base radius
			*/
			PX_INLINE Cone(PxReal _height, PxReal _radius) : height(_height), radius(_radius) {}
		};
	};

	/**
	\brief Convex core geometry class.
	
	This class allows users to create a variety of convex shapes. Each shape is defined by:
	1. A core, specified by one of the pre-authored GJK support functions.
	2. A margin, which is an arbitrary distance that extends the core.
	
	The resulting convex shape includes both the core and the surrounding space within the margin.
	
	Simple examples include:
	- A sphere: created from a point core with a non-zero margin (radius).
	- A capsule: created from a segment core with a non-zero margin.
	*/
	class PxConvexCoreGeometry : public PxGeometry
	{
	public:

		/**
		\brief Default constructor
		*/
		PX_INLINE PxConvexCoreGeometry();

		/**
		\brief Constructor
		\tparam Core The type of the core
		\param core The core to use
		\param margin The margin to add around the core. Defaults to 0
		*/
		template <typename Core>
		PX_INLINE PxConvexCoreGeometry(const Core& core, PxReal margin = 0);

		/**
		\brief Copy constructor
		\param that The geometry to copy from
		*/
		PX_INLINE PxConvexCoreGeometry(const PxConvexCoreGeometry& that);

		/**
		\brief Assignment operator
		\param that The geometry to assign from
		*/
		PX_INLINE PxConvexCoreGeometry& operator=(const PxConvexCoreGeometry& that);

		/**
		\brief Get the type of the core
		\return The type of the core
		*/
		PX_INLINE PxConvexCore::Type getCoreType() const;

		/// Maximum size of the core data in bytes.
		static const PxU32 MAX_CORE_SIZE = sizeof(PxReal) * 6;

		/**
		\brief Get a pointer to the core data.
		\return A pointer to the core data.
		*/
		PX_INLINE const void* getCoreData() const;

		/**
		\brief Get the core.
		\return The core.
		*/
		template <typename Core>
		PX_INLINE const Core& getCore() const;

		/**
		\brief Get the margin of the convex core geometry.
		\return The margin size.
		*/
		PX_INLINE PxReal getMargin() const;

		/**
		\brief Check if the convex core geometry is valid.
		\return True if the geometry is valid, false otherwise.
		*/
		PX_PHYSX_COMMON_API bool isValid() const;

	private:

		// Core type
		PxConvexCore::Type mCoreType;
		// Core data
		PxU8 mCore[MAX_CORE_SIZE];
		// Margin
		PxReal mMargin;
	};

	PX_INLINE PxConvexCoreGeometry::PxConvexCoreGeometry()
		:
		PxGeometry(PxGeometryType::eCONVEXCORE),
		mCoreType(PxConvexCore::Type(-1)), mMargin(0)
	{}

	template <typename Core>
	PX_INLINE PxConvexCoreGeometry::PxConvexCoreGeometry(const Core& core, PxReal margin)
		:
		PxGeometry(PxGeometryType::eCONVEXCORE),
		mCoreType(Core::TYPE), mMargin(margin)
	{
		PX_ASSERT(sizeof(Core) <= MAX_CORE_SIZE);
		PxMemCopy(mCore, &core, sizeof(Core));
	}

	PX_INLINE PxConvexCoreGeometry::PxConvexCoreGeometry(const PxConvexCoreGeometry& that)
		:
		PxGeometry(that),
		mCoreType(that.getCoreType()), mMargin(that.getMargin())
	{
		PxMemCopy(mCore, that.mCore, MAX_CORE_SIZE);
	}

	PX_INLINE PxConvexCoreGeometry& PxConvexCoreGeometry::operator = (const PxConvexCoreGeometry& that)
	{
		mType = that.mType;
		mCoreType = that.mCoreType;
		mMargin = that.mMargin;
		PxMemCopy(mCore, that.mCore, MAX_CORE_SIZE);
		return *this;
	}

	PX_INLINE PxConvexCore::Type PxConvexCoreGeometry::getCoreType() const
	{
		return mCoreType;
	}

	PX_INLINE const void* PxConvexCoreGeometry::getCoreData() const
	{
		return mCore;
	}

	template <typename Core>
	PX_INLINE const Core& PxConvexCoreGeometry::getCore() const
	{
		PX_ASSERT(Core::TYPE == mCoreType);
		return *reinterpret_cast<const Core*>(getCoreData());
	}

	PX_INLINE PxReal PxConvexCoreGeometry::getMargin() const
	{
		return mMargin;
	}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

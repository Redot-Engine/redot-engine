// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CUSTOM_GEOMETRY_EXT_H
#define PX_CUSTOM_GEOMETRY_EXT_H

#include "geometry/PxCustomGeometry.h"
#include "geometry/PxGjkQuery.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxGeometry;
class PxMassProperties;
class PxGeometryHolder;
struct PxContactPoint;

/**
\brief Pre-made custom geometry callbacks implementations.
*/
class PxCustomGeometryExt
{
public:

	/// \cond PRIVATE
	struct BaseConvexCallbacks : PxCustomGeometry::Callbacks, PxGjkQuery::Support
	{
		BaseConvexCallbacks(float _margin) : margin(_margin) {}

		// override PxCustomGeometry::Callbacks
		virtual PxBounds3 getLocalBounds(const PxGeometry& geometry) const;
		virtual bool generateContacts(const PxGeometry& geom0, const PxGeometry& geom1, const PxTransform& pose0, const PxTransform& pose1,
			const PxReal contactDistance, const PxReal meshContactMargin, const PxReal toleranceLength,
			PxContactBuffer& contactBuffer) const;
		virtual PxU32 raycast(const PxVec3& origin, const PxVec3& unitDir, const PxGeometry& geom, const PxTransform& pose,
			PxReal maxDist, PxHitFlags hitFlags, PxU32 maxHits, PxGeomRaycastHit* rayHits, PxU32 stride, PxRaycastThreadContext*) const;
		virtual bool overlap(const PxGeometry& geom0, const PxTransform& pose0, const PxGeometry& geom1, const PxTransform& pose1, PxOverlapThreadContext*) const;
		virtual bool sweep(const PxVec3& unitDir, const PxReal maxDist,
			const PxGeometry& geom0, const PxTransform& pose0, const PxGeometry& geom1, const PxTransform& pose1,
			PxGeomSweepHit& sweepHit, PxHitFlags hitFlags, const PxReal inflation, PxSweepThreadContext*) const;
		virtual bool usePersistentContactManifold(const PxGeometry& geometry, PxReal& breakingThreshold) const;

		// override PxGjkQuery::Support
		virtual PxReal getMargin() const { return margin; }
		// set margin
		void setMargin(float m);

	protected:

		// Shape margin
		float margin;

		// Substitute geometry
		virtual bool useSubstituteGeometry(PxGeometryHolder& geom, PxTransform& preTransform, const PxContactPoint& p, const PxTransform& pose0) const = 0;
	};
	/// \endcond

	/**
	\brief Cylinder geometry callbacks
	*/
	struct CylinderCallbacks : BaseConvexCallbacks
	{
		/**
		\brief Construct cylinder geometry callbacks object

		\param[in] height The cylinder height.
		\param[in] radius The cylinder radius.
		\param[in] axis The cylinder axis (0 - X, 1 - Y, 2 - Z).
		\param[in] margin The cylinder margin.
		*/
		CylinderCallbacks(float height, float radius, int axis = 0, float margin = 0);

		/// \brief Set cylinder height
		/// \param[in] h The cylinder height
		void setHeight(float h);
		/// \brief Get cylinder height
		/// \return	The cylinder height
		float getHeight() const { return height; }

		/// \brief Set cylinder radius
		/// \param[in] r The cylinder radius.
		void setRadius(float r);
		/// \brief Get cylinder radius
		/// \return	The cylinder radius
		float getRadius() const { return radius; }

		/// \brief Set cylinder axis
		/// \param[in] a The cylinder axis (0 - X, 1 - Y, 2 - Z).
		void setAxis(int a);
		/// \brief Get cylinder axis
		/// \return	The cylinder axis
		int getAxis() const { return axis; }

		/// \cond PRIVATE
		// override PxCustomGeometry::Callbacks
		DECLARE_CUSTOM_GEOMETRY_TYPE
		virtual void visualize(const PxGeometry&, PxRenderOutput&, const PxTransform&, const PxBounds3&) const;
		virtual void computeMassProperties(const PxGeometry& geometry, PxMassProperties& massProperties) const;

		// override PxGjkQuery::Support
		virtual PxVec3 supportLocal(const PxVec3& dir) const;

		virtual PxBounds3 getLocalBounds(const PxGeometry& geometry) const;

	protected:

		// Cylinder height
		float height;
		// Cylinder radius
		float radius;
		// Cylinder axis
		int axis;

		// Substitute geometry
		virtual bool useSubstituteGeometry(PxGeometryHolder& geom, PxTransform& preTransform, const PxContactPoint& p, const PxTransform& pose0) const;

		// Radius at height
		float getRadiusAtHeight(float height) const;
		/// \endcond
	};

	/**
	\brief Cone geometry callbacks
	*/
	struct ConeCallbacks : BaseConvexCallbacks
	{
		/**
		\brief Construct cone geometry callbacks object

		\param[in] height The cylinder height.
		\param[in] radius The cylinder radius.
		\param[in] axis The cylinder axis (0 - X, 1 - Y, 2 - Z).
		\param[in] margin The cylinder margin.
		*/
		ConeCallbacks(float height, float radius, int axis = 0, float margin = 0);

		/// \brief Set cone height
		/// \param[in] h The cone height
		void setHeight(float h);
		/// \brief Get cone height
		/// \return	The cone height
		float getHeight() const { return height; }

		/// \brief Set cone radius
		/// \param[in] r The cone radius
		void setRadius(float r);
		/// \brief Get cone radius
		/// \return	The cone radius
		float getRadius() const { return radius; }

		/// \brief Set cone axis
		/// \param[in] a The cone axis
		void setAxis(int a);
		/// \brief Get cone axis
		/// \return	The cone axis
		int getAxis() const { return axis; }

		/// \cond PRIVATE
		// override PxCustomGeometry::Callbacks
		DECLARE_CUSTOM_GEOMETRY_TYPE
		virtual void visualize(const PxGeometry&, PxRenderOutput&, const PxTransform&, const PxBounds3&) const;
		virtual void computeMassProperties(const PxGeometry& geometry, PxMassProperties& massProperties) const;

		// override PxGjkQuery::Support
		virtual PxVec3 supportLocal(const PxVec3& dir) const;

	protected:

		// Cone height
		float height;
		// Cone radius
		float radius;
		// Cone axis
		int axis;

		// Substitute geometry
		virtual bool useSubstituteGeometry(PxGeometryHolder& geom, PxTransform& preTransform, const PxContactPoint& p, const PxTransform& pose0) const;

		// Radius at height
		float getRadiusAtHeight(float height) const;
		/// \endcond
	};
};

/// \cond PRIVATE
// OmniPVD friendly aliases
typedef PxCustomGeometryExt::BaseConvexCallbacks PxCustomGeometryExtBaseConvexCallbacks;
typedef PxCustomGeometryExt::CylinderCallbacks PxCustomGeometryExtCylinderCallbacks;
typedef PxCustomGeometryExt::ConeCallbacks PxCustomGeometryExtConeCallbacks;
/// \endcond

#if !PX_DOXYGEN
}
#endif

#endif

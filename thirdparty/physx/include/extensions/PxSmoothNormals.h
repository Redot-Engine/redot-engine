// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SMOOTH_NORMALS_H
#define PX_SMOOTH_NORMALS_H

#include "foundation/PxVec3.h"
#include "common/PxPhysXCommonConfig.h"

/**
\brief Builds smooth vertex normals over a mesh.

- "smooth" because smoothing groups are not supported here
- takes angles into account for correct cube normals computation

To use 32bit indices pass a pointer in dFaces and set wFaces to zero. Alternatively pass a pointer to 
wFaces and set dFaces to zero.

\param[in] nbTris Number of triangles
\param[in] nbVerts Number of vertices
\param[in] verts Array of vertices
\param[in] dFaces Array of dword triangle indices, or null
\param[in] wFaces Array of word triangle indices, or null
\param[out] normals Array of computed normals (assumes nbVerts vectors)
\param[in] flip Flips the normals or not
\return True on success.
*/
PX_C_EXPORT bool PX_CALL_CONV PxBuildSmoothNormals(physx::PxU32 nbTris, physx::PxU32 nbVerts, const physx::PxVec3* verts,
												   const physx::PxU32* dFaces, const physx::PxU16* wFaces, physx::PxVec3* normals, bool flip);

#endif

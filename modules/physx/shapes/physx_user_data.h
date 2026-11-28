/**************************************************************************/
/*  physx_user_data.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

/**
 * @file physx_user_data.h
 * @brief userData bridge structures between PhysX and Godot.
 *
 * PhysX provides a raw `void* userData` field on PxActor and PxShape. This
 * module uses it to store a back-pointer to the Godot-side wrapper object so
 * that query results and simulation callbacks can recover the Godot RID and
 * ObjectID without maintaining a separate lookup table.
 *
 * Convention: every PxActor/PxShape created by this module MUST have its
 * userData set to the appropriate struct below. Consumers MUST cast to the
 * documented type — never to PhysXObject3D* or PhysXBody3D* directly.
 *
 * Note: PxShape::userData points at the shared PhysXShape3D* shape_blueprint
 * (the same pointer returned by PhysXShape3D::create_shape's caller).
 * The shape_index for each PxShape is tracked in the owning body/area's
 * shapes[] array, not stored in the shape itself.
 */

#ifndef PHYSX_USER_DATA_H
#define PHYSX_USER_DATA_H

#include "core/object/object.h"
#include "core/templates/rid.h"

// Intentionally a LEAF header: only RID/ObjectID/Px-POD definitions, so TUs
// that must not see the shapes/ headers (e.g. the vehicle nodes, which use
// unqualified Px names relying on PxPhysicsAPI.h's `using namespace physx`
// and would collide with physx_shape_3d.h's global forward declarations) can
// attach a PhysXActorUserData without the rest of the module. Consumers that
// need physx_resolve_shape_index() define it where PhysXShapedObject3D is
// available (physx_direct_space_state_3d.cpp).

class PhysXObject3D;

/**
 * @brief Attached to PxActor::userData.
 *
 * This is the single source of truth that bridges a PhysX actor back to
 * Godot-side data during queries (raycast, overlap, sweep) and simulation
 * callbacks (onContact, onTrigger).
 *
 * bounce/friction carry the Godot-side SIGNED values (negative = absorbent /
 * rough) so the contact-modify callback — which runs on a worker thread and
 * must not touch Godot objects — can implement Godot's material combiner. They
 * are written from the main thread before simulate() (in on_pre_step /
 * _apply_params_to_actor) and only read during simulate(), so plain floats are
 * safe under the synchronous step model.
 */
struct PhysXActorUserData {
	RID rid; ///< Godot RID of the owning body/area.
	ObjectID object_id; ///< Godot ObjectID of the scene-tree node.
	PhysXObject3D *object = nullptr; ///< Back-pointer to the wrapper (for filter/callback use).
	float bounce = 0.0f; ///< Signed bounce (negative = absorbent). Read by contact-modify.
	float friction = 1.0f; ///< Signed friction (negative = rough). Read by contact-modify.
};

#endif // PHYSX_USER_DATA_H

/**************************************************************************/
/*  physx_contact_modify_callback.h                                       */
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
 * @file physx_contact_modify_callback.h
 * @brief PxContactModifyCallback implementing Godot's material combiner.
 *
 * Godot's engine-wide material combine contract (shared by godot_physics_3d and
 * jolt_physics) is:
 *   combined_bounce    = CLAMP(bounceA + bounceB, 0, 1)
 *   combined_friction  = abs(min(frictionA, frictionB))
 *
 * The negative-sign trick (PhysicsMaterial::computed_bounce returns -bounce when
 * "absorbent", computed_friction returns -friction when "rough") is how Godot
 * signals "drive the combined value toward zero / toward the minimum": a
 * negative bounce cancels the other side's bounce, a negative friction is
 * equivalent to its absolute value under min().
 *
 * PhysX 5 cannot express "sum, clamped" with its built-in PxCombineMode enum
 * (eAVERAGE/eMIN/eMULTIPLY/eMAX), AND it misinterprets a negative restitution
 * as a compliant (spring-damper) contact — which is why an absorbent body
 * falls through the floor. The contact-modify callback is PhysX's sanctioned
 * mechanism for custom per-pair material combining: it runs after narrowphase
 * computes contacts but before the solver, letting us overwrite the per-contact
 * restitution/friction with the Godot-combined values.
 *
 * The body's SIGNED bounce/friction are read from the PxActor userData (which
 * bridges back to PhysXBody3D). Bodies clamp to [0,1] when writing to their
 * PxMaterial so no negative restitution ever reaches PhysX's compliant path.
 */

#ifndef PHYSX_CONTACT_MODIFY_CALLBACK_H
#define PHYSX_CONTACT_MODIFY_CALLBACK_H

#include "PxContactModifyCallback.h"

class PhysXContactModifyCallback : public physx::PxContactModifyCallback {
public:
	virtual void onContactModify(physx::PxContactModifyPair *p_pairs, physx::PxU32 p_count) override;
};

#endif // PHYSX_CONTACT_MODIFY_CALLBACK_H

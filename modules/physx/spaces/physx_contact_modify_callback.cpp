/**************************************************************************/
/*  physx_contact_modify_callback.cpp                                     */
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

/**
 * @file physx_contact_modify_callback.cpp
 * @brief Implements Godot's material combiner via PhysX contact modification.
 */

#include "physx_contact_modify_callback.h"
#include "../shapes/physx_user_data.h"

#include "core/math/math_funcs.h"

// physx_user_data.h is a leaf header (no Px includes); the PxRigidActor API
// (the userData access below) comes from here.
#include <PxPhysicsAPI.h>

// Reads the signed bounce/friction from each actor's userData and applies
// Godot's combine contract to every contact point in the pair:
//   combined_bounce   = CLAMP(bounceA + bounceB, 0, 1)
//   combined_friction = abs(min(frictionA, frictionB))
void PhysXContactModifyCallback::onContactModify(physx::PxContactModifyPair *p_pairs, physx::PxU32 p_count) {
	for (physx::PxU32 p = 0; p < p_count; p++) {
		physx::PxContactModifyPair &pair = p_pairs[p];

		// PxContactModifyPair::actor[] can reference deleted actors in an
		// async pipeline, but this engine steps synchronously (PxScene::simulate
		// blocks the main thread, and actors are only released from it between
		// steps), so the actors are guaranteed alive for the whole solve. The
		// null checks below guard the "no userData attached" case, not deletion.
		// Recover each side's signed bounce/friction from the actor userData.
		float bounce[2] = { 0.0f, 0.0f };
		float friction[2] = { 1.0f, 1.0f };
		for (int s = 0; s < 2; s++) {
			if (pair.actor[s] && pair.actor[s]->userData) {
				const auto *ud = static_cast<const PhysXActorUserData *>(pair.actor[s]->userData);
				bounce[s] = ud->bounce;
				friction[s] = ud->friction;
			}
		}

		// Godot's engine-wide material combiner (matches godot_physics_3d and
		// jolt_physics): bounce is summed then clamped (a negative/absorbent
		// value cancels the other side); friction is the minimum then absolute
		// (a negative/rough value behaves like its magnitude under min()).
		const float combined_bounce = CLAMP(bounce[0] + bounce[1], 0.0f, 1.0f);
		const float combined_friction = Math::abs(MIN(friction[0], friction[1]));

		physx::PxContactSet &contacts = pair.contacts;
		const physx::PxU32 nb = contacts.size();
		for (physx::PxU32 i = 0; i < nb; i++) {
			contacts.setRestitution(i, combined_bounce);
			contacts.setStaticFriction(i, combined_friction);
			contacts.setDynamicFriction(i, combined_friction);
		}
	}
}

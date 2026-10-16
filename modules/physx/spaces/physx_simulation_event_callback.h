/**************************************************************************/
/*  physx_simulation_event_callback.h                                     */
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
 * @file physx_simulation_event_callback.h
 * @brief PxSimulationEventCallback implementation for one PhysX scene.
 *
 * Translates PhysX simulation events into Godot-side data:
 *   - onContact: populates per-body contact buffers (PhysXBody3D::contacts),
 *     read by PhysXDirectBodyState3D's contact getters.
 *   - onTrigger: maintains area overlap state and defers Area3D monitor
 *     callbacks to flush_queries() (AREA_BODY_ADDED / AREA_BODY_REMOVED).
 *
 * The callback resolves actors back to PhysXBody3D / PhysXArea3D via the
 * PhysXActorUserData attached to each PxActor's userData field.
 */

#ifndef PHYSX_SIMULATION_EVENT_CALLBACK_H
#define PHYSX_SIMULATION_EVENT_CALLBACK_H

#include "PxSimulationEventCallback.h"

class PhysXSpace3D;

// PxSimulationEventCallback implementation for one PhysX scene. Translates
// PhysX contact and trigger events into Godot-side data:
//   - onContact populates per-body contact buffers (read by DirectBodyState).
//   - onTrigger dispatches Area3D monitor callbacks.
//
// The callback owns no state of its own; it resolves actors back to
// PhysXBody3D / PhysXArea3D via the userData attached to each PxActor.
class PhysXSimulationEventCallback : public physx::PxSimulationEventCallback {
public:
	explicit PhysXSimulationEventCallback(PhysXSpace3D *p_space) :
			space(p_space) {}

	virtual void onConstraintBreak(physx::PxConstraintInfo *constraints, physx::PxU32 count) override;
	virtual void onWake(physx::PxActor **actors, physx::PxU32 count) override;
	virtual void onSleep(physx::PxActor **actors, physx::PxU32 count) override;
	virtual void onContact(const physx::PxContactPairHeader &pairHeader, const physx::PxContactPair *pairs, physx::PxU32 nbPairs) override;
	virtual void onTrigger(physx::PxTriggerPair *pairs, physx::PxU32 count) override;
	virtual void onAdvance(const physx::PxRigidBody *const *bodyBuffer, const physx::PxTransform *poseBuffer, physx::PxU32 count) override;

private:
	PhysXSpace3D *space = nullptr;
};
#endif // PHYSX_SIMULATION_EVENT_CALLBACK_H

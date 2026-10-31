/**************************************************************************/
/*  physx_pair_filter_callback.h                                          */
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
 * @file physx_pair_filter_callback.h
 * @brief PxSimulationFilterCallback that enforces collision exceptions.
 *
 * The simulation filter shader cannot consult Godot objects (it runs stateless
 * on the sim thread), so collision exceptions are enforced here — in the
 * post-shader filter callback, which has access to both actors' userData and
 * can look up the wrapper objects to check their collision_exceptions sets.
 */

#ifndef PHYSX_PAIR_FILTER_CALLBACK_H
#define PHYSX_PAIR_FILTER_CALLBACK_H

#include <PxPhysicsAPI.h>

class PhysXPairFilterCallback : public physx::PxSimulationFilterCallback {
public:
	physx::PxFilterFlags pairFound(
			physx::PxU64 pairID,
			physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0,
			const physx::PxActor *a0, const physx::PxShape *s0,
			physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
			const physx::PxActor *a1, const physx::PxShape *s1,
			physx::PxPairFlags &pairFlags) override;

	virtual void pairLost(
			physx::PxU64 pairID,
			physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0,
			physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
			bool objectRemoved) override {
		// No-op — we don't need to track pair lifecycles for exceptions.
	}

	virtual bool statusChange(
			physx::PxU64 &pairID,
			physx::PxPairFlags &pairFlags,
			physx::PxFilterFlags &filterFlags) override {
		return false; // No dynamic status changes needed.
	}
};

#endif // PHYSX_PAIR_FILTER_CALLBACK_H

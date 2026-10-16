/**************************************************************************/
/*  physx_project_settings.h                                              */
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
 * @file physx_project_settings.h
 * @brief Module settings under Project Settings → Physics → PhysX 3D.
 *
 * Settings are registered at MODULE_INITIALIZATION_LEVEL_SERVERS
 * (register_types.cpp) and read once in PhysXServer3D::init(), before any
 * PxScene exists.
 */

#ifndef PHYSX_PROJECT_SETTINGS_H
#define PHYSX_PROJECT_SETTINGS_H

class PhysXProjectSettings {
public:
	// physics/physx_3d/simulation/enhanced_determinism
	//
	// Sets PxSceneFlag::eENABLE_ENHANCED_DETERMINISM: the CPU simulation then
	// produces identical results across runs on the same binary/platform,
	// independent of the CPU worker count and API call order (it is NOT
	// cross-platform deterministic). Has a performance cost.
	//
	// GPU dynamics is never deterministic, so enabling this forces the CPU
	// solver even when a CUDA device is available.
	inline static bool enhanced_determinism = false;

	// physics/physx_3d/simulation/solver_type
	//
	// 0 = PGS (Projected Gauss-Seidel) -- the DEFAULT. PhysX's classic solver
	// and the stable, well-understood choice for this backend.
	// 1 = TGS (Temporal Gauss-Seidel) -- EXPERIMENTAL in this module. It can
	// hold joint chains steadier under sustained external forces (wind,
	// thrusters), but can be looser on joints in large mixed rigid-body
	// scenes and has seen far less validation here. Opt in per project if
	// you want to play with it; default remains PGS.
	inline static int solver_type = 0;

	// physics/physx_3d/simulation/allow_sleep
	//
	// When false, no rigid body ever sleeps (equivalent to RigidBody3D.can_sleep
	// off on every body). Useful for debugging and for setups that need every
	// body integrated every step.
	inline static bool allow_sleep = true;

	// physics/physx_3d/simulation/stabilization
	//
	// PxSceneFlag::eENABLE_STABILIZATION. Damps low-mass stacked/piled bodies
	// toward rest so they settle and can sleep instead of jittering; the same
	// mechanism Unity and Unreal call "stabilization".
	//
	// Default OFF in this module: the flag
	// applies an extra damping force to low-kinetic-energy bodies, which on a
	// sphere sliding down a ramp suppresses the angular acceleration from
	// contact friction — the sphere slides instead of rolling. Stacks are
	// already handled by the solver iteration count, so only enable this when
	// pile jitter is the greater evil. CPU-path only; silently ignored on the
	// GPU dynamics path (PhysX does not support it there).
	inline static bool stabilization = false;

	// physics/physx_3d/simulation/cpu_worker_threads
	//
	// 0 = auto (most of the machine for the CPU solver, a small pool for the
	// GPU path). A fixed value is useful for reproducible profiling. The
	// dispatcher is shared by every PxScene the server creates.
	inline static int cpu_worker_threads = 0;

	// physics/physx_3d/simulation/max_depenetration_velocity
	//
	// Caps how fast a depenetrating body may pop out of an overlap (m/s) —
	// PxRigidDynamic::setMaxDepenetrationVelocity on every rigid body. Without
	// the cap, bodies spawned interpenetrating (or grown into each other) can
	// eject violently. 0 disables the cap (PhysX's unbounded default is 3.4e38).
	inline static float max_depenetration_velocity = 4.0f;

	// physics/physx_3d/simulation/async_step
	//
	// Optional async stepping (default OFF). When on, PhysXSpace3D::step()
	// only kicks PxScene::simulate(); the fetchResults + GPU readbacks +
	// post-step hooks run in sync(), which the engine calls at the start of
	// the NEXT tick (main.cpp order: sync -> flush_queries -> scripts ->
	// end_sync -> step). The solve therefore overlaps the rest of the frame.
	// Read live per server step, so it can be toggled at runtime; flipping it
	// mid-flight is safe (step() fetches defensively).
	//
	// Caveats when on:
	//  - A scene query issued while a solve is in flight forces an immediate
	//    fetch (PhysX forbids queries against a running scene).
	//  - Server calls that land in the in-flight window (deferred calls,
	//    node destruction) have their actor add/remove/release queued until
	//    the fetch; the wrappers must route releases through the space.
	//  - Body/actor data writes from scripts still happen after sync() (the
	//    engine order keeps scripts out of the in-flight window).
	inline static bool async_step = false;

	// physics/physx_3d/soft_body/mode
	//
	// Resolution for stock SoftBody3D bodies: 0 = Auto (GPU PxDeformableVolume
	// when the mesh tetrahedralizes and CUDA is available, else the CPU XPBD
	// solver — decided per body), 1 = CPU, 2 = GPU. A per-body override is
	// available via the node metadata "physx_soft_mode" = "cpu" / "gpu".
	// enhanced_determinism forces every soft body to CPU (no CUDA context).
	inline static int soft_body_mode = 0;

	// physics/physx_3d/flow/device_api
	//
	// GPU backend for the NVIDIA Flow runtime (PhysXFlowSimulation3D nodes).
	// Flow owns its own GPU device/queue (its SDK is deliberately not a PhysX
	// subsystem), independent of the renderer's device. 0 = Auto (Vulkan,
	// falling back to the CPU context when no Vulkan device can be created),
	// 1 = Vulkan only, 2 = CPU only (slow, for machines without usable GPU
	// drivers). Read when the first Flow simulation initializes.
	inline static int flow_device_api = 0;

	// physics/physx_3d/flow/device_index
	//
	// Which GPU to run Flow on when several exist (0 = first). Ignored by the
	// CPU backend.
	inline static int flow_device_index = 0;

	// physics/physx_3d/flow/device_validation
	//
	// Enables Flow SDK validation layers in debug/editor builds (Vulkan
	// backend). Diagnostic aid only; costs performance.
	inline static bool flow_device_validation = false;

	// physics/physx_3d/flow/verbose_logs
	//
	// Passes the Flow SDK's info-level log (per-allocation traces etc.)
	// through to Godot's console. Errors/warnings always print.
	inline static bool flow_verbose_logs = false;

	static void register_settings();
	static void read_settings();
};

#endif // PHYSX_PROJECT_SETTINGS_H

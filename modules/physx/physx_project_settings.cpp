/**************************************************************************/
/*  physx_project_settings.cpp                                            */
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
 * @file physx_project_settings.cpp
 * @brief Registers and reads the physics/physx_3d project settings (prefix physics/physx_3d/).
 */

#include "physx_project_settings.h"

#include "core/config/project_settings.h"

void PhysXProjectSettings::register_settings() {
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/physx_3d/simulation/enhanced_determinism"), false);
	// Off: no CUDA context at all -- the CPU solver with its full worker pool,
	// the same path as a machine without CUDA. Distinct from
	// enhanced_determinism, which keeps the context for non-simulation CUDA
	// work (soft-body CUDA paths) and only forces the CPU simulation.
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/physx_3d/simulation/gpu_dynamics"), true);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/physx_3d/simulation/solver_type", PROPERTY_HINT_ENUM, "PGS,TGS"), 0);
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/physx_3d/simulation/allow_sleep"), true);
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/physx_3d/simulation/stabilization"), false);
	// Optional async stepping: step() only kicks PxScene::simulate() and the
	// fetch happens in sync() at the START of the next tick, so the solve
	// overlaps the rest of the frame instead of blocking it. Default off —
	// see the header for the behavioral caveats. Read live (per server step),
	// so it can be toggled at runtime.
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/physx_3d/simulation/async_step"), false);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/physx_3d/simulation/cpu_worker_threads", PROPERTY_HINT_RANGE, U"0,32,1"), 0);
	// Depenetration speed cap: bodies spawned overlapping (or pushed into each
	// other) pop apart at a bounded rate instead of ejecting violently.
	// 0 = off, which is the default for Godot parity: godot_physics ejects
	// embedded bodies positionally with no cap (measured at the same recovery
	// rate as PhysX's own uncapped default). A cap > 0 is a comfort option
	// that deviates from that contract (a 4 m/s cap cleared a deep embed 7x
	// slower than Godot in the parity measurement).
	// Applied per rigid body in _apply_surface_params_to_actor.
	GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/physx_3d/simulation/max_depenetration_velocity", PROPERTY_HINT_RANGE, "0,100,0.1,suffix:m/s"), 0.0);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/physx_3d/soft_body/mode", PROPERTY_HINT_ENUM, "Auto,CPU,GPU"), 0);
	// PVD used to connect unconditionally on every debug build.
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/physx_3d/debug/pvd"), false);
	// NVIDIA Flow runtime device selection (see flow/flow_runtime.cpp).
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/physx_3d/flow/device_api", PROPERTY_HINT_ENUM, "Auto,Vulkan,CPU"), 0);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "physics/physx_3d/flow/device_index", PROPERTY_HINT_RANGE, U"0,7,1"), 0);
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/physx_3d/flow/device_validation"), false);
	GLOBAL_DEF(PropertyInfo(Variant::BOOL, "physics/physx_3d/flow/verbose_logs"), false);
}

void PhysXProjectSettings::read_settings() {
	enhanced_determinism = GLOBAL_GET("physics/physx_3d/simulation/enhanced_determinism");
	gpu_dynamics = GLOBAL_GET("physics/physx_3d/simulation/gpu_dynamics");
	solver_type = GLOBAL_GET("physics/physx_3d/simulation/solver_type");
	allow_sleep = GLOBAL_GET("physics/physx_3d/simulation/allow_sleep");
	stabilization = GLOBAL_GET("physics/physx_3d/simulation/stabilization");
	cpu_worker_threads = GLOBAL_GET("physics/physx_3d/simulation/cpu_worker_threads");
	max_depenetration_velocity = GLOBAL_GET("physics/physx_3d/simulation/max_depenetration_velocity");
	soft_body_mode = GLOBAL_GET("physics/physx_3d/soft_body/mode");
	flow_device_api = GLOBAL_GET("physics/physx_3d/flow/device_api");
	flow_device_index = GLOBAL_GET("physics/physx_3d/flow/device_index");
	flow_device_validation = GLOBAL_GET("physics/physx_3d/flow/device_validation");
}

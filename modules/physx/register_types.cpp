/**************************************************************************/
/*  register_types.cpp                                                    */
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
 * @file register_types.cpp
 * @brief Module entry point — registers PhysX as a selectable 3D physics backend.
 *
 * At MODULE_INITIALIZATION_LEVEL_SERVERS, this file registers the PhysX server
 * factory with PhysicsServer3DManager. The user selects it in Project Settings
 * under "Physics > 3D > Physics Engine" (the dropdown is populated automatically
 * by PhysicsServer3DManager::on_servers_changed()).
 */

#include "register_types.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"

#include "cloth/physx_skinned_cloth_3d.h"
#include "nodes/physx_chunk_emitter_3d.h"
#include "nodes/physx_cloth_3d.h"
#include "nodes/physx_gas_3d.h"
#include "nodes/physx_gas_emitter_3d.h"
#include "nodes/physx_granular_3d.h"
#include "nodes/physx_particle_fluid_3d.h"
#include "objects/physx_direct_body_state_3d.h"
#include "physx_project_settings.h"
#include "physx_server.h"
#include "servers/physics_3d/physics_server_3d_wrap_mt.h"
#include "water/physx_boat_3d.h"
#include "water/physx_buoyancy_3d.h"
#include "water/physx_water_spray_3d.h"
#include "water/physx_water_surface_3d.h"
#include "water/physx_water_wake_3d.h"
#include "water/water_ripple_probe.h"

// Node-level vehicle stack (PxVehicle2 compositions behind plain Node3D
// authoring): PhysXVehicle3D + PhysXVehicleWheel3D, PhysXMotorcycle3D,
// PhysXTank3D, and the headless *Probe bridges used by the GDScript tests.
// Coexists with the server-RID vehicle API (physx_vehicle_server.h).
#include "vehicles/physx_motorcycle_3d.h"
#include "vehicles/physx_motorcycle_probe.h"
#include "vehicles/physx_tank_3d.h"
#include "vehicles/physx_tank_probe.h"
#include "vehicles/physx_vehicle_3d.h"
#include "vehicles/physx_vehicle_probe.h"
#include "vehicles/physx_vehicle_wheel_3d.h"

#ifdef GODOT_PHYSX_BLAST
#include "blast/physx_blast_asset.h"
#include "blast/physx_blast_authoring.h"
#include "blast/physx_destructible_3d.h"
#endif

#ifdef GODOT_PHYSX_FLOW
#include "flow/flow_runtime.h"
#include "flow/physx_flow_collider_3d.h"
#include "flow/physx_flow_emitter_3d.h"
#include "flow/physx_flow_render_effect.h"
#include "flow/physx_flow_simulation_3d.h"
#if defined(GODOT_PHYSX_BLAST)
#include "flow/physx_flow_blast_bridge_3d.h"
#endif
#endif

#ifdef TOOLS_ENABLED
#include "editor/physx_cloth_paint_plugin.h"
#include "editor/physx_editor_plugin.h"
#ifdef GODOT_PHYSX_FLOW
#include "editor/physx_flow_editor_plugin.h"
#endif
#endif

/**
 * @brief Factory function that creates the PhysX physics server.
 *
 * The returned PhysicsServer3DWrapMT is owned by PhysicsServer3DManager after
 * registration; do not memdelete it here.
 */
static PhysicsServer3D *create_physx_server() {
#ifdef THREADS_ENABLED
	bool run_on_separate_thread = GLOBAL_GET("physics/3d/run_on_separate_thread");
#else
	bool run_on_separate_thread = false;
#endif

	// The inner PhysXServer3D is created with a default constructor; threading
	// is handled by the wrapper, not by the server itself.
	PhysXServer3D *physics_server = memnew(PhysXServer3D);

	return memnew(PhysicsServer3DWrapMT(physics_server, run_on_separate_thread));
}

void initialize_physx_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		// Register the physics/physx_3d settings before the server factory
		// runs, so init() reads the user's values (not the defaults).
		PhysXProjectSettings::register_settings();

		// Class registration for Godot (and GDScript/Doctool)
		ClassDB::register_class<PhysXServer3D>();
		ClassDB::register_class<PhysXDirectBodyState3D>();

		// "PhysX" registration for appearance in the Project Settings inside physics engine dropdown.
		PhysicsServer3DManager::get_singleton()->register_server("PhysX", callable_mp_static(&create_physx_server));
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Scene-level node classes provided by the module.
		ClassDB::register_class<PhysXChunkEmitter3D>();
		ClassDB::register_class<PhysXCloth3D>();
		ClassDB::register_class<PhysXSkinnedCloth3D>();
		ClassDB::register_class<PhysXWaterSurface3D>();
		// Floats a parent RigidBody3D on a PhysXWaterSurface3D (Archimedes
		// per sample point); PhysXBoat3D drives the same body's propeller,
		// rudder and hull drag; PhysXWaterWake3D simulates a moving wake
		// grid drawn by the surface; PhysXWaterSpray3D drives GPUParticles3D
		// spray emitters from the hull's motion.
		ClassDB::register_class<PhysXBuoyancy3D>();
		ClassDB::register_class<PhysXBoat3D>();
		ClassDB::register_class<PhysXWaterWake3D>();
		ClassDB::register_class<PhysXWaterSpray3D>();
		ClassDB::register_class<WaterRippleProbe>();
		ClassDB::register_class<PhysXGranular3D>();
		ClassDB::register_class<PhysXParticleFluid3D>();
		ClassDB::register_class<PhysXGas3D>();
		ClassDB::register_class<PhysXGasEmitter3D>();
		// Node-level vehicle stack (see the include block above). The *Probe
		// classes are headless test bridges, kept registered like upstream.
		ClassDB::register_class<PhysXVehicle3D>();
		ClassDB::register_class<PhysXVehicleWheel3D>();
		ClassDB::register_class<PhysXMotorcycle3D>();
		ClassDB::register_class<PhysXTank3D>();
		ClassDB::register_class<PhysXVehicleProbe>();
		ClassDB::register_class<PhysXMotorcycleProbe>();
		ClassDB::register_class<PhysXTankProbe>();
#ifdef GODOT_PHYSX_BLAST
		ClassDB::register_class<PhysXDestructible3D>();
		ClassDB::register_class<PhysXBlastAsset>();
		ClassDB::register_class<PhysXBlastAuthoring>();
#endif
#ifdef GODOT_PHYSX_FLOW
		// NVIDIA Flow scene nodes (fluid/fire/smoke). Flow is architecturally
		// a sibling of PhysX/Blast (own SDK, own GPU runtime -- see
		// flow/docs/IMPLEMENTATION_NOTES.md); it is registered here only
		// because this module is the packaging unit.
		ClassDB::register_class<PhysXFlowSimulation3D>();
		ClassDB::register_class<PhysXFlowEmitter3D>();
		ClassDB::register_class<PhysXFlowCollider3D>();
#if defined(GODOT_PHYSX_BLAST)
		// Optional destruction-event adapter (Blast fracture -> Flow dust).
		ClassDB::register_class<PhysXFlowBlastBridge3D>();
#endif
#endif
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// Viewport gizmos for the module's scene nodes, plus the Blast
		// fracture dialog and the skinned-cloth max-distance paint tool.
		EditorPlugins::add_by_type<PhysXEditorPlugin>();
		EditorPlugins::add_by_type<PhysXClothPaintPlugin>();
#ifdef GODOT_PHYSX_FLOW
		EditorPlugins::add_by_type<PhysXFlowEditorPlugin>();
#endif
	}
#endif
}

void uninitialize_physx_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SERVERS) {
		return;
	}
	// Inner PhysXServer3D lifetime is managed by PhysicsServer3DWrapMT,
	// which the PhysicsServer3DManager owns and tears down at shutdown.
#ifdef GODOT_PHYSX_FLOW
	// All scene nodes (and thus Flow simulations) are freed before SERVERS
	// modules uninitialize; if anything still holds the shared Flow GPU
	// runtime, tear it down anyway with a diagnostic rather than leak the
	// device across an engine restart.
	if (FlowRuntime::get_singleton() != nullptr) {
		FlowRuntime::get_singleton()->release_for_shutdown();
	}
	// The shared render effect holds RenderingServer RIDs (its compositor
	// effect + copy pipeline); release it while the RS can still free them.
	PhysXFlowRenderEffect::free_shared();
#endif
}

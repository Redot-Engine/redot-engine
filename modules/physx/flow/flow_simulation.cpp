/**************************************************************************/
/*  flow_simulation.cpp                                                   */
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

#include "flow_simulation.h"

#ifdef GODOT_PHYSX_FLOW

#include "../physx_server.h"
#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/templates/vector.h"
#include "servers/rendering/rendering_server.h"

#include <string.h>

// ---- Conversions ---------------------------------------------------------

// Flow matrices follow the USD/row-vector convention used by the SDK:
// world = mul(float4(local, 1), M) = local.x*M.x + local.y*M.y + local.z*M.z + M.w
// i.e. rows x/y/z are the basis axes and row w is the translation. (Verified
// empirically at runtime; see test/flow_readback_check.gd.)
NvFlowFloat4x4 FlowSimulation::_to_flow_matrix(const Transform3D &p_xform) {
	NvFlowFloat4x4 m;
	const Basis b = p_xform.basis;
	m.x = { b.get_column(0).x, b.get_column(0).y, b.get_column(0).z, 0.0f };
	m.y = { b.get_column(1).x, b.get_column(1).y, b.get_column(1).z, 0.0f };
	m.z = { b.get_column(2).x, b.get_column(2).y, b.get_column(2).z, 0.0f };
	m.w = { p_xform.origin.x, p_xform.origin.y, p_xform.origin.z, 1.0f };
	return m;
}

// Rigid-body point-velocity transform: worldVel(p_local) =
// angular x (R * p_local) + linear. As a Flow matrix: rows 0..2 are
// cross(omega, axis_i) and row 3 is the linear velocity.
NvFlowFloat4x4 FlowSimulation::_to_flow_velocity_matrix(const Transform3D &p_xform, const Vector3 &p_linear, const Vector3 &p_angular) {
	NvFlowFloat4x4 m;
	const Basis b = p_xform.basis;
	const Vector3 rx = p_angular.cross(b.get_column(0));
	const Vector3 ry = p_angular.cross(b.get_column(1));
	const Vector3 rz = p_angular.cross(b.get_column(2));
	m.x = { rx.x, rx.y, rx.z, 0.0f };
	m.y = { ry.x, ry.y, ry.z, 0.0f };
	m.z = { rz.x, rz.y, rz.z, 0.0f };
	m.w = { p_linear.x, p_linear.y, p_linear.z, 0.0f };
	return m;
}

// ---- Context (RenderingDevice or the SDK's own fallback device) -----------

NvFlowContextInterface *FlowSimulation::_context_iface() const {
	if (using_rd && rd_context != nullptr) {
		return FlowContextRD::get_interface();
	}
	return runtime != nullptr ? runtime->context_interface() : nullptr;
}

NvFlowContext *FlowSimulation::_context() const {
	if (using_rd && rd_context != nullptr) {
		return FlowContextRD::get_flow_context(rd_context);
	}
	return runtime != nullptr ? runtime->context() : nullptr;
}

void FlowSimulation::_flush_frame(uint64_t *r_frame_id) {
	if (using_rd && rd_context != nullptr) {
		// The RD context fences every frame it submits; completion is gated
		// on the context's own frame id (the one this flush is about to
		// schedule its fence under).
		if (r_frame_id != nullptr) {
			*r_frame_id = FlowContextRD::get_interface()->getCurrentFrame(FlowContextRD::get_flow_context(rd_context));
		}
		FlowContextRD::flush(rd_context);
		return;
	}
	if (runtime != nullptr) {
		runtime->flush(r_frame_id);
	} else if (r_frame_id != nullptr) {
		*r_frame_id = 0;
	}
}

uint64_t FlowSimulation::_last_frame_completed() const {
	if (using_rd && rd_context != nullptr) {
		return FlowContextRD::get_interface()->getLastFrameCompleted(FlowContextRD::get_flow_context(rd_context));
	}
	return runtime != nullptr ? runtime->get_last_frame_completed() : 0;
}

String FlowSimulation::get_backend_name() const {
	if (using_rd && rd_context != nullptr) {
		return "rd";
	}
	if (runtime != nullptr && runtime->is_available()) {
		return runtime->get_backend_name();
	}
	return String();
}

// ---- Lifecycle -----------------------------------------------------------

FlowSimulation::~FlowSimulation() {
	_destroy_grid();
	if (runtime != nullptr) {
		runtime->release();
		runtime = nullptr;
	}
}

void FlowSimulation::_destroy_grid() {
	// Main-thread teardown (node exit / destructor): the grid's RenderingDevice
	// resources are freed where the device lives. Render-thread callers (a
	// rebuild inside _rt_configure) run inline.
	const bool threaded = separate_render_thread && using_rd && Thread::get_caller_id() == Thread::get_main_id();
	if (threaded) {
		RenderingServer::get_singleton()->call_on_render_thread(Callable(memnew(GodotPhysXQueuedCall<DestroyFn>(this))));
		RenderingServer::get_singleton()->sync();
		return;
	}
	_destroy_grid_now();
}

void FlowSimulation::_rt_destroy_grid() {
	_destroy_grid_now();
}

void FlowSimulation::_destroy_grid_now() {
	{
		// Decoded frames of the dead grid must not survive a later rebuild
		// (poll_readback would otherwise deliver pre-destroy data).
		MutexLock lock(step_mutex);
		decoded_frames.clear();
	}
	// Readback buffers FIRST: they live in the context, and this function
	// destroys that context -- releasing them through it afterwards would
	// hand freed pointers to a dead Context (v1's context outlived them;
	// the RD path's does not).
	_release_readback_buffers();
	if (mapped_snapshot != nullptr && runtime != nullptr) {
		runtime->get_grid_params_interface()->unmapParamsDesc(grid_params, mapped_snapshot);
		mapped_snapshot = nullptr;
	}
	// The desc's storage went with grid_params: a rebuilt grid must not be
	// rendered through the stale descriptor (render() checks desc_active).
	desc_active = false;
	mapped_desc = {};
	if (grid_params != nullptr && runtime != nullptr) {
		runtime->get_grid_params_interface()->destroyGridParams(grid_params);
		grid_params = nullptr;
	}
	if (grid != nullptr && runtime != nullptr) {
		// Make sure no in-flight GPU work still references the grid before
		// destroying it (the grid's transient resources retire with frames;
		// on the RD path everything defers behind flush() instead).
		if (!using_rd) {
			runtime->wait_idle();
		}
		runtime->get_grid_interface()->destroyGrid(_context(), grid);
		runtime->grid_destroyed();
		grid = nullptr;
	}
	if (rd_context != nullptr) {
		FlowContextRD::destroy(rd_context);
		rd_context = nullptr;
	}
	using_rd = false;
}

void FlowSimulation::configure(const FlowSimGridConfig &p_config) {
	separate_render_thread = (int)GLOBAL_GET("rendering/driver/threads/thread_model") == (int)OS::RENDER_SEPARATE_THREAD;
	if (runtime == nullptr) {
		runtime = FlowRuntime::acquire();
	}
	const bool threaded = separate_render_thread && RenderingServer::get_singleton()->get_rendering_device() != nullptr;
	if (threaded) {
		grid_config = p_config;
		RenderingServer::get_singleton()->call_on_render_thread(Callable(memnew(GodotPhysXQueuedCall<CfgFn>(this))));
		// The node reads availability right after this; the grid must exist.
		RenderingServer::get_singleton()->sync();
		return;
	}
	_rt_configure();
}

void FlowSimulation::_rt_configure() {
	// FLOW-2: the availability check must run on EVERY entry, not only when
	// the runtime was first acquired -- a second configure() after a failed
	// init skipped the guard and invoked createGrid with a null context.
	if (!runtime->is_available()) {
		return;
	}

	// The grid runs on Godot's own RenderingDevice whenever one exists (the
	// gas never leaves VRAM; Flow's ray marcher composites it into the scene
	// color). Headless and Compatibility have no RenderingDevice: fall back
	// to the SDK's own Vulkan/CPU device -- the v1 path the headless test
	// suite still drives. A grid can't switch context families while alive,
	// so a device change rebuilds it.
	RenderingDevice *rd = RenderingServer::get_singleton()->get_rendering_device();
	const bool want_rd = rd != nullptr;
	if (grid != nullptr && using_rd != want_rd) {
		_destroy_grid();
	}

	NvFlowGridInterface *gi = runtime->get_grid_interface();
	if (gi->createGrid == nullptr) {
		return;
	}
	NvFlowGridDesc desc = {};
	desc.maxLocations = MAX(64u, grid_config.max_blocks);
	desc.maxLocationsIsosurface = 0u;

	if (grid != nullptr) {
		_destroy_grid();
	}
	if (want_rd) {
		rd_context = FlowContextRD::create(rd);
		grid = gi->createGrid(FlowContextRD::get_interface(), FlowContextRD::get_flow_context(rd_context),
				runtime->get_op_list(), runtime->get_ext_op_list(), &desc);
		using_rd = grid != nullptr;
	} else {
		if (!runtime->ensure_device()) {
			using_rd = false;
			return;
		}
		grid = gi->createGrid(runtime->context_interface(), runtime->context(),
				runtime->get_op_list(), runtime->get_ext_op_list(), &desc);
	}
	if (grid != nullptr) {
		runtime->grid_created();
		// Fresh grid -> fresh params database (they are paired by the grid's
		// internal luid registry).
		grid_params = runtime->get_grid_params_interface()->createGridParams();
		if (grid_params == nullptr) {
			ERR_PRINT("[Flow] createGridParams failed -- emitters will not inject.");
		}
	} else {
		ERR_PRINT("[Flow] createGrid failed.");
		if (rd_context != nullptr) {
			FlowContextRD::destroy(rd_context);
			rd_context = nullptr;
		}
		using_rd = false;
	}
}

void FlowSimulation::reset() {
	const bool threaded = separate_render_thread && using_rd;
	if (threaded) {
		RenderingServer::get_singleton()->call_on_render_thread(Callable(memnew(GodotPhysXQueuedCall<ResetFn>(this))));
		return;
	}
	_rt_reset();
}

void FlowSimulation::_rt_reset() {
	if (grid == nullptr || runtime == nullptr || _context() == nullptr) {
		return;
	}
	NvFlowGridInterface *gi = runtime->get_grid_interface();
	NvFlowGridDesc desc = {};
	desc.maxLocations = MAX(64u, grid_config.max_blocks);
	desc.maxLocationsIsosurface = 0u;
	if (!using_rd) {
		runtime->wait_idle();
	}
	gi->resetGrid(_context(), grid, &desc);
	for (ReadbackSlot &s : slots) {
		s.inflight.frame_index = 0;
		s.consumed = true;
	}
	{
		// Pre-reset frames must not survive the reset: poll_readback would
		// otherwise deliver data from before the grid was cleared.
		MutexLock lock(step_mutex);
		decoded_frames.clear();
	}
	frames_simulated = 0;
	sim_time = 0.0;
	last_readback = FlowReadbackFrame();
}

// ---- Parameter assembly --------------------------------------------------

static void apply_settings_to_layer(NvFlowGridSimulateLayerParams &r_layer, const FlowSimSettings &p_settings) {
	r_layer = NvFlowGridSimulateLayerParams_default;
	// The sim layer keeps v1's explicit luid: the SDK dereferences the
	// layer by it (0 crashes). Offscreen/render request 0 and let the
	// params database bind them to the committed sim layer.
	r_layer.luid = 1;
	r_layer.layer = 0;
	r_layer.level = 0;
	r_layer.levelCount = 1;
	r_layer.densityCellSize = p_settings.cell_size;
	r_layer.autoCellSize = (p_settings.cell_size <= 0.0f) ? NV_FLOW_TRUE : NV_FLOW_FALSE;
	r_layer.enableSmallBlocks = NV_FLOW_FALSE;
	r_layer.blockMinLifetime = p_settings.block_min_lifetime;
	r_layer.stepsPerSecond = MAX(1.0f, p_settings.steps_per_second);
	r_layer.timeScale = MAX(0.0f, p_settings.time_scale);
	r_layer.maxStepsPerSimulate = MAX(1u, p_settings.max_steps_per_simulate);
	r_layer.simulateWhenPaused = p_settings.simulate_when_paused ? NV_FLOW_TRUE : NV_FLOW_FALSE;
	r_layer.velocitySubSteps = MAX(1u, p_settings.velocity_substeps);
	r_layer.physicsCollisionEnabled = NV_FLOW_TRUE;
	r_layer.physicsConvexCollision = NV_FLOW_TRUE;

	// Advection / combustion.
	r_layer.advection = NvFlowAdvectionCombustionParams_default;
	r_layer.advection.enabled = NV_FLOW_TRUE;
	r_layer.advection.combustionEnabled = p_settings.combustion_enabled ? NV_FLOW_TRUE : NV_FLOW_FALSE;
	r_layer.advection.buoyancyPerTemp = p_settings.buoyancy_per_temp;
	r_layer.advection.buoyancyPerSmoke = p_settings.buoyancy_per_smoke;
	r_layer.advection.coolingRate = p_settings.cooling_rate;
	r_layer.advection.gravity = { p_settings.gravity.x, p_settings.gravity.y, p_settings.gravity.z };
	r_layer.advection.smoke.fade = p_settings.smoke_fade;
	r_layer.advection.temperature.fade = p_settings.temperature_fade;
	r_layer.advection.fuel.fade = p_settings.fuel_fade;

	// Vorticity confinement / pressure projection.
	r_layer.vorticity = NvFlowVorticityParams_default;
	r_layer.vorticity.forceScale = p_settings.vorticity_force_scale;
	r_layer.pressure = NvFlowPressureParams_default;
	r_layer.pressure.enabled = p_settings.pressure_enabled ? NV_FLOW_TRUE : NV_FLOW_FALSE;

	// Sparse block allocation thresholds (SDK defaults).
	r_layer.summaryAllocate = NvFlowSummaryAllocateParams_default;
}

// Fills the offscreen (colormap + shadow) and render (raymarch tuning) layer
// params from the node's render settings. The snapshot database points into
// these members, so they live for the duration of the committed frame.
void FlowSimulation::_apply_render_params(const FlowRenderSettings &p_render) {
	const float colormap_max = MAX(p_render.temperature_range, 0.001f);

	offscreen_params = NvFlowGridOffscreenLayerParams_default;
	offscreen_params.luid = 1;
	offscreen_params.layer = 0;
	offscreen_params.level = 0;
	NvFlowShadowParams &shadow = offscreen_params.shadow;
	shadow.enabled = p_render.shadow_steps > 0 ? NV_FLOW_TRUE : NV_FLOW_FALSE;
	shadow.lightDirection = { p_render.light_direction.x, p_render.light_direction.y, p_render.light_direction.z };
	shadow.attenuation = 0.9f * p_render.attenuation;
	shadow.minIntensity = p_render.shadow_min_intensity;
	shadow.numSteps = (NvFlowUint)MAX(1, p_render.shadow_steps);
	shadow.colormapXMin = 0.0f;
	shadow.colormapXMax = colormap_max;
	for (int i = 0; i < FlowRenderSettings::COLORMAP_POINTS; i++) {
		colormap_x[i] = (float)i / (float)(FlowRenderSettings::COLORMAP_POINTS - 1);
		colormap_rgba[i] = { p_render.colormap_rgb[i][0], p_render.colormap_rgb[i][1], p_render.colormap_rgb[i][2], 1.0f };
		colormap_scale[i] = 1.0f;
	}
	NvFlowRayMarchColormapParams &colormap = offscreen_params.colormap;
	colormap.xPoints = colormap_x;
	colormap.xPointCount = FlowRenderSettings::COLORMAP_POINTS;
	colormap.rgbaPoints = colormap_rgba;
	colormap.rgbaPointCount = FlowRenderSettings::COLORMAP_POINTS;
	colormap.colorScalePoints = colormap_scale;
	colormap.colorScalePointCount = FlowRenderSettings::COLORMAP_POINTS;
	colormap.colorScale = 1.0f;

	render_layer_params = NvFlowGridRenderLayerParams_default;
	render_layer_params.luid = 1;
	render_layer_params.layer = 0;
	render_layer_params.level = 0;
	render_layer_params.rayMarch.attenuation = p_render.attenuation;
	render_layer_params.rayMarch.colormapXMin = 0.0f;
	render_layer_params.rayMarch.colormapXMax = colormap_max;
	offscreen_ptr = (NvFlowUint8 *)&offscreen_params;
	render_ptr = (NvFlowUint8 *)&render_layer_params;
}

// The node's thread gathers the frame's inputs; the grid's RenderingDevice
// work runs where the device lives. Single-threaded rendering: right here.
void FlowSimulation::step(double p_absolute_time, float p_delta,
		const FlowEmitterData *p_emitters, int p_emitter_count,
		const FlowColliderData *p_colliders, int p_collider_count,
		const FlowSimSettings &p_settings, const FlowRenderSettings &p_render,
		bool p_force_clear, bool p_paused, bool p_want_readback) {
	const bool threaded = separate_render_thread && using_rd;
	if (!threaded) {
		_rt_step(p_absolute_time, p_delta, p_emitters, p_emitter_count, p_colliders, p_collider_count,
				p_settings, p_render, p_force_clear, p_paused, p_want_readback);
		return;
	}
	{
		MutexLock lock(step_mutex);
		queued_time = p_absolute_time;
		queued_delta = p_delta;
		queued_emitters.clear();
		for (int i = 0; i < p_emitter_count; i++) {
			queued_emitters.push_back(p_emitters[i]);
		}
		queued_colliders.clear();
		for (int i = 0; i < p_collider_count; i++) {
			queued_colliders.push_back(p_colliders[i]);
		}
		queued_settings = p_settings;
		queued_render = p_render;
		queued_force_clear = p_force_clear;
		queued_paused = p_paused;
		queued_want_readback = p_want_readback;
		queued_valid = true;
	}
	RenderingServer::get_singleton()->call_on_render_thread(Callable(memnew(GodotPhysXQueuedCall<StepFn>(this))));
}

// The posted step: pops the gathered inputs and runs the grid work where the
// RenderingDevice lives.
void FlowSimulation::_rt_step() {
	double time;
	float delta;
	LocalVector<FlowEmitterData> emitters;
	LocalVector<FlowColliderData> colliders;
	FlowSimSettings settings;
	FlowRenderSettings render;
	bool force_clear;
	bool paused;
	bool want_readback;
	{
		MutexLock lock(step_mutex);
		if (!queued_valid) {
			return;
		}
		time = queued_time;
		delta = queued_delta;
		emitters = queued_emitters;
		colliders = queued_colliders;
		settings = queued_settings;
		render = queued_render;
		force_clear = queued_force_clear;
		paused = queued_paused;
		want_readback = queued_want_readback;
		queued_valid = false;
	}
	_rt_step(time, delta, emitters.ptr(), (int)emitters.size(), colliders.ptr(), (int)colliders.size(),
			settings, render, force_clear, paused, want_readback);
}

void FlowSimulation::_rt_step(double p_absolute_time, float p_delta,
		const FlowEmitterData *p_emitters, int p_emitter_count,
		const FlowColliderData *p_colliders, int p_collider_count,
		const FlowSimSettings &p_settings, const FlowRenderSettings &p_render,
		bool p_force_clear, bool p_paused, bool p_want_readback) {
	NvFlowGridInterface *gi = runtime != nullptr ? runtime->get_grid_interface() : nullptr;
	NvFlowContextInterface *ci = _context_iface();
	NvFlowContext *fc = _context();
	if (grid == nullptr || gi == nullptr || ci == nullptr || fc == nullptr) {
		return; // slots age out via consumed flags; nothing to invalidate
	}
	step_entry_time = p_absolute_time; // FLOW-8: readback stamps use THIS step's time
	const uint64_t usec0 = Time::get_singleton()->get_ticks_usec();

	apply_settings_to_layer(layer_params, p_settings);
	layer_params.simulateWhenPaused = (p_paused && p_settings.simulate_when_paused) ? NV_FLOW_TRUE : NV_FLOW_FALSE;
	if (p_paused && !p_settings.simulate_when_paused) {
		// Paused without the keep-simulating escape hatch: still record the
		// readback copies so rendering keeps the last frame alive, but skip
		// the solver step (forceDisableCoreSimulation).
		layer_params.forceDisableCoreSimulation = NV_FLOW_TRUE;
		layer_params.forceDisableEmitters = NV_FLOW_TRUE;
	} else {
		layer_params.forceDisableCoreSimulation = NV_FLOW_FALSE;
		layer_params.forceDisableEmitters = NV_FLOW_FALSE;
	}

	// --- Emitters -> SDK params -------------------------------------------
	sphere_params.clear();
	box_params.clear();
	mesh_params.clear();
	mesh_arrays.clear();
	sphere_ptrs.clear();
	box_ptrs.clear();
	mesh_ptrs.clear();

	for (int i = 0; i < p_emitter_count; i++) {
		const FlowEmitterData &e = p_emitters[i];
		if (!e.enabled) {
			continue;
		}
		if (e.is_box) {
			NvFlowGridEmitterBoxParams p = NvFlowEmitterBoxParams_default;
			p.luid = e.luid;
			p.enabled = NV_FLOW_TRUE;
			p.localToWorld = _to_flow_matrix(e.world_xform);
			// The velocity path transforms DIRECTIONS: the basis only, or the
			// emitter's world position adds to every injected velocity.
			p.localToWorldVelocity = _to_flow_matrix(Transform3D(e.world_xform.basis, Vector3()));
			p.velocityIsWorldSpace = NV_FLOW_FALSE;
			p.layer = 0;
			p.level = 0;
			p.halfSize = { e.half_size.x, e.half_size.y, e.half_size.z };
			p.velocity = { e.velocity.x, e.velocity.y, e.velocity.z };
			p.divergence = e.divergence;
			p.temperature = e.temperature;
			p.fuel = e.fuel;
			p.burn = e.burn;
			p.smoke = e.smoke;
			p.coupleRateVelocity = e.couple_rate;
			p.coupleRateTemperature = e.couple_rate;
			p.coupleRateFuel = e.couple_rate;
			p.coupleRateSmoke = e.couple_rate;
			p.isPhysicsCollision = NV_FLOW_FALSE;
			box_params.push_back(p);
		} else {
			NvFlowGridEmitterSphereParams p = NvFlowEmitterSphereParams_default_init;
			p.luid = e.luid;
			p.enabled = NV_FLOW_TRUE;
			p.localToWorld = _to_flow_matrix(e.world_xform);
			// Directions, not points: no translation (see the box emitter).
			p.localToWorldVelocity = _to_flow_matrix(Transform3D(e.world_xform.basis, Vector3()));
			p.velocityIsWorldSpace = NV_FLOW_FALSE;
			p.position = { 0.0f, 0.0f, 0.0f };
			p.layer = 0;
			p.level = 0;
			p.radius = e.radius;
			p.radiusIsWorldSpace = NV_FLOW_TRUE;
			p.velocity = { e.velocity.x, e.velocity.y, e.velocity.z };
			p.divergence = e.divergence;
			p.temperature = e.temperature;
			p.fuel = e.fuel;
			p.burn = e.burn;
			p.smoke = e.smoke;
			p.coupleRateVelocity = e.couple_rate;
			p.coupleRateTemperature = e.couple_rate;
			p.coupleRateFuel = e.couple_rate;
			p.coupleRateSmoke = e.couple_rate;
			p.numSubSteps = MAX(1u, e.sub_steps);
			sphere_params.push_back(p);
		}
	}

	// --- Colliders -> SDK params (collision emitters) ----------------------
	for (int i = 0; i < p_collider_count; i++) {
		const FlowColliderData &c = p_colliders[i];
		if (!c.enabled) {
			continue;
		}
		if (c.is_mesh) {
			NvFlowGridEmitterMeshParams p = NvFlowEmitterMeshParams_default;
			p.luid = c.luid;
			p.enabled = NV_FLOW_TRUE;
			p.localToWorld = _to_flow_matrix(c.world_xform);
			p.localToWorldVelocity = _to_flow_velocity_matrix(c.world_xform, c.linear_velocity, c.angular_velocity);
			p.layer = 0;
			p.level = 0;
			p.velocity = { 0.0f, 0.0f, 0.0f };
			p.temperature = 0.0f;
			p.fuel = 0.0f;
			p.smoke = 0.0f;
			p.burn = 0.0f;
			p.divergence = 0.0f;
			p.coupleRateVelocity = 100.0f;
			p.coupleRateDivergence = 100.0f;
			p.coupleRateTemperature = 100.0f;
			p.coupleRateFuel = 100.0f;
			p.coupleRateBurn = 100.0f;
			p.coupleRateSmoke = 100.0f;
			MeshArrays arrays;
			const int vtx_count = c.mesh_positions.size() / 3;
			arrays.positions.resize(vtx_count);
			for (int v = 0; v < vtx_count; v++) {
				arrays.positions[v] = { c.mesh_positions[v * 3 + 0], c.mesh_positions[v * 3 + 1], c.mesh_positions[v * 3 + 2] };
			}
			// Flow wants flattened per-face vertex indices plus a per-face
			// vertex count; emit a plain triangle list (3 verts per face).
			const int tri_count = vtx_count / 3;
			arrays.indices.resize(vtx_count);
			arrays.face_counts.resize(tri_count);
			for (int t = 0; t < vtx_count; t++) {
				arrays.indices[t] = t;
			}
			for (int t = 0; t < tri_count; t++) {
				arrays.face_counts[t] = 3;
			}
			// NOTE: the buffer pointers are bound AFTER the loop, once the
			// arrays live in mesh_arrays -- binding them to the local copy
			// here would dangle the moment this iteration ends (and any
			// later mesh_arrays growth would leave earlier pointers stale).
			p.meshPositionCount = vtx_count;
			p.meshPositionVersion++;
			p.meshFaceVertexIndexCount = vtx_count;
			p.meshFaceVertexIndexVersion++;
			p.meshFaceVertexCountCount = tri_count;
			p.meshFaceVertexCountVersion++;
			// Godot meshes are CCW-front left-handed... Flow expects
			// orientationLeftHanded=FALSE for CCW (same default); validated
			// visually at runtime.
			p.orientationLeftHanded = NV_FLOW_FALSE;
			p.minDistance = -0.8f;
			p.maxDistance = 0.3f;
			mesh_arrays.push_back(arrays);
			mesh_params.push_back(p);
		} else {
			NvFlowGridEmitterBoxParams p = NvFlowEmitterBoxParams_default;
			p.luid = c.luid;
			p.enabled = NV_FLOW_TRUE;
			p.localToWorld = _to_flow_matrix(c.world_xform);
			p.localToWorldVelocity = _to_flow_velocity_matrix(c.world_xform, c.linear_velocity, c.angular_velocity);
			p.velocityIsWorldSpace = NV_FLOW_TRUE;
			p.layer = 0;
			p.level = 0;
			p.halfSize = { c.half_size.x, c.half_size.y, c.half_size.z };
			p.velocity = { 0.0f, 0.0f, 0.0f };
			p.temperature = 0.0f;
			p.fuel = 0.0f;
			p.smoke = 0.0f;
			p.burn = 0.0f;
			p.divergence = 0.0f;
			// Solid idiom (matches the reference flow_backend): zero injection
			// with full-channel coupling and post-pressure apply -- this is what
			// makes the gas deflect around the wall instead of blowing through
			// it. No allocation: a solid only acts where gas already is.
			p.coupleRateVelocity = 100.0f;
			p.coupleRateDivergence = 100.0f;
			p.coupleRateTemperature = 100.0f;
			p.coupleRateFuel = 100.0f;
			p.coupleRateBurn = 100.0f;
			p.coupleRateSmoke = 100.0f;
			p.applyPostPressure = NV_FLOW_TRUE;
			p.allocationScale = 0.0f;
			p.physicsVelocityScale = 1.0f;
			p.isPhysicsCollision = NV_FLOW_TRUE;
			box_params.push_back(p);
		}
	}

	// --- Floor: one wide solid under the grid ----------------------------
	if (p_settings.floor_enabled) {
		// Centered on the emitters (that is where the grid is); a solid
		// reserves no blocks and holds no gas, so whatever sinks below the
		// floor is gone -- and its blocks with it.
		Vector3 center;
		int centered_on = 0;
		for (int i = 0; i < p_emitter_count; i++) {
			if (p_emitters[i].enabled) {
				center += p_emitters[i].world_xform.origin;
				centered_on++;
			}
		}
		if (centered_on > 0) {
			center /= (float)centered_on;
			constexpr float FLOOR_HALF_W = 4096.0f;
			constexpr float FLOOR_HALF_T = 64.0f;
			NvFlowGridEmitterBoxParams p = NvFlowEmitterBoxParams_default;
			p.luid = 0x464c4f52; // ASCII bytes 'F', 'L', 'O', 'R': a stable id no node stands behind
			p.enabled = NV_FLOW_TRUE;
			p.localToWorld = _to_flow_matrix(Transform3D(Basis(), Vector3(center.x, p_settings.floor_height - FLOOR_HALF_T, center.z)));
			p.localToWorldVelocity = _to_flow_velocity_matrix(Transform3D(Basis(), Vector3(center.x, p_settings.floor_height - FLOOR_HALF_T, center.z)), Vector3(), Vector3());
			p.velocityIsWorldSpace = NV_FLOW_TRUE;
			p.layer = 0;
			p.level = 0;
			p.halfSize = { FLOOR_HALF_W, FLOOR_HALF_T, FLOOR_HALF_W };
			p.velocity = { 0.0f, 0.0f, 0.0f };
			p.temperature = 0.0f;
			p.fuel = 0.0f;
			p.smoke = 0.0f;
			p.burn = 0.0f;
			p.divergence = 0.0f;
			p.coupleRateVelocity = 100.0f;
			p.coupleRateDivergence = 100.0f;
			p.coupleRateTemperature = 100.0f;
			p.coupleRateFuel = 100.0f;
			p.coupleRateBurn = 100.0f;
			p.coupleRateSmoke = 100.0f;
			p.applyPostPressure = NV_FLOW_TRUE;
			p.allocationScale = 0.0f;
			p.physicsVelocityScale = 1.0f;
			p.isPhysicsCollision = NV_FLOW_TRUE;
			box_params.push_back(p);
		}
	}

	for (const NvFlowGridEmitterSphereParams &p : sphere_params) {
		sphere_ptrs.push_back(const_cast<NvFlowGridEmitterSphereParams *>(&p));
	}
	for (const NvFlowGridEmitterBoxParams &p : box_params) {
		box_ptrs.push_back(const_cast<NvFlowGridEmitterBoxParams *>(&p));
	}
	// Bind the mesh buffer pointers now that the arrays live in their final
	// storage (see the NOTE in the collider loop above).
	for (uint32_t i = 0; i < mesh_params.size(); i++) {
		mesh_params[i].meshPositions = mesh_arrays[i].positions.ptr();
		mesh_params[i].meshFaceVertexIndices = mesh_arrays[i].indices.ptr();
		mesh_params[i].meshFaceVertexCounts = mesh_arrays[i].face_counts.ptr();
	}
	for (const NvFlowGridEmitterMeshParams &p : mesh_params) {
		mesh_ptrs.push_back(const_cast<NvFlowGridEmitterMeshParams *>(&p));
	}

	// --- Snapshot database -------------------------------------------------
	static const NvFlowReflectDataType *kSimLayerType = &NvFlowGridSimulateLayerParams_NvFlowReflectDataType;
	static const NvFlowReflectDataType *kSphereType = &NvFlowGridEmitterSphereParams_NvFlowReflectDataType;
	static const NvFlowReflectDataType *kBoxType = &NvFlowGridEmitterBoxParams_NvFlowReflectDataType;
	static const NvFlowReflectDataType *kMeshType = &NvFlowGridEmitterMeshParams_NvFlowReflectDataType;
	static const NvFlowReflectDataType *kOffscreenType = &NvFlowGridOffscreenLayerParams_NvFlowReflectDataType;
	static const NvFlowReflectDataType *kRenderType = &NvFlowGridRenderLayerParams_NvFlowReflectDataType;

	// Render-path params (offscreen colormap/shadow + raymarch tuning); the
	// snapshot database points into the members, so they live on the sim.
	_apply_render_params(p_render);

	// instanceDatas is an array of POINTERS to instances; every level of this
	// chain is member storage because the database only shallow-copies the
	// desc and re-reads these pointers at draw time (see flow_simulation.h).
	layer_ptr = (NvFlowUint8 *)&layer_params;
	offscreen_ptr = (NvFlowUint8 *)&offscreen_params;
	render_ptr = (NvFlowUint8 *)&render_layer_params;

	NvFlowDatabaseTypeSnapshot *type_snapshots = commit_type_snapshots;
	type_snapshots[0] = {};
	type_snapshots[0].version = 1;
	type_snapshots[0].dataType = kSimLayerType;
	type_snapshots[0].instanceDatas = &layer_ptr;
	type_snapshots[0].instanceCount = 1;
	type_snapshots[1] = {};
	type_snapshots[1].version = 1;
	type_snapshots[1].dataType = kSphereType;
	type_snapshots[1].instanceDatas = reinterpret_cast<NvFlowUint8 **>(sphere_ptrs.ptr());
	type_snapshots[1].instanceCount = sphere_ptrs.size();
	type_snapshots[2] = {};
	type_snapshots[2].version = 1;
	type_snapshots[2].dataType = kBoxType;
	type_snapshots[2].instanceDatas = reinterpret_cast<NvFlowUint8 **>(box_ptrs.ptr());
	type_snapshots[2].instanceCount = box_ptrs.size();
	type_snapshots[3] = {};
	type_snapshots[3].version = 1;
	type_snapshots[3].dataType = kMeshType;
	type_snapshots[3].instanceDatas = reinterpret_cast<NvFlowUint8 **>(mesh_ptrs.ptr());
	type_snapshots[3].instanceCount = mesh_ptrs.size();
	type_snapshots[4] = {};
	type_snapshots[4].version = 1;
	type_snapshots[4].dataType = kOffscreenType;
	type_snapshots[4].instanceDatas = &offscreen_ptr;
	type_snapshots[4].instanceCount = 1;
	type_snapshots[5] = {};
	type_snapshots[5].version = 1;
	type_snapshots[5].dataType = kRenderType;
	type_snapshots[5].instanceDatas = &render_ptr;
	type_snapshots[5].instanceCount = 1;

	commit_db_snapshot = {};
	commit_db_snapshot.version = 1;
	commit_db_snapshot.typeSnapshots = type_snapshots;
	commit_db_snapshot.typeSnapshotCount = 6;

	commit_snapshot = {};
	commit_snapshot.snapshot = commit_db_snapshot;
	commit_snapshot.absoluteSimTime = p_absolute_time;
	commit_snapshot.deltaTime = p_delta;
	commit_snapshot.globalForceClear = p_force_clear ? NV_FLOW_TRUE : NV_FLOW_FALSE;

	// Route the frame's params through the SDK's transaction database:
	// commit ours, pull the processed snapshot, and map the desc simulate()
	// consumes. The database assigns the layer luids / staging versions the
	// grid's internal ops require (a desc straight from our arrays was
	// silently ignored -- context ran, zero blocks ever allocated).
	NvFlowGridParamsInterface *gpi = runtime->get_grid_params_interface();
	if (grid_params != nullptr && gpi->commitParams != nullptr) {
		NvFlowGridParamsDescSnapshot committed = commit_snapshot;
		gpi->commitParams(grid_params, &committed);
	}
	NvFlowGridParamsDesc params_desc = {};
	params_desc.snapshots = &commit_snapshot;
	params_desc.snapshotCount = 1;
	if (grid_params != nullptr && gpi->getParamsSnapshot != nullptr && gpi->mapParamsDesc != nullptr) {
		if (mapped_snapshot != nullptr) {
			gpi->unmapParamsDesc(grid_params, mapped_snapshot);
			mapped_snapshot = nullptr;
			desc_active = false;
		}
		mapped_snapshot = gpi->getParamsSnapshot(grid_params, p_absolute_time, pull_id++);
		if (mapped_snapshot != nullptr && gpi->mapParamsDesc(grid_params, mapped_snapshot, &mapped_desc)) {
			params_desc = mapped_desc;
			desc_active = true;
			// desc now points at the database's processed snapshots.
			if (diag_flags & DIAG_SPARSE_LAYOUT) {
				static bool once = false;
				if (!once) {
					once = true;
					print_line("[Flow] mapped desc path ACTIVE");
				}
			}
		} else {
			mapped_snapshot = nullptr;
			// Fall back to our raw desc (degenerate but keeps work flowing).
			params_desc.snapshots = &commit_snapshot;
			params_desc.snapshotCount = 1;
		}
	}

	if (diag_flags & DIAG_SPARSE_LAYOUT) {
		String dump = "[Flow] params_desc: snapshots=" + itos((int)params_desc.snapshotCount);
		for (uint64_t s = 0; s < params_desc.snapshotCount && params_desc.snapshots; s++) {
			const NvFlowGridParamsDescSnapshot &gs = params_desc.snapshots[s];
			dump += vformat(" [s%d: types=%d]", (int)s, (int)gs.snapshot.typeSnapshotCount);
			for (uint64_t t = 0; t < gs.snapshot.typeSnapshotCount; t++) {
				const NvFlowDatabaseTypeSnapshot &ts = gs.snapshot.typeSnapshots[t];
				dump += vformat(" %s=%d", ts.dataType ? ts.dataType->structTypename : "?", (int)ts.instanceCount);
			}
		}
		print_line(dump);
	}

	gi->simulate(fc, grid, &params_desc, p_force_clear ? NV_FLOW_TRUE : NV_FLOW_FALSE);

	// --- Record readback copies for this frame (alternating slot) ------------
	// FLOW-1 (paused): the uploaded fog texture already holds the last decoded
	// frame and persists on the GPU -- recording fresh copies while paused
	// made poll_readback deliver "new" (identical) frames forever, so the
	// node re-uploaded identical data at physics rate. Skip recording when
	// paused without the keep-simulating escape hatch. On the RenderingDevice
	// path the readback is also opt-in (p_want_readback): the GPU raymarcher
	// renders straight from the grid, so the CPU copy exists only for the
	// query API (smoke sampling / bounds).
	const bool record_readback = p_want_readback && (!p_paused || p_settings.simulate_when_paused);
	ReadbackSlot &slot = slots[record_slot & 1]; // the flush tail stamps this slot
	bool recorded = false;
	if (record_readback) {
		record_slot++;
		recorded = _record_readback_copies(slot, ci, gi);
	}

	gi->offscreen(fc, grid, &params_desc);

	if (diag_flags & DIAG_SPARSE_LAYOUT) {
		static int dump_frame = 0;
		if ((dump_frame++ % 30) == 0) {
			NvFlowGridRenderData rd = {};
			gi->getRenderData(fc, grid, &rd);
			const NvFlowSparseParams &sp2 = rd.sparseParams;
			print_line(vformat("[Flow] renderData: densityTex=%d sparseBuf=%d levels=%d layers=%d locs=%d activeBlocks=%d",
					rd.densityTexture != nullptr ? 1 : 0, rd.sparseBuffer != nullptr ? 1 : 0,
					(int)sp2.levelCount, (int)sp2.layerCount, (int)sp2.locationCount,
					(int)gi->getActiveBlockCount(grid)));
			if (sp2.layerCount > 0 && sp2.layers) {
				const NvFlowSparseLayerParams &lp = sp2.layers[0];
				print_line(vformat("[Flow] layer0: blockSize=(%f,%f,%f) locMin=(%d,%d,%d) locMax=(%d,%d,%d) numLocs=%d dt=%f",
						(float)lp.blockSizeWorld.x, (float)lp.blockSizeWorld.y, (float)lp.blockSizeWorld.z,
						lp.locationMin.x, lp.locationMin.y, lp.locationMin.z,
						lp.locationMax.x, lp.locationMax.y, lp.locationMax.z,
						(int)lp.numLocations, (float)lp.deltaTime));
			}
			if (sp2.levelCount > 0 && sp2.levels) {
				const NvFlowSparseLevelParams &lv = sp2.levels[0];
				print_line(vformat("[Flow] level0: blockDimBits=(%d,%d,%d) dim=(%d,%d,%d) numLocations=%d maxLocations=%d",
						lv.blockDimBits.x, lv.blockDimBits.y, lv.blockDimBits.z,
						lv.dim.x, lv.dim.y, lv.dim.z, (int)lv.numLocations, (int)lv.maxLocations));
			}
		}
	}

	// The mapped desc STAYS alive: render() composites this frame with the
	// same committed params; the next step() unmaps it.
	frames_simulated++;
	sim_time = p_absolute_time;

	// Flush, then stamp this slot's copy with the frame id completion is
	// gated on. The id is 0 when nothing was submitted (e.g. the grid had no
	// render data yet) -- the copy then stays unstampable and is dropped
	// rather than left waiting on an id that never completes.
	const uint64_t usec1 = Time::get_singleton()->get_ticks_usec();
	uint64_t flushed_frame = 0;
	_flush_frame(&flushed_frame);
	// Only a slot this step actually recorded into: stamping the other slot
	// would re-mark its two-frames-old copies as fresh.
	if (recorded && flushed_frame > 0) {
		slot.inflight.frame_index = flushed_frame;
		slot.consumed = false;
	}
	timing.last_submit_usec = usec1 - usec0;
	timing.last_flush_usec = Time::get_singleton()->get_ticks_usec() - usec1;

	// Separate render thread: the decode runs here too (it maps the SDK's
	// readback buffers, a RenderingDevice-adjacent call), and the main
	// thread's poll_readback pops the decoded frames. Single-threaded:
	// poll_readback decodes as before.
	if (separate_render_thread && using_rd) {
		const uint64_t completed = _last_frame_completed();
		for (ReadbackSlot &sl : slots) {
			if (sl.consumed || sl.inflight.frame_index == 0 || completed < sl.inflight.frame_index) {
				continue;
			}
			FlowReadbackFrame decoded;
			if (_decode_slot(sl, decoded)) {
				MutexLock lock(step_mutex);
				decoded_frames.push_back(decoded);
				if (decoded_frames.size() > 4) {
					decoded_frames.remove_at(0);
				}
			}
			sl.consumed = true;
		}
	}
}

// Records texture + sparse-table copies into the persistent readback
// buffers, and snapshots the CPU-side sparse layout that matches the frame.
// Returns false when nothing was recorded -- the caller then leaves the slot
// alone (its buffers still hold the previous record until it is decoded).
bool FlowSimulation::_record_readback_copies(ReadbackSlot &r_slot, NvFlowContextInterface *p_ci, NvFlowGridInterface *p_gi) {
	NvFlowContext *fc = _context();
	if (fc == nullptr || p_ci == nullptr) {
		return false;
	}
	NvFlowGridRenderData render_data = {};
	p_gi->getRenderData(fc, grid, &render_data);
	if (render_data.densityTexture == nullptr || render_data.sparseBuffer == nullptr) {
		return false;
	}

	// Extract the sparse layout that owns the density texture this frame.
	const NvFlowSparseParams &sp = render_data.sparseParams;
	if (sp.levelCount == 0 || sp.levels == nullptr || sp.layerCount == 0 || sp.layers == nullptr) {
		return false; // grid not allocating yet -- nothing to read back
	}
	// Level 0 = finest (density) level.
	const NvFlowSparseLevelParams &level = sp.levels[0];

	// The grid's density texture format is r16g16b16a16_float (rgba half:
	// rgb = colormap-colorized density channels, a = smoke density) -- the
	// same texture the SDK's own raymarch samples; byte layout below must
	// match or the decode produces noise (validated by the readback test).
	const uint32_t bytes_per_pixel = 8;
	const uint64_t texture_bytes =
			(uint64_t)level.dim.x * level.dim.y * level.dim.z * bytes_per_pixel;

	if (texture_bytes == 0) {
		return false;
	}
	// The copy covers the level's FULL virtual dimension, which grows with
	// the block budget: past ~512 MB a readback is not a query anymore, it
	// is a device-spanning copy every step. Skip with a one-time warning
	// instead -- rendering is unaffected; only the CPU query surface (max
	// smoke / bounds) goes quiet.
	constexpr uint64_t READBACK_BYTES_LIMIT = 512ull << 20;
	if (texture_bytes > READBACK_BYTES_LIMIT) {
		if (!readback_limit_warned) {
			readback_limit_warned = true;
			WARN_PRINT(vformat("[Flow] readback skipped: the grid's virtual volume is %d MB (limit %d MB). Rendering is unaffected; the CPU query surface (max smoke / bounds) is unavailable at this block budget.",
					texture_bytes >> 20, READBACK_BYTES_LIMIT >> 20));
		}
		return false;
	}
	_ensure_readback_buffers(r_slot, texture_bytes, 0);

	if (r_slot.texture_buffer == nullptr) {
		return false;
	}

	NvFlowBufferTransient *dst_tex = p_ci->registerBufferAsTransient(fc, r_slot.texture_buffer);
	if (dst_tex == nullptr) {
		return false;
	}

	NvFlowPassCopyTextureToBufferParams copy_tex = {};
	copy_tex.src = render_data.densityTexture;
	copy_tex.dst = dst_tex;
	copy_tex.textureOffset = { 0, 0, 0 };
	copy_tex.textureExtent = { level.dim.x, level.dim.y, level.dim.z };
	copy_tex.bufferOffset = 0;
	copy_tex.bufferRowPitch = level.dim.x * bytes_per_pixel;
	copy_tex.bufferDepthPitch = (uint64_t)level.dim.x * level.dim.y * bytes_per_pixel;
	copy_tex.debugLabel = "godotFlowDensityReadback";
	p_ci->addPassCopyTextureToBuffer(fc, &copy_tex);

	// Sparse table copy: bytes needed = last offset used by the level
	// (blockLevelOffsetLocal + 32 uints per location, plus slack).
	const uint64_t table_bytes = (uint64_t)(level.blockLevelOffsetLocal + (level.numLocations << 5) + 64) * sizeof(NvFlowUint);
	if (table_bytes == 0) {
		return false;
	}
	_ensure_readback_buffers(r_slot, texture_bytes, table_bytes);
	if (r_slot.table_buffer == nullptr) {
		return false;
	}
	NvFlowBufferTransient *dst_table = p_ci->registerBufferAsTransient(fc, r_slot.table_buffer);
	if (dst_table == nullptr) {
		return false;
	}
	NvFlowPassCopyBufferParams copy_tbl = {};
	copy_tbl.src = render_data.sparseBuffer;
	copy_tbl.dst = dst_table;
	copy_tbl.srcOffset = 0;
	copy_tbl.dstOffset = 0;
	copy_tbl.numBytes = table_bytes;
	copy_tbl.debugLabel = "godotFlowTableReadback";
	p_ci->addPassCopyBuffer(fc, &copy_tbl);

	// Snapshot the sparse layout for the decode (CPU-side copies).
	// FLOW-8: `sim_time` was still the PREVIOUS step's time here (the caller
	// assigns the new one after the readback records) -- first frame reported
	// t=0 and every frame lagged one step. The step() entry time is what this
	// frame's data belongs to.
	r_slot.inflight.frame_index = 0; // stamped by step() after flush
	r_slot.inflight.absolute_sim_time = step_entry_time;
	if (sp.layerCount > 0 && sp.layers != nullptr) {
		r_slot.inflight.layer = sp.layers[0];
	}
	r_slot.inflight.level = level;
	r_slot.inflight.texture_format = eNvFlowFormat_r16g16b16a16_float;
	r_slot.inflight.texture_width = level.dim.x;
	r_slot.inflight.texture_height = level.dim.y;
	r_slot.inflight.texture_depth = level.dim.z;
	r_slot.inflight.table_bytes = table_bytes;
	if (diag_flags & DIAG_SPARSE_LAYOUT) {
		print_line(vformat("[Flow] sparse layout: dim=%d,%d,%d locations=%d/%d layers=%d blockSize=%f tableBytes=%d texBytes=%d",
				(int)level.dim.x, (int)level.dim.y, (int)level.dim.z, (int)level.numLocations, (int)level.maxLocations,
				(int)sp.layerCount, (float)(sp.layers ? sp.layers[0].blockSizeWorld.y : 0.0f),
				(int64_t)table_bytes, (int64_t)texture_bytes));
	}
	return true;
}

// ---- Readback buffers ----------------------------------------------------

// Readback buffers grow in 16 MB buckets and never shrink: the grid's
// virtual dimension (and so the copy size) climbs in small steps while a
// plume develops -- an exact-size buffer would be destroyed and recreated
// every few steps (each recreation a 10-100 MB alloc, plus a device sync
// on the fallback path), per slot, per simulation.
constexpr uint64_t READBACK_BUCKET = 16ull << 20;

static uint64_t readback_bucket(uint64_t p_bytes) {
	return (p_bytes + READBACK_BUCKET - 1) / READBACK_BUCKET * READBACK_BUCKET;
}

void FlowSimulation::_ensure_readback_buffers(ReadbackSlot &r_slot, uint64_t p_texture_bytes, uint64_t p_table_bytes) {
	NvFlowContextInterface *ci = _context_iface();
	NvFlowContext *fc = _context();
	if (ci == nullptr || fc == nullptr) {
		return;
	}
	const uint64_t texture_bucket = readback_bucket(p_texture_bytes);
	const uint64_t table_bucket = readback_bucket(p_table_bytes);
	if (p_texture_bytes > 0 && (r_slot.texture_buffer == nullptr || r_slot.texture_buffer_bytes < texture_bucket)) {
		if (r_slot.texture_buffer != nullptr) {
			// The RD context defers frees behind flush(); only the fallback
			// device needs an explicit idle before destroying.
			if (!using_rd) {
				runtime->wait_idle();
			}
			ci->destroyBuffer(fc, r_slot.texture_buffer);
			r_slot.texture_buffer = nullptr;
		}
		NvFlowBufferDesc desc = {};
		desc.usageFlags = eNvFlowBufferUsage_bufferCopyDst;
		desc.sizeInBytes = texture_bucket;
		r_slot.texture_buffer = ci->createBuffer(fc, eNvFlowMemoryType_readback, &desc);
		r_slot.texture_buffer_bytes = texture_bucket;
		if (r_slot.texture_buffer == nullptr) {
			ERR_PRINT(vformat("[Flow] failed to allocate %d byte density readback buffer.", (int64_t)p_texture_bytes));
			r_slot.texture_buffer_bytes = 0;
		}
	}
	if (p_table_bytes > 0 && (r_slot.table_buffer == nullptr || r_slot.table_buffer_bytes < table_bucket)) {
		if (r_slot.table_buffer != nullptr) {
			if (!using_rd) {
				runtime->wait_idle();
			}
			ci->destroyBuffer(fc, r_slot.table_buffer);
			r_slot.table_buffer = nullptr;
		}
		NvFlowBufferDesc desc = {};
		desc.usageFlags = eNvFlowBufferUsage_bufferCopyDst;
		desc.sizeInBytes = table_bucket;
		r_slot.table_buffer = ci->createBuffer(fc, eNvFlowMemoryType_readback, &desc);
		r_slot.table_buffer_bytes = table_bucket;
		if (r_slot.table_buffer == nullptr) {
			ERR_PRINT(vformat("[Flow] failed to allocate %d byte table readback buffer.", (int64_t)p_table_bytes));
			r_slot.table_buffer_bytes = 0;
		}
	}
}

void FlowSimulation::_release_readback_buffers() {
	if (runtime == nullptr) {
		return;
	}
	NvFlowContextInterface *ci = _context_iface();
	NvFlowContext *fc = _context();
	if (ci != nullptr && fc != nullptr) {
		if (!using_rd) {
			runtime->wait_idle();
		}
		for (ReadbackSlot &s : slots) {
			if (s.texture_buffer != nullptr) {
				ci->destroyBuffer(fc, s.texture_buffer);
				s.texture_buffer = nullptr;
			}
			if (s.table_buffer != nullptr) {
				ci->destroyBuffer(fc, s.table_buffer);
				s.table_buffer = nullptr;
			}
			s.texture_buffer_bytes = 0;
			s.table_buffer_bytes = 0;
			s.inflight.frame_index = 0;
			s.consumed = true;
		}
	}
	record_slot = 0;
}

// ---- Decode --------------------------------------------------------------

static float half_to_float_fast(uint16_t h) {
	// IEEE 754 half -> float (standard bit twiddle).
	const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
	uint32_t exp = (h >> 10) & 0x1Fu;
	uint32_t mant = h & 0x3FFu;
	uint32_t bits;
	if (exp == 0u) {
		if (mant == 0u) {
			bits = sign;
		} else {
			// Subnormal: normalize.
			exp = 127u - 15u + 1u;
			while ((mant & 0x400u) == 0u) {
				mant <<= 1;
				exp--;
			}
			mant &= 0x3FFu;
			bits = sign | (exp << 23) | (mant << 13);
		}
	} else if (exp == 0x1Fu) {
		bits = sign | 0x7F800000u | (mant << 13);
	} else {
		bits = sign | ((exp - 15u + 127u) << 23) | (mant << 13);
	}
	float out;
	memcpy(&out, &bits, sizeof(out));
	return out;
}

bool FlowSimulation::poll_readback(FlowReadbackFrame &r_frame) {
	if (runtime == nullptr) {
		return false;
	}
	if (separate_render_thread && using_rd) {
		MutexLock lock(step_mutex);
		if (decoded_frames.is_empty()) {
			return false;
		}
		// The decode loop pushes in slot order, not frame_index order, so
		// queue front != newest. Return the highest frame_index (the header
		// contract) and drop the rest: a backlog is stale query data by the
		// time it is consumed.
		uint32_t newest = 0;
		for (uint32_t i = 1; i < decoded_frames.size(); i++) {
			if (decoded_frames[i].frame_index > decoded_frames[newest].frame_index) {
				newest = i;
			}
		}
		r_frame = decoded_frames[newest];
		decoded_frames.clear();
		return true;
	}
	const uint64_t completed = _last_frame_completed();
	for (ReadbackSlot &s : slots) {
		if (s.consumed || s.inflight.frame_index == 0) {
			continue;
		}
		if (completed < s.inflight.frame_index) {
			continue; // still in flight
		}
		const uint64_t decode0 = Time::get_singleton()->get_ticks_usec();
		const bool ok = _decode_slot(s, r_frame);
		timing.last_decode_usec = Time::get_singleton()->get_ticks_usec() - decode0;
		s.consumed = true; // decoded (or unrecoverable) -- do not retry
		return ok;
	}
	return false;
}

bool FlowSimulation::_decode_slot(ReadbackSlot &r_slot, FlowReadbackFrame &r_frame) {
	NvFlowContextInterface *ci = _context_iface();
	NvFlowContext *fc = _context();
	if (ci == nullptr || fc == nullptr || r_slot.texture_buffer == nullptr || r_slot.table_buffer == nullptr) {
		return false;
	}
	const uint32_t *table = static_cast<const uint32_t *>(ci->mapBuffer(fc, r_slot.table_buffer));
	const uint16_t *tex = static_cast<const uint16_t *>(ci->mapBuffer(fc, r_slot.texture_buffer));
	if (table == nullptr || tex == nullptr) {
		if (tex != nullptr) {
			ci->unmapBuffer(fc, r_slot.texture_buffer);
		}
		if (table != nullptr) {
			ci->unmapBuffer(fc, r_slot.table_buffer);
		}
		return false;
	}

	r_frame = FlowReadbackFrame();
	r_frame.frame_index = r_slot.inflight.frame_index;
	r_frame.absolute_sim_time = r_slot.inflight.absolute_sim_time;

	const NvFlowSparseLevelParams &level = r_slot.inflight.level;
	const NvFlowSparseLayerParams &layer = r_slot.inflight.layer;
	// Blocks are anisotropic (blockDimBits (5,4,4) with blockSizeWorld
	// (8,4,4) keeps cubic cells): every block-space step uses the per-axis
	// dim, never blockDimBits.x for all axes.
	const int32_t block_dim[3] = {
		1 << level.blockDimBits.x,
		1 << level.blockDimBits.y,
		1 << level.blockDimBits.z,
	};
	const float cell_size = MAX(layer.blockSizeWorld.x / (float)block_dim[0],
			MAX(layer.blockSizeWorld.y / (float)block_dim[1],
					layer.blockSizeWorld.z / (float)block_dim[2]));

	// Dense volume bounds from the sparse layer's location range, anchored at
	// the layer's worldMin — the same anchor the render path reads.
	const int32_t loc_min[3] = { layer.locationMin.x, layer.locationMin.y, layer.locationMin.z };
	const int32_t loc_max[3] = { layer.locationMax.x, layer.locationMax.y, layer.locationMax.z };
	const int32_t loc_min_cells[3] = {
		loc_min[0] * block_dim[0],
		loc_min[1] * block_dim[1],
		loc_min[2] * block_dim[2],
	};
	int32_t span_blocks[3];
	for (int a = 0; a < 3; a++) {
		span_blocks[a] = MAX(1, loc_max[a] - loc_min[a]);
	}

	// Cap the dense volume; coarsen (stride) if the sparse domain grew huge.
	int dims[3];
	int stride = 1;
	const int max_dim = 160;
	for (int a = 0; a < 3; a++) {
		dims[a] = span_blocks[a] * (int)block_dim[a];
	}
	while (MAX(dims[0] / stride, MAX(dims[1] / stride, dims[2] / stride)) > max_dim) {
		stride <<= 1;
	}
	for (int a = 0; a < 3; a++) {
		dims[a] = MAX(1, dims[a] / stride);
	}
	if (dims[0] < 1 || dims[1] < 1 || dims[2] < 1 ||
			(uint64_t)dims[0] * dims[1] * dims[2] > (64u << 20)) {
		ci->unmapBuffer(fc, r_slot.texture_buffer);
		ci->unmapBuffer(fc, r_slot.table_buffer);
		return false;
	}
	if (diag_flags & DIAG_READBACK_STATS) {
		print_line(vformat("[Flow] decode: locMin=(%d,%d,%d) locMax=(%d,%d,%d) worldMin=(%f,%f,%f) blockSize=(%f,%f,%f) blockDim=(%d,%d,%d) dim=(%d,%d,%d) locationOffset=%d blockLevelOffsetGlobal=%d",
				loc_min[0], loc_min[1], loc_min[2], loc_max[0], loc_max[1], loc_max[2],
				(float)layer.worldMin.x, (float)layer.worldMin.y, (float)layer.worldMin.z,
				(float)layer.blockSizeWorld.x, (float)layer.blockSizeWorld.y, (float)layer.blockSizeWorld.z,
				block_dim[0], block_dim[1], block_dim[2], dims[0], dims[1], dims[2],
				(int)level.locationOffset, (int)level.blockLevelOffsetGlobal));
	}
	r_frame.dims = Vector3i(dims[0], dims[1], dims[2]);
	r_frame.cell_size = cell_size * stride;
	r_frame.world_min = Vector3((float)layer.worldMin.x, (float)layer.worldMin.y, (float)layer.worldMin.z);
	r_frame.world_size = Vector3(dims[0], dims[1], dims[2]) * r_frame.cell_size;
	r_frame.voxels_rgbaH.resize(dims[0] * dims[1] * dims[2] * 4);
	memset(r_frame.voxels_rgbaH.ptrw(), 0, r_frame.voxels_rgbaH.size() * sizeof(uint16_t));

	// Decode blocks straight from the sparse table — the source the GPU-side
	// lookups read (NvFlowLocationToBlockIdx + NvFlowTableValueToIndexRead,
	// NvFlowShader.hlsli): the table lists each block's location at
	// (blockIdx << 2) + locationOffset and the block's packed atlas base at
	// blockIdx + blockLevelOffsetGlobal. The exported location array's order
	// is not the table's, so it must not drive the pairing.
	uint32_t valid_blocks = 0;
	int32_t first_nonzero_cells[3] = { 0, 0, 0 };
	bool first_nonzero_found = false;
	const uint32_t table_uints = (uint32_t)(r_slot.inflight.table_bytes / sizeof(uint32_t));
	const uint32_t num_blocks = MIN(level.numLocations,
			table_uints > level.locationOffset + 4u ? (table_uints - level.locationOffset) / 4u : 0u);
	for (uint32_t blockIdx = 0; blockIdx < num_blocks; blockIdx++) {
		const uint32_t table_value = table[(uint64_t)blockIdx + level.blockLevelOffsetGlobal];
		const bool valid = (table_value >> 31u) != 0u;
		const int32_t location[3] = {
			(int32_t)table[((uint64_t)blockIdx << 2u) + level.locationOffset + 0u],
			(int32_t)table[((uint64_t)blockIdx << 2u) + level.locationOffset + 1u],
			(int32_t)table[((uint64_t)blockIdx << 2u) + level.locationOffset + 2u],
		};
		if (diag_flags & DIAG_READBACK_STATS) {
			print_line(vformat("[Flow]   block %d: loc=(%d,%d,%d) tableValue=%d valid=%d",
					(int)blockIdx, location[0], location[1], location[2],
					table_value, valid ? 1 : 0));
		}
		if (!valid) {
			continue;
		}
		valid_blocks++;
		const int32_t vbase[3] = {
			location[0] * block_dim[0],
			location[1] * block_dim[1],
			location[2] * block_dim[2],
		};
		for (int32_t bz = 0; bz < block_dim[2]; bz++) {
			for (int32_t by = 0; by < block_dim[1]; by++) {
				for (int32_t bx = 0; bx < block_dim[0]; bx++) {
					// Packed atlas base + in-block texel, per
					// NvFlowTableValueToIndexRead (the sum wraps at the
					// field width).
					const int32_t rx = (int32_t)((((table_value << 1u) | 1u) + (uint32_t)bx) & 0xFFFu);
					const int32_t ry = (int32_t)((((table_value >> 10u) | 1u) + (uint32_t)by) & 0x7FFu);
					const int32_t rz = (int32_t)((((table_value >> 20u) | 1u) + (uint32_t)bz) & 0x7FFu);
					if ((uint32_t)rx >= r_slot.inflight.texture_width ||
							(uint32_t)ry >= r_slot.inflight.texture_height ||
							(uint32_t)rz >= r_slot.inflight.texture_depth) {
						continue;
					}
					// Sparse texture texel (rgba half).
					const uint64_t texel = ((uint64_t)rz * r_slot.inflight.texture_height + ry) * r_slot.inflight.texture_width + rx;
					const uint16_t *rgba = tex + texel * 4;
					const float r = half_to_float_fast(rgba[0]);
					const float g = half_to_float_fast(rgba[1]);
					const float b = half_to_float_fast(rgba[2]);
					const float a = half_to_float_fast(rgba[3]);
					if (r == 0.0f && g == 0.0f && b == 0.0f && a == 0.0f) {
						continue;
					}
					// Virtual voxel -> dense index (with stride coarsening;
					// overlapping stride cells keep the max component-wise).
					const int32_t vx = vbase[0] + bx;
					const int32_t vy = vbase[1] + by;
					const int32_t vz = vbase[2] + bz;
					const int dx = (vx - loc_min_cells[0]) / stride;
					const int dy = (vy - loc_min_cells[1]) / stride;
					const int dz = (vz - loc_min_cells[2]) / stride;
					if (dx < 0 || dy < 0 || dz < 0 || dx >= dims[0] || dy >= dims[1] || dz >= dims[2]) {
						continue;
					}
					uint16_t *dst = r_frame.voxels_rgbaH.ptrw() + (((uint64_t)dz * dims[1] + dy) * dims[0] + dx) * 4;
					dst[0] = rgba[0];
					dst[1] = rgba[1];
					dst[2] = rgba[2];
					dst[3] = rgba[3];
					r_frame.max_smoke = MAX(r_frame.max_smoke, a);
					r_frame.max_temperature = MAX(r_frame.max_temperature, r);
					if (!first_nonzero_found) {
						first_nonzero_found = true;
						first_nonzero_cells[0] = vx;
						first_nonzero_cells[1] = vy;
						first_nonzero_cells[2] = vz;
					}
				}
			}
		}
	}

	ci->unmapBuffer(fc, r_slot.texture_buffer);
	ci->unmapBuffer(fc, r_slot.table_buffer);
	r_frame.active_blocks = valid_blocks;
	r_frame.valid = true;

	if (diag_flags & DIAG_READBACK_STATS) {
		print_line(vformat("[Flow] readback frame %d: blocks=%d/%d dims=%d,%d,%d stride=%d cell=%f maxSmoke=%f maxTemp=%f",
				(int64_t)r_frame.frame_index, (int)valid_blocks, (int)num_blocks,
				dims[0], dims[1], dims[2], stride, r_frame.cell_size, r_frame.max_smoke, r_frame.max_temperature));
		if (first_nonzero_found) {
			print_line(vformat("[Flow]   first nonzero voxel: virtual=(%d,%d,%d) world=(%f,%f,%f)",
					first_nonzero_cells[0], first_nonzero_cells[1], first_nonzero_cells[2],
					first_nonzero_cells[0] * cell_size, first_nonzero_cells[1] * cell_size, first_nonzero_cells[2] * cell_size));
		}
	}
	return true;
}

// ---- Render (Flow's own ray marcher, RenderingDevice path only) ------------

uint64_t FlowSimulation::render(const float p_view[16], const float p_projection[16], uint32_t p_width, uint32_t p_height,
		uint64_t p_depth_rid, uint32_t p_depth_width, uint32_t p_depth_height, uint64_t p_color_rid, FlowScatterSource *r_scatter, bool p_lighting_pass) {
	if (!is_using_rd() || grid == nullptr || runtime == nullptr || p_width == 0 || p_height == 0) {
		return 0;
	}
	NvFlowContextInterface *ci = _context_iface();
	NvFlowContext *fc = _context();
	NvFlowGridInterface *gi = runtime->get_grid_interface();
	NvFlowGridParamsInterface *gpi = runtime->get_grid_params_interface();
	if (ci == nullptr || fc == nullptr || gi == nullptr || gpi == nullptr) {
		return 0;
	}

	// The desc step() mapped is still alive (until the next step unmaps it):
	// render composites with the SAME committed offscreen/raymarch params the
	// simulation ran with. Re-pulling the snapshot by time here returned the
	// database's freed staging state after the first pull and crashed the
	// SDK's render (five sims, first second).
	if (!desc_active) {
		return 0; // no committed params yet this frame
	}
	const NvFlowGridParamsDesc &params_desc = mapped_desc;

	NvFlowFloat4x4 view;
	NvFlowFloat4x4 projection;
	memcpy(&view, p_view, sizeof(view));
	memcpy(&projection, p_projection, sizeof(projection));

	// Godot's scene depth (r32 float, reverse-Z in 0..1) and scene color
	// (rgba16f), borrowed as transients for this frame only.
	NvFlowTextureTransient *depth = (NvFlowTextureTransient *)FlowContextRD::import_texture(rd_context,
			p_depth_rid, p_depth_width, p_depth_height, eNvFlowFormat_r32_float);
	NvFlowTextureTransient *color = (NvFlowTextureTransient *)FlowContextRD::import_texture(rd_context,
			p_color_rid, p_width, p_height, eNvFlowFormat_r16g16b16a16_float);
	NvFlowTextureTransient *out = nullptr;
	// offscreen() colorizes the grid (colormap + shadowing from the committed
	// offscreen layer), render() raymarches it with the scene depth for
	// occlusion and composites over the color. The colorized volume is
	// view-independent: under XR the first view runs it and the rest raymarch
	// the same output.
	if (p_lighting_pass) {
		gi->offscreen(fc, grid, &params_desc);
	}
	gi->render(fc, grid, &params_desc, &view, &projection, &projection,
			p_width, p_height, p_depth_width, p_depth_height,
			1.0f, depth, eNvFlowFormat_r16g16b16a16_float, color, &out);
	if (r_scatter != nullptr) {
		// The volume the ray march just read (after offscreen(): Flow's shadow
		// output, its sun light in z), and the table to find blocks in it.
		*r_scatter = FlowScatterSource();
		NvFlowGridRenderData data = {};
		gi->getRenderData(fc, grid, &data);
		const NvFlowSparseParams &sp = data.sparseParams;
		if (data.densityTexture != nullptr && data.sparseBuffer != nullptr && sp.levelCount > 0 && sp.layerCount > 0) {
			static_assert(sizeof(NvFlowSparseLevelParams) == sizeof(r_scatter->level), "NvFlowSparseLevelParams changed size");
			memcpy(r_scatter->level, &sp.levels[0], sizeof(r_scatter->level)); // density is level 0
			const NvFlowSparseLayerParams &layer = sp.layers[0];
			r_scatter->block_size_world[0] = layer.blockSizeWorld.x;
			r_scatter->block_size_world[1] = layer.blockSizeWorld.y;
			r_scatter->block_size_world[2] = layer.blockSizeWorld.z;
			r_scatter->layer_and_level = layer.layerAndLevel;
			r_scatter->world_min[0] = layer.worldMin.x;
			r_scatter->world_min[1] = layer.worldMin.y;
			r_scatter->world_min[2] = layer.worldMin.z;
			r_scatter->world_max[0] = layer.worldMax.x;
			r_scatter->world_max[1] = layer.worldMax.y;
			r_scatter->world_max[2] = layer.worldMax.z;
			r_scatter->density_rid = FlowContextRD::transient_texture_rid(data.densityTexture);
			r_scatter->table_rid = FlowContextRD::transient_buffer_rid(data.sparseBuffer);
			r_scatter->valid = r_scatter->density_rid != 0 && r_scatter->table_rid != 0 && layer.numLocations > 0;
		}
	}
	// The desc stays mapped until the next step (see above).
	return out != nullptr ? FlowContextRD::transient_texture_rid(out) : 0;
}

void FlowSimulation::render_finish() {
	if (is_using_rd() && rd_context != nullptr) {
		FlowContextRD::flush(rd_context);
	}
}

uint32_t FlowSimulation::get_active_block_count() const {
	if (grid == nullptr || runtime == nullptr) {
		return 0;
	}
	return runtime->get_grid_interface()->getActiveBlockCount(grid);
}

#endif // GODOT_PHYSX_FLOW

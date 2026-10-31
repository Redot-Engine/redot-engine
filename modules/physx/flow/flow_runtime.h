/**************************************************************************/
/*  flow_runtime.h                                                        */
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

#ifdef GODOT_PHYSX_FLOW

#include "core/string/ustring.h"
#include "core/templates/local_vector.h"

// NvFlow C API (loaded entirely at runtime through NvFlowLoader -- nothing
// links against nvflow*.lib; see SCsub).
#include "NvFlowContext.h"
#include "NvFlowExt.h"

// Shared NVIDIA Flow 2.2 device runtime: one dynamically-loaded SDK
// (nvflow.dll + nvflowext.dll), one Flow device manager / device / queue /
// context shared by every FlowSimulation in the process, per the SDK's
// intended multi-grid usage (one device per process, many NvFlowGrids).
//
// Lifetime: refcounted. FlowSimulation3D acquires on ENTER_WORLD, releases on
// EXIT_WORLD; the underlying device stays alive across scene reloads so the
// editor does not rebuild a Vulkan device each Play/Stop. The module tears it
// down at SERVERS shutdown (after all scene nodes are gone) with an explicit
// queue waitIdle.
//
// Threading: owned by the main thread. Every Flow call in this integration
// happens on the main thread (flow nodes step from NOTIFICATION_INTERNAL_
// PHYSICS_PROCESS like PhysXGas3D), so no locking is needed around the
// context; the SDK itself may use its internal thread pool for CPU-side work.
class FlowRuntime {
public:
	enum DeviceApiPreference {
		API_AUTO = 0,
		API_VULKAN = 1,
		API_CPU = 2,
	};

	struct Stats {
		String backend_name; // "vulkan", "d3d12", "cpu" or "" when unavailable
		uint32_t simulation_count = 0; // grids currently alive on this runtime
		uint64_t frames_submitted = 0;
		uint64_t last_frame_completed = 0;
		uint64_t device_memory_bytes = 0;
		uint64_t upload_memory_bytes = 0;
		uint64_t readback_memory_bytes = 0;
	};

private:
	int references = 0;

	bool initialized = false;
	bool available = false; // DLLs loaded + interface tables duplicated
	bool device_ready = false; // own-device fallback created (never on the RD path)
	String unavailable_reason;

	NvFlowContextApi active_api = eNvFlowContextApi_cpu;

	// Opaque SDK handles, destroyed in reverse order in _destroy().
	void *loader_storage = nullptr; // NvFlowLoader (header type hidden here)
	NvFlowDeviceManager *device_manager = nullptr;
	NvFlowDevice *device = nullptr;
	NvFlowDeviceQueue *queue = nullptr;
	NvFlowContextOpt *context_opt = nullptr;
	NvFlowThreadPool *thread_pool_storage = nullptr;
	NvFlowThreadPoolInterface thread_pool_interface_storage = {};

	NvFlowContextInterface *ctx_interface = nullptr; // ContextOpt-wrapped
	NvFlowContext *ctx = nullptr;

	NvFlowOpList op_list = {};
	NvFlowExtOpList ext_op_list = {};
	NvFlowGridInterface grid_interface = {};
	NvFlowGridParamsInterface grid_params_interface = {};
	NvFlowContextOptInterface context_opt_interface = {};
	NvFlowDeviceInterface device_interface = {};

	uint64_t frames_submitted = 0;
	uint32_t grid_count = 0;

	static FlowRuntime *singleton;

	bool _init();
	void _destroy();
	bool _ensure_device_locked();
	static void _log_print(NvFlowLogLevel p_level, const char *p_format, ...);
	static void _loader_error(const char *p_str, void *p_userdata);
	// NvFlowLoaderInit* with absolute library paths next to the executable
	// (loader type is hidden here, hence the void *).
	static void _loader_init(void *p_loader, NvFlowContextApi p_api, String *r_reason);

public:
	static FlowRuntime *acquire(); // get-or-create, +1 ref
	void release(); // -1 ref, destroy at 0

	// Module shutdown hook: destroys the device even if a caller forgot to
	// release (with a diagnostic), so an editor restart never inherits a
	// stale GPU device.
	void release_for_shutdown();

	// True once nvflow.dll/nvflowext.dll are loaded and their interface
	// tables duplicated -- grids on Godot's RenderingDevice only need this.
	bool is_available() const { return available; }
	String get_unavailable_reason() const { return unavailable_reason; }

	// Creates the SDK's own Vulkan/CPU device (the fallback path for
	// headless / Compatibility, where no RenderingDevice exists). False,
	// with the reason in get_unavailable_reason(), when that fails too.
	bool ensure_device();
	bool is_device_ready() const { return device_ready; }

	NvFlowContextInterface *context_interface() const { return ctx_interface; }
	NvFlowContext *context() const { return ctx; }
	String get_backend_name() const;
	NvFlowOpList *get_op_list() { return &op_list; }
	NvFlowExtOpList *get_ext_op_list() { return &ext_op_list; }
	NvFlowGridInterface *get_grid_interface() const {
		return const_cast<NvFlowGridInterface *>(&grid_interface);
	}
	NvFlowGridParamsInterface *get_grid_params_interface() {
		return &grid_params_interface;
	}
	NvFlowDeviceInterface *get_device_interface() const {
		return const_cast<NvFlowDeviceInterface *>(&device_interface);
	}

	// Submits all recorded work on the Flow queue. Optional external
	// semaphores are a future interop hook (v1 passes none -- the runtime
	// owns its queue and only CPU readback crosses to Godot).
	void flush(uint64_t *r_flushed_frame_id = nullptr);

	uint64_t get_last_frame_completed() const;
	void wait_idle();

	void grid_created() { grid_count++; }
	void grid_destroyed() { grid_count--; }

	Stats get_stats() const;

	// Process-wide access for diagnostics; null when never created.
	static FlowRuntime *get_singleton() { return singleton; }

	// Public ctor/dtor: instances exist only through acquire()/release(), but
	// memnew/memdelete (free templates) must be able to construct/destroy.
public:
	FlowRuntime() = default;
	~FlowRuntime();
};

#endif // GODOT_PHYSX_FLOW

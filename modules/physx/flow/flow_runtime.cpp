/**************************************************************************/
/*  flow_runtime.cpp                                                      */
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

#include "flow_runtime.h"

#ifdef GODOT_PHYSX_FLOW

#include "core/config/project_settings.h"
#include "core/os/os.h"

#include <stdarg.h>
#include <string.h>

// Windows.h (via NvFlowLoader.h) and Godot's own headers both define
// min/max-adjacent macros; keep the loader include last and isolated in
// this translation unit only -- nothing else in the module includes it.
#include "NvFlowLoader.h"

FlowRuntime *FlowRuntime::singleton = nullptr;

void FlowRuntime::_log_print(NvFlowLogLevel p_level, const char *p_format, ...) {
	char buffer[2048];
	va_list args;
	va_start(args, p_format);
	// The SDK's logPrint contract is printf-style; it formats English
	// diagnostics only. No user-facing strings flow through here.
	vsnprintf(buffer, sizeof(buffer), p_format, args);
	va_end(args);
	// Strip a trailing newline -- Godot's print adds one.
	for (size_t len = strlen(buffer); len > 0 && buffer[len - 1] == '\n'; len--) {
		buffer[len - 1] = '\0';
	}
	// Info spam (per-allocation traces) is gated behind a project setting;
	// errors and warnings always print.
	if (p_level == eNvFlowLogLevel_info) {
		static bool verbose = (bool)GLOBAL_GET("physics/physx_3d/flow/verbose_logs");
		if (!verbose) {
			return;
		}
	}
	const char *prefix = "[Flow] ";
	switch (p_level) {
		case eNvFlowLogLevel_error:
			prefix = "[Flow ERROR] ";
			break;
		case eNvFlowLogLevel_warning:
			prefix = "[Flow WARN] ";
			break;
		default:
			break;
	}
	print_line(String(prefix) + String::utf8(buffer));
}

void FlowRuntime::_loader_error(const char *p_str, void *p_userdata) {
	String *reason = static_cast<String *>(p_userdata);
	print_line(String("[Flow] loader failed: ") + String::utf8(p_str));
	if (reason != nullptr) {
		*reason = String::utf8(p_str);
	}
}

bool FlowRuntime::_init() {
	ERR_FAIL_COND_V(initialized, false);
	initialized = true;

	NvFlowLoader *loader = memnew(NvFlowLoader);
	loader_storage = loader;

	String load_reason;
	NvFlowLoaderInit(loader, _loader_error, &load_reason);
	if (loader->module_nvflow == nullptr || loader->module_nvflowext == nullptr) {
		unavailable_reason = load_reason.is_empty() ? String("nvflow.dll / nvflowext.dll not found next to the executable") : load_reason;
		print_line(vformat("[Flow] runtime unavailable: %s", unavailable_reason));
		NvFlowLoaderDestroy(loader);
		memdelete(loader);
		loader_storage = nullptr;
		return false;
	}

	// Duplicate the interface tables out of the DLLs (ABI-safe copies).
	op_list = loader->opList;
	ext_op_list = loader->extOpList;
	grid_interface = loader->gridInterface;
	context_opt_interface = loader->contextOptInterface;
	device_interface = loader->deviceInterface;
	// The GridParams transaction interface isn't part of NvFlowLoader's
	// struct -- fetch it from the ext DLL directly (same pattern as the
	// thread pool below).
	{
		PFN_NvFlowGetGridParamsInterface get_gpi =
				(PFN_NvFlowGetGridParamsInterface)NvFlowGetProcAddress(loader->module_nvflowext, "NvFlowGetGridParamsInterface");
		if (get_gpi != nullptr) {
			NvFlowGridParamsInterface *gpi = get_gpi();
			if (gpi != nullptr) {
				NvFlowGridParamsInterface_duplicate(&grid_params_interface, gpi);
			}
		}
	}

	available = true;
	print_line("[Flow] NVIDIA Flow 2.2 loaded (grids on Godot's RenderingDevice; SDK device created only on fallback).");
	return true;
}

bool FlowRuntime::_ensure_device_locked() {
	if (device_ready) {
		return true;
	}
	if (!available || loader_storage == nullptr) {
		return false;
	}
	NvFlowLoader *loader = static_cast<NvFlowLoader *>(loader_storage);

	// The Flow device settings live under physics/physx_3d/flow (registered
	// with all module settings at SERVERS init). Read just these three keys
	// here -- GLOBAL_GET falls back to the registered defaults if the runtime
	// is somehow created before registration (e.g. headless tests).
	int api_preference = (int)GLOBAL_GET("physics/physx_3d/flow/device_api");
	if (api_preference < API_AUTO || api_preference > API_CPU) {
		api_preference = API_AUTO;
	}

	const int device_index = (int)GLOBAL_GET("physics/physx_3d/flow/device_index");
#ifdef DEBUG_ENABLED
	const bool validation = (bool)GLOBAL_GET("physics/physx_3d/flow/device_validation");
#else
	const bool validation = false;
#endif

	// Try the preferred API first, then the remaining candidates. AUTO means
	// vulkan then cpu (d3d12 is reserved for a future same-backend fast path;
	// the fallback path only serves headless runs and CPU readback, where
	// vulkan is the useful one).
	NvFlowContextApi candidates[2] = { eNvFlowContextApi_vulkan, eNvFlowContextApi_cpu };
	if (api_preference == API_CPU) {
		candidates[0] = eNvFlowContextApi_cpu;
		candidates[1] = eNvFlowContextApi_vulkan;
	}

	String fail_reason;
	for (int attempt = 0; attempt < 2; attempt++) {
		const NvFlowContextApi api = candidates[attempt];
		if (api_preference == API_VULKAN && api == eNvFlowContextApi_cpu) {
			break; // explicit vulkan preference: no silent fallback
		}

		String attempt_reason;
		NvFlowLoaderInitDeviceAPI(loader, _loader_error, &attempt_reason, api);
		if (loader->module_nvflow == nullptr || loader->module_nvflowext == nullptr) {
			fail_reason = attempt_reason;
			break;
		}

		// Re-duplicate the interface tables for this API (the device
		// interface export differs per API; the rest are stable).
		op_list = loader->opList;
		ext_op_list = loader->extOpList;
		grid_interface = loader->gridInterface;
		context_opt_interface = loader->contextOptInterface;
		device_interface = loader->deviceInterface;

		if (device_interface.createDeviceManager == nullptr) {
			fail_reason = "NvFlowDeviceInterface incomplete";
			continue;
		}

		// The device manager runs CPU-side SDK work on this pool (mandatory
		// for the CPU context backend, useful elsewhere). Fetched straight
		// from the ext DLL -- NvFlowLoader doesn't carry this interface.
		NvFlowThreadPool *thread_pool = nullptr;
		PFN_NvFlowThreadPoolInterface get_tp_interface =
				(PFN_NvFlowThreadPoolInterface)NvFlowGetProcAddress(loader->module_nvflowext, "NvFlowGetThreadPoolInterface");
		NvFlowThreadPoolInterface *tp_interface = nullptr;
		if (get_tp_interface != nullptr) {
			tp_interface = get_tp_interface();
			if (tp_interface != nullptr && tp_interface->create != nullptr) {
				thread_pool = tp_interface->create(tp_interface->getDefaultThreadCount(), 0);
				thread_pool_storage = thread_pool;
				thread_pool_interface_storage = *tp_interface;
			}
		}

		device_manager = device_interface.createDeviceManager(
				validation ? NV_FLOW_TRUE : NV_FLOW_FALSE,
				tp_interface,
				0);
		if (device_manager == nullptr) {
			fail_reason = "createDeviceManager failed";
			if (thread_pool != nullptr && tp_interface != nullptr) {
				tp_interface->destroy(thread_pool);
				thread_pool_storage = nullptr;
			}
			continue;
		}

		// Resolve the device index up front: fall back to 0 when requested
		// index does not exist so createDevice never gets a stale number.
		NvFlowUint resolved_device_index = (device_index >= 0) ? (NvFlowUint)device_index : 0u;
		NvFlowPhysicalDeviceDesc desc = {};
		if (device_index > 0 && !device_interface.enumerateDevices(device_manager, resolved_device_index, &desc)) {
			WARN_PRINT(vformat("[Flow] device index %d unavailable, using 0.", device_index));
			resolved_device_index = 0u;
		}

		NvFlowDeviceDesc device_desc = {};
		device_desc.deviceIndex = resolved_device_index;
		device_desc.enableExternalUsage = NV_FLOW_FALSE; // fallback only: CPU readback, no external interop
		device_desc.logPrint = &_log_print;
		device = device_interface.createDevice(device_manager, &device_desc);
		if (device == nullptr) {
			fail_reason = vformat("createDevice(%s) failed", api == eNvFlowContextApi_vulkan ? "vulkan" : "cpu");
			device_interface.destroyDeviceManager(device_manager);
			device_manager = nullptr;
			if (thread_pool != nullptr && tp_interface != nullptr) {
				tp_interface->destroy(thread_pool);
				thread_pool_storage = nullptr;
			}
			continue;
		}

		queue = device_interface.getDeviceQueue(device);
		if (queue == nullptr) {
			fail_reason = "getDeviceQueue failed";
			device_interface.destroyDevice(device_manager, device);
			device = nullptr;
			device_interface.destroyDeviceManager(device_manager);
			device_manager = nullptr;
			// FLOW-5: this branch (and getContext below) used to leak the
			// thread pool created above -- destroy it like the earlier
			// failure branches do.
			if (thread_pool != nullptr && tp_interface != nullptr) {
				tp_interface->destroy(thread_pool);
				thread_pool_storage = nullptr;
			}
			continue;
		}

		// Wrap the backend context in the optimization layer (transient
		// resource reuse/caching). Grids are created and destroyed against
		// this wrapped context; it is torn down before the backend device.
		NvFlowContextInterface *backend_ctx_interface = device_interface.getContextInterface(queue);
		NvFlowContext *backend_ctx = device_interface.getContext(queue);
		if (backend_ctx_interface == nullptr || backend_ctx == nullptr) {
			fail_reason = "getContext failed";
			device_interface.destroyDevice(device_manager, device);
			device = nullptr;
			device_interface.destroyDeviceManager(device_manager);
			device_manager = nullptr;
			// FLOW-5: same thread-pool leak as the getDeviceQueue branch.
			if (thread_pool != nullptr && tp_interface != nullptr) {
				tp_interface->destroy(thread_pool);
				thread_pool_storage = nullptr;
			}
			continue;
		}
		context_opt = context_opt_interface.create(backend_ctx_interface, backend_ctx);
		context_opt_interface.getContext(context_opt, &ctx_interface, &ctx);

		active_api = api;
		device_ready = true;
		print_line(vformat("[Flow] NVIDIA Flow 2.2 fallback device initialized (backend=%s, deviceIndex=%d).",
				api == eNvFlowContextApi_vulkan ? "vulkan" : (api == eNvFlowContextApi_d3d12 ? "d3d12" : "cpu"),
				device_desc.deviceIndex));
		return true;
	}

	unavailable_reason = fail_reason.is_empty() ? String("no Flow fallback device could be created") : fail_reason;
	print_line(vformat("[Flow] fallback device unavailable: %s", unavailable_reason));
	return false;
}

bool FlowRuntime::ensure_device() {
	return _ensure_device_locked();
}

String FlowRuntime::get_backend_name() const {
	if (device_ready) {
		switch (active_api) {
			case eNvFlowContextApi_vulkan:
				return "vulkan";
			case eNvFlowContextApi_d3d12:
				return "d3d12";
			case eNvFlowContextApi_cpu:
				return "cpu";
			default:
				break;
		}
	}
	return String();
}

void FlowRuntime::_destroy() {
	if (loader_storage == nullptr) {
		return;
	}
	if (device_ready) {
		// No grids may outlive the runtime (each FlowSimulation releases its
		// grid in its own destructor while holding a runtime reference).
		if (grid_count != 0) {
			ERR_PRINT(vformat("[Flow] %d grid(s) still alive at runtime shutdown; forcing queue idle first.", grid_count));
		}
		device_interface.waitIdle(queue);
		if (context_opt != nullptr) {
			context_opt_interface.destroy(context_opt);
			context_opt = nullptr;
		}
		ctx_interface = nullptr;
		ctx = nullptr;
		queue = nullptr;
		if (device != nullptr) {
			device_interface.destroyDevice(device_manager, device);
			device = nullptr;
		}
		if (device_manager != nullptr) {
			device_interface.destroyDeviceManager(device_manager);
			device_manager = nullptr;
		}
		if (thread_pool_storage != nullptr && thread_pool_interface_storage.destroy != nullptr) {
			thread_pool_interface_storage.destroy(thread_pool_storage);
			thread_pool_storage = nullptr;
		}
		device_ready = false;
		print_line("[Flow] fallback device shut down.");
	}
	NvFlowLoader *loader = static_cast<NvFlowLoader *>(loader_storage);
	NvFlowLoaderDestroy(loader);
	memdelete(loader);
	loader_storage = nullptr;
	available = false;
	print_line("[Flow] runtime shut down.");
}

FlowRuntime *FlowRuntime::acquire() {
	if (singleton == nullptr) {
		singleton = memnew(FlowRuntime);
		singleton->_init();
	}
	singleton->references++;
	return singleton;
}

void FlowRuntime::release() {
	ERR_FAIL_NULL(singleton);
	ERR_FAIL_COND(this != singleton);
	references--;
	if (references <= 0) {
		if (references < 0) {
			ERR_PRINT("[Flow] FlowRuntime over-released.");
		}
		_destroy();
		memdelete(this);
		singleton = nullptr;
	}
}

void FlowRuntime::release_for_shutdown() {
	if (singleton == nullptr) {
		return;
	}
	if (singleton->references > 0) {
		ERR_PRINT(vformat("[Flow] FlowRuntime still has %d reference(s) at module shutdown; forcing teardown.", singleton->references));
	}
	singleton->_destroy();
	memdelete(singleton);
	singleton = nullptr;
}

void FlowRuntime::flush(uint64_t *r_flushed_frame_id) {
	if (!device_ready) {
		if (r_flushed_frame_id != nullptr) {
			*r_flushed_frame_id = 0;
		}
		return;
	}
	// Passes recorded through the ContextOpt wrapper (ours AND the grids')
	// sit in the opt layer's batch cache until THIS flush runs -- without it
	// the device queue flush submits nothing new and copy passes recorded
	// on the context never execute (observed: readback buffers stayed zero).
	if (context_opt != nullptr && context_opt_interface.flush != nullptr) {
		context_opt_interface.flush(context_opt);
	}
	// NvFlowUint64 (unsigned long long) is a distinct type from uint64_t
	// (unsigned long) on gcc/clang Linux; the device flush wants exactly it.
	NvFlowUint64 flushed = 0;
	device_interface.flush(queue, &flushed, nullptr, nullptr);
	if (flushed != 0) {
		frames_submitted = flushed;
	}
	if (r_flushed_frame_id != nullptr) {
		*r_flushed_frame_id = flushed;
	}
}

uint64_t FlowRuntime::get_last_frame_completed() const {
	if (!device_ready || queue == nullptr) {
		return 0;
	}
	return device_interface.getLastFrameCompleted(queue);
}

void FlowRuntime::wait_idle() {
	if (!device_ready || queue == nullptr) {
		return;
	}
	device_interface.waitIdle(queue);
}

FlowRuntime::Stats FlowRuntime::get_stats() const {
	Stats stats;
	if (!device_ready) {
		return stats;
	}
	switch (active_api) {
		case eNvFlowContextApi_vulkan:
			stats.backend_name = "vulkan";
			break;
		case eNvFlowContextApi_d3d12:
			stats.backend_name = "d3d12";
			break;
		case eNvFlowContextApi_cpu:
			stats.backend_name = "cpu";
			break;
		default:
			break;
	}
	stats.simulation_count = grid_count;
	stats.frames_submitted = frames_submitted;
	stats.last_frame_completed = get_last_frame_completed();
	if (device != nullptr && device_interface.getMemoryStats != nullptr) {
		NvFlowDeviceMemoryStats mem = {};
		device_interface.getMemoryStats(device, &mem);
		stats.device_memory_bytes = mem.deviceMemoryBytes;
		stats.upload_memory_bytes = mem.uploadMemoryBytes;
		stats.readback_memory_bytes = mem.readbackMemoryBytes;
	}
	return stats;
}

FlowRuntime::~FlowRuntime() {
	_destroy();
}

#endif // GODOT_PHYSX_FLOW

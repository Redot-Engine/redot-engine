/**************************************************************************/
/*  physx_gpu_dispatch_stub.cpp                                           */
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

// Inert stand-ins for the two GPU-dispatch entry points the vendored CPU
// static libraries reference (verified by symbol audit against the vendored
// archives):
//   - PxCreatePhysXGpu               (libPhysX_static_64.a)
//   - PxSetPhysXGpuProfilerCallback  (libPhysXPvdSDK_static_64.a)
// Only NVIDIA's closed libPhysXGpu_64.so runtime defines the real ones. With
// physx_gpu=yes that runtime satisfies them at link time (see the module
// SCsub). With physx_gpu=no the runtime is neither linked nor staged, so the
// stand-ins below keep the link resolving.
//
// They are unreachable in that configuration: GODOT_PHYSX_GPU is undefined,
// so PhysXServer3D::init() never creates a CUDA context and no GPU-enabled
// PxScene exists that would call into the GPU module.
//
// Windows always links the PhysXGpu_64 import library and never needs this.

#include "foundation/PxPreprocessor.h"

#if defined(_WIN32) || defined(GODOT_PHYSX_GPU)
// Real entry points come from the PhysXGpu runtime (import library on
// Windows, linked .so on Linux). PX_DUMMY_SYMBOL keeps the TU non-empty
// under MSVC (LNK4221); it expands to nothing on other toolchains.
PX_DUMMY_SYMBOL
#else
namespace physx {
class PxPhysXGpu;
class PxProfilerCallback;
} //namespace physx

// Signature per the SDK's own loader typedef (PxCreatePhysXGpu_FUNC in
// lowlevel/api/src/px_globals.cpp): no parameters, returns the GPU module
// instance. Returning null keeps PhysX core on its CPU paths.
extern "C" physx::PxPhysXGpu *PxCreatePhysXGpu() {
	return nullptr;
}

// Declared publicly in gpu/PxGpu.h when the SDK's GPU headers are active;
// redeclared here so the stub does not depend on that switch.
extern "C" void PxSetPhysXGpuProfilerCallback(physx::PxProfilerCallback *p_profiler_callback) {
	(void)p_profiler_callback;
}
#endif // _WIN32 || GODOT_PHYSX_GPU

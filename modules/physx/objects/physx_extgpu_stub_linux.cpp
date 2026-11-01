/**************************************************************************/
/*  physx_extgpu_stub_linux.cpp                                           */
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

// Linux: the vendored CPU static libraries and NVIDIA's prebuilt GPU runtime
// do not carry ExtGpu's particle+diffuse buffer factory (it ships only in the
// GPU-enabled Windows libraries). Provide an explicit nullptr stub so the GPU
// particle fluid node links and degrades with its own error message instead
// of failing the engine link; GPU rigid-body dynamics is unaffected.

#include "extensions/PxParticleExt.h"

#if defined(_WIN32)
// Windows links the real factory from the GPU-enabled PhysXExtensions lib;
// keep the TU non-empty to avoid a no-public-symbols linker warning.
PX_DUMMY_SYMBOL
#else // !_WIN32
namespace physx {
namespace ExtGpu {

PxParticleAndDiffuseBuffer *PxCreateAndPopulateParticleAndDiffuseBuffer(const PxParticleAndDiffuseBufferDesc &p_desc, PxCudaContextManager *p_cuda_context_manager) {
	(void)p_desc;
	(void)p_cuda_context_manager;
	return nullptr;
}

} // namespace ExtGpu
} // namespace physx
#endif // _WIN32

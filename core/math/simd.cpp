/**************************************************************************/
/*  simd.cpp                                                              */
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

#include "simd.h"

#include <cstdlib>
#include <cstring>

#if defined(GODOT_SIMD_X86)
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

std::atomic<uint32_t> CPUFeatures::features{ 0 };

#ifdef GODOT_SIMD_X86
namespace simd {
GODOT_SIMD_TARGET_FMA f32x4 cubic_interpolate4_fma(f32x4 p_from, f32x4 p_to, f32x4 p_pre, f32x4 p_post, float p_weight) {
	f32x4 w = splat4(p_weight);
	f32x4 w2 = w * w;
	f32x4 w3 = w * w2;
	f32x4 a = -p_pre + p_to;
	f32x4 b = ((splat4(2.0f) * p_pre - splat4(5.0f) * p_from) + splat4(4.0f) * p_to) - p_post;
	f32x4 c = ((-p_pre + splat4(3.0f) * p_from) - splat4(3.0f) * p_to) + p_post;
	__m128 r = _mm_fmadd_ps(a.v, w.v, (splat4(2.0f) * p_from).v);
	r = _mm_fmadd_ps(b.v, w2.v, r);
	r = _mm_fmadd_ps(c.v, w3.v, r);
	return splat4(0.5f) * f32x4(r);
}
} // namespace simd
#endif
#if defined(GODOT_SIMD_X86)
static void _cpuid(uint32_t p_leaf, uint32_t p_subleaf, uint32_t r_regs[4]) {
#if defined(_MSC_VER) && !defined(__clang__)
	int regs[4];
	__cpuidex(regs, (int)p_leaf, (int)p_subleaf);
	r_regs[0] = (uint32_t)regs[0];
	r_regs[1] = (uint32_t)regs[1];
	r_regs[2] = (uint32_t)regs[2];
	r_regs[3] = (uint32_t)regs[3];
#else
	__cpuid_count(p_leaf, p_subleaf, r_regs[0], r_regs[1], r_regs[2], r_regs[3]);
#endif
}

static uint64_t _xgetbv0() {
#if defined(_MSC_VER) && !defined(__clang__)
	return _xgetbv(0);
#else
	uint32_t eax, edx;
	__asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
	return ((uint64_t)edx << 32) | eax;
#endif
}
#endif

static uint32_t _detect_hardware() {
	uint32_t f = CPUFeatures::INITIALIZED;
#if defined(GODOT_SIMD_X86)
	uint32_t regs[4];
	_cpuid(0, 0, regs);
	uint32_t max_leaf = regs[0];
	if (max_leaf >= 1) {
		_cpuid(1, 0, regs);
		const uint32_t ecx = regs[2];
		if (ecx & (1u << 20)) {
			f |= CPUFeatures::SSE42;
		}
		const bool osxsave = (ecx & (1u << 27)) != 0;
		const bool avx = (ecx & (1u << 28)) != 0;
		bool ymm_enabled = false;
		if (osxsave && avx) {
			ymm_enabled = (_xgetbv0() & 0x6) == 0x6;
		}
		if (ymm_enabled) {
			f |= CPUFeatures::AVX;
			if (ecx & (1u << 12)) {
				f |= CPUFeatures::FMA;
			}
			if (ecx & (1u << 29)) {
				f |= CPUFeatures::F16C;
			}
			if (max_leaf >= 7) {
				_cpuid(7, 0, regs);
				if (regs[1] & (1u << 5)) {
					f |= CPUFeatures::AVX2;
				}
			}
		}
	}
#endif
	return f;
}

// Function-local static: initialized exactly once, even when several threads get here first.
static uint32_t _hardware_features() {
	static const uint32_t hardware = _detect_hardware();
	return hardware;
}

uint32_t CPUFeatures::_detect() {
	uint32_t f = _hardware_features();
	// GODOT_SIMD_LEVEL=sse disables every runtime-dispatched extension, =avx keeps only AVX.
#if defined(_MSC_VER)
	// MSVC/clang-cl deprecate getenv under /WX; _dupenv_s is the supported form (frees via the heap).
	char *level = nullptr;
	size_t level_size = 0;
	_dupenv_s(&level, &level_size, "GODOT_SIMD_LEVEL");
#else
	const char *level = std::getenv("GODOT_SIMD_LEVEL");
#endif
	if (level) {
		if (strcmp(level, "sse") == 0 || strcmp(level, "baseline") == 0) {
			f &= ~(AVX | AVX2 | FMA | F16C);
		} else if (strcmp(level, "avx") == 0) {
			f &= ~(AVX2 | FMA | F16C);
		}
	}
#if defined(_MSC_VER)
	free(level);
#endif
	return f;
}

void CPUFeatures::set_override(uint32_t p_mask) {
	features.store((_hardware_features() & p_mask) | INITIALIZED, std::memory_order_relaxed);
}

void CPUFeatures::reset_override() {
	features.store(0, std::memory_order_relaxed);
}

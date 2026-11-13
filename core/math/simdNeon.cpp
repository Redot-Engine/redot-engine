/**************************************************************************/
/*  simdNeon.cpp                                                          */
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

#include <asm/hwcap.h>
#include <sys/auxv.h>

#include "core/math/simdNeon.h"

class CPUFeatures {
public:
	enum Feature {
		CPUFEATURE_INIT,
		CPUFEATURE_SIMD,
		CPUFEATURE_FMADD,
		CPUFEATURE_FP16,
		CPUFEATURE_MAX,
	};
	enum Features : size_t {
		CPUFEATURES_NONE = 0,
		CPUFEATURES_INIT = (1 << CPUFEATURE_INIT),
		CPUFEATURES_SIMD = (1 << CPUFEATURE_SIMD),
		CPUFEATURES_FMADD = (1 << CPUFEATURE_FMADD),
		CPUFEATURES_FP16 = (1 << CPUFEATURE_FP16),
	};

	static Features get();

private:
	static size_t simdBits;
	static Features cpuFeatureFlags;
};

CPUFeatures::Features CPUFeatures::cpuFeatureFlags = CPUFeatures::Features::CPUFEATURES_NONE;
size_t CPUFeatures::simdBits = 0;
// TODO(Victor Sohier): Make this work on Windows
#if defined(__aarch64__)
CPUFeatures::Features CPUFeatures::get() {
	unsigned long val;
	simdBits = 128;
	if (!(cpuFeatureFlags & CPUFeatures::Features::CPUFEATURES_INIT)) {
		val = getauxval(AT_HWCAP);
		if (val & HWCAP_ASIMD) {
			cpuFeatureFlags = (CPUFeatures::Features)(cpuFeatureFlags | (CPUFeatures::Features::CPUFEATURES_SIMD));
		}
		// This combination of flags tells you if you have FMADD
		if ((val & (HWCAP_FP | HWCAP_ASIMD)) == (HWCAP_FP | HWCAP_ASIMD)) {
			cpuFeatureFlags = (CPUFeatures::Features)(cpuFeatureFlags | (CPUFeatures::Features::CPUFEATURES_FMADD));
		}

		if ((val & (HWCAP_FPHP | HWCAP_ASIMDHP | HWCAP_ASIMDFHM)) == (HWCAP_FPHP | HWCAP_ASIMDHP | HWCAP_ASIMDFHM)) {
			cpuFeatureFlags = (CPUFeatures::Features)(cpuFeatureFlags | (CPUFeatures::Features::CPUFEATURES_FMADD));
		}

		cpuFeatureFlags = (CPUFeatures::Features)(cpuFeatureFlags | (CPUFeatures::Features::CPUFEATURES_INIT));
	}
	return cpuFeatureFlags;
}
#elif defined(__arm__)
CPUFeatures::Features CPUFeatures::get() {
	unsigned long val;
	simdBits = 128;
	if (!(CPUFeatures::cpuFeatureFlags & CPUFeatures::Features::CPUFEATURES_INIT)) {
		val = getauxval(AT_HWCAP);
		if (val & HWCAP_NEON) {
			cpuFeatureFlags = (CPUFeatures::Features)(cpuFeatureFlags | (CPUFeatures::Features::CPUFEATURES_SIMD));
		}
		// This combination of flags tells you if you have FMADD
		if ((val & (HWCAP_VFPv3 | HWCAP_VFPv4)) == (HWCAP_VFPv3 | HWCAP_VFPv4)) {
			cpuFeatureFlags = (CPUFeatures::Features)(cpuFeatureFlags | (CPUFeatures::Features::CPUFEATURES_FMADD));
		}

		if ((val & (HWCAP_FPHP | HWCAP_ASIMDHP | HWCAP_ASIMDFHM)) == (HWCAP_FPHP | HWCAP_ASIMDHP | HWCAP_ASIMDFHM)) {
			cpuFeatureFlags = (CPUFeatures::Features)(cpuFeatureFlags | (CPUFeatures::Features::CPUFEATURES_FMADD));
		}

		cpuFeatureFlags = (CPUFeatures::Features)(cpuFeatureFlags | (CPUFeatures::Features::CPUFEATURES_INIT));
	}
	return CPUFeatures::cpuFeatureFlags;
}
#endif

#ifdef GODOT_SIMD_NEON

namespace simd {

f32x4 cubic_interpolate4_fma(
		f32x4 p_from,
		f32x4 p_to,
		f32x4 p_pre,
		f32x4 p_post,
		float p_weight) {
	f32x4 w = splat4(p_weight);
	f32x4 w2 = w * w;
	f32x4 w3 = w * w2;
	f32x4 a = -p_pre + p_to;
	f32x4 b = ((splat4(2.0f) * p_pre - splat4(5.0f) * p_from) + splat4(4.0f) * p_to) - p_post;
	f32x4 c = ((-p_pre + splat4(3.0f) * p_from) - splat4(3.0f) * p_to) + p_post;
	f32x4 r = vfmaq_f32(a, w, (splat4(2) * p_from));
	r = vfmaq_f32(b, w2, r);
	r = vfmaq_f32(c, w3, r);
	return splat4(0.5f) * r;
}

} //namespace simd
#endif

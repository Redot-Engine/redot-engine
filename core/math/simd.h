/**************************************************************************/
/*  simd.h                                                                */
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

#include <stddef.h>
#include <stdint.h>

#if defined(__amd64__) || defined(__i386__)
#include "core/math/simdx86.h"
#elif defined(__arm__) || defined(__aarch64__)
#include "core/math/simdNeon.h"
#endif

// You *will* make this interface architecture agnostic
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
		CPUFEATURES_ALL = SIZE_MAX,
	};

	static Features get();

private:
	static size_t simdBits;
	static Features cpuFeatureFlags;

public:
	static _ALWAYS_INLINE_ bool has(Features p_features) {
		return (get() & p_features) == p_features;
	}

	static _ALWAYS_INLINE_ size_t getSimdWidth() {
		get();
		return CPUFeatures::simdBits;
	}

	static _ALWAYS_INLINE_ void reset_override() {
		CPUFeatures::cpuFeatureFlags = CPUFEATURES_NONE;
		CPUFeatures::simdBits = 0;
	}

	static _ALWAYS_INLINE_ void set_override(CPUFeatures::Features p_mask, size_t width) {
		reset_override();
		Features features = get();
		CPUFeatures::cpuFeatureFlags = (CPUFeatures::Features)(p_mask & features);
		CPUFeatures::simdBits = MIN(CPUFeatures::simdBits, width);
	}
};

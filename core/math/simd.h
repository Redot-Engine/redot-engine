#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(__amd64__) || defined(__i386__)
#include "core/math/simdx86.h"
#elif defined(__arm__) || defined(__aarch64__)
#include "core/math/simdNeon.h"
#endif

// You *will* make this interface architecture agnostic
class CPUFeatures
{
public:
	enum Feature
	{
		CPUFEATURE_INIT,
		CPUFEATURE_SIMD,
		CPUFEATURE_FMADD,
		CPUFEATURE_FP16,
		CPUFEATURE_MAX,
	};
	enum Features : size_t
	{
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
	static _ALWAYS_INLINE_ bool has(Features p_features)
	{
		return (get() & p_features) == p_features;
	}

	static _ALWAYS_INLINE_ size_t getSimdWidth()
	{
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

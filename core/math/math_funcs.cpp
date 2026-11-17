/**************************************************************************/
/*  math_funcs.cpp                                                        */
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
 * @file math_funcs.cpp
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "math_funcs.h"

#include "core/error/error_macros.h"
#include "core/math/random_pcg.h"

static RandomPCG default_rand;

uint32_t Math::rand_from_seed(uint64_t *p_seed) noexcept {
	// TODO: mcdubh - Rework core/math/random_pcg.h
	RandomPCG rng = RandomPCG(*p_seed);
	uint32_t r = rng.rand();
	*p_seed = rng.get_seed();
	return r;
}

void Math::seed(uint64_t p_value) noexcept {
	default_rand.seed(p_value);
}

void Math::randomize() noexcept {
	default_rand.randomize();
}

uint32_t Math::rand() noexcept {
	return default_rand.rand();
}

double Math::randfn(double p_mean, double p_deviation) noexcept {
	return default_rand.randfn(p_mean, p_deviation);
}

int Math::step_decimals(double p_step) noexcept {
	static constexpr int maxn = 10;
	static constexpr double sd[maxn] = {
		0.9999, // somehow compensate for floating point error
		0.09999,
		0.009999,
		0.0009999,
		0.00009999,
		0.000009999,
		0.0000009999,
		0.00000009999,
		0.000000009999,
		0.0000000009999
	};

	double int_part;
	double decs = Math::modf_with_ptr(Math::abs(p_step), &int_part); // strip int part.

	for (int i = 0; i < maxn; i++) {
		if (decs >= sd[i]) {
			return i;
		}
	}

	return 0;
}

// Only meant for editor usage in float ranges, where a step of 0
// means that decimal digits should not be limited in String::num.
int Math::range_step_decimals(double p_step) noexcept {
	if (p_step < 1e-13) {
		return 16; // Max value hardcoded in String::num
	}
	return step_decimals(p_step);
}

double Math::ease(double p_x, double p_c) noexcept {
	// clamp p_x to [0,1]
	if (p_x < 0) {
		p_x = 0;
	} else if (p_x > 1.0) {
		p_x = 1.0;
	}

	// No ease (raw)
	if (p_c == 0.0) {
		return 0;
	}

	// Ease-out / ease-in
	if (p_c > 0.0) {
		if (p_c < 1.0) {
			return 1.0 - Math::pow(1.0 - p_x, 1.0 / p_c);
		}
		return Math::pow(p_x, p_c);
	}

	// In-out ease
	if (p_x < 0.5) {
		return Math::pow(p_x * 2.0, -p_c) * 0.5;
	}

	return (1.0 - Math::pow(1.0 - (p_x - 0.5) * 2.0, -p_c)) * 0.5 + 0.5;
}

double Math::snapped(double p_value, double p_step) noexcept {
	if (p_step != 0.0) {
		p_value = Math::floor(p_value / p_step + 0.5) * p_step;
	}
	return p_value;
}

uint32_t Math::larger_prime(uint32_t p_val) noexcept {
	static const uint32_t primes[] = {
		5,
		13,
		23,
		47,
		97,
		193,
		389,
		769,
		1543,
		3079,
		6151,
		12289,
		24593,
		49157,
		98317,
		196613,
		393241,
		786433,
		1572869,
		3145739,
		6291469,
		12582917,
		25165843,
		50331653,
		100663319,
		201326611,
		402653189,
		805306457,
		1610612741,
		0,
	};

	for (auto prime : primes) {
		if (prime == 0) {
			// sentinel value at the end there..
			break;
		}
		if (prime > p_val) {
			return prime;
		}
	}

	// No larger prime in the table
	return 0;
}

double Math::random(double p_from, double p_to) noexcept {
	return default_rand.random(p_from, p_to);
}

float Math::random(float p_from, float p_to) noexcept {
	return default_rand.random(p_from, p_to);
}

int Math::random(int p_from, int p_to) noexcept {
	return default_rand.random(p_from, p_to);
}

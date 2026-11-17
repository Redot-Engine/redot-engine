/**************************************************************************/
/*  audio_mix_kernels.h                                                   */
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

/**
 * @file audio_mix_kernels.h
 *
 * Per-buffer mixing loops used by the audio server and effects. Each kernel has a
 * scalar form (the reference) and SSE forms, plus AVX forms selected at runtime,
 * that produce bit-identical output.
 */

#include "core/math/audio_frame.h"
#include "core/math/simd.h"
#include "servers/audio/effects/eq_filter.h"

namespace AudioMixKernels {

/// Converts stereo frames to the int32 output format: clamp to [-1, 1], scale to 20 bits, then
/// shift the magnitude up by 11 bits keeping the sign.
_FORCE_INLINE_ void convert_stereo_to_int32(const AudioFrame *p_src, int32_t *p_dst, int p_frames) {
	int j = 0;
#ifdef GODOT_SIMD_F32X4
	{
		using namespace simd;
		const f32x4 lo = splat4(-1.0f);
		const f32x4 hi = splat4(1.0f);
		const f32x4 scale = splat4((float)((1 << 20) - 1));
		for (; j + 2 <= p_frames; j += 2) {
			f32x4 v = clamp4(load4(&p_src[j].left), lo, hi) * scale;
			__m128i vi = _mm_cvttps_epi32(v.v);
			__m128i sign = _mm_srai_epi32(vi, 31);
			__m128i mag = _mm_sub_epi32(_mm_xor_si128(vi, sign), sign);
			mag = _mm_slli_epi32(mag, 11);
			_mm_storeu_si128((__m128i *)&p_dst[j * 2], _mm_sub_epi32(_mm_xor_si128(mag, sign), sign));
		}
	}
#endif
	for (; j < p_frames; j++) {
		float l = CLAMP(p_src[j].left, -1.0, 1.0);
		int32_t vl = l * ((1 << 20) - 1);
		p_dst[j * 2] = (vl < 0 ? -1 : 1) * (Math::abs(vl) << 11);
		float r = CLAMP(p_src[j].right, -1.0, 1.0);
		int32_t vr = r * ((1 << 20) - 1);
		p_dst[j * 2 + 1] = (vr < 0 ? -1 : 1) * (Math::abs(vr) << 11);
	}
}

/// buf[j] *= volume, tracking the peak of |sample| per channel (`l > peak ? l : peak`).
_FORCE_INLINE_ void apply_volume_and_peak_scalar(AudioFrame *p_buf, uint32_t p_from, uint32_t p_frames, float p_volume, float &r_peak_l, float &r_peak_r) {
	for (uint32_t j = p_from; j < p_frames; j++) {
		p_buf[j] *= p_volume;
		float l = Math::abs(p_buf[j].left);
		if (l > r_peak_l) {
			r_peak_l = l;
		}
		float r = Math::abs(p_buf[j].right);
		if (r > r_peak_r) {
			r_peak_r = r;
		}
	}
}

#ifdef GODOT_SIMD_AVX_DISPATCH
GODOT_SIMD_TARGET_AVX static void apply_volume_and_peak_avx(AudioFrame *p_buf, uint32_t p_frames, float p_volume, AudioFrame &r_peak) {
	__m256 vol = _mm256_set1_ps(p_volume);
	__m256 peak = _mm256_setzero_ps();
	const __m256 abs_mask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7FFFFFFF));
	uint32_t j = 0;
	for (; j + 4 <= p_frames; j += 4) {
		__m256 v = _mm256_mul_ps(_mm256_loadu_ps(&p_buf[j].left), vol);
		_mm256_storeu_ps(&p_buf[j].left, v);
		peak = _mm256_max_ps(_mm256_and_ps(v, abs_mask), peak);
	}
	// Even lanes are left, odd lanes right. maxps(a, b) is `a > b ? a : b`, like the scalar loop.
	const __m128 peak_lo = _mm256_castps256_ps128(peak);
	const __m128 peak_hi = _mm256_extractf128_ps(peak, 1);
	// MSVC without /arch:AVX does not insert this itself; the SSE code below would then pay the AVX-SSE transition.
	_mm256_zeroupper();
	__m128 p = _mm_max_ps(peak_lo, peak_hi);
	p = _mm_max_ps(_mm_movehl_ps(p, p), p);
	float pl = _mm_cvtss_f32(p);
	float pr = _mm_cvtss_f32(_mm_shuffle_ps(p, p, _MM_SHUFFLE(1, 1, 1, 1)));
	apply_volume_and_peak_scalar(p_buf, j, p_frames, p_volume, pl, pr);
	r_peak = AudioFrame(pl, pr);
}

GODOT_SIMD_TARGET_AVX static void add_frames_avx(AudioFrame *p_dst, const AudioFrame *p_src, uint32_t p_frames) {
	uint32_t j = 0;
	for (; j + 4 <= p_frames; j += 4) {
		_mm256_storeu_ps(&p_dst[j].left, _mm256_add_ps(_mm256_loadu_ps(&p_dst[j].left), _mm256_loadu_ps(&p_src[j].left)));
	}
	_mm256_zeroupper();
	for (; j < p_frames; j++) {
		p_dst[j] += p_src[j];
	}
}

GODOT_SIMD_TARGET_AVX static void mix_ramped_avx(AudioFrame *p_out, const AudioFrame *p_src, AudioFrame p_vol_start, AudioFrame p_vol_final, uint32_t p_frames) {
	const __m256 frames = _mm256_set1_ps((float)p_frames);
	const __m256 one = _mm256_set1_ps(1.0f);
	const __m256 vol_start = _mm256_setr_ps(p_vol_start.left, p_vol_start.right, p_vol_start.left, p_vol_start.right, p_vol_start.left, p_vol_start.right, p_vol_start.left, p_vol_start.right);
	const __m256 vol_final = _mm256_setr_ps(p_vol_final.left, p_vol_final.right, p_vol_final.left, p_vol_final.right, p_vol_final.left, p_vol_final.right, p_vol_final.left, p_vol_final.right);
	const __m256i idx_ofs = _mm256_setr_epi32(0, 0, 1, 1, 2, 2, 3, 3);
	uint32_t i = 0;
	for (; i + 4 <= p_frames; i += 4) {
		// (float)(i + k) exactly, as in the scalar loop; adding in float would round above 2^24.
		const __m128i i_lo = _mm_add_epi32(_mm_set1_epi32((int)i), _mm256_castsi256_si128(idx_ofs));
		const __m128i i_hi = _mm_add_epi32(_mm_set1_epi32((int)i), _mm256_extractf128_si256(idx_ofs, 1));
		__m256 t = _mm256_div_ps(_mm256_cvtepi32_ps(_mm256_insertf128_si256(_mm256_castsi128_si256(i_lo), i_hi, 1)), frames);
		__m256 vol = _mm256_add_ps(_mm256_mul_ps(vol_final, t), _mm256_mul_ps(vol_start, _mm256_sub_ps(one, t)));
		_mm256_storeu_ps(&p_out[i].left, _mm256_add_ps(_mm256_loadu_ps(&p_out[i].left), _mm256_mul_ps(vol, _mm256_loadu_ps(&p_src[i].left))));
	}
	_mm256_zeroupper();
	for (; i < p_frames; i++) {
		float lerp_param = (float)i / p_frames;
		p_out[i] += (p_vol_final * lerp_param + (1 - lerp_param) * p_vol_start) * p_src[i];
	}
}
#endif // GODOT_SIMD_AVX_DISPATCH

_FORCE_INLINE_ void apply_volume_and_peak(AudioFrame *p_buf, uint32_t p_frames, float p_volume, AudioFrame &r_peak) {
#ifdef GODOT_SIMD_AVX_DISPATCH
	if (CPUFeatures::has_avx()) {
		apply_volume_and_peak_avx(p_buf, p_frames, p_volume, r_peak);
		return;
	}
#endif
	float pl = 0.0f;
	float pr = 0.0f;
	uint32_t j = 0;
#ifdef GODOT_SIMD_F32X4
	{
		using namespace simd;
		f32x4 vol = splat4(p_volume);
		f32x4 peak = zero4();
		for (; j + 2 <= p_frames; j += 2) {
			f32x4 v = load4(&p_buf[j].left) * vol;
			store4(&p_buf[j].left, v);
			peak = max4(abs4(v), peak);
		}
		pl = get_lane<0>(peak);
		pr = get_lane<1>(peak);
		pl = get_lane<2>(peak) > pl ? get_lane<2>(peak) : pl;
		pr = get_lane<3>(peak) > pr ? get_lane<3>(peak) : pr;
	}
#endif
	apply_volume_and_peak_scalar(p_buf, j, p_frames, p_volume, pl, pr);
	r_peak = AudioFrame(pl, pr);
}

_FORCE_INLINE_ void add_frames(AudioFrame *p_dst, const AudioFrame *p_src, uint32_t p_frames) {
#ifdef GODOT_SIMD_AVX_DISPATCH
	if (CPUFeatures::has_avx()) {
		add_frames_avx(p_dst, p_src, p_frames);
		return;
	}
#endif
	uint32_t j = 0;
#ifdef GODOT_SIMD_F32X4
	{
		using namespace simd;
		for (; j + 2 <= p_frames; j += 2) {
			store4(&p_dst[j].left, load4(&p_dst[j].left) + load4(&p_src[j].left));
		}
	}
#endif
	for (; j < p_frames; j++) {
		p_dst[j] += p_src[j];
	}
}

/// out[i] += (vol_final * t + (1 - t) * vol_start) * src[i] with t = (float)i / frames.
_FORCE_INLINE_ void mix_ramped(AudioFrame *p_out, const AudioFrame *p_src, AudioFrame p_vol_start, AudioFrame p_vol_final, uint32_t p_frames) {
#ifdef GODOT_SIMD_AVX_DISPATCH
	if (CPUFeatures::has_avx()) {
		mix_ramped_avx(p_out, p_src, p_vol_start, p_vol_final, p_frames);
		return;
	}
#endif
	uint32_t i = 0;
#ifdef GODOT_SIMD_F32X4
	{
		using namespace simd;
		const f32x4 frames = splat4((float)p_frames);
		const f32x4 one = splat4(1.0f);
		const f32x4 vol_start = load2x2(&p_vol_start.left);
		const f32x4 vol_final = load2x2(&p_vol_final.left);
		for (; i + 2 <= p_frames; i += 2) {
			f32x4 t = set4((float)i, (float)i, (float)(i + 1), (float)(i + 1)) / frames;
			f32x4 vol = vol_final * t + vol_start * (one - t);
			store4(&p_out[i].left, load4(&p_out[i].left) + vol * load4(&p_src[i].left));
		}
	}
#endif
	for (; i < p_frames; i++) {
		float lerp_param = (float)i / p_frames;
		p_out[i] += (p_vol_final * lerp_param + (1 - lerp_param) * p_vol_start) * p_src[i];
	}
}

/// Runs an EQ over a buffer. Four bands per lane with the band state kept in SoA form; every band
/// sees the same input and the per-band filter is the EQ::BandProcess::process_one expression.
struct EQProcess {
	static void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count, EQ::BandProcess *p_proc_l, EQ::BandProcess *p_proc_r, const float *p_gain, int p_band_count) {
#ifdef GODOT_SIMD_F32X4
		if (p_band_count >= 4 && p_band_count <= 32 && p_frame_count > 0) {
			process_simd(p_src_frames, p_dst_frames, p_frame_count, p_proc_l, p_proc_r, p_gain, p_band_count);
			return;
		}
#endif
		for (int i = 0; i < p_frame_count; i++) {
			AudioFrame src = p_src_frames[i];
			AudioFrame dst = AudioFrame(0, 0);

			for (int j = 0; j < p_band_count; j++) {
				float l = src.left;
				float r = src.right;

				p_proc_l[j].process_one(l);
				p_proc_r[j].process_one(r);

				dst.left += l * p_gain[j];
				dst.right += r * p_gain[j];
			}

			p_dst_frames[i] = dst;
		}
	}

#ifdef GODOT_SIMD_F32X4
	struct GroupSoA {
		simd::f32x4 c1, c2, c3;
		simd::f32x4 a2, a3, b2, b3;
		simd::f32x4 gain;
	};

	static void process_simd(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count, EQ::BandProcess *p_proc_l, EQ::BandProcess *p_proc_r, const float *p_gain, int p_band_count) {
		using namespace simd;
		constexpr int MAX_GROUPS = 8;
		const int groups = p_band_count / 4;
		const int vector_bands = groups * 4;
		GroupSoA soa[2][MAX_GROUPS];
		EQ::BandProcess *procs[2] = { p_proc_l, p_proc_r };
		for (int ch = 0; ch < 2; ch++) {
			for (int g = 0; g < groups; g++) {
				float c1[4], c2[4], c3[4], a2[4], a3[4], b2[4], b3[4];
				for (int l = 0; l < 4; l++) {
					const EQ::BandProcess &bp = procs[ch][g * 4 + l];
					c1[l] = bp.c1;
					c2[l] = bp.c2;
					c3[l] = bp.c3;
					a2[l] = bp.history.a2;
					a3[l] = bp.history.a3;
					b2[l] = bp.history.b2;
					b3[l] = bp.history.b3;
				}
				GroupSoA &s = soa[ch][g];
				s.c1 = load4(c1);
				s.c2 = load4(c2);
				s.c3 = load4(c3);
				s.a2 = load4(a2);
				s.a3 = load4(a3);
				s.b2 = load4(b2);
				s.b3 = load4(b3);
				s.gain = load4(&p_gain[g * 4]);
			}
		}

		for (int i = 0; i < p_frame_count; i++) {
			AudioFrame src = p_src_frames[i];
			AudioFrame dst = AudioFrame(0, 0);
			float weighted[2][MAX_GROUPS * 4];

			for (int ch = 0; ch < 2; ch++) {
				f32x4 a1 = splat4(src.levels[ch]);
				for (int g = 0; g < groups; g++) {
					GroupSoA &s = soa[ch][g];
					f32x4 b1 = (s.c1 * (a1 - s.a3) + s.c3 * s.b2) - s.c2 * s.b3;
					s.a3 = s.a2;
					s.a2 = a1;
					s.b3 = s.b2;
					s.b2 = b1;
					store4(&weighted[ch][g * 4], b1 * s.gain);
				}
			}
			// The accumulation must stay in band order.
			for (int j = 0; j < vector_bands; j++) {
				dst.left += weighted[0][j];
				dst.right += weighted[1][j];
			}
			for (int j = vector_bands; j < p_band_count; j++) {
				float l = src.left;
				float r = src.right;
				p_proc_l[j].process_one(l);
				p_proc_r[j].process_one(r);
				dst.left += l * p_gain[j];
				dst.right += r * p_gain[j];
			}

			p_dst_frames[i] = dst;
		}

		for (int ch = 0; ch < 2; ch++) {
			for (int g = 0; g < groups; g++) {
				float a2[4], a3[4], b2[4], b3[4];
				const GroupSoA &s = soa[ch][g];
				store4(a2, s.a2);
				store4(a3, s.a3);
				store4(b2, s.b2);
				store4(b3, s.b3);
				for (int l = 0; l < 4; l++) {
					EQ::BandProcess &bp = procs[ch][g * 4 + l];
					// After process_one, a1 == a2 and b1 == b2.
					bp.history.a1 = a2[l];
					bp.history.a2 = a2[l];
					bp.history.a3 = a3[l];
					bp.history.b1 = b2[l];
					bp.history.b2 = b2[l];
					bp.history.b3 = b3[l];
				}
			}
		}
	}
#endif
};

} // namespace AudioMixKernels

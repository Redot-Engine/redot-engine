/**************************************************************************/
/*  ustring_simd.cpp                                                      */
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

#include "ustring_simd.h"

#ifdef GODOT_STRING_SIMD

#include "core/math/simd.h"

#include <bit>
#include <cstring>
#include <type_traits>

namespace StringSIMD {

// djb2 reaches hash*33^n + sum(c_i * 33^(n-1-i)) after n characters. Rather than reduce every block,
// two lane-accumulators carry the per-position partial sums with no horizontal step in the loop, and
// are collapsed once at the end; two independent chains hide the multiply latency. Bit-identical to
// the scalar loop (all arithmetic is mod 2^32, matching uint32 overflow).
HashBlocks hash_blocks(const char32_t *p_chr, int64_t p_length, uint32_t p_hash) {
	const char32_t *chr = p_chr;
	const uint32_t p1 = 33u, p2 = p1 * 33u, p3 = p2 * 33u, p4 = p3 * 33u;
	const uint32_t p5 = p4 * 33u, p6 = p5 * 33u, p7 = p6 * 33u, p8 = p4 * p4;
	const __m128i step = _mm_set1_epi32((int)p8);
	const __m128i zero = _mm_setzero_si128();
	__m128i acc0 = zero;
	__m128i acc1 = zero;
	uint32_t pown = 1; // 33^(characters consumed so far)
	int64_t i = 0;
	for (; i + 8 <= p_length; i += 8) {
		const __m128i ca = _mm_loadu_si128((const __m128i *)(chr + i));
		const __m128i cb = _mm_loadu_si128((const __m128i *)(chr + i + 4));
		if (_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi32(ca, zero), _mm_cmpeq_epi32(cb, zero))) != 0) {
			break;
		}
		// Even and odd 4-blocks run as independent chains; combined with the right powers at the end.
		acc0 = _mm_add_epi32(_mm_mullo_epi32(acc0, step), ca);
		acc1 = _mm_add_epi32(_mm_mullo_epi32(acc1, step), cb);
		pown *= p8;
	}
	if (i > 0) {
		// Positions 8e+j weigh 33^(7-j) (first block of the pair) and 33^(3-j) (second block).
		const __m128i w0 = _mm_setr_epi32((int)p7, (int)p6, (int)p5, (int)p4);
		const __m128i w1 = _mm_setr_epi32((int)p3, (int)p2, (int)p1, 1);
		__m128i m = _mm_add_epi32(_mm_mullo_epi32(acc0, w0), _mm_mullo_epi32(acc1, w1));
		m = _mm_add_epi32(m, _mm_shuffle_epi32(m, _MM_SHUFFLE(1, 0, 3, 2)));
		m = _mm_add_epi32(m, _mm_shuffle_epi32(m, _MM_SHUFFLE(2, 3, 0, 1)));
		p_hash = p_hash * pown + (uint32_t)_mm_cvtsi128_si32(m);
	}
	return { p_hash, i };
}

int64_t latin1_equal_blocks(const char32_t *p_str32, const char *p_str, int64_t p_length) {
	int64_t i = 0;
	for (; i + 4 <= p_length; i += 4) {
		uint32_t packed;
		memcpy(&packed, p_str + i, 4);
		__m128i c = _mm_cvtsi32_si128((int)packed);
		c = std::is_signed_v<char> ? _mm_cvtepi8_epi32(c) : _mm_cvtepu8_epi32(c);
		if (_mm_movemask_epi8(_mm_cmpeq_epi32(c, _mm_loadu_si128((const __m128i *)(p_str32 + i)))) != 0xFFFF) {
			return -1;
		}
	}
	return i;
}

int64_t ascii_case_blocks(bool p_upper, const char32_t *p_src, char32_t *p_dst, int64_t p_length) {
	// The case tables map a-z and A-Z onto each other and leave every other ASCII character alone.
	const __m128i lo = _mm_set1_epi32(p_upper ? 'a' - 1 : 'A' - 1);
	const __m128i hi = _mm_set1_epi32(p_upper ? 'z' + 1 : 'Z' + 1);
	const __m128i delta = _mm_set1_epi32(32);
	const __m128i max_ascii = _mm_set1_epi32(0x7F);
	const __m128i one = _mm_set1_epi32(1);
	int64_t i = 0;
	for (; i + 4 <= p_length; i += 4) {
		__m128i c = _mm_loadu_si128((const __m128i *)(p_src + i));
		// Stops at NUL, where the scalar loop ends, and at code points >= 0x80000000, which look
		// negative to the signed compare.
		const __m128i non_ascii = _mm_or_si128(_mm_cmpgt_epi32(c, max_ascii), _mm_cmpgt_epi32(one, c));
		if (_mm_movemask_epi8(non_ascii) != 0) {
			break;
		}
		const __m128i letter = _mm_and_si128(_mm_cmpgt_epi32(c, lo), _mm_cmpgt_epi32(hi, c));
		const __m128i adj = _mm_and_si128(letter, delta);
		c = p_upper ? _mm_sub_epi32(c, adj) : _mm_add_epi32(c, adj);
		_mm_storeu_si128((__m128i *)(p_dst + i), c);
	}
	return i;
}

// Left-packs the 32-bit lanes selected by a 4-bit mask.
struct PackTable {
	alignas(16) uint8_t shuffle[16][16];
	constexpr PackTable() :
			shuffle() {
		for (int mask = 0; mask < 16; mask++) {
			int out = 0;
			for (int lane = 0; lane < 4; lane++) {
				if (mask & (1 << lane)) {
					for (int b = 0; b < 4; b++) {
						shuffle[mask][out * 4 + b] = uint8_t(lane * 4 + b);
					}
					out++;
				}
			}
			for (int b = out * 4; b < 16; b++) {
				shuffle[mask][b] = 0x80;
			}
		}
	}
};
static constexpr PackTable pack_table;

// Errors in a byte p_b that follows a lead p_prev: overlong (C0, C1, E0 + 80..9F, F0 + 80..8F), surrogate
// (ED + A0..BF) and above U+10FFFF (F4 + 90..BF, F5..FF), looked up by nibbles.
static _FORCE_INLINE_ __m128i _lead_errors(__m128i p_prev_hi, __m128i p_prev_lo, __m128i p_hi) {
	const __m128i by_prev_hi = _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0, 0x03, 0x2C);
	const __m128i by_prev_lo = _mm_setr_epi8(0x15, 0x10, 0, 0, 0x08, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x22, 0x20, 0x20);
	const __m128i by_hi = _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0x35, 0x39, 0x3A, 0x3A, 0, 0, 0, 0);
	return _mm_and_si128(_mm_and_si128(_mm_shuffle_epi8(by_prev_hi, p_prev_hi), _mm_shuffle_epi8(by_prev_lo, p_prev_lo)), _mm_shuffle_epi8(by_hi, p_hi));
}

// Decodes the 4 positions 4 * G .. 4 * G + 3 of a validated block and stores those that start a character.
// p_payload holds each byte without its length bits, p_pad the number of zero bytes that right-align a
// character of that lead in a 32-bit lane.
template <int G>
static _FORCE_INLINE_ char32_t *_decode_group(__m128i p_payload, __m128i p_payload_next, __m128i p_pad, int p_leads, char32_t *p_dst) {
	const __m128i bytes = G == 0 ? p_payload : _mm_alignr_epi8(p_payload_next, p_payload, 4 * G);
	const __m128i pad = _mm_shuffle_epi8(p_pad, _mm_setr_epi8(4 * G, 4 * G, 4 * G, 4 * G, 4 * G + 1, 4 * G + 1, 4 * G + 1, 4 * G + 1, 4 * G + 2, 4 * G + 2, 4 * G + 2, 4 * G + 2, 4 * G + 3, 4 * G + 3, 4 * G + 3, 4 * G + 3));
	__m128i index = _mm_sub_epi8(_mm_setr_epi8(0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6), pad);
	index = _mm_or_si128(index, _mm_cmpgt_epi8(pad, _mm_setr_epi8(0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3)));
	// Lanes are [0.., lead, continuations...]; (b0 << 18) | (b1 << 12) | (b2 << 6) | b3 is the code point.
	__m128i x = _mm_shuffle_epi8(bytes, index);
	x = _mm_madd_epi16(_mm_maddubs_epi16(x, _mm_set1_epi16(0x0140)), _mm_set1_epi32(0x00011000));
	const int mask = (p_leads >> (4 * G)) & 0xF;
	_mm_storeu_si128((__m128i *)p_dst, _mm_shuffle_epi8(x, _mm_load_si128((const __m128i *)pack_table.shuffle[mask])));
	return p_dst + std::popcount(unsigned(mask));
}

DecodedBlocks decode_utf8_blocks(const uint8_t *p_src, const uint8_t *p_limit, char32_t *p_dst, bool p_skip_cr) {
	const uint8_t *src = p_src;
	char32_t *dst = p_dst;
	const __m128i zero = _mm_setzero_si128();
	const __m128i nibble = _mm_set1_epi8(0x0F);
	// Indexed by the high nibble of a byte.
	const __m128i payload_table = _mm_setr_epi8(0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x3F, 0x3F, 0x3F, 0x3F, 0x1F, 0x1F, 0x0F, 0x07);
	const __m128i follow_table = _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 3, 7);
	const __m128i pad_table = _mm_setr_epi8(3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 2, 2, 1, 0);
	// Without p_skip_cr this matches NUL, which already ends the loop.
	const __m128i cr = _mm_set1_epi8(p_skip_cr ? '\r' : 0);
	// Each block starts on a character boundary; its last character may end up to 3 bytes into the next 16.
	while (p_limit - src >= 32) {
		const __m128i v = _mm_loadu_si128((const __m128i *)src);
		if (_mm_movemask_epi8(_mm_cmpeq_epi8(v, zero)) != 0) {
			break;
		}
		const int cr_bits = _mm_movemask_epi8(_mm_cmpeq_epi8(v, cr));
		if (_mm_movemask_epi8(v) == 0 && cr_bits == 0) {
			const __m128i w16_lo = _mm_unpacklo_epi8(v, zero);
			const __m128i w16_hi = _mm_unpackhi_epi8(v, zero);
			_mm_storeu_si128((__m128i *)(dst + 0), _mm_unpacklo_epi16(w16_lo, zero));
			_mm_storeu_si128((__m128i *)(dst + 4), _mm_unpackhi_epi16(w16_lo, zero));
			_mm_storeu_si128((__m128i *)(dst + 8), _mm_unpacklo_epi16(w16_hi, zero));
			_mm_storeu_si128((__m128i *)(dst + 12), _mm_unpackhi_epi16(w16_hi, zero));
			src += 16;
			dst += 16;
			continue;
		}
		const __m128i n = _mm_loadu_si128((const __m128i *)(src + 16));
		const __m128i hi_v = _mm_and_si128(_mm_srli_epi16(v, 4), nibble);
		const __m128i hi_n = _mm_and_si128(_mm_srli_epi16(n, 4), nibble);
		const __m128i lo_v = _mm_and_si128(v, nibble);
		const __m128i lo_n = _mm_and_si128(n, nibble);
		const __m128i cont_v = _mm_cmpgt_epi8(_mm_set1_epi8(-64), v);
		const __m128i cont_n = _mm_cmpgt_epi8(_mm_set1_epi8(-64), n);

		// Positions that must hold a continuation byte, in this block and in the 3 bytes after it.
		const __m128i follow = _mm_shuffle_epi8(follow_table, hi_v);
		const __m128i one = _mm_set1_epi8(1);
		const __m128i two = _mm_set1_epi8(2);
		const __m128i four = _mm_set1_epi8(4);
		const __m128i req_v = _mm_cmpgt_epi8(_mm_or_si128(_mm_or_si128(_mm_and_si128(_mm_alignr_epi8(follow, zero, 15), one), _mm_and_si128(_mm_alignr_epi8(follow, zero, 14), two)), _mm_and_si128(_mm_alignr_epi8(follow, zero, 13), four)), zero);
		const __m128i req_n = _mm_cmpgt_epi8(_mm_or_si128(_mm_or_si128(_mm_and_si128(_mm_alignr_epi8(zero, follow, 15), one), _mm_and_si128(_mm_alignr_epi8(zero, follow, 14), two)), _mm_and_si128(_mm_alignr_epi8(zero, follow, 13), four)), zero);

		__m128i bad = _mm_xor_si128(cont_v, req_v);
		bad = _mm_or_si128(bad, _mm_andnot_si128(cont_n, req_n));
		bad = _mm_or_si128(bad, _lead_errors(_mm_alignr_epi8(hi_v, zero, 15), _mm_alignr_epi8(lo_v, zero, 15), hi_v));
		bad = _mm_or_si128(bad, _mm_and_si128(req_n, _lead_errors(_mm_alignr_epi8(hi_n, hi_v, 15), _mm_alignr_epi8(lo_n, lo_v, 15), hi_n)));
		if (!_mm_testz_si128(bad, bad)) {
			break;
		}

		const __m128i payload = _mm_and_si128(v, _mm_shuffle_epi8(payload_table, hi_v));
		const __m128i payload_next = _mm_and_si128(n, _mm_set1_epi8(0x3F));
		const __m128i pad = _mm_shuffle_epi8(pad_table, hi_v);
		const int leads = ~(_mm_movemask_epi8(cont_v) | cr_bits) & 0xFFFF;
		dst = _decode_group<0>(payload, payload_next, pad, leads, dst);
		dst = _decode_group<1>(payload, payload_next, pad, leads, dst);
		dst = _decode_group<2>(payload, payload_next, pad, leads, dst);
		dst = _decode_group<3>(payload, payload_next, pad, leads, dst);
		src += 16 + std::popcount(unsigned(_mm_movemask_epi8(req_n)));
	}
	return { src - p_src, dst - p_dst };
}

} // namespace StringSIMD

#endif // GODOT_STRING_SIMD

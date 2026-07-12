/**************************************************************************/
/*  color.h                                                               */
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

#include "core/math/math_funcs.h"
#include <bit>

class String;

namespace color_pack {

constexpr uint32_t floats_to_uint32(const float f1, const float f2, const float f3, const float f4) noexcept {
	constexpr auto scale = static_cast<float>(std::numeric_limits<uint8_t>::max());
	uint32_t c = static_cast<uint8_t>(Math::round(f1 * scale));
	c <<= 8;
	c |= static_cast<uint8_t>(Math::round(f2 * scale));
	c <<= 8;
	c |= static_cast<uint8_t>(Math::round(f3 * scale));
	c <<= 8;
	c |= static_cast<uint8_t>(Math::round(f4 * scale));
	return c;
}

constexpr uint64_t floats_to_uint64(const float f1, const float f2, const float f3, const float f4) noexcept {
	constexpr auto scale = static_cast<float>(std::numeric_limits<uint16_t>::max());
	uint64_t c = static_cast<uint16_t>(Math::round(f1 * scale));
	c <<= 16;
	c |= static_cast<uint16_t>(Math::round(f2 * scale));
	c <<= 16;
	c |= static_cast<uint16_t>(Math::round(f3 * scale));
	c <<= 16;
	c |= static_cast<uint16_t>(Math::round(f4 * scale));
	return c;
}
} //namespace color_pack

struct [[nodiscard]] Color {
	union {
		// NOLINTBEGIN(modernize-use-default-member-init)
		struct {
			float r;
			float g;
			float b;
			float a;
		};
		float components[4] = { 0, 0, 0, 1.0 };
		// NOLINTEND(modernize-use-default-member-init)
	};

	constexpr Color() noexcept :
			r(0), g(0), b(0), a(1) {}

	/**
	 * RGBA construct parameters.
	 * Alpha is not optional as otherwise we can't bind the RGB version for scripting.
	 */
	constexpr Color(float p_r, float p_g, float p_b, float p_a) noexcept :
			r(p_r), g(p_g), b(p_b), a(p_a) {}

	/**
	 * RGB construct parameters.
	 */
	constexpr Color(float p_r, float p_g, float p_b) noexcept :
			r(p_r), g(p_g), b(p_b), a(1) {}

	/**
	 * Construct a Color from another Color, but with the specified alpha value.
	 */
	constexpr Color(const Color &p_c, float p_a) noexcept :
			r(p_c.r), g(p_c.g), b(p_c.b), a(p_a) {}

	// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)
	Color(const String &p_code) {
		if (html_is_valid(p_code)) {
			*this = html(p_code);
		} else {
			*this = named(p_code);
		}
	}

	Color(const String &p_code, float p_a) :
			Color(p_code) { a = p_a; }
	// NOLINTEND(cppcoreguidelines-pro-type-member-init)

	constexpr uint32_t to_rgba32() const noexcept {
		return color_pack::floats_to_uint32(r, g, b, a);
	}

	constexpr uint32_t to_argb32() const noexcept {
		return color_pack::floats_to_uint32(a, r, g, b);
	}

	constexpr uint32_t to_abgr32() const noexcept {
		return color_pack::floats_to_uint32(a, b, g, r);
	}

	constexpr uint64_t to_rgba64() const noexcept {
		return color_pack::floats_to_uint64(r, g, b, a);
	}

	constexpr uint64_t to_argb64() const noexcept {
		return color_pack::floats_to_uint64(a, r, g, b);
	}

	constexpr uint64_t to_abgr64() const noexcept {
		return color_pack::floats_to_uint64(a, b, g, r);
	}

	String to_html(bool p_alpha = true) const;
	constexpr float get_h() const noexcept {
		const float min = MIN(r, MIN(g, b));
		const float max = MAX(r, MAX(g, b));
		const float delta = max - min;

		if (delta == 0.0f) {
			return 0.0f;
		}

		float h;
		if (r == max) {
			h = (g - b) / delta; // between yellow & magenta
		} else if (g == max) {
			h = 2 + (b - r) / delta; // between cyan & yellow
		} else {
			h = 4 + (r - g) / delta; // between magenta & cyan
		}

		h /= 6.0f;
		if (h < 0.0f) {
			h += 1.0f;
		}

		return h;
	}

	constexpr float get_s() const noexcept {
		const float min = MIN(r, MIN(g, b));
		const float max = MAX(r, MAX(g, b));
		const float delta = max - min;
		return (max != 0.0f) ? (delta / max) : 0.0f;
	}

	constexpr float get_v() const noexcept {
		return MAX(r, MAX(g, b));
	}

	void set_hsv(float p_h, float p_s, float p_v, float p_alpha = 1.0f) noexcept;
	float get_ok_hsl_h() const;
	float get_ok_hsl_s() const;
	float get_ok_hsl_l() const;
	void set_ok_hsl(float p_h, float p_s, float p_l, float p_alpha = 1.0f);
	void set_ok_hsv(float p_h, float p_s, float p_v, float p_alpha = 1.0f);

	constexpr float &operator[](const int p_idx) noexcept {
		return components[p_idx];
	}
	constexpr const float &operator[](const int p_idx) const noexcept {
		return components[p_idx];
	}

	constexpr bool operator==(const Color &p_color) const noexcept {
		return r == p_color.r && g == p_color.g && b == p_color.b && a == p_color.a;
	}

	constexpr Color operator+(const Color &p_color) const noexcept;
	constexpr Color &operator+=(const Color &p_color) noexcept;

	constexpr Color operator-() const noexcept;
	constexpr Color operator-(const Color &p_color) const noexcept;
	constexpr Color &operator-=(const Color &p_color) noexcept;

	constexpr Color operator*(const Color &p_color) const noexcept;
	constexpr Color operator*(float p_scalar) const noexcept;
	constexpr Color &operator*=(const Color &p_color) noexcept;
	constexpr Color &operator*=(float p_scalar) noexcept;

	constexpr Color operator/(const Color &p_color) const noexcept;
	constexpr Color operator/(float p_scalar) const noexcept;
	constexpr Color &operator/=(const Color &p_color) noexcept;
	constexpr Color &operator/=(float p_scalar) noexcept;

	constexpr bool is_equal_approx(const Color &p_color) const noexcept {
		return Math::is_equal_approx(r, p_color.r) && Math::is_equal_approx(g, p_color.g) && Math::is_equal_approx(b, p_color.b) && Math::is_equal_approx(a, p_color.a);
	}

	constexpr bool is_same(const Color &p_color) const noexcept {
		return Math::is_same(r, p_color.r) && Math::is_same(g, p_color.g) && Math::is_same(b, p_color.b) && Math::is_same(a, p_color.a);
	}

	constexpr Color clamp(const Color &p_min = { 0, 0, 0, 0 }, const Color &p_max = { 1, 1, 1, 1 }) const noexcept {
		return Color{
			CLAMP(r, p_min.r, p_max.r),
			CLAMP(g, p_min.g, p_max.g),
			CLAMP(b, p_min.b, p_max.b),
			CLAMP(a, p_min.a, p_max.a)
		};
	}

	constexpr void invert() noexcept {
		r = 1.0f - r;
		g = 1.0f - g;
		b = 1.0f - b;
	}
	constexpr Color inverted() const noexcept {
		Color c = *this;
		c.invert();
		return c;
	}

	constexpr float get_luminance() const noexcept {
		return 0.2126f * r + 0.7152f * g + 0.0722f * b;
	}

	/**
	 * Applies an intensity adjustment to the color by modifying the RGB components.
	 * The alpha component remains unchanged. The intensity is applied as a
	 * power-of-two multiplier to the RGB values, clamped between 0.0 and 1.0.
	 *
	 * @param p_intensity The intensity level to apply. A value of 0 will keep the color unchanged.
	 *                    Positive values increase brightness, and negative values decrease brightness.
	 * @return A new Color instance with the adjusted RGB components and the original alpha value.
	 */
	Color apply_intensity(float p_intensity) const;

	constexpr Color lerp(const Color &p_to, const float p_weight) const noexcept {
		Color res = *this;
		res.r = Math::lerp(res.r, p_to.r, p_weight);
		res.g = Math::lerp(res.g, p_to.g, p_weight);
		res.b = Math::lerp(res.b, p_to.b, p_weight);
		res.a = Math::lerp(res.a, p_to.a, p_weight);
		return res;
	}

	constexpr Color darkened(const float p_amount) const noexcept {
		Color res = *this;
		res.r *= 1.0f - p_amount;
		res.g *= 1.0f - p_amount;
		res.b *= 1.0f - p_amount;
		return res;
	}

	constexpr Color lightened(const float p_amount) const noexcept {
		Color res = *this;
		res.r += (1.0f - res.r) * p_amount;
		res.g += (1.0f - res.g) * p_amount;
		res.b += (1.0f - res.b) * p_amount;
		return res;
	}

	constexpr uint32_t to_rgbe9995() const noexcept {
		// https://github.com/microsoft/DirectX-Graphics-Samples/blob/v10.0.19041.0/MiniEngine/Core/Color.cpp
		constexpr float kMaxVal{ 0x1FF << 7 };
		constexpr float kMinVal{ 1.f / (1 << 16) };

		// Clamp RGB to [0, 1.FF*2^16]
		const float _r = CLAMP(r, 0.0f, kMaxVal);
		const float _g = CLAMP(g, 0.0f, kMaxVal);
		const float _b = CLAMP(b, 0.0f, kMaxVal);

		// Compute the maximum channel, no less than 1.0*2^-15
		const float MaxChannel = MAX(MAX(_r, _g), MAX(_b, kMinVal));

		// Take the exponent of the maximum channel (rounding up the 9th bit) and
		// add 15 to it.  When added to the channels, it causes the implicit '1.0'
		// bit and the first 8 mantissa bits to be shifted down to the low 9 bits
		// of the mantissa, rounding the truncated bits.

		float E = MaxChannel;
		auto Ei = std::bit_cast<uint32_t>(E);
		Ei += 0x07804000; // Add 15 to the exponent and 0x4000 to the mantissa
		Ei &= 0x7F800000; // Zero the mantissa
		E = std::bit_cast<float>(Ei);

		// This shifts the 9-bit values we need into the lowest bits, rounding as
		// needed. Note that if the channel has a smaller exponent than the max
		// channel, it will shift even more.  This is intentional.
		const float R = _r + E;
		const float G = _g + E;
		const float B = _b + E;

		// Convert the Bias to the correct exponent in the upper 5 bits.
		Ei <<= 4;
		Ei += 0x10000000;

		// Combine the fields. RGB floats have unwanted data in the upper 9
		// bits. Only red needs to mask them off because green and blue shift
		// it out to the left.
		return Ei | std::bit_cast<uint32_t>(B) << 18U | std::bit_cast<uint32_t>(G) << 9U | (std::bit_cast<uint32_t>(R) & 511U);
	}

	constexpr Color blend(const Color &p_over) const noexcept {
		Color res;
		const float sa = 1.0f - p_over.a;
		res.a = a * sa + p_over.a;
		if (res.a == 0) {
			return Color(0, 0, 0, 0);
		} else {
			res.r = (r * a * sa + p_over.r * p_over.a) / res.a;
			res.g = (g * a * sa + p_over.g * p_over.a) / res.a;
			res.b = (b * a * sa + p_over.b * p_over.a) / res.a;
		}
		return res;
	}

	constexpr Color srgb_to_linear() const noexcept {
		return Color(
				r < 0.04045f ? r * (1.0f / 12.92f) : Math::pow(float((r + 0.055) * (1.0 / (1.0 + 0.055))), 2.4f),
				g < 0.04045f ? g * (1.0f / 12.92f) : Math::pow(float((g + 0.055) * (1.0 / (1.0 + 0.055))), 2.4f),
				b < 0.04045f ? b * (1.0f / 12.92f) : Math::pow(float((b + 0.055) * (1.0 / (1.0 + 0.055))), 2.4f),
				a);
	}

	constexpr Color linear_to_srgb() const noexcept {
		return Color(
				r < 0.0031308f ? 12.92f * r : (1.0 + 0.055) * Math::pow(r, 1.0f / 2.4f) - 0.055,
				g < 0.0031308f ? 12.92f * g : (1.0 + 0.055) * Math::pow(g, 1.0f / 2.4f) - 0.055,
				b < 0.0031308f ? 12.92f * b : (1.0 + 0.055) * Math::pow(b, 1.0f / 2.4f) - 0.055, a);
	}

	static constexpr Color hex(uint32_t p_hex) noexcept {
		const float a = (p_hex & 0xFF) / 255.0f;
		p_hex >>= 8;
		const float b = (p_hex & 0xFF) / 255.0f;
		p_hex >>= 8;
		const float g = (p_hex & 0xFF) / 255.0f;
		p_hex >>= 8;
		const float r = (p_hex & 0xFF) / 255.0f;
		return Color{ r, g, b, a };
	}
	static constexpr Color hex64(uint64_t p_hex) noexcept {
		const float a = (p_hex & 0xFFFF) / 65535.0f;
		p_hex >>= 16;
		const float b = (p_hex & 0xFFFF) / 65535.0f;
		p_hex >>= 16;
		const float g = (p_hex & 0xFFFF) / 65535.0f;
		p_hex >>= 16;
		const float r = (p_hex & 0xFFFF) / 65535.0f;
		return Color{ r, g, b, a };
	}
	static Color html(const String &p_rgba);
	static bool html_is_valid(const String &p_color);
	static Color named(const String &p_name);
	static Color named(const String &p_name, const Color &p_default);
	static int find_named_color(const String &p_name);
	static int get_named_color_count();
	static String get_named_color_name(int p_idx);
	static Color get_named_color(int p_idx);
	static Color from_string(const String &p_string, const Color &p_default);
	static Color from_hsv(float p_h, float p_s, float p_v, float p_alpha = 1.0f);
	static Color from_ok_hsl(float p_h, float p_s, float p_l, float p_alpha = 1.0f);
	static Color from_ok_hsv(float p_h, float p_s, float p_l, float p_alpha = 1.0f);
	static Color from_rgbe9995(uint32_t p_rgbe) noexcept;
	static Color from_rgba8(int64_t p_r8, int64_t p_g8, int64_t p_b8, int64_t p_a8 = 255);

	constexpr bool operator<(const Color &p_color) const noexcept; // Used in set keys.
	explicit operator String() const;

	// For the binder.
	constexpr void set_r8(int32_t r8) noexcept { r = (CLAMP(r8, 0, 255) / 255.0f); }
	constexpr int32_t get_r8() const noexcept { return int32_t(CLAMP(Math::round(r * 255.0f), 0.0f, 255.0f)); }
	constexpr void set_g8(int32_t g8) noexcept { g = (CLAMP(g8, 0, 255) / 255.0f); }
	constexpr int32_t get_g8() const noexcept { return int32_t(CLAMP(Math::round(g * 255.0f), 0.0f, 255.0f)); }
	constexpr void set_b8(int32_t b8) noexcept { b = (CLAMP(b8, 0, 255) / 255.0f); }
	constexpr int32_t get_b8() const noexcept { return int32_t(CLAMP(Math::round(b * 255.0f), 0.0f, 255.0f)); }
	constexpr void set_a8(int32_t a8) noexcept { a = (CLAMP(a8, 0, 255) / 255.0f); }
	constexpr int32_t get_a8() const noexcept { return int32_t(CLAMP(Math::round(a * 255.0f), 0.0f, 255.0f)); }

	void set_h(float p_h) { set_hsv(p_h, get_s(), get_v(), a); }
	void set_s(float p_s) { set_hsv(get_h(), p_s, get_v(), a); }
	void set_v(float p_v) { set_hsv(get_h(), get_s(), p_v, a); }
	void set_ok_hsl_h(float p_h) { set_ok_hsl(p_h, get_ok_hsl_s(), get_ok_hsl_l(), a); }
	void set_ok_hsl_s(float p_s) { set_ok_hsl(get_ok_hsl_h(), p_s, get_ok_hsl_l(), a); }
	void set_ok_hsl_l(float p_l) { set_ok_hsl(get_ok_hsl_h(), get_ok_hsl_s(), p_l, a); }
};

constexpr Color Color::operator+(const Color &p_color) const noexcept {
	return Color{ r + p_color.r, g + p_color.g, b + p_color.b, a + p_color.a };
}

constexpr Color &Color::operator+=(const Color &p_color) noexcept {
	r += p_color.r;
	g += p_color.g;
	b += p_color.b;
	a += p_color.a;
	return *this;
}

constexpr Color Color::operator-(const Color &p_color) const noexcept {
	return Color{ r - p_color.r, g - p_color.g, b - p_color.b, a - p_color.a };
}

constexpr Color &Color::operator-=(const Color &p_color) noexcept {
	r -= p_color.r;
	g -= p_color.g;
	b -= p_color.b;
	a -= p_color.a;
	return *this;
}

constexpr Color Color::operator*(const Color &p_color) const noexcept {
	return Color{ r * p_color.r, g * p_color.g, b * p_color.b, a * p_color.a };
}

constexpr Color Color::operator*(float p_scalar) const noexcept {
	return Color{ r * p_scalar, g * p_scalar, b * p_scalar, a * p_scalar };
}

constexpr Color &Color::operator*=(const Color &p_color) noexcept {
	r *= p_color.r;
	g *= p_color.g;
	b *= p_color.b;
	a *= p_color.a;
	return *this;
}

constexpr Color &Color::operator*=(float p_scalar) noexcept {
	r *= p_scalar;
	g *= p_scalar;
	b *= p_scalar;
	a *= p_scalar;
	return *this;
}

constexpr Color Color::operator/(const Color &p_color) const noexcept {
	return Color{ r / p_color.r, g / p_color.g, b / p_color.b, a / p_color.a };
}

constexpr Color Color::operator/(const float p_scalar) const noexcept {
	return Color{ r / p_scalar, g / p_scalar, b / p_scalar, a / p_scalar };
}

constexpr Color &Color::operator/=(const Color &p_color) noexcept {
	r /= p_color.r;
	g /= p_color.g;
	b /= p_color.b;
	a /= p_color.a;
	return *this;
}

constexpr Color &Color::operator/=(float p_scalar) noexcept {
	r /= p_scalar;
	g /= p_scalar;
	b /= p_scalar;
	a /= p_scalar;
	return *this;
}

constexpr Color Color::operator-() const noexcept {
	return Color{ 1.0f - r, 1.0f - g, 1.0f - b, 1.0f - a };
}

constexpr bool Color::operator<(const Color &p_color) const noexcept {
	if (r == p_color.r) {
		if (g == p_color.g) {
			if (b == p_color.b) {
				return (a < p_color.a);
			} else {
				return (b < p_color.b);
			}
		} else {
			return g < p_color.g;
		}
	} else {
		return r < p_color.r;
	}
}

constexpr Color operator*(float p_scalar, const Color &p_color) {
	return p_color * p_scalar;
}

// Named colors:
struct Colors {
	static constexpr auto AliceBlue = Color::hex(0xF0F8FFFF);
	static constexpr auto AntiqueWhite = Color::hex(0xFAEBD7FF);
	static constexpr auto Aqua = Color::hex(0x00FFFFFF);
	static constexpr auto Aquamarine = Color::hex(0x7FFFD4FF);
	static constexpr auto Azure = Color::hex(0xF0FFFFFF);
	static constexpr auto Beige = Color::hex(0xF5F5DCFF);
	static constexpr auto Bisque = Color::hex(0xFFE4C4FF);
	static constexpr auto Black = Color::hex(0x000000FF);
	static constexpr auto BlanchedAlmond = Color::hex(0xFFEBCDFF);
	static constexpr auto Blue = Color::hex(0x0000FFFF);
	static constexpr auto BlueViolet = Color::hex(0x8A2BE2FF);
	static constexpr auto Brown = Color::hex(0xA52A2AFF);
	static constexpr auto BurlyWood = Color::hex(0xDEB887FF);
	static constexpr auto CadetBlue = Color::hex(0x5F9EA0FF);
	static constexpr auto Chartreuse = Color::hex(0x7FFF00FF);
	static constexpr auto Chocolate = Color::hex(0xD2691EFF);
	static constexpr auto Coral = Color::hex(0xFF7F50FF);
	static constexpr auto CornflowerBlue = Color::hex(0x6495EDFF);
	static constexpr auto Cornsilk = Color::hex(0xFFF8DCFF);
	static constexpr auto Crimson = Color::hex(0xDC143CFF);
	static constexpr auto Cyan = Aqua;
	static constexpr auto DarkBlue = Color::hex(0x00008BFF);
	static constexpr auto DarkCyan = Color::hex(0x008B8BFF);
	static constexpr auto DarkGoldenrod = Color::hex(0xB8860BFF);
	static constexpr auto DarkGray = Color::hex(0xA9A9A9FF);
	static constexpr auto DarkGreen = Color::hex(0x006400FF);
	static constexpr auto DarkKhaki = Color::hex(0xBDB76BFF);
	static constexpr auto DarkMagenta = Color::hex(0x8B008BFF);
	static constexpr auto DarkOliveGreen = Color::hex(0x556B2FFF);
	static constexpr auto DarkOrange = Color::hex(0xFF8C00FF);
	static constexpr auto DarkOrchid = Color::hex(0x9932CCFF);
	static constexpr auto DarkRed = Color::hex(0x8B0000FF);
	static constexpr auto DarkSalmon = Color::hex(0xE9967AFF);
	static constexpr auto DarkSeaGreen = Color::hex(0x8FBC8FFF);
	static constexpr auto DarkSlateBlue = Color::hex(0x483D8BFF);
	static constexpr auto DarkSlateGray = Color::hex(0x2F4F4FFF);
	static constexpr auto DarkTurquoise = Color::hex(0x00CED1FF);
	static constexpr auto DarkViolet = Color::hex(0x9400D3FF);
	static constexpr auto DeepPink = Color::hex(0xFF1493FF);
	static constexpr auto DeepSkyBlue = Color::hex(0x00BFFFFF);
	static constexpr auto DimGray = Color::hex(0x696969FF);
	static constexpr auto DodgerBlue = Color::hex(0x1E90FFFF);
	static constexpr auto Firebrick = Color::hex(0xB22222FF);
	static constexpr auto FloralWhite = Color::hex(0xFFFAF0FF);
	static constexpr auto ForestGreen = Color::hex(0x228B22FF);
	static constexpr auto Fuchsia = Color::hex(0xFF00FFFF);
	static constexpr auto Gainsboro = Color::hex(0xDCDCDCFF);
	static constexpr auto GhostWhite = Color::hex(0xF8F8FFFF);
	static constexpr auto Gold = Color::hex(0xFFD700FF);
	static constexpr auto Goldenrod = Color::hex(0xDAA520FF);
	static constexpr auto Gray = Color::hex(0xBEBEBEFF);
	static constexpr auto Green = Color::hex(0x00FF00FF);
	static constexpr auto GreenYellow = Color::hex(0xADFF2FFF);
	static constexpr auto Honeydew = Color::hex(0xF0FFF0FF);
	static constexpr auto HotPink = Color::hex(0xFF69B4FF);
	static constexpr auto IndianRed = Color::hex(0xCD5C5CFF);
	static constexpr auto Indigo = Color::hex(0x4B0082FF);
	static constexpr auto Ivory = Color::hex(0xFFFFF0FF);
	static constexpr auto Khaki = Color::hex(0xF0E68CFF);
	static constexpr auto Lavender = Color::hex(0xE6E6FAFF);
	static constexpr auto LavenderBlush = Color::hex(0xFFF0F5FF);
	static constexpr auto LawnGreen = Color::hex(0x7CFC00FF);
	static constexpr auto LemonChiffon = Color::hex(0xFFFACDFF);
	static constexpr auto LightBlue = Color::hex(0xADD8E6FF);
	static constexpr auto LightCoral = Color::hex(0xF08080FF);
	static constexpr auto LightCyan = Color::hex(0xE0FFFFFF);
	static constexpr auto LightGoldenrod = Color::hex(0xFAFAD2FF);
	static constexpr auto LightGray = Color::hex(0xD3D3D3FF);
	static constexpr auto LightGreen = Color::hex(0x90EE90FF);
	static constexpr auto LightPink = Color::hex(0xFFB6C1FF);
	static constexpr auto LightSalmon = Color::hex(0xFFA07AFF);
	static constexpr auto LightSeaGreen = Color::hex(0x20B2AAFF);
	static constexpr auto LightSkyBlue = Color::hex(0x87CEFAFF);
	static constexpr auto LightSlateGray = Color::hex(0x778899FF);
	static constexpr auto LightSteelBlue = Color::hex(0xB0C4DEFF);
	static constexpr auto LightYellow = Color::hex(0xFFFFE0FF);
	static constexpr auto Lime = Green;
	static constexpr auto LimeGreen = Color::hex(0x32CD32FF);
	static constexpr auto Linen = Color::hex(0xFAF0E6FF);
	static constexpr auto Magenta = Fuchsia;
	static constexpr auto Maroon = Color::hex(0xB03060FF);
	static constexpr auto MediumAquamarine = Color::hex(0x66CDAAFF);
	static constexpr auto MediumBlue = Color::hex(0x0000CDFF);
	static constexpr auto MediumOrchid = Color::hex(0xBA55D3FF);
	static constexpr auto MediumPurple = Color::hex(0x9370DBFF);
	static constexpr auto MediumSeaGreen = Color::hex(0x3CB371FF);
	static constexpr auto MediumSlateBlue = Color::hex(0x7B68EEFF);
	static constexpr auto MediumSpringGreen = Color::hex(0x00FA9AFF);
	static constexpr auto MediumTurquoise = Color::hex(0x48D1CCFF);
	static constexpr auto MediumVioletRed = Color::hex(0xC71585FF);
	static constexpr auto MidnightBlue = Color::hex(0x191970FF);
	static constexpr auto MintCream = Color::hex(0xF5FFFAFF);
	static constexpr auto MistyRose = Color::hex(0xFFE4E1FF);
	static constexpr auto Moccasin = Color::hex(0xFFE4B5FF);
	static constexpr auto NavajoWhite = Color::hex(0xFFDEADFF);
	static constexpr auto NavyBlue = Color::hex(0x000080FF);
	static constexpr auto OldLace = Color::hex(0xFDF5E6FF);
	static constexpr auto Olive = Color::hex(0x808000FF);
	static constexpr auto OliveDrab = Color::hex(0x6B8E23FF);
	static constexpr auto Orange = Color::hex(0xFFA500FF);
	static constexpr auto OrangeRed = Color::hex(0xFF4500FF);
	static constexpr auto Orchid = Color::hex(0xDA70D6FF);
	static constexpr auto PaleGoldenrod = Color::hex(0xEEE8AAFF);
	static constexpr auto PaleGreen = Color::hex(0x98FB98FF);
	static constexpr auto PaleTorquoise = Color::hex(0xAFEEEEFF);
	static constexpr auto PaleVioletRed = Color::hex(0xDB7093FF);
	static constexpr auto PapayaWhip = Color::hex(0xFFEFD5FF);
	static constexpr auto PeachPuff = Color::hex(0xFFDAB9FF);
	static constexpr auto Peru = Color::hex(0xCD853FFF);
	static constexpr auto Pink = Color::hex(0xFFC0CBFF);
	static constexpr auto Plum = Color::hex(0xDDA0DDFF);
	static constexpr auto PowderBlue = Color::hex(0xB0E0E6FF);
	static constexpr auto Purple = Color::hex(0xA020F0FF);
	static constexpr auto RebeccaPurple = Color::hex(0x663399FF);
	static constexpr auto Red = Color::hex(0xFF0000FF);
	static constexpr auto RosyBrown = Color::hex(0xBC8F8FFF);
	static constexpr auto RoyalBlue = Color::hex(0x4169E1FF);
	static constexpr auto SaddleBrown = Color::hex(0x8B4513FF);
	static constexpr auto Salmon = Color::hex(0xFA8072FF);
	static constexpr auto SandyBrown = Color::hex(0xF4A460FF);
	static constexpr auto SeaGreen = Color::hex(0x2E8B57FF);
	static constexpr auto Seashell = Color::hex(0xFFF5EEFF);
	static constexpr auto Sienna = Color::hex(0xA0522DFF);
	static constexpr auto Silver = Color::hex(0xC0C0C0FF);
	static constexpr auto SkyBlue = Color::hex(0x87CEEBFF);
	static constexpr auto SlateBlue = Color::hex(0x6A5ACDFF);
	static constexpr auto SlateGray = Color::hex(0x708090FF);
	static constexpr auto Snow = Color::hex(0xFFFAFAFF);
	static constexpr auto SpringGreen = Color::hex(0x00FF7FFF);
	static constexpr auto SteelBlue = Color::hex(0x4682B4FF);
	static constexpr auto Tan = Color::hex(0xD2B48CFF);
	static constexpr auto Teal = Color::hex(0x008080FF);
	static constexpr auto Thistle = Color::hex(0xD8BFD8FF);
	static constexpr auto Tomato = Color::hex(0xFF6347FF);
	static constexpr auto Transparent = Color::hex(0xFFFFFF00);
	static constexpr auto Turquoise = Color::hex(0x40E0D0FF);
	static constexpr auto Violet = Color::hex(0xEE82EEFF);
	static constexpr auto WebGray = Color::hex(0x808080FF);
	static constexpr auto WebGreen = Color::hex(0x008000FF);
	static constexpr auto WebMaroon = Color::hex(0x800000FF);
	static constexpr auto WebPurple = Color::hex(0x800080FF);
	static constexpr auto Wheat = Color::hex(0xF5DEB3FF);
	static constexpr auto White = Color::hex(0xFFFFFFFF);
	static constexpr auto WhiteSmoke = Color::hex(0xF5F5F5FF);
	static constexpr auto Yellow = Color::hex(0xFFFF00FF);
	static constexpr auto YellowGreen = Color::hex(0x9ACD32FF);
};

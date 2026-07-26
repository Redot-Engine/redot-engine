/**************************************************************************/
/*  constants.h                                                           */
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

// Terrain3D Godot plugin: Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include <bit>
#include <numbers>

#include "core/math/color.h"
#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/math/vector3.h"
#include "core/math/vector3i.h"

// Macros
//#define PhysicsS PhysicsServer3D::get_singleton()
#define IS_EDITOR Engine::get_singleton()->is_editor_hint()

// Set class name for logger.h

#define CLASS_NAME() const String __class__ = String(get_class_static()) + \
		String("#") + String::num_uint64(get_instance_id()).right(4);

#define CLASS_NAME_STATIC(p_name) static inline const char *__class__ = p_name;

// Validation macros

// TODO find the push/print UtilityFunctions
#if 0
#define ASSERT(cond, ret)                                                                            \
	if (!(cond)) {                                                                                   \
		UtilityFunctions::push_error("Assertion '", #cond, "' failed at ", __FILE__, ":", __LINE__); \
		return ret;                                                                                  \
	}
#else
#define ASSERT(cond, ret)
#endif

#define VOID // a return value for void, to avoid compiler warnings

#define IS_INIT(ret) \
	if (!_terrain) { \
		return ret;  \
	}

#define IS_INIT_MESG(mesg, ret) \
	if (!_terrain) {            \
		print_error(mesg);      \
		return ret;             \
	}

#define IS_INIT_COND(cond, ret) \
	if (!_terrain || cond) {    \
		return ret;             \
	}

#define IS_INIT_COND_MESG(cond, mesg, ret) \
	if (!_terrain || cond) {               \
		LOG(ERROR, mesg);                  \
		return ret;                        \
	}

#define IS_INSTANCER_INIT(ret)                     \
	if (!_terrain || !_terrain->get_instancer()) { \
		return ret;                                \
	}

#define IS_INSTANCER_INIT_MESG(mesg, ret)          \
	if (!_terrain || !_terrain->get_instancer()) { \
		LOG(ERROR, mesg);                          \
		return ret;                                \
	}

#define IS_DATA_INIT(ret)                     \
	if (!_terrain || !_terrain->get_data()) { \
		return ret;                           \
	}

#define IS_DATA_INIT_MESG(mesg, ret)          \
	if (!_terrain || !_terrain->get_data()) { \
		LOG(ERROR, mesg);                     \
		return ret;                           \
	}

// Constants
inline constexpr Color COLOR_NAN{ NAN, NAN, NAN, NAN };
inline constexpr Color COLOR_BLACK{ 0.0f, 0.0f, 0.0f, 1.0f };
inline constexpr Color COLOR_WHITE{ 1.0f, 1.0f, 1.0f, 1.0f };
inline constexpr Color COLOR_ROUGHNESS{ 1.0f, 1.0f, 1.0f, 0.5f };
inline constexpr Color COLOR_CHECKED{ 1.f, 1.f, 1.0f, 1.0f };
inline constexpr Color COLOR_NORMAL{ 0.5f, 0.5f, 1.0f, 1.0f };
inline constexpr Color COLOR_CONTROL{ std::bit_cast<float>(uint32_t{ true & 0x1 }), 0.f, 0.f, 1.0f };

inline constexpr real_t Math_PI = std::numbers::pi_v<real_t>;
inline constexpr real_t Math_SQRT2 = std::numbers::sqrt2_v<real_t>;
inline constexpr real_t Math_SQRT12 = real_t{ 0.5f } * Math_SQRT2;
inline constexpr real_t Math_LN2 = std::numbers::ln2_v<real_t>;
inline constexpr real_t Math_TAU = real_t{ 2.f } * Math_PI;
inline constexpr real_t Math_E = std::numbers::e_v<real_t>;

// For consistency between MSVC, gcc, clang
#ifndef FLT_MAX
#define FLT_MAX __FLT_MAX__
#endif
#ifndef FLT_MIN
#define FLT_MIN __FLT_MIN__
#endif

#define V2(x) Vector2(x, x)
#define V2I(x) Vector2i(x, x)
inline constexpr Vector2 V2_ZERO{ 0.f, 0.f };
inline constexpr Vector2i V2I_ZERO{ 0, 0 };
inline constexpr Vector2 V2_MAX{ FLT_MAX, FLT_MAX };
inline constexpr Vector2i V2I_MAX{ INT32_MAX, INT32_MAX };

#define V3(x) Vector3(x, x, x)
#define V3_(x) Vector3(x, 0.f, x)
inline constexpr Vector3 V3_ZERO{ 0.f, 0.f, 0.f };
inline constexpr Vector3 V3_MAX{ FLT_MAX, FLT_MAX, FLT_MAX };

// WorldScape3D::_warnings is uint8_t
inline constexpr uint8_t WARN_MISMATCHED_SIZE = 0x01;
inline constexpr uint8_t WARN_MISMATCHED_FORMAT = 0x02;
inline constexpr uint8_t WARN_MISMATCHED_MIPMAPS = 0x04;
inline constexpr uint8_t WARN_ALL = 0xFF;

// Global Types

struct Vector2iHash {
	std::size_t operator()(const Vector2i &v) const {
		std::size_t h1 = std::hash<int>()(v.x);
		std::size_t h2 = std::hash<int>()(v.y);
		return h1 ^ (h2 << 1);
	}
};

struct Vector3Hash {
	std::size_t operator()(const Vector3 &v) const {
		std::size_t h1 = std::hash<float>()(v.x);
		std::size_t h2 = std::hash<float>()(v.y);
		std::size_t h3 = std::hash<float>()(v.z);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

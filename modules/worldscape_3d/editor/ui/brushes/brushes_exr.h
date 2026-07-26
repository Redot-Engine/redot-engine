/**************************************************************************/
/*  brushes_exr.h                                                         */
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

// Terrain3D Godot plugin: Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#pragma once

// Acrylic
#include "acrylic1.h"

// Circles
#include "circle0.h"
#include "circle1.h"
#include "circle2.h"
#include "circle3.h"
#include "circle4.h"

// Hills
#include "hill1.h"
#include "hill2.h"

// Mountains
#include "mountain1.h"
#include "mountain2.h"
#include "mountain3.h"
#include "mountain4.h"

// Peaks
#include "peak1.h"
#include "peak2.h"
#include "peak3.h"

// Rings
#include "ring1.h"

// Smoke
#include "smoke.h"

// Squares
#include "square1.h"
#include "square2.h"
#include "square3.h"
#include "square4.h"
#include "square5.h"

// Stones
#include "stones.h"

// Terrain
#include "terrain1.h"
#include "terrain2.h"
#include "terrain3.h"
#include "terrain4.h"
#include "terrain5.h"
#include "terrain6.h"

// Textures
#include "texture1.h"
#include "texture2.h"
#include "texture3.h"
#include "texture4.h"
#include "texture5.h"

// Vegetation
#include "vegetation1.h"

#include <array>
#include <span>
#include <utility>

constexpr auto BRUSHES = std::array{
	std::pair{ "Acrylic", std::span{ acrylic1, acrylic1_len } },
	std::pair{ "Circle0", std::span{ circle0, circle0_len } },
	std::pair{ "Circle1", std::span{ circle1, circle1_len } },
	std::pair{ "Circle2", std::span{ circle2, circle2_len } },
	std::pair{ "Circle3", std::span{ circle3, circle3_len } },
	std::pair{ "Circle4", std::span{ circle4, circle4_len } },
	std::pair{ "Hill1", std::span{ hill1, hill1_len } },
	std::pair{ "Hill2", std::span{ hill2, hill2_len } },
	std::pair{ "Mountain1", std::span{ mountain1, mountain1_len } },
	std::pair{ "Mountain2", std::span{ mountain2, mountain2_len } },
	std::pair{ "Mountain3", std::span{ mountain3, mountain3_len } },
	std::pair{ "Mountain4", std::span{ mountain4, mountain4_len } },
	std::pair{ "Peak1", std::span{ peak1, peak1_len } },
	std::pair{ "Peak2", std::span{ peak2, peak2_len } },
	std::pair{ "Peak3", std::span{ peak3, peak3_len } },
	std::pair{ "Smoke", std::span{ smoke, smoke_len } },
	std::pair{ "Square1", std::span{ square1, square1_len } },
	std::pair{ "Square2", std::span{ square2, square2_len } },
	std::pair{ "Square3", std::span{ square3, square3_len } },
	std::pair{ "Square4", std::span{ square4, square4_len } },
	std::pair{ "Square5", std::span{ square5, square5_len } },
	std::pair{ "Stones", std::span{ stones, stones_len } },
	std::pair{ "Terrain1", std::span{ terrain1, terrain1_len } },
	std::pair{ "Terrain2", std::span{ terrain2, terrain2_len } },
	std::pair{ "Terrain3", std::span{ terrain3, terrain3_len } },
	std::pair{ "Terrain4", std::span{ terrain4, terrain4_len } },
	std::pair{ "Terrain5", std::span{ terrain5, terrain5_len } },
	std::pair{ "Terrain6", std::span{ terrain6, terrain6_len } },
	std::pair{ "Texture1", std::span{ texture1, texture1_len } },
	std::pair{ "Texture2", std::span{ texture2, texture2_len } },
	std::pair{ "Texture3", std::span{ texture3, texture3_len } },
	std::pair{ "Texture4", std::span{ texture4, texture4_len } },
	std::pair{ "Texture5", std::span{ texture5, texture5_len } },
	std::pair{ "Vegetation", std::span{ vegetation1, vegetation1_len } }
};

constexpr const unsigned char *DEFAULT_BRUSH = circle0;

/**************************************************************************/
/*  physx_blast_icons.h                                                   */
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

#ifdef GODOT_PHYSX_BLAST

#include "core/object/ref_counted.h"

class Texture2D;

// A small flat-color "cracked square" icon for PhysXBlastAsset -- same
// square-resource silhouette and accent color (#ffca5f) core's own
// ArrayMesh/Mesh icons use, split by a jagged gap so it reads as "fractured"
// rather than a plain mesh at the FileSystem dock's icon size. Built from an
// inline SVG string (Image::load_svg_from_string) rather than a loose file:
// this module ships as part of the engine binary, not a project/addon, so
// there's no res:// path a loose .svg under modules/ could be loaded from.
// Null on failure (e.g. an editor build without the svg module) -- callers
// should treat that as "no custom icon, keep the engine's generic fallback".
Ref<Texture2D> physx_blast_asset_make_icon();

#endif // GODOT_PHYSX_BLAST

/**************************************************************************/
/*  physx_blast_icons.cpp                                                 */
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

//////////////////////////////////////////////////////////////////////////
/*  physx_blast_icons.cpp                                               */
//////////////////////////////////////////////////////////////////////////
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/* Copyright (c) 2024-present Redot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */

#include "physx_blast_icons.h"

#ifdef GODOT_PHYSX_BLAST

#include "core/io/image.h"
#include "scene/resources/image_texture.h"

Ref<Texture2D> physx_blast_asset_make_icon() {
	// A square (same silhouette/accent color as core's ArrayMesh.svg) split
	// by a jagged gap into two pieces -- two subpaths in one <path>, same
	// technique ArrayMesh.svg itself uses for its four dots.
	static const char *svg =
			"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\">"
			"<path fill=\"#ffca5f\" d=\"M2 2H8.4L6.4 6L9.4 8L5.4 11L7.4 14H2Z "
			"M9.6 2H14V14H8.6L6.6 11L9.6 8L7.6 6Z\"/></svg>";

	Ref<Image> img;
	img.instantiate();
	if (img->load_svg_from_string(svg) != OK) {
		return Ref<Texture2D>();
	}
	return ImageTexture::create_from_image(img);
}

#endif // GODOT_PHYSX_BLAST

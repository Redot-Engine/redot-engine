/**************************************************************************/
/*  generated_texture.cpp                                                 */
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

#include "servers/rendering_server.h"

#include "generated_texture.h"
#include "logger.h"
#include "worldscape_3d.h"

///////////////////////////
// Public Functions
///////////////////////////

void GeneratedTexture::clear() {
	if (_rid.is_valid()) {
		LOG(EXTREME, "GeneratedTexture freeing ", _rid);
		RenderingServer::get_singleton()->free(_rid);
	}
	if (_image.is_valid()) {
		LOG(EXTREME, "GeneratedTexture unref image", _image);
		_image.unref();
	}
	_rid = RID();
	_dirty = true;
}

RID GeneratedTexture::create(const TypedArray<Image> &p_layers) {
	if (!p_layers.is_empty()) {
		Vector<Ref<Image>> layers;
		if (WorldScape3D::debug_level >= DEBUG) {
			LOG(EXTREME, "RenderingServer creating Texture2DArray, layers size: ", p_layers.size());
		}
		for (int i = 0; i < p_layers.size(); i++) {
			Ref<Image> img = p_layers[i];
			if (img.is_valid()) {
				layers.push_back(img);
				if (WorldScape3D::debug_level >= DEBUG) {
					LOG(EXTREME, i, ": ", img, ", empty: ", img->is_empty(), ", size: ", img->get_size(), ", format: ", img->get_format());
				}
			}
		}
		_rid = RenderingServer::get_singleton()->texture_2d_layered_create(layers, RenderingServer::TEXTURE_LAYERED_2D_ARRAY);
		_dirty = false;
	} else {
		clear();
	}
	return _rid;
}

void GeneratedTexture::update(const Ref<Image> &p_image, const int p_layer) {
	LOG(EXTREME, "RenderingServer updating Texture2DArray at index: ", p_layer);
	RenderingServer::get_singleton()->texture_2d_update(_rid, p_image, p_layer);
}

RID GeneratedTexture::create(const Ref<Image> &p_image) {
	LOG(EXTREME, "RenderingServer creating Texture2D");
	_image = p_image;
	_rid = RenderingServer::get_singleton()->texture_2d_create(_image);
	_dirty = false;
	return _rid;
}

/**************************************************************************/
/*  mode7_sprite_2d.h                                                           */
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
 * @file sprite_2d.h
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "scene/2d/sprite_2d.h"
#include "scene/resources/texture.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/2d/mode7_scanline_override.h"

class Mode7Sprite2D : public Sprite2D {
	GDCLASS(Mode7Sprite2D, Sprite2D);

	/// @name Mode 7
	/// @{
	bool mode7_enabled = false;
	TypedArray<Mode7ScanlineOverride> mode7_scanline_overrides;// Array of Transform2D, one per output row (UV.y band)

	Ref<ShaderMaterial> _mode7_material;
	Ref<ImageTexture> _mode7_scanline_tex;

	void _mode7_rebuild_material();
	void _mode7_rebuild_scanline_texture();
	void _on_mode7_override_changed();

	bool mode7_tiling = false;
	RS::CanvasItemTextureRepeat _saved_texture_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;

	real_t  mode7_global_rotation = 0.0f;
	Vector2 mode7_global_pivot    = Vector2(0.5f, 0.5f);
	/// @}

protected:
	static void _bind_methods();

public:
	/// @name Mode7
	/// @{
	void set_mode7_enabled(bool p_enabled);
	bool is_mode7_enabled() const;

	void set_mode7_scanline_overrides(const TypedArray<Mode7ScanlineOverride> &p_overrides);
	TypedArray<Mode7ScanlineOverride> get_mode7_scanline_overrides() const;

	void set_mode7_tiling(bool p_tiling);
	bool is_mode7_tiling() const;

	void    set_mode7_global_rotation(real_t p_radians);
	real_t  get_mode7_global_rotation() const;
	void    set_mode7_global_pivot(const Vector2 &p_pivot);
	Vector2 get_mode7_global_pivot() const;
	/// @}

	Mode7Sprite2D();
};

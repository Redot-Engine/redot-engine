/**************************************************************************/
/*  image_frames.h                                                        */
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

#include "core/io/image.h"
#include "core/io/resource.h"
#include "core/variant/variant.h"

class ImageFrames;
typedef Ref<ImageFrames> (*ImageFramesMemLoadFunc)(const uint8_t *p_png, int p_size, int p_max_frames);

class ImageFrames : public Resource {
	GDCLASS(ImageFrames, Resource);

public:
	static inline ImageFramesMemLoadFunc _gif_mem_loader_func = nullptr;
	static inline ImageFramesMemLoadFunc _apng_mem_loader_func = nullptr;
	static inline ImageFramesMemLoadFunc _webp_mem_loader_func = nullptr;

private:
	struct Frame {
		Ref<Image> image;
		float delay = 1.0;
	};

	Vector<Frame> frames;
	int loop_count = 0;

	Error _load_from_buffer(const Vector<uint8_t> &p_array, ImageFramesMemLoadFunc p_loader, int p_max_frames);

protected:
	static void _bind_methods();

public:
	void set_frame_count(int p_frames);
	int get_frame_count() const;

	void set_frame_image(int p_frame, Ref<Image> p_image);
	Ref<Image> get_frame_image(int p_frame) const;

	void set_frame_delay(int p_frame, float p_delay);
	float get_frame_delay(int p_frame) const;

	void set_loop_count(int p_loop);
	int get_loop_count() const;

	bool is_empty() const;

	ImageFrames() = default; // Create empty image frames.
	ImageFrames(const uint8_t *p_mem_apng, int p_len);
	ImageFrames(const Vector<Ref<Image>> &p_images, float p_delay = 1.0); // Import images from an image vector and delay.
	ImageFrames(const Vector<Ref<Image>> &p_images, const Vector<float> &p_delays); // Import images from an image vector and delay vector.

	~ImageFrames() {}

	Error load(const String &p_path);
	static Ref<ImageFrames> load_from_file(const String &p_path);

	Error load_apng_from_buffer(const PackedByteArray &p_array, int p_max_frames = 0);
	Error load_webp_from_buffer(const PackedByteArray &p_array, int p_max_frames = 0);
	Error load_gif_from_buffer(const PackedByteArray &p_array, int p_max_frames = 0);

	void copy_internals_from(const Ref<ImageFrames> &p_frames) {
		ERR_FAIL_COND_MSG(p_frames.is_null(), "Cannot copy image internals: invalid ImageFrames object.");
		frames = p_frames->frames;
	}
};

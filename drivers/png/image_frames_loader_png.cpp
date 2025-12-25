/**************************************************************************/
/*  image_frames_loader_png.cpp                                           */
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

#include "image_frames_loader_png.h"

#include "drivers/png/png_driver_common.h"

Error ImageFramesLoaderPNG::load_image_frames(Ref<ImageFrames> p_image, Ref<FileAccess> f, BitField<ImageFramesFormatLoader::LoaderFlags> p_flags, float p_scale, int p_max_frames) {
	const uint64_t buffer_size = f->get_length();
	Vector<uint8_t> file_buffer;
	Error err = file_buffer.resize(buffer_size);
	if (err) {
		return err;
	}
	{
		uint8_t *writer = file_buffer.ptrw();
		f->get_buffer(writer, buffer_size);
	}
	const uint8_t *reader = file_buffer.ptr();
	return PNGDriverCommon::apng_to_image_frames(reader, buffer_size, p_flags & FLAG_FORCE_LINEAR, p_max_frames, p_image);
}

void ImageFramesLoaderPNG::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("png");
	p_extensions->push_back("apng");
}

Ref<ImageFrames> ImageFramesLoaderPNG::load_mem_apng(const uint8_t *p_png, int p_size, int p_max_frames) {
	Ref<ImageFrames> img_frames;
	img_frames.instantiate();

	// the value of p_force_linear does not matter since it only applies to 16 bit
	Error err = PNGDriverCommon::apng_to_image_frames(p_png, p_size, false, p_max_frames, img_frames);
	ERR_FAIL_COND_V(err, Ref<ImageFrames>());

	return img_frames;
}

ImageFramesLoaderPNG::ImageFramesLoaderPNG() {
	ImageFrames::_apng_mem_loader_func = load_mem_apng;
}

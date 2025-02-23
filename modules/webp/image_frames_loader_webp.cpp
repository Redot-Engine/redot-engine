/**************************************************************************/
/*  image_frames_loader_webp.cpp                                          */
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

#include "image_frames_loader_webp.h"

#include "webp_common.h"

static Ref<ImageFrames> _webp_mem_loader_func(const uint8_t *p_webp_data, int p_size, int p_max_frames) {
	Ref<ImageFrames> frames;
	frames.instantiate();
	Error err = WebPCommon::webp_load_image_frames_from_buffer(frames.ptr(), p_webp_data, p_size, p_max_frames);
	ERR_FAIL_COND_V(err, Ref<Image>());
	return frames;
}

Error ImageFramesLoaderWebP::load_image_frames(Ref<ImageFrames> p_image, Ref<FileAccess> f, BitField<ImageFramesFormatLoader::LoaderFlags> p_flags, float p_scale, int p_max_frames) {
	Vector<uint8_t> src_image;
	uint64_t src_image_len = f->get_length();
	ERR_FAIL_COND_V(src_image_len == 0, ERR_FILE_CORRUPT);
	src_image.resize(src_image_len);

	uint8_t *w = src_image.ptrw();

	f->get_buffer(&w[0], src_image_len);

	Error err = WebPCommon::webp_load_image_frames_from_buffer(p_image.ptr(), w, src_image_len, p_max_frames);

	return err;
}

void ImageFramesLoaderWebP::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("webp");
}

ImageFramesLoaderWebP::ImageFramesLoaderWebP() {
	ImageFrames::_webp_mem_loader_func = _webp_mem_loader_func;
}

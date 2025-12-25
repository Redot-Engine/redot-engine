/**************************************************************************/
/*  test_image_frames.h                                                   */
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
#include "core/io/image_frames.h"
#include "core/os/os.h"

#include "tests/test_utils.h"

#include "modules/modules_enabled.gen.h"

#include "thirdparty/doctest/doctest.h"

namespace TestImageFrames {

TEST_CASE("[ImageFrames] Instantiation") {
	Vector<Ref<Image>> images = { memnew(Image(8, 4, false, Image::FORMAT_RGBA8)), memnew(Image(16, 8, false, Image::FORMAT_RGBA8)) };
	Ref<ImageFrames> image_frames = memnew(ImageFrames({ images }));
	CHECK_MESSAGE(
			!image_frames->is_empty(),
			"Image frames created with images should not be empty at first.");

	for (int index = 0; index < image_frames->get_frame_count(); index++) {
		PackedByteArray image_data = image_frames->get_frame_image(index)->get_data();
		for (int i = 0; i < image_data.size(); i++) {
			CHECK_MESSAGE(
					image_data[i] == 0,
					"An image of image frames created without data specified should have its data zeroed out.");
		}
	}

	Ref<ImageFrames> image_frames_copy = memnew(ImageFrames());
	CHECK_MESSAGE(
			image_frames_copy->is_empty(),
			"Image frames created without any specified images should be empty at first.");
	image_frames_copy->copy_internals_from(image_frames);

	CHECK_MESSAGE(
			image_frames->get_frame_count() == image_frames_copy->get_frame_count(),
			"Duplicated image frames should have the same frame count.");

	for (int index = 0; index < image_frames->get_frame_count(); index++) {
		CHECK_MESSAGE(
				image_frames->get_frame_image(index)->get_data() == image_frames_copy->get_frame_image(index)->get_data(),
				"Duplicated image frames should have the same image data.");

		PackedByteArray image_data = image_frames->get_frame_image(index)->get_data();
		Ref<Image> image_from_data = memnew(Image(images[index]->get_width(), images[index]->get_height(), images[index]->has_mipmaps(), images[index]->get_format(), image_data));
		CHECK_MESSAGE(
				image_frames->get_frame_image(index)->get_data() == image_from_data->get_data(),
				"An image created from data of an image frame should have the same data of the original image.");
	}
}

TEST_CASE("[ImageFrames] Loading") {
	[[maybe_unused]] Error err = OK;

#ifdef MODULE_GIF_ENABLED
	// Load GIF
	Ref<ImageFrames> image_frames_gif = memnew(ImageFrames());
	Ref<FileAccess> f_gif = FileAccess::open(TestUtils::get_data_path("image_frames/icon.gif"), FileAccess::READ, &err);
	REQUIRE(f_gif.is_valid());
	PackedByteArray data_gif;
	data_gif.resize(f_gif->get_length() + 1);
	f_gif->get_buffer(data_gif.ptrw(), f_gif->get_length());
	CHECK_MESSAGE(
			image_frames_gif->load_gif_from_buffer(data_gif) == OK,
			"The GIF image frame should load successfully.");
#endif

#ifdef MODULE_WEBP_ENABLED
	// Load WebP
	Ref<ImageFrames> image_frames_webp = memnew(ImageFrames());
	Ref<FileAccess> f_webp = FileAccess::open(TestUtils::get_data_path("image_frames/icon.webp"), FileAccess::READ, &err);
	REQUIRE(f_webp.is_valid());
	PackedByteArray data_webp;
	data_webp.resize(f_webp->get_length() + 1);
	f_webp->get_buffer(data_webp.ptrw(), f_webp->get_length());
	CHECK_MESSAGE(
			image_frames_webp->load_webp_from_buffer(data_webp) == OK,
			"The WebP image should load successfully.");
#endif // MODULE_WEBP_ENABLED

	// Load APNG
	Ref<ImageFrames> image_frames_apng = memnew(ImageFrames());
	Ref<FileAccess> f_apng = FileAccess::open(TestUtils::get_data_path("image_frames/icon.apng"), FileAccess::READ, &err);
	REQUIRE(f_apng.is_valid());
	PackedByteArray data_apng;
	data_apng.resize(f_apng->get_length() + 1);
	f_apng->get_buffer(data_apng.ptrw(), f_apng->get_length());
	CHECK_MESSAGE(
			image_frames_apng->load_apng_from_buffer(data_apng) == OK,
			"The APNG image frame should load successfully.");
}

TEST_CASE("[ImageFrames] Basic getters") {
	Vector<Ref<Image>> images = { memnew(Image(8, 4, false, Image::FORMAT_RGBA8)), memnew(Image(16, 8, false, Image::FORMAT_L8)) };
	Vector<float> delays = { 0.1, 0.2 };
	Ref<ImageFrames> image_frames = memnew(ImageFrames(images, delays));
	CHECK(image_frames->get_frame_count() == images.size());
	CHECK(image_frames->get_loop_count() == 0);
	for (int index = 0; index < image_frames->get_frame_count(); index++) {
		CHECK(image_frames->get_frame_image(index) == images[index]);
		CHECK(image_frames->get_frame_delay(index) == delays[index]);
	}
}

} //namespace TestImageFrames

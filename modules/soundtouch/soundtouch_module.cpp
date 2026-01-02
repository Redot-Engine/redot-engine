/**************************************************************************/
/*  soundtouch_module.cpp                                                 */
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

#include "soundtouch_module.h"

void SoundTouch::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tempo", "tempo"), &SoundTouch::set_tempo);
	ClassDB::bind_method(D_METHOD("put_samples", "samples"), &SoundTouch::put_samples);
	ClassDB::bind_method(D_METHOD("receive_samples"), &SoundTouch::receive_samples);
	ClassDB::bind_method(D_METHOD("flush"), &SoundTouch::flush);
}

SoundTouch::SoundTouch() {
	// TODO: expose these later so the user can set them
	st.setChannels(1);
	st.setSampleRate(44100);
}

SoundTouch::~SoundTouch() {}

void SoundTouch::set_tempo(float tempo) {
	st.setTempo(tempo);
}

void SoundTouch::put_samples(const PackedFloat32Array &p_samples) {
	const int count = p_samples.size();
	if (count == 0) {
		return;
	}

	// SoundTouch expects a raw pointer to floats
	st.putSamples(p_samples.ptr(), count);
}

PackedFloat32Array SoundTouch::receive_samples() {
	// We don't know how many samples SoundTouch has ready, so use a safe buffer
	const int max_samples = 65536;

	PackedFloat32Array out;
	out.resize(max_samples);

	int received = st.receiveSamples(out.ptrw(), max_samples);

	if (received <= 0) {
		return PackedFloat32Array();
	}

	out.resize(received);
	return out;
}

void SoundTouch::flush() {
	st.flush();
}

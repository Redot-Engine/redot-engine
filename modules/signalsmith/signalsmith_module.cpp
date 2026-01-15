/**************************************************************************/
/*  signalsmith_module.cpp                                                */
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

#include "signalsmith_module.h"

#include "core/os/memory.h"

#include <cmath>
#include <vector>

void SignalSmith::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sample_rate", "rate"), &SignalSmith::set_sample_rate);
	ClassDB::bind_method(D_METHOD("set_channels", "channels"), &SignalSmith::set_channels);
	ClassDB::bind_method(D_METHOD("set_pitch", "pitch"), &SignalSmith::set_pitch);
	ClassDB::bind_method(D_METHOD("set_tempo", "tempo"), &SignalSmith::set_tempo);
	ClassDB::bind_method(D_METHOD("reset"), &SignalSmith::reset);
	ClassDB::bind_method(D_METHOD("process", "input"), &SignalSmith::process);
}

SignalSmith::SignalSmith() {
	stretch.presetDefault(channels, sample_rate);
}

SignalSmith::~SignalSmith() {}

void SignalSmith::set_sample_rate(int p_rate) {
	if (p_rate < 1) {
		return;
	}

	sample_rate = p_rate;
	stretch.presetDefault(channels, sample_rate);
}

void SignalSmith::set_channels(int p_channels) {
	if (p_channels < 1) {
		return;
	}

	channels = p_channels;
	stretch.presetDefault(channels, sample_rate);
}

void SignalSmith::set_pitch(float p_pitch) {
	if (!(p_pitch > 0.0f)) {
		return;
	}

	stretch.setTransposeFactor(p_pitch);
}

void SignalSmith::set_tempo(float p_tempo) {
	if (!(p_tempo > 0.0f)) {
		return;
	}

	tempo = p_tempo;
}

void SignalSmith::reset() {
	stretch.reset();
}

PackedFloat32Array SignalSmith::process(const PackedFloat32Array &input) {
	PackedFloat32Array output;

	if (channels < 1) {
		return output;
	}

	const int total_samples = input.size();

	if (total_samples <= 0) {
		return output;
	}

	if (total_samples % channels != 0) {
		ERR_FAIL_V_MSG(output, "Input array size must be a multiple of channel count.");
	}

	const int input_frames = total_samples / channels;

	if (input_frames <= 0) {
		return output;
	}

	const float tf = (tempo > 0.0f) ? tempo : 1.0f;
	int output_frames = (int)std::lround((double)input_frames / (double)tf);

	if (output_frames < 0) {
		output_frames = 0;
	}

	// Deinterleave
	std::vector<std::vector<float>> in_ch;
	in_ch.resize((size_t)channels);

	for (int c = 0; c < channels; c++) {
		in_ch[(size_t)c].resize((size_t)input_frames);
	}

	const float *src = input.ptr();

	for (int i = 0; i < input_frames; i++) {
		const int base = i * channels;

		for (int c = 0; c < channels; c++) {
			in_ch[(size_t)c][(size_t)i] = src[base + c];
		}
	}

	// Output buffers
	std::vector<std::vector<float>> out_ch;
	out_ch.resize((size_t)channels);

	for (int c = 0; c < channels; c++) {
		out_ch[(size_t)c].assign((size_t)output_frames, 0.0f);
	}

	std::vector<float *> in_ptrs((size_t)channels, nullptr);
	std::vector<float *> out_ptrs((size_t)channels, nullptr);

	for (int c = 0; c < channels; c++) {
		in_ptrs[(size_t)c] = in_ch[(size_t)c].data();
		out_ptrs[(size_t)c] = out_ch[(size_t)c].data();
	}

	// Process: (inputs, inputSamples, outputs, outputSamples)
	stretch.process(in_ptrs.data(), input_frames, out_ptrs.data(), output_frames);

	// Interleave
	output.resize(output_frames * channels);
	float *dst = output.ptrw();

	for (int i = 0; i < output_frames; i++) {
		const int base = i * channels;

		for (int c = 0; c < channels; c++) {
			dst[base + c] = out_ch[(size_t)c][(size_t)i];
		}
	}

	return output;
}

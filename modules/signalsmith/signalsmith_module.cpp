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

#include "core/io/file_access.h"
#include "core/os/memory.h"

#include "modules/minimp3/audio_stream_mp3.h"
#include "scene/resources/audio_stream_wav.h"
#include "servers/audio/audio_stream.h"
#include "servers/audio_server.h"

#include <cmath>
#include <vector>

void SignalSmith::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sample_rate", "rate"), &SignalSmith::set_sample_rate);
	ClassDB::bind_method(D_METHOD("set_channels", "channels"), &SignalSmith::set_channels);
	ClassDB::bind_method(D_METHOD("set_pitch", "pitch"), &SignalSmith::set_pitch);
	ClassDB::bind_method(D_METHOD("set_tempo", "tempo"), &SignalSmith::set_tempo);
	ClassDB::bind_method(D_METHOD("get_last_sample_rate"), &SignalSmith::get_last_sample_rate);
	ClassDB::bind_method(D_METHOD("get_last_channels"), &SignalSmith::get_last_channels);
	ClassDB::bind_method(D_METHOD("reset"), &SignalSmith::reset);
	ClassDB::bind_method(D_METHOD("process", "input"), &SignalSmith::process);
	ClassDB::bind_method(D_METHOD("change_tempo", "path", "tempo", "pitch"), &SignalSmith::change_tempo, DEFVAL(1.0f));
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

int SignalSmith::get_last_sample_rate() const {
	return sample_rate;
}

int SignalSmith::get_last_channels() const {
	return channels;
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

Ref<AudioStreamWAV> SignalSmith::change_tempo(const String &path, float p_tempo, float p_pitch) {
	Ref<AudioStreamWAV> out;

	Ref<AudioStreamMP3> mp3 = AudioStreamMP3::load_from_file(path);
	ERR_FAIL_COND_V(mp3.is_null(), out);

	Ref<AudioStreamPlayback> pb_base = mp3->instantiate_playback();
	ERR_FAIL_COND_V(pb_base.is_null(), out);

	AudioStreamPlaybackResampled *pb = Object::cast_to<AudioStreamPlaybackResampled>(pb_base.ptr());

	ERR_FAIL_COND_V(pb == nullptr, out);

	pb->start(0.0);

	const int channels = mp3->is_monophonic() ? 1 : 2;
	const int sample_rate = AudioServer::get_singleton()->get_mix_rate();

	Vector<AudioFrame> frames;
	const int block = 1024;

	while (true) {
		int old = frames.size();
		frames.resize(old + block);
		int mixed = pb->mix(frames.ptrw() + old, 1.0f, block);

		if (mixed <= 0) {
			frames.resize(old);

			break;
		}
	}

	ERR_FAIL_COND_V(frames.is_empty(), out);

	// Convert AudioFrame to float PCM
	PackedFloat32Array input;
	input.resize(frames.size() * channels);
	float *input_w = input.ptrw();

	for (int i = 0; i < frames.size(); i++) {
		if (channels == 2) {
			input_w[i * 2 + 0] = frames[i].left;
			input_w[i * 2 + 1] = frames[i].right;
		} else {
			input_w[i] = frames[i].left;
		}
	}

	set_sample_rate(sample_rate);
	set_channels(channels);
	set_tempo(p_tempo);
	set_pitch(p_pitch);

	reset();

	PackedFloat32Array processed = process(input);
	ERR_FAIL_COND_V(processed.is_empty(), out);

	// convert float pcm to pcm16
	PackedByteArray pcm16;
	pcm16.resize(processed.size() * 2);
	uint8_t *pcm_w = pcm16.ptrw();

	const float *proc_r = processed.ptr();

	for (int i = 0; i < processed.size(); i++) {
		float s = CLAMP(proc_r[i], -1.0f, 1.0f);
		int16_t v = int16_t(s * 32767.0f);

		pcm_w[i * 2 + 0] = uint8_t(v & 0xff);
		pcm_w[i * 2 + 1] = uint8_t((v >> 8) & 0xff);
	}

	// Builds a streamable WAV
	out.instantiate();
	out->set_mix_rate(sample_rate);
	out->set_stereo(channels == 2);
	out->set_format(AudioStreamWAV::FORMAT_16_BITS);
	out->set_data(pcm16);

	return out;
}

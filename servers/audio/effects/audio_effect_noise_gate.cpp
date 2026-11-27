/**************************************************************************/
/*  audio_effect_noise_gate.cpp                                           */
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

#include "audio_effect_noise_gate.h"

#include "core/math/math_funcs.h"
#include "servers/audio/audio_server.h"

void AudioEffectNoiseGateInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	float mix_rate = AudioServer::get_singleton()->get_mix_rate();

	// Snapshot params once per buffer (audio thread reads them unsynchronized).
	float threshold = Math::db_to_linear(base->threshold_db);
	// -80 is the lowest value, and this maps it to 0 gain in that event - completely silent
	float floor_gain = base->threshold_db <= -80.0f ? 0.0f : Math::db_to_linear(base->attenuation_db);
	bool invert = base->invert;

	// One-pole coefficients from time constants.
	float attack_coeff = Math::exp(-1.0f / (MAX(base->attack_ms, 0.1f) * 0.001f * mix_rate));
	float release_coeff = Math::exp(-1.0f / (MAX(base->release_ms, 0.1f) * 0.001f * mix_rate));

	// The envelope detector gets its own fast, fixed release (~10 ms),
	// independent of release_ms. Its only job is to decide whether the
	// signal is above or below threshold.
	// release_ms belongs to the gain stage below.
	// 10 ms is short enough that the gate starts closing
	// promptly, but long enough to ride over a super low note, for example.
	float env_release_coeff = Math::exp(-1.0f / (0.010f * mix_rate));

	float env = envelope;
	float g = gain;

	if (!gain_initialized) {
		// First buffer: start at the resting gain instead of 0.
		// Normal mode rests at the floor (gate closed); inverted mode
		// rests at full gain (signal passes until the key exceeds
		// threshold). Starting at 0 would fade in audio that should pass unchanged.
		g = invert ? 1.0f : floor_gain;
		gain_initialized = true;
	}

	for (int i = 0; i < p_frame_count; i++) {
		float level = MAX(Math::abs(p_src_frames[i].left), Math::abs(p_src_frames[i].right));

		if (level > env) {
			// Signal got louder: snap the envelope up immediately so the
			// gate can react without delay ("instant attack").
			env = level;
		} else {
			// Signal got quieter: envelope falls quickly (fixed ~10 ms
			// constant) so the gate can react soon after the input
			// actually crosses the threshold.
			env = level + (env - level) * env_release_coeff;
		}

		// Gate target. Normal: full gain above threshold, floor below.
		// Inverted (ducker): floor above threshold, full gain below.
		float target;
		if (env >= threshold) {
			target = invert ? floor_gain : 1.0f;
		} else {
			target = invert ? 1.0f : floor_gain;
		}

		// Pick the coefficient by whether the gain is moving toward the
		// ACTIVE state, not by the key's direction. This keeps the
		// controls consistent when normal or inverted
		// attack_ms is always "how fast the effect engages" and release_ms is "how fast it lets go."
		//   Normal:   engaging = g rising toward 1.0 (gate opening).
		//   Inverted: engaging = g falling toward floor (duck engaging).
		bool engaging = (target > g) != invert;
		float coeff = engaging ? attack_coeff : release_coeff;
		g = target + (g - target) * coeff;

		// Exponential decay never quite reaches the target.
		// Once it's stupidly close, land the plane.
		if (Math::abs(g - target) < 1e-5f) {
			g = target;
		}

		p_dst_frames[i] = p_src_frames[i] * g;
	}

	envelope = env;
	gain = g;
}

Ref<AudioEffectInstance> AudioEffectNoiseGate::instantiate() {
	Ref<AudioEffectNoiseGateInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectNoiseGate>(this);
	return ins;
}

void AudioEffectNoiseGate::set_threshold_db(float p_threshold_db) {
	threshold_db = CLAMP(p_threshold_db, -80.0f, 0.0f);
}

float AudioEffectNoiseGate::get_threshold_db() const {
	return threshold_db;
}

void AudioEffectNoiseGate::set_attack_ms(float p_attack_ms) {
	attack_ms = CLAMP(p_attack_ms, 0.1f, 1000.0f);
}

float AudioEffectNoiseGate::get_attack_ms() const {
	return attack_ms;
}

void AudioEffectNoiseGate::set_release_ms(float p_release_ms) {
	release_ms = CLAMP(p_release_ms, 1.0f, 5000.0f);
}

float AudioEffectNoiseGate::get_release_ms() const {
	return release_ms;
}

void AudioEffectNoiseGate::set_attenuation_db(float p_attenuation_db) {
	attenuation_db = CLAMP(p_attenuation_db, -80.0f, 0.0f);
}

float AudioEffectNoiseGate::get_attenuation_db() const {
	return attenuation_db;
}

void AudioEffectNoiseGate::set_invert(bool p_invert) {
	invert = p_invert;
}

bool AudioEffectNoiseGate::get_invert() const {
	return invert;
}

void AudioEffectNoiseGate::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_threshold_db", "threshold_db"), &AudioEffectNoiseGate::set_threshold_db);
	ClassDB::bind_method(D_METHOD("get_threshold_db"), &AudioEffectNoiseGate::get_threshold_db);
	ClassDB::bind_method(D_METHOD("set_attack_ms", "attack_ms"), &AudioEffectNoiseGate::set_attack_ms);
	ClassDB::bind_method(D_METHOD("get_attack_ms"), &AudioEffectNoiseGate::get_attack_ms);
	ClassDB::bind_method(D_METHOD("set_release_ms", "release_ms"), &AudioEffectNoiseGate::set_release_ms);
	ClassDB::bind_method(D_METHOD("get_release_ms"), &AudioEffectNoiseGate::get_release_ms);
	ClassDB::bind_method(D_METHOD("set_attenuation_db", "attenuation_db"), &AudioEffectNoiseGate::set_attenuation_db);
	ClassDB::bind_method(D_METHOD("get_attenuation_db"), &AudioEffectNoiseGate::get_attenuation_db);
	ClassDB::bind_method(D_METHOD("set_invert", "invert"), &AudioEffectNoiseGate::set_invert);
	ClassDB::bind_method(D_METHOD("is_invert"), &AudioEffectNoiseGate::get_invert);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "threshold_db", PROPERTY_HINT_RANGE, "-80,0,0.1,suffix:dB"), "set_threshold_db", "get_threshold_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_ms", PROPERTY_HINT_RANGE, "0.1,1000,0.1,suffix:ms"), "set_attack_ms", "get_attack_ms");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "release_ms", PROPERTY_HINT_RANGE, "1,5000,1,suffix:ms"), "set_release_ms", "get_release_ms");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attenuation_db", PROPERTY_HINT_RANGE, "-80,0,0.1,suffix:dB"), "set_attenuation_db", "get_attenuation_db");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "invert"), "set_invert", "is_invert");
}

AudioEffectNoiseGate::AudioEffectNoiseGate() {
}

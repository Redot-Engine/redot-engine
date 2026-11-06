/**************************************************************************/
/*  audio_effect_noise_gate.h                                             */
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

#include "servers/audio/audio_effect.h"

class AudioEffectNoiseGate;

class AudioEffectNoiseGateInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectNoiseGateInstance, AudioEffectInstance);
	friend class AudioEffectNoiseGate;
	Ref<AudioEffectNoiseGate> base;

	/// @brief Input level follower (linear)
	/// @details `envelope` tracks the signal's loudness: it jumps up when the
	/// input gets louder, but falls back down gradually (over `release_ms`).
	float envelope = 0.0f;
	float gain = 0.0f; ///< current applied gain (linear)
	bool gain_initialized = false;

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
};

class AudioEffectNoiseGate : public AudioEffect {
	GDCLASS(AudioEffectNoiseGate, AudioEffect);

	friend class AudioEffectNoiseGateInstance;

	float threshold_db = -40.0f;
	float attack_ms = 10.0f; ///< Time for gate to open
	float release_ms = 200.0f; ///< Time for gate to close
	float attenuation_db = -60.0f; ///< Gain floor (how much the signal is reduced) when gate is closed. -80 = full mute
	bool invert = false; ///< Attenuate signals ABOVE the threshold instead of below

protected:
	static void _bind_methods();

public:
	Ref<AudioEffectInstance> instantiate() override;

	void set_threshold_db(float p_threshold_db);
	float get_threshold_db() const;

	void set_attack_ms(float p_attack_ms);
	float get_attack_ms() const;

	void set_release_ms(float p_release_ms);
	float get_release_ms() const;

	void set_attenuation_db(float p_attenuation_db);
	float get_attenuation_db() const;

	void set_invert(bool p_invert);
	bool get_invert() const;

	AudioEffectNoiseGate();
};

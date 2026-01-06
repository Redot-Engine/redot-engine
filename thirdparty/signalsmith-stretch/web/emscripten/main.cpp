#include "../../signalsmith-stretch.h"
#include <vector>

#include <emscripten.h>
int main() {}

using Sample = float;
using Stretch = signalsmith::stretch::SignalsmithStretch<Sample>;
Stretch stretch;

// Allocates memory for buffers, and returns it
std::vector<Sample> buffers;
std::vector<Sample *> buffersIn, buffersOut;

extern "C" {
	Sample * EMSCRIPTEN_KEEPALIVE setBuffers(int channels, int length) {
		buffers.resize(length*channels*2);
		Sample *data = buffers.data();
		buffersIn.resize(0);
		buffersOut.resize(0);
		for (int c = 0; c < channels; ++c) {
			buffersIn.push_back(data + length*c);
			buffersOut.push_back(data + length*(c + channels));
		}
		return data;
	}

	int EMSCRIPTEN_KEEPALIVE blockSamples() {
		return stretch.blockSamples();
	}
	int EMSCRIPTEN_KEEPALIVE intervalSamples() {
		return stretch.intervalSamples();
	}
	int EMSCRIPTEN_KEEPALIVE inputLatency() {
		return stretch.inputLatency();
	}
	int EMSCRIPTEN_KEEPALIVE outputLatency() {
		return stretch.outputLatency();
	}
	void EMSCRIPTEN_KEEPALIVE reset() {
		stretch.reset();
	}
	void EMSCRIPTEN_KEEPALIVE presetDefault(int nChannels, Sample sampleRate) {
		stretch.presetDefault(nChannels, sampleRate);
	}
	void EMSCRIPTEN_KEEPALIVE presetCheaper(int nChannels, Sample sampleRate) {
		stretch.presetCheaper(nChannels, sampleRate);
	}
	void EMSCRIPTEN_KEEPALIVE configure(int nChannels, int blockSamples, int intervalSamples, bool splitComputation) {
		stretch.configure(nChannels, blockSamples, intervalSamples, splitComputation);
	}
	void EMSCRIPTEN_KEEPALIVE setTransposeFactor(Sample multiplier, Sample tonalityLimit) {
		stretch.setTransposeFactor(multiplier, tonalityLimit);
	}
	void EMSCRIPTEN_KEEPALIVE setTransposeSemitones(Sample semitones, Sample tonalityLimit) {
		stretch.setTransposeSemitones(semitones, tonalityLimit);
	}
	void EMSCRIPTEN_KEEPALIVE setFormantFactor(Sample multiplier, bool compensate) {
		stretch.setFormantFactor(multiplier, compensate);
	}
	void EMSCRIPTEN_KEEPALIVE setFormantSemitones(Sample semitones, bool compensate) {
		stretch.setFormantSemitones(semitones, compensate);
	}
	void EMSCRIPTEN_KEEPALIVE setFormantBase(Sample freq) {
		stretch.setFormantBase(freq);
	}
	// We can't do setFreqMap()
	void EMSCRIPTEN_KEEPALIVE seek(int inputSamples, double playbackRate) {
		stretch.seek(buffersIn, inputSamples, playbackRate);
	}
	void EMSCRIPTEN_KEEPALIVE process(int inputSamples, int outputSamples) {
		stretch.process(buffersIn, inputSamples, buffersOut, outputSamples);
	}
	void EMSCRIPTEN_KEEPALIVE flush(int outputSamples) {
		stretch.flush(buffersOut, outputSamples);
	}
}

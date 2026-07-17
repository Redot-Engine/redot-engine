// helper for debugging
#include <iostream>
#define LOG_EXPR(expr) std::cout << #expr << " = " << (expr) << "\n";

#include "signalsmith-stretch/signalsmith-stretch.h"
using SignalsmithStretch = signalsmith::stretch::SignalsmithStretch<float>;

#include "./util/simple-args.h"
#include "./util/wav.h"

int main(int argc, char* argv[]) {
	SimpleArgs args(argc, argv);
	
	if (args.hasFlag("v", "prints the version")) {
		auto &version = SignalsmithStretch::version;
		std::cout << version[0] << "." << version[1] << "." << version[2] << "\n";
		return 0;
	}

	std::string inputWav = args.arg<std::string>("input.wav", "16-bit WAV file");
	std::string outputWav = args.arg<std::string>("output.wav", "output WAV file");
	double semitones = args.flag<double>("semitones", "pitch-shift amount", 0);
	double formants = args.flag<double>("formant", "formant-shift amount (semitones)", 0);
	bool formantComp = args.hasFlag("formant-comp", "formant compensation");
	double formantBase = args.flag<double>("formant-base", "formant base frequency (Hz, 0=auto)", 100);
	double tonality = args.flag<double>("tonality", "tonality limit (Hz)", 8000);
	double time = args.flag<double>("time", "time-stretch factor", 1);
	bool splitComputation = args.hasFlag("split-computation", "distributes the computation more evenly (but higher latency)");
	args.errorExit(); // exits on error, or with `--help`

	std::cout << inputWav << " -> " << outputWav << "\n";

	Wav inWav;
	if (!inWav.read(inputWav).warn()) args.errorExit("failed to read WAV");
	
	size_t inputLength = inWav.length();
	size_t outputLength = std::round(inputLength*time);
	
	Wav outWav;
	outWav.channels = inWav.channels;
	outWav.sampleRate = inWav.sampleRate;
	outWav.resize(outputLength);

	SignalsmithStretch stretch;
	stretch.presetDefault(int(inWav.channels), inWav.sampleRate, splitComputation);
	stretch.setTransposeSemitones(semitones, tonality/inWav.sampleRate);
	stretch.setFormantSemitones(formants, formantComp);
	stretch.setFormantBase(formantBase/inWav.sampleRate);

	/* Since the WAV helper allows sample access like `wav[c][index]`, we could just call:
	
		stretch.exact(inWav, int(inputLength), outWav, int(outputLength));
		
	However, we'll do it in separate stages to show more of the API. */
	
	// First, an "output seek", where we provide a chunk of input.
	// This is suitable for starting playback of a sample at a given playback rate.
	auto seekLength = stretch.outputSeekLength(1/time);
	stretch.outputSeek(inWav, seekLength);
	// At this point, the next output samples we get will correspond to the beginning of the audio file.

	// We're going to process until *just* before the end of the audio file (so we can get a tidier end using `.flush()`.
	int outputIndex = outputLength - stretch.intervalSamples();

	// Stretch's internal output position is slightly ahead of the output samples we get
	int outputPos = outputIndex + stretch.outputLatency();
	// Time-map: where do we want the input position to be at that moment?
	int inputPos = std::round(outputPos/time);
	// And therefore which input samples do we need to be supplying?
	int inputIndex = inputPos + stretch.inputLatency();
	
	// In this particular case, our `inputPos` will be at the end of the file
	// and `inputIndex` will be beyond the end, so we pad with 0s to have enough input
	inWav.resize(inputIndex);

	// OK, go for it
	inWav.offset = seekLength;
	stretch.process(inWav, inputIndex - seekLength, outWav, outputIndex);
	
	// And as promised, get the last bits using `.flush()`, which does some extra stuff to avoid introducing clicks.
	outWav.offset = outputIndex;
	stretch.flush(outWav, outputLength - outputIndex);
	outWav.offset = 0;

	if (!outWav.write(outputWav).warn()) args.errorExit("failed to write WAV");
}

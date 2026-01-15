// helper for debugging
#include <iostream>
#define LOG_EXPR(expr) std::cout << #expr << " = " << (expr) << "\n";

#define PROFILE_PLOT_CHUNKS
#ifdef PROFILE_PLOT_CHUNKS
size_t activeStepIndex = 0;
void profileProcessStart(int, int);
void profileProcessEndStep();
void profileProcessStep(size_t, size_t);
void profileProcessEnd();
#	define SIGNALSMITH_STRETCH_PROFILE_PROCESS_START profileProcessStart
#	define SIGNALSMITH_STRETCH_PROFILE_PROCESS_STEP profileProcessStep
#	define SIGNALSMITH_STRETCH_PROFILE_PROCESS_ENDSTEP profileProcessEndStep
#	define SIGNALSMITH_STRETCH_PROFILE_PROCESS_END profileProcessEnd
#endif

#include "signalsmith-stretch/signalsmith-stretch.h"

#include "./util/stopwatch.h"
#include "./util/memory-tracker.hxx"
#include "./util/simple-args.h"
#include "./util/wav.h"

#ifdef PROFILE_PLOT_CHUNKS
#include "plot/plot.h"
std::vector<signalsmith::Stopwatch> processStopwatches;
signalsmith::Stopwatch processStopwatchStart, processStopwatchEnd;
bool started = false;
bool activeStep = false;
void profileProcessStart(int /*inputSamples*/, int /*outputSamples*/) {
	activeStep = false;
	started = true;
	processStopwatchStart.startLap();
}
void profileProcessEndStep() {
	if (activeStep) {
		activeStep = false;
		processStopwatches[activeStepIndex].lap();
	} else if (started) {
		started = false;
		processStopwatchStart.lap();
	}
	processStopwatchEnd.startLap();
}
void profileProcessStep(size_t step, size_t count) {
	profileProcessEndStep();
	activeStep = true;
	activeStepIndex = step;
	if (processStopwatches.size() < count) {
		processStopwatches.resize(count);
	}
	processStopwatches[step].startLap();
}
void profileProcessEnd() {
	processStopwatchEnd.lap();
}
#endif

int main(int argc, char* argv[]) {
	signalsmith::stretch::SignalsmithStretch<float/*, std::ranlux48_base*/> stretch; // optional cheaper RNG for performance comparison

#ifdef PROFILE_PLOT_CHUNKS
	processStopwatches.reserve(1000);
#endif

	SimpleArgs args(argc, argv);
	
	if (args.hasFlag("v", "prints the version")) {
		std::cout << stretch.version[0] << "." << stretch.version[1] << "." << stretch.version[2] << "\n";
		return 0;
	}
	
	std::string inputWav = args.arg<std::string>("input.wav", "16-bit WAV file");
	std::string outputWav = args.arg<std::string>("output.wav", "output WAV file");
	
	double semitones = args.flag<double>("semitones", "pitch-shift amount", 0);
	double formants = args.flag<double>("formant", "formant-shift amount (semitones)", 0);
	bool formantComp = args.hasFlag("formant-comp", "formant compensation");
	double formantBase = args.flag<double>("formant-base", "formant base frequency (Hz, 0=auto)", 0);
	double tonality = args.flag<double>("tonality", "tonality limit (Hz)", 8000);
	double time = args.flag<double>("time", "time-stretch factor", 1);
	bool exactLength = args.hasFlag("exact", "trims the start/end so the output has the correct length");
	bool splitComputation = args.hasFlag("split-computation", "distributes the computation more evenly (but higher latency)");
	args.errorExit();
	
	std::cout << Console::Bright << inputWav << Console::Reset;
	std::cout << " -> ";
	std::cout << Console::Bright << outputWav << Console::Reset << "\n";
	std::cout << "\tsemitones: " << semitones << "\n\t     time: " << time << "x" << (exactLength ? " (exact)" : "") << "\n\t tonality: " << tonality << "Hz\n";

	Wav inWav;
	std::cout << inputWav << " -> " << outputWav << "\n";
	if (!inWav.read(inputWav).warn()) args.errorExit("failed to read WAV");
	size_t inputLength = inWav.samples.size()/inWav.channels;
	
	Wav prevWav; // Used during development, to compare against known-good previous render
	bool compareReference = (time <= 1.6);
	if (compareReference && !prevWav.read(outputWav + "-reference.wav")) {
		if (prevWav.read(outputWav)) {
			prevWav.write(outputWav + "-reference.wav");
		}
	}

	Wav outWav;
	outWav.channels = inWav.channels;
	outWav.sampleRate = inWav.sampleRate;
	int outputLength = std::round(inputLength*time);

	signalsmith::MemoryTracker initMemory;
	signalsmith::Stopwatch stopwatch;

	stopwatch.start();
	stretch.presetDefault(int(inWav.channels), inWav.sampleRate, splitComputation);
	stretch.setTransposeSemitones(semitones, tonality/inWav.sampleRate);
	stretch.setFormantSemitones(formants, formantComp);
	stretch.setFormantBase(formantBase/inWav.sampleRate);
	double initSeconds = stopwatch.lap();

	initMemory = initMemory.diff();
	std::cout << "Setup:\n\t" << initSeconds << "s\n";
	if (initMemory.implemented) {
		std::cout << "\tallocated " << (initMemory.allocBytes/1000) << "kB, freed " << (initMemory.freeBytes/1000) << "kB\n";
	}

	signalsmith::MemoryTracker processMemory;

	if (exactLength) {
		outWav.samples.resize(outputLength*outWav.channels);
		stopwatch.start();
		processMemory = {};
		stretch.exact(inWav, int(inputLength), outWav, int(outputLength));
	} else {
		// pad the input at the end, since we'll be reading slightly ahead
		size_t paddedInputLength = inputLength + stretch.inputLatency();
		inWav.samples.resize(paddedInputLength*inWav.channels);
		// pad the output at the end, since we have output latency as well
		int tailSamples = exactLength ? stretch.outputLatency() : (stretch.outputLatency() + stretch.inputLatency()); // if we don't need exact length, add a bit more output to catch any wobbles past the end
		int paddedOutputLength = outputLength + tailSamples;
		outWav.samples.resize(paddedOutputLength*outWav.channels);

		stopwatch.start();
		// The simplest way to deal with input latency (when have access to the audio buffer) is to always be slightly ahead in the input
		stretch.seek(inWav, stretch.inputLatency(), 1/time);
		inWav.offset += stretch.inputLatency();
		// Process it all in one call, although it works just the same if we split into smaller blocks
		processMemory = {};
		stretch.process(inWav, int(inputLength), outWav, int(outputLength));
		// Read the last bit of output without giving it any more input
		outWav.offset += outputLength;
		stretch.flush(outWav, tailSamples);
		outWav.offset -= outputLength;
	}

	double processSeconds = stopwatch.lap();
	double processRate = (inWav.length()/inWav.sampleRate)/processSeconds;
	double processPercent = 100/processRate;
	processMemory = processMemory.diff();
	std::cout << "Process:\n\t" << processSeconds << "s, " << processRate << "x realtime, " << processPercent << "% CPU\n";
	if (processMemory.implemented) {
		std::cout << "\tallocated " << (processMemory.allocBytes/1000) << "kB, freed " << (processMemory.freeBytes/1000) << "kB\n";
		if (processMemory) args.errorExit("allocated during process()");
	}
	
#ifdef PROFILE_PLOT_CHUNKS
	signalsmith::plot::Figure figure;
	auto &plot = figure(0, 0).plot(400, 150);
	plot.x.blank().label("step");
	plot.y.major(0, "");
	plot.title("computation time");
	auto &cumulativePlot = figure(1, 0).plot(150, 150);
	cumulativePlot.x.major(processStopwatches.size(), "");
	cumulativePlot.y.major(0, "");
	cumulativePlot.title("cumulative");
	auto &line = plot.line().fillToY(0);
	auto &extraLine = plot.line().fillToY(0);
	auto &cumulativeLine = cumulativePlot.line();
	auto &flatLine = cumulativePlot.line();
	double cumulativeTime = 0;
	line.add(0, 0);
	cumulativeLine.add(0, 0);
	for (size_t i = 0; i < processStopwatches.size(); ++i) {
		double time = processStopwatches[i].total();
		if (i%5 == 0) {
			plot.x.tick(i + 0.5, std::to_string(i));
		} else {
			plot.x.tick(i + 0.5, "");
		}
		line.add(i, time);
		line.add(i + 1, time);

		cumulativeTime += time;
		cumulativeLine.add(i, cumulativeTime);
		cumulativeLine.add(i + 1, cumulativeTime);
	}
	line.add(processStopwatches.size(), 0);
	extraLine.add(0, 0);
	extraLine.add(0, processStopwatchStart.total());
	extraLine.add(1, processStopwatchStart.total());
	extraLine.add(1, 0);
	extraLine.add(processStopwatches.size() - 1, 0);
	extraLine.add(processStopwatches.size() - 1, processStopwatchEnd.total());
	extraLine.add(processStopwatches.size(), processStopwatchEnd.total());
	extraLine.add(processStopwatches.size(), 0);
	flatLine.add(0, 0);
	flatLine.add(processStopwatches.size(), cumulativeTime);
	figure.write("profile.svg");
#endif

	if (!outWav.write(outputWav).warn()) args.errorExit("failed to write WAV");
	
	if (compareReference && prevWav.result) {
		outWav.read(outputWav);
		if (prevWav.length() != outWav.length()) args.errorExit("lengths differ");
		double diff2 = 0;
		for (size_t i = 0; i < prevWav.samples.size(); ++i) {
			double diff = prevWav.samples[i] - outWav.samples[i];
			diff2 += diff*diff;
		}
		diff2 /= prevWav.samples.size();
		double diffDb = 10*std::log10(diff2);
		std::cout << "Reference:\n\tdifference: ";
		if (diff2 < 1e-6) {
			std::cout << Console::Yellow;
		} else if (diff2 < 1e-10) {
			std::cout << Console::Green;
		} else {
			std::cout << Console::Red;
		}
		
		std::cout << Console::Bright << diffDb << Console::Reset << " dB\n";
		if (diffDb > -60) args.errorExit("too much difference\n");
	}
}

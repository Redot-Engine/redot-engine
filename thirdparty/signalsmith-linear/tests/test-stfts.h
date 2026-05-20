#include "../stft.h"
#include "./others/dsp/spectral.h"

#include "./test-runner.h"

#include "plot/plot.h"

template<class Sample, bool splitComputation, bool modified>
void testStft(size_t channels, size_t blockSize, size_t minInterval, size_t maxInterval, unsigned seed=24680) {
	signalsmith::plot::Figure figure;

	double debugTime = 0;
	auto debugTick = [&](){
		double t = debugTime;
		debugTime += 0.1;
		return t;
	};

	std::default_random_engine randomEngine(seed);
	std::uniform_int_distribution<size_t> intervalDist{minInterval, maxInterval};
	
	size_t length = blockSize*10;
	RunData<Sample> data(length);
	
	std::vector<Sample *> input, output;
	for (size_t c = 0; c < channels; ++c) {
		input.push_back(data.real(c));
		for (size_t i = 0; i < length; ++i) {
			input.back()[i] += 2*std::sin(i*2.0/blockSize);
		}
		output.push_back(data.real(c + channels));
	}
	
	signalsmith::linear::DynamicSTFT<Sample> stft;
	size_t extraInputHistory = 0, extraInputLatency = 0;
	{
		std::uniform_real_distribution<double> dist{0, 1};
		if (dist(randomEngine) > 0.5) {
			extraInputHistory = dist(randomEngine)*blockSize*2;
			extraInputLatency = std::round(dist(randomEngine)*extraInputHistory);
		}
	}
	size_t defaultInterval = std::max<size_t>(intervalDist(randomEngine), blockSize/16);
	stft.configure(channels, channels, blockSize, extraInputHistory, defaultInterval);
	
	const size_t plotChannel = 0;
	
	auto &resultPlot = figure(0, -1).plot(800, 200);
	resultPlot.y.major(0);
	auto &outputLine = resultPlot.line();
	auto &inputLine = resultPlot.line();
	auto &diffLine = resultPlot.line();
	resultPlot.legend(1, 1).add(outputLine, "output").add(inputLine, "input").add(diffLine, "error");
	for (size_t i = 0; i < length; ++i) {
		inputLine.add(i, input[plotChannel][i]);
	}
	
#ifdef STFT_DEBUG_PRIVATE
	auto &inputPlot = figure(0, 0)(0, 0).plot(200, 200).title("input");
	inputPlot.y.major(0);
	auto &historyLine = inputPlot.line();
	auto &inputTimeLine = inputPlot.line();

	auto &outputPlot = figure(0, 0)(1, 0).plot(200, 200).title("output");
	outputPlot.y.major(0);
	auto &sumLine = outputPlot.line();
	auto &outputTimeLine = outputPlot.line();
	auto &windowProductLine = outputPlot.line();
	outputPlot.legend(2, 1).add(sumLine, "rolling buffer").add(outputTimeLine, "current block (centred)").add(windowProductLine, "window product");
#endif

	auto &windowPlot = figure(0, 0)(0, 1).plot(200, 200).title("window");
	windowPlot.y.major(0);
	windowPlot.x.major(0).minors(-int(blockSize)/2, int(blockSize)/2);
	auto &aWindowLine = windowPlot.line();
	auto &sWindowLine = windowPlot.line();
	windowPlot.legend(-1, 1).add(aWindowLine, "analysis").add(sWindowLine, "synthesis");

	auto &spectrumPlot = figure(0, 0)(1, 1).plot(200, 200).title("spectrum");
	spectrumPlot.y.major(0);
	auto &realLine = spectrumPlot.line();
	auto &imagLine = spectrumPlot.line();
	
	{
		std::uniform_real_distribution<double> dist{-1, 1};
		int maxOffsetWobble = int(blockSize - maxInterval - 1);
		stft.analysisOffset(blockSize*2);
		stft.synthesisOffset(0);
		while (std::abs(int(stft.analysisOffset()) - int(stft.synthesisOffset())) > maxOffsetWobble) {
			stft.analysisOffset(blockSize/2 + dist(randomEngine)*maxOffsetWobble);
			stft.synthesisOffset(blockSize/2 + dist(randomEngine)*maxOffsetWobble);
		}
		int offset = int(stft.analysisOffset()) - int(stft.synthesisOffset());
		
		if (dist(randomEngine) < 0.5) {
			for (int i = 0; i < offset; ++i) {
				stft.analysisWindow()[i] = 0;
			}
			for (int i = offset; i < 0; ++i) {
				stft.analysisWindow()[blockSize + i] = 0;
			}
		} else {
			for (int i = 0; i < -offset; ++i) {
				stft.synthesisWindow()[i] = 0;
			}
			for (int i = -offset; i < 0; ++i) {
				stft.synthesisWindow()[blockSize + i] = 0;
			}
		}
	}
	stft.reset(); // The initial window weights depend on the offsets

	size_t totalLatency = stft.latency() + extraInputLatency;

	size_t start = 0;
	while (start + blockSize < length) {
		size_t interval = intervalDist(randomEngine);
		
		// Randomly mess the windows up to make sure we still have perfect reconstruction
		{
			std::uniform_real_distribution<double> dist{0, 1};
			Sample factor = 0.5 + 1*dist(randomEngine);
			size_t windowModWidth = dist(randomEngine)*blockSize/2;
			size_t windowModPos = dist(randomEngine)*(blockSize - windowModWidth);
			for (size_t i = 0; i < windowModWidth; i++) {
				stft.analysisWindow()[i + windowModPos] *= factor;
			}
		}
		{
			std::uniform_real_distribution<double> dist{0, 1};
			Sample factor = 0.5 + 1*dist(randomEngine);
			size_t windowModWidth = dist(randomEngine)*blockSize/2;
			size_t windowModPos = dist(randomEngine)*(blockSize - windowModWidth);
			for (size_t i = 0; i < windowModWidth; i++) {
				stft.synthesisWindow()[i + windowModPos] *= factor;
			}
		}
		
		for (size_t i = 0; i < blockSize; ++i) {
			aWindowLine.add(i*1.0 - stft.analysisOffset(), stft.analysisWindow()[i]);
			sWindowLine.add(i*1.0 - stft.synthesisOffset(), stft.synthesisWindow()[i]);
		}
		windowPlot.toFrame(debugTick());
		
		for (size_t c = 0; c < channels; ++c) {
			stft.readOutput(c, 0, interval, output[c] + start);
		}
#ifdef STFT_DEBUG_PRIVATE
		for (size_t i = 0; i < blockSize; ++i) {
			sumLine.add(i, stft.sumBuffer[i]);
			windowProductLine.add(i, stft.sumWindowProducts[i]);
		}
		for (size_t i = 0; i < stft.timeBuffer.size(); ++i) {
			outputTimeLine.add(i, stft.timeBuffer[i]);
		}
		sumLine.marker(stft.outputPos, 0);
		outputPlot.toFrame(debugTick());
#endif

		for (size_t c = 0; c < channels; ++c) {
			stft.writeInput(c, 0, interval, input[c] + start);
		}
		stft.moveInput(interval);

		stft.analyse(extraInputLatency);

#ifdef STFT_DEBUG_PRIVATE
		for (size_t i = 0; i < blockSize; ++i) {
			historyLine.add(i, stft.inputBuffer[i]);
		}
		historyLine.marker(stft.inputPos, 0);
		for (size_t i = 0; i < stft.timeBuffer.size(); ++i) {
			inputTimeLine.add(i, stft.timeBuffer[i]);
		}
		inputPlot.toFrame(debugTick());
#endif
		
		for (size_t f = 0; f < stft.bands(); ++f) {
			realLine.add(f, stft.spectrum(plotChannel)[f].real());
			imagLine.add(f, stft.spectrum(plotChannel)[f].imag());
		}
		spectrumPlot.toFrame(debugTick());
		
		// randomise phase (for debugging)
		/*
		for (size_t f = 0; f < stft.bands(); ++f) {
			auto &v = stft.spectrum(plotChannel)[f];
			std::uniform_real_distribution<Sample> dist{0, 2*M_PI};
			v *= std::polar(Sample(1), dist(randomEngine));
		}
		*/

		stft.moveOutput(interval);
		stft.synthesise();

#ifdef STFT_DEBUG_PRIVATE
		for (size_t i = 0; i < blockSize; ++i) {
			sumLine.add(i, stft.sumBuffer[i]);
			windowProductLine.add(i, stft.sumWindowProducts[i]);
		}
		for (size_t i = 0; i < stft.timeBuffer.size(); ++i) {
			outputTimeLine.add(i, stft.timeBuffer[i]);
		}
		sumLine.marker(stft.outputPos, 0);
		outputPlot.toFrame(debugTick());
#endif

		start += interval;

		for (size_t i = 0; i < start; ++i) {
			outputLine.add(i, output[plotChannel][i]);
			Sample v = (i < totalLatency) ? Sample(0) : input[plotChannel][i - totalLatency];
			diffLine.add(i, output[plotChannel][i] - v);
		}
		outputLine.toFrame(debugTick());
		diffLine.toFrame(debugTick());
	}
	
	double error = 0;
	for (size_t i = 0; i + totalLatency < start; ++i) {
		for (size_t c = 0; c < channels; ++c) {
			error += std::abs(output[c][i + totalLatency] - input[c][i]);
		}
	}
	figure.loopFrame(debugTick());
	figure.write("stft-debug.svg");

	if (error > length*0.001) {
		LOG_EXPR(error);
		LOG_EXPR(typeid(Sample).name());
		LOG_EXPR(splitComputation);
		LOG_EXPR(modified);
		LOG_EXPR(channels);
		LOG_EXPR(blockSize);
		LOG_EXPR(minInterval);
		LOG_EXPR(maxInterval);
		LOG_EXPR(stft.bands());
		LOG_EXPR(stft.analysisOffset());
		LOG_EXPR(stft.synthesisOffset());
		LOG_EXPR(stft.latency());
		abort();
	}
}

void compareStfts() {
	signalsmith::linear::DynamicSTFT<float, false, true> dynamic;
	dynamic.configure(1, 1, 255, 0);
	dynamic.setInterval(45, dynamic.kaiser);
	
	signalsmith::spectral::STFT<float> dsp(1, 255, 45);
	dsp.setWindow(dsp.kaiser, true);
	
	signalsmith::linear::ModifiedRealFFT<float> mrfft(255);
	
	std::vector<float> input(256), output, tmp(256);

	std::default_random_engine randomEngine(12345);
	std::uniform_real_distribution<float> dist{-1, 1};
	
	signalsmith::plot::Figure figure;
	auto &plot = figure(0, 0).plot();
	plot.y.major(0);
	auto &real1 = plot.line();
	auto &imag1 = plot.line();
	auto &real2 = plot.line();
	auto &imag2 = plot.line();
	
	auto &inversePlot = figure(1, 0).plot();
	auto &inverse1 = inversePlot.line(), &inverse2 = inversePlot.line();
	auto &inputPlot = figure(1, 1).plot();
	auto &inputLine = inputPlot.line();
	
	auto &windowPlot = figure(0, 1).plot();
	auto &window1 = windowPlot.line(), &window2 = windowPlot.line();
	for (size_t i = 0; i < 255; ++i) {
		window1.add(i, dynamic.analysisWindow()[i]);
		window2.add(i, dsp.window()[i]);
	}

	for (size_t r = 0; r < 10; ++r) {
		std::rotate(input.begin(), input.begin() + 45, input.end());
		for (size_t i = 0; i < 45; ++i) {
			input[255 - 45 + i] = dist(randomEngine);
		}

		dynamic.writeInput(0, 45, input.data() + 255 - 45);
		dynamic.moveInput(45);
		dynamic.analyse();
		
		dsp.analyse(0, input.data());
		
		double e2 = 0;
		for (size_t f = 0; f < 128; ++f) {
			auto b1 = dynamic.spectrum(0)[f];
			auto b2 = dsp.spectrum[0][f];
			
			real1.add(f, b1.real());
			imag1.add(f, b1.imag());
			real2.add(f, b2.real());
			imag2.add(f, b2.imag());
			
			e2 += std::norm(b1 - b2);
		}

		mrfft.ifft(dynamic.spectrum(0), tmp.data());
		for (size_t f = 0; f < 255; ++f) {
			inverse1.add(f, tmp[f]);
		}
		mrfft.ifft(dsp.spectrum[0], tmp.data());
		for (size_t f = 0; f < 255; ++f) {
			inverse2.add(f, tmp[f]);
		}
		for (size_t f = 0; f < 255; ++f) {
			inputLine.add(f, input[f]);
		}

		plot.toFrame(r*0.2);
		inversePlot.toFrame(r*0.2);
		inputPlot.toFrame(r*0.2);
		
		if (std::sqrt(e2) > 0.001) {
			figure.write("stft-compare.svg");
			abort();
		}
	}
	
	figure.write("stft-compare.svg");
}

void testUnpackedStft(unsigned seed) {
	signalsmith::linear::DynamicSTFT<float, false, 0> packed;
	signalsmith::linear::DynamicSTFT<float, false, 2> unpacked;
	packed.configure(1, 1, 256);
	unpacked.configure(1, 1, 256);
	if (unpacked.bands() != packed.bands() + 1) {
		LOG_EXPR(packed.bands());
		LOG_EXPR(unpacked.bands());
		abort();
	}

	std::default_random_engine randomEngine(seed);
	std::uniform_int_distribution<size_t> intervalDist{1, 200};
	std::uniform_real_distribution<float> realDist{-10, 10};
	
	std::vector<float> buffer1(256), buffer2(256);

	for (size_t r = 0; r < 10; ++r) {
		size_t interval = intervalDist(randomEngine);
		for (size_t i = 0; i < interval; ++i) {
			buffer1[i] = realDist(randomEngine);
		}
		
		packed.writeInput(0, interval, buffer1.data());
		unpacked.writeInput(0, interval, buffer1.data());
		packed.moveInput(interval);
		unpacked.moveInput(interval);
		
		packed.analyse();
		unpacked.analyse();
		
		for (size_t b = 1; b < packed.bands(); ++b) {
			auto pV = packed.spectrum(0)[b];
			auto uV = packed.spectrum(0)[b];
			if (pV != uV) {
				LOG_EXPR(b);
				LOG_EXPR(pV);
				LOG_EXPR(uV);
				abort();
			}
		}
		auto p0 = packed.spectrum(0)[0];
		auto u0 = unpacked.spectrum(0)[0];
		auto uN = unpacked.spectrum(0)[unpacked.bands() - 1];
		if (p0.real() != u0.real() || u0.imag() != 0 || uN.real() != p0.imag() || uN.imag() != 0) {
			LOG_EXPR(p0);
			LOG_EXPR(u0);
			LOG_EXPR(uN);
			abort();
		}

		for (size_t b = 1; b < packed.bands(); ++b) {
			auto &pV = packed.spectrum(0)[b];
			auto &uV = packed.spectrum(0)[b];
			pV = uV = {realDist(randomEngine), realDist(randomEngine)};
		}
		p0 = {realDist(randomEngine), realDist(randomEngine)};
		packed.spectrum(0)[0] = p0;
		unpacked.spectrum(0)[0] = {p0.real(), realDist(randomEngine)/*discarded*/};
		unpacked.spectrum(0)[unpacked.bands() - 1] = {p0.imag(), realDist(randomEngine)/*discarded*/};

		packed.synthesise();
		unpacked.synthesise();
		
		// also randomise synthesis interval
		interval = intervalDist(randomEngine);
		packed.readOutput(0, interval, buffer1.data());
		unpacked.readOutput(0, interval, buffer2.data());
		packed.moveOutput(interval);
		unpacked.moveOutput(interval);
		
		for (size_t i = 0; i < interval; ++i) {
			if (buffer1[i] != buffer2[i]) {
				LOG_EXPR(buffer1[i]);
				LOG_EXPR(buffer2[i]);
				abort();
			}
		}
	}

}

void testStfts(int, double) {
	std::cout << "STFT\n----\n";
	
	compareStfts();
	
	unsigned seed = 0;
	for (size_t r = 0; r < 10; ++r) {
		testStft<float, false, false>(3, 513, 1, 500, seed += 24680);
		testStft<float, true, false>(2, 65, 16, 48, seed += 12505);
		testStft<float, false, true>(1, 63, 32, 32, seed += 8223);
		testStft<float, true, true>(1, 256, 32, 192, seed += 22763);
		testStft<double, false, false>(1, 151, 1, 150, seed += 24680);
		testStft<double, true, false>(2, 96, 16, 48, seed += 12505);
		testStft<double, false, true>(1, 97, 1, 64, seed += 8223);
		testStft<double, true, true>(2, 256, 32, 192, seed += 22763);
	}
}

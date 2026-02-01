#include <iostream>
#define LOG_EXPR(expr) std::cout << #expr << " = " << (expr) << std::endl;

#include "./test-linear.h"
#include "./test-ffts.h"
#include "./test-stfts.h"

#include <cstdlib>

#include "./stop-denormals.h"

int main(int argc, char *argv[]) {
	StopDenormals scoped;

#ifdef SIGNALSMITH_USE_ACCELERATE
	std::cout << u8"✅ SIGNALSMITH_USE_ACCELERATE\n";
#else
	std::cout << u8"❌ SIGNALSMITH_USE_ACCELERATE\n";
#endif
#ifdef SIGNALSMITH_USE_IPP
	std::cout << u8"✅ SIGNALSMITH_USE_IPP\n";
#else
	std::cout << u8"❌ SIGNALSMITH_USE_IPP\n";
#endif
#ifdef SIGNALSMITH_USE_PFFFT
	std::cout << u8"✅ SIGNALSMITH_USE_PFFFT\n";
#else
	std::cout << u8"❌ SIGNALSMITH_USE_PFFFT\n";
#endif
#ifdef SIGNALSMITH_USE_PFFFT_DOUBLE
	std::cout << u8"✅ SIGNALSMITH_USE_PFFFT_DOUBLE\n";
#else
	std::cout << u8"❌ SIGNALSMITH_USE_PFFFT_DOUBLE\n";
#endif
#ifdef __FAST_MATH__
	std::cout << u8"✅ __FAST_MATH__\n";
#else
	std::cout << u8"❌ __FAST_MATH__\n";
#endif

	int maxSize = 8192;
	double benchmarkSeconds = 0;
	if (argc > 1 && !std::strcmp(argv[1], "benchmark")) {
		maxSize = 65536*8;
		benchmarkSeconds = 0.05;
		if (argc > 2) {
			benchmarkSeconds = std::strtod(argv[2], nullptr);
		}
		
	}
	if (argc <= 3) {
		testFfts(maxSize, benchmarkSeconds);
		testStfts(maxSize, benchmarkSeconds);
		testLinear(maxSize, benchmarkSeconds);
	} else {
		for (int i = 3; i < argc; ++i) {
			if (!std::strcmp(argv[i], "fft")) {
				testFfts(maxSize, benchmarkSeconds);
			} else if (!std::strcmp(argv[i], "stft")) {
				testStfts(maxSize, benchmarkSeconds);
			} else if (!std::strcmp(argv[i], "linear")) {
				testLinear(maxSize, benchmarkSeconds);
			}
		}
	}
}

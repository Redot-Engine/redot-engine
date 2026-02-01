#ifndef SIGNALSMITH_FFT_V5_prev
#define SIGNALSMITH_FFT_V5_prev
// So we can easily include multiple versions by redefining this
#ifndef SIGNALSMITH_FFT_NAMESPACE
#define SIGNALSMITH_FFT_NAMESPACE signalsmith
#endif

#include <vector>
#include <complex>
#include <cmath>
#include <array>
#include <memory>

#ifndef SIGNALSMITH_INLINE
#ifdef __GNUC__
#define SIGNALSMITH_INLINE __attribute__((always_inline)) inline
#elif defined(__MSVC__)
#define SIGNALSMITH_INLINE __forceinline inline
#else
#define SIGNALSMITH_INLINE inline
#endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

namespace SIGNALSMITH_FFT_NAMESPACE {

	namespace perf {
		// Complex multiplication has edge-cases around Inf/NaN - handling those properly makes std::complex non-inlineable, so we use our own
		template <bool conjugateSecond, typename V>
		SIGNALSMITH_INLINE std::complex<V> complexMul(const std::complex<V> &a, const std::complex<V> &b) {
			return conjugateSecond ? std::complex<V>{
				b.real()*a.real() + b.imag()*a.imag(),
				b.real()*a.imag() - b.imag()*a.real()
			} : std::complex<V>{
				a.real()*b.real() - a.imag()*b.imag(),
				a.real()*b.imag() + a.imag()*b.real()
			};
		}

		template<bool flipped, typename V>
		SIGNALSMITH_INLINE std::complex<V> complexAddI(const std::complex<V> &a, const std::complex<V> &b) {
			return flipped ? std::complex<V>{
				a.real() + b.imag(),
				a.imag() - b.real()
			} : std::complex<V>{
				a.real() - b.imag(),
				a.imag() + b.real()
			};
		}
	}
	
	// Use SFINAE to get an iterator from std::begin(), if supported - otherwise assume the value itself is an iterator
	template<typename T, typename=void>
	struct GetIterator {
		static T get(const T &t) {
			return t;
		}
	};
	template<typename T>
	struct GetIterator<T, decltype((void)std::begin(std::declval<T>()))> {
		static auto get(const T &t) -> decltype(std::begin(t)) {
			return std::begin(t);
		}
	};

	template<typename V>
	class FFT {
		using complex = std::complex<V>;
		size_t _size;
		std::vector<complex> workingVector;
		
		enum class StepType {
			generic, step2, step3, step4
		};
		struct Step {
			StepType type;
			size_t factor;
			size_t startIndex;
			size_t innerRepeats;
			size_t outerRepeats;
			size_t twiddleIndex;
		};
		std::vector<size_t> factors;
		std::vector<Step> plan;
		std::vector<complex> twiddleVector;
		
		struct PermutationPair {size_t from, to;};
		std::vector<PermutationPair> permutation;
		
		void addPlanSteps(size_t factorIndex, size_t start, size_t length, size_t repeats) {
			if (factorIndex >= factors.size()) return;
			
			size_t factor = factors[factorIndex];
			if (factorIndex + 1 < factors.size()) {
				if (factors[factorIndex] == 2 && factors[factorIndex + 1] == 2) {
					++factorIndex;
					factor = 4;
				}
			}

			size_t subLength = length/factor;
			Step mainStep{StepType::generic, factor, start, subLength, repeats, twiddleVector.size()};

			if (factor == 2) mainStep.type = StepType::step2;
			if (factor == 3) mainStep.type = StepType::step3;
			if (factor == 4) mainStep.type = StepType::step4;

			// Twiddles
			bool foundStep = false;
			for (const Step &existingStep : plan) {
				if (existingStep.factor == mainStep.factor && existingStep.innerRepeats == mainStep.innerRepeats) {
					foundStep = true;
					mainStep.twiddleIndex = existingStep.twiddleIndex;
					break;
				}
			}
			if (!foundStep) {
				for (size_t i = 0; i < subLength; ++i) {
					for (size_t f = 0; f < factor; ++f) {
						V phase = 2*M_PI*i*f/length;
						complex twiddle = {cos(phase), -sin(phase)};
						twiddleVector.push_back(twiddle);
					}
				}
			}

			if (repeats == 1 && sizeof(complex)*subLength > 65536) {
				for (size_t i = 0; i < factor; ++i) {
					addPlanSteps(factorIndex + 1, start + i*subLength, subLength, 1);
				}
			} else {
				addPlanSteps(factorIndex + 1, start, subLength, repeats*factor);
			}
			plan.push_back(mainStep);
		}
		void setPlan() {
			factors.resize(0);
			size_t size = _size, f = 2;
			while (size > 1) {
				if (size%f == 0) {
					factors.push_back(f);
					size /= f;
				} else if (f > sqrt(size)) {
					f = size;
				} else {
					++f;
				}
			}

			plan.resize(0);
			twiddleVector.resize(0);
			addPlanSteps(0, 0, _size, 1);
			
			permutation.resize(0);
			permutation.push_back(PermutationPair{0, 0});
			size_t indexLow = 0, indexHigh = factors.size();
			size_t inputStepLow = _size, outputStepLow = 1;
			size_t inputStepHigh = 1, outputStepHigh = _size;
			while (outputStepLow*inputStepHigh < _size) {
				size_t f, inputStep, outputStep;
				if (outputStepLow <= inputStepHigh) {
					f = factors[indexLow++];
					inputStep = (inputStepLow /= f);
					outputStep = outputStepLow;
					outputStepLow *= f;
				} else {
					f = factors[--indexHigh];
					inputStep = inputStepHigh;
					inputStepHigh *= f;
					outputStep = (outputStepHigh /= f);
				}
				size_t oldSize = permutation.size();
				for (size_t i = 1; i < f; ++i) {
					for (size_t j = 0; j < oldSize; ++j) {
						PermutationPair pair = permutation[j];
						pair.from += i*inputStep;
						pair.to += i*outputStep;
						permutation.push_back(pair);
					}
				}
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		void fftStepGeneric(RandomAccessIterator &&origData, const Step &step) {
			complex *working = workingVector.data();
			const size_t stride = step.innerRepeats;

			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				RandomAccessIterator data = origData;
				
				const complex *twiddles = twiddleVector.data() + step.twiddleIndex;
				const size_t factor = step.factor;
				for (size_t repeat = 0; repeat < step.innerRepeats; ++repeat) {
					for (size_t i = 0; i < step.factor; ++i) {
						working[i] = perf::complexMul<inverse>(data[i*stride], twiddles[i]);
					}
					for (size_t f = 0; f < factor; ++f) {
						complex sum = working[0];
						for (size_t i = 1; i < factor; ++i) {
							V phase = 2*M_PI*f*i/factor;
							complex factor = {cos(phase), -sin(phase)};
							sum += perf::complexMul<inverse>(working[i], factor);
						}
						data[f*stride] = sum;
					}
					++data;
					twiddles += factor;
				}
				origData += step.factor*step.innerRepeats;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		void fftStep2(RandomAccessIterator &&origData, const Step &step) {
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex B = perf::complexMul<inverse>(data[stride], twiddles[1]);
					
					data[0] = A + B;
					data[stride] = A - B;
					twiddles += 2;
				}
				origData += 2*stride;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		void fftStep3(RandomAccessIterator &&origData, const Step &step) {
			constexpr complex factor3 = {-0.5, inverse ? 0.8660254037844386 : -0.8660254037844386};
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex B = perf::complexMul<inverse>(data[stride], twiddles[1]);
					complex C = perf::complexMul<inverse>(data[stride*2], twiddles[2]);
					
					complex realSum = A + (B + C)*factor3.real();
					complex imagSum = (B - C)*factor3.imag();

					data[0] = A + B + C;
					data[stride] = perf::complexAddI<false>(realSum, imagSum);
					data[stride*2] = perf::complexAddI<true>(realSum, imagSum);

					twiddles += 3;
				}
				origData += 3*stride;
			}
		}

		template<bool inverse, typename RandomAccessIterator>
		void fftStep4(RandomAccessIterator &&origData, const Step &step) {
			const size_t stride = step.innerRepeats;
			const complex *origTwiddles = twiddleVector.data() + step.twiddleIndex;
			
			for (size_t outerRepeat = 0; outerRepeat < step.outerRepeats; ++outerRepeat) {
				const complex* twiddles = origTwiddles;
				for (RandomAccessIterator data = origData; data < origData + stride; ++data) {
					complex A = data[0];
					complex C = perf::complexMul<inverse>(data[stride], twiddles[2]);
					complex B = perf::complexMul<inverse>(data[stride*2], twiddles[1]);
					complex D = perf::complexMul<inverse>(data[stride*3], twiddles[3]);

					complex sumAC = A + C, sumBD = B + D;
					complex diffAC = A - C, diffBD = B - D;

					data[0] = sumAC + sumBD;
					data[stride] = perf::complexAddI<!inverse>(diffAC, diffBD);
					data[stride*2] = sumAC - sumBD;
					data[stride*3] = perf::complexAddI<inverse>(diffAC, diffBD);

					twiddles += 4;
				}
				origData += 4*stride;
			}
		}
		
		template<typename InputIterator, typename OutputIterator>
		void permute(InputIterator input, OutputIterator data) {
			for (auto pair : permutation) {
				data[pair.from] = input[pair.to];
			}
		}

		template<bool inverse, typename InputIterator, typename OutputIterator>
		void run(InputIterator &&input, OutputIterator &&data) {
			permute(input, data);
			
			for (const Step &step : plan) {
				switch (step.type) {
					case StepType::generic:
						fftStepGeneric<inverse>(data + step.startIndex, step);
						break;
					case StepType::step2:
						fftStep2<inverse>(data + step.startIndex, step);
						break;
					case StepType::step3:
						fftStep3<inverse>(data + step.startIndex, step);
						break;
					case StepType::step4:
						fftStep4<inverse>(data + step.startIndex, step);
						break;
				}
			}
		}

		static bool validSize(size_t size) {
			constexpr static bool filter[32] = {
				1, 1, 1, 1, 1, 0, 1, 0, 1, 1, // 0-9
				0, 0, 1, 0, 0, 0, 1, 0, 1, 0, // 10-19
				0, 0, 0, 0, 1, 0, 0, 0, 0, 0, // 20-29
				0, 0
			};
			return filter[size];
		}
	public:
		static size_t sizeMinimum(size_t size) {
			size_t power2 = 1;
			while (size >= 32) {
				size = (size - 1)/2 + 1;
				power2 *= 2;
			}
			while (size < 32 && !validSize(size)) {
				++size;
			}
			return power2*size;
		}
		static size_t sizeMaximum(size_t size) {
			size_t power2 = 1;
			while (size >= 32) {
				size /= 2;
				power2 *= 2;
			}
			while (size > 1 && !validSize(size)) {
				--size;
			}
			return power2*size;
		}

		FFT(size_t size, int fastDirection=0) : _size(0) {
			if (fastDirection > 0) size = sizeMinimum(size);
			if (fastDirection < 0) size = sizeMaximum(size);
			this->setSize(size);
		}

		size_t setSize(size_t size) {
			if (size != _size) {
				_size = size;
				workingVector.resize(size);
				setPlan();
			}
			return _size;
		}
		size_t setSizeMinimum(size_t size) {
			return setSize(sizeMinimum(size));
		}
		size_t setSizeMaximum(size_t size) {
			return setSize(sizeMaximum(size));
		}
		const size_t & size() const {
			return _size;
		}

		template<typename InputIterator, typename OutputIterator>
		void fft(InputIterator &&input, OutputIterator &&output) {
			auto inputIter = GetIterator<InputIterator>::get(input);
			auto outputIter = GetIterator<OutputIterator>::get(output);
			return run<false>(inputIter, outputIter);
		}

		template<typename InputIterator, typename OutputIterator>
		void ifft(InputIterator &&input, OutputIterator &&output) {
			auto inputIter = GetIterator<InputIterator>::get(input);
			auto outputIter = GetIterator<OutputIterator>::get(output);
			return run<true>(inputIter, outputIter);
		}
	};

	struct FFTOptions {
		static constexpr int halfFreqShift = 1;
	};

	template<typename V, int optionFlags=0>
	class RealFFT {
		static constexpr bool modified = (optionFlags&FFTOptions::halfFreqShift);

		using complex = std::complex<V>;
		std::vector<complex> complexBuffer1, complexBuffer2;
		std::vector<complex> twiddlesMinusI;
		std::vector<complex> modifiedRotations;
		FFT<V> complexFft;
	public:
		static size_t sizeMinimum(size_t size) {
			return (FFT<V>::sizeMinimum((size - 1)/2) + 1)*2;
		}
		static size_t sizeMaximum(size_t size) {
			return FFT<V>::sizeMinimum(size/2)*2;
		}

		RealFFT(size_t size, int fastDirection=0) : complexFft(0) {
			if (fastDirection > 0) size = sizeMinimum(size);
			if (fastDirection < 0) size = sizeMaximum(size);
			this->setSize(size);
		}

		size_t setSize(size_t size) {
			complexBuffer1.resize(size/2);
			complexBuffer2.resize(size/2);

			size_t hhSize = size/4 + 1;
			twiddlesMinusI.resize(hhSize);
			for (size_t i = 0; i < hhSize; ++i) {
				double rotPhase = -2*M_PI*(modified ? i + 0.5 : i)/size;
				twiddlesMinusI[i] = {sin(rotPhase), -cos(rotPhase)};
			}
			if (modified) {
				modifiedRotations.resize(size/2);
				for (size_t i = 0; i < size/2; ++i) {
					double rotPhase = -2*M_PI*i/size;
					modifiedRotations[i] = {cos(rotPhase), sin(rotPhase)};
				}
			}
			
			return complexFft.setSize(size/2);
		}
		size_t setSizeMinimum(size_t size) {
			return setSize(sizeMinimum(size));
		}
		size_t setSizeMaximum(size_t size) {
			return setSize(sizeMaximum(size));
		}
		size_t size() const {
			return complexFft.size()*2;
		}

		template<typename InputIterator, typename OutputIterator>
		void fft(InputIterator &&input, OutputIterator &&output) {
			size_t hSize = complexFft.size();
			for (size_t i = 0; i < hSize; ++i) {
				if (modified) {
					complexBuffer1[i] = perf::complexMul<false>({input[2*i], input[2*i + 1]}, modifiedRotations[i]);
				} else {
					complexBuffer1[i] = {input[2*i], input[2*i + 1]};
				}
			}
			
			complexFft.fft(complexBuffer1.data(), complexBuffer2.data());
			
			if (!modified) output[0] = {
				complexBuffer2[0].real() + complexBuffer2[0].imag(),
				complexBuffer2[0].real() - complexBuffer2[0].imag()
			};
			for (size_t i = modified ? 0 : 1; i <= hSize/2; ++i) {
				size_t conjI = modified ? (hSize  - 1 - i) : (hSize - i);
				
				complex odd = (complexBuffer2[i] + conj(complexBuffer2[conjI]))*(V)0.5;
				complex evenI = (complexBuffer2[i] - conj(complexBuffer2[conjI]))*(V)0.5;
				complex evenRotMinusI = perf::complexMul<false>(evenI, twiddlesMinusI[i]);

				output[i] = odd + evenRotMinusI;
				output[conjI] = conj(odd - evenRotMinusI);
			}
		}

		template<typename InputIterator, typename OutputIterator>
		void ifft(InputIterator &&input, OutputIterator &&output) {
			size_t hSize = complexFft.size();
			if (!modified) complexBuffer1[0] = {
				input[0].real() + input[0].imag(),
				input[0].real() - input[0].imag()
			};
			for (size_t i = modified ? 0 : 1; i <= hSize/2; ++i) {
				size_t conjI = modified ? (hSize  - 1 - i) : (hSize - i);
				complex v = input[i], v2 = input[conjI];

				complex odd = v + conj(v2);
				complex evenRotMinusI = v - conj(v2);
				complex evenI = perf::complexMul<true>(evenRotMinusI, twiddlesMinusI[i]);
				
				complexBuffer1[i] = odd + evenI;
				complexBuffer1[conjI] = conj(odd - evenI);
			}
			
			complexFft.ifft(complexBuffer1.data(), complexBuffer2.data());
			
			for (size_t i = 0; i < hSize; ++i) {
				complex v = complexBuffer2[i];
				if (modified) v = perf::complexMul<true>(v, modifiedRotations[i]);
				output[2*i] = v.real();
				output[2*i + 1] = v.imag();
			}
		}
	};

	template<typename V>
	struct ModifiedRealFFT : public RealFFT<V, FFTOptions::halfFreqShift> {
		using RealFFT<V, FFTOptions::halfFreqShift>::RealFFT;
	};
}

#undef SIGNALSMITH_FFT_NAMESPACE
#endif // SIGNALSMITH_FFT_V5

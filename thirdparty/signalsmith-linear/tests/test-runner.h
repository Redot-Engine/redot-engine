#pragma once

#include "../linear.h"

#include "./stopwatch.h"
#include "plot/plot.h"

#include <complex>
#include <vector>
#include <random>

template<class Data, typename V, size_t typeIndex>
struct RunDataGetter;

template<class Data, typename V>
struct RunDataGetter<Data, V, 0> {
	static V * get(Data &data, size_t index) {
		return data.real(index);
	}
};
template<class Data, typename V>
struct RunDataGetter<Data, V, 1> {
	static V * get(Data &data, size_t index) {
		return data.positive(index);
	}
};
template<class Data, typename V>
struct RunDataGetter<Data, V, 2> {
	static std::complex<V> * get(Data &data, size_t index) {
		return data.complex(index);
	}
};
template<class Data, typename V>
struct RunDataGetter<Data, V, 3> {
	static signalsmith::linear::SplitPointer<V> get(Data &data, size_t index) {
		return data.split(index);
	}
};

template<typename Sample, size_t alignBytes=32>
struct RunData {
	using Complex = std::complex<Sample>;
	using SplitPointer = signalsmith::linear::SplitPointer<Sample>;

	static constexpr size_t extraAlignmentItems = alignBytes/sizeof(Sample);
	template<class V>
	static V * nextAligned(V *ptr) {
		return (V *)((size_t(ptr) + (alignBytes - 1))&~(alignBytes - 1));
	}

	const size_t size;
	std::vector<Sample *> reals;
	std::vector<std::string> realNames;
	std::vector<Sample *> positives;
	std::vector<std::string> positiveNames;
	std::vector<Complex *> complexes;
	std::vector<std::string> complexNames;
	std::vector<SplitPointer> splits;
	std::vector<std::string> splitNames;

	RunData(size_t size, int seed=0) : size(size), seed(seed) {}
	RunData(const RunData &other) : size(other.size), seed(other.seed) {
		for (auto *otherP : other.reals) {
			auto *thisP = real(reals.size());
			for (size_t i = 0; i < size; ++i) thisP[i] = otherP[i];
		}
		for (auto *otherP : other.positives) {
			auto *thisP = positive(positives.size());
			for (size_t i = 0; i < size; ++i) thisP[i] = otherP[i];
		}
		for (auto *otherP : other.complexes) {
			auto *thisP = complex(complexes.size());
			for (size_t i = 0; i < size; ++i) thisP[i] = otherP[i];
		}
		for (auto otherP : other.splits) {
			auto thisP = split(splits.size());
			for (size_t i = 0; i < size; ++i) {
				thisP.real[i] = otherP.real[i];
				thisP.imag[i] = otherP.imag[i];
			}
		}
	}
	
	Sample * real(size_t index, std::string name="") {
		while (index >= realVectors.size()) {
			std::default_random_engine engine(unsigned(seed + 85221*realVectors.size()));
			std::uniform_real_distribution<Sample> dist{-1, 1};
			
			realVectors.emplace_back(size + extraAlignmentItems);
			reals.push_back(nextAligned(realVectors.back().data()));
			for (size_t i = 0; i < size; ++i) reals.back()[i] = dist(engine);
			realNames.push_back("r" + std::to_string(realNames.size()));
		}
		if (name.size()) realNames[index] = name;
		return reals[index];
	}
	Sample * positive(size_t index, std::string name="") {
		while (index >= positiveVectors.size()) {
			std::default_random_engine engine(unsigned(seed + 578892*positiveVectors.size()));
			std::uniform_real_distribution<Sample> dist{0, 1};
			
			positiveVectors.emplace_back(size + extraAlignmentItems);
			positives.push_back(nextAligned(positiveVectors.back().data()));
			for (size_t i = 0; i < size; ++i) {
				Sample v = dist(engine);
				while (v <= 0) v = dist(engine);
				positives.back()[i] = v;
			}
			positiveNames.push_back("p" + std::to_string(positiveNames.size()));
		}
		if (name.size()) positiveNames[index] = name;
		return positives[index];
	}
	Complex * complex(size_t index, std::string name="") {
		while (index >= complexVectors.size()) {
			std::default_random_engine engine(unsigned(seed + 13857*complexVectors.size()));
			std::uniform_real_distribution<Sample> dist{-1, 1};
			
			complexVectors.emplace_back(size + extraAlignmentItems*2);
			complexes.push_back(nextAligned(complexVectors.back().data()));
			for (size_t i = 0; i < size; ++i) {
				Complex v = {dist(engine), dist(engine)};
				complexes.back()[i] = v;
			}
			complexNames.push_back("c" + std::to_string(complexNames.size()));
		}
		if (name.size()) complexNames[index] = name;
		return complexes[index];
	}
	SplitPointer split(size_t index, std::string name="") {
		while (index >= splits.size()) {
			std::default_random_engine engine(unsigned(seed + 279328*splits.size()));
			std::uniform_real_distribution<Sample> dist{-1, 1};
			
			splitVectors.emplace_back(size + extraAlignmentItems);
			Sample *real = nextAligned(splitVectors.back().data());
			splitVectors.emplace_back(size + extraAlignmentItems);
			Sample *imag = nextAligned(splitVectors.back().data());
			splits.push_back({real, imag});
			for (size_t i = 0; i < size; ++i) {
				real[i] = dist(engine);
				imag[i] = dist(engine);
			}
			splitNames.push_back("s" + std::to_string(splitNames.size()));
		}
		if (name.size()) splitNames[index] = name;
		return splits[index];
	}
	
	void complexToSplit(size_t index) {
		auto *c = complex(index);
		auto s = split(index);
		for (size_t i = 0; i < size; ++i) {
			s.real[i] = c[i].real();
			s.imag[i] = c[i].imag();
		}
	}
	void splitToComplex(size_t index) {
		auto *c = complex(index);
		auto s = split(index);
		for (size_t i = 0; i < size; ++i) {
			c[i] = {s.real[i], s.imag[i]};
		}
	}
	
	template<int dataType>
	auto get(size_t index) -> decltype(RunDataGetter<RunData, Sample, dataType>::get(*this, index)) {
		return RunDataGetter<RunData, Sample, dataType>::get(*this, index);
	}

	double distance(const RunData<Sample> &other) const {
		double error2 = 0;
		
		for (size_t vi = 0; vi < reals.size(); ++vi) {
			auto *thisVector = reals[vi];
			auto *otherVector = other.reals[vi];
			for (size_t i = 0; i < size; ++i) {
				auto diff = thisVector[i] - otherVector[i];
				error2 += diff*diff;
			}
		}
		for (size_t vi = 0; vi < positives.size(); ++vi) {
			auto *thisVector = positives[vi];
			auto *otherVector = other.positives[vi];
			for (size_t i = 0; i < size; ++i) {
				auto diff = thisVector[i] - otherVector[i];
				error2 += diff*diff;
			}
		}
		for (size_t vi = 0; vi < complexes.size(); ++vi) {
			auto *thisVector = complexes[vi];
			auto *otherVector = other.complexes[vi];
			for (size_t i = 0; i < size; ++i) {
				error2 += std::norm(thisVector[i] - otherVector[i]);
			}
		}
		for (size_t vi = 0; vi < splits.size(); ++vi) {
			auto thisVector = splits[vi];
			auto otherVector = other.splits[vi];
			for (size_t i = 0; i < size; ++i) {
				error2 += std::norm(thisVector[i] - otherVector[i]);
			}
		}
		
		return std::sqrt(error2/size);
	}
	
	void log() const {
		for (size_t i = 0; i < reals.size(); ++i) {
			std::cout << "\t" << realNames[i];
		}
		for (size_t i = 0; i < positives.size(); ++i) {
			std::cout << "\t" << positiveNames[i];
		}
		for (size_t i = 0; i < complexes.size(); ++i) {
			std::cout << "\t" << complexNames[i];
		}
		for (size_t i = 0; i < splits.size(); ++i) {
			std::cout << "\t" << splitNames[i];
		}
		std::cout << "\n";
		for (size_t i = 0; i < size; ++i) {
			for (auto *vector : reals) {
				std::cout << "\t" << vector[i];
			}
			for (auto *vector : positives) {
				std::cout << "\t" << vector[i];
			}
			for (auto *vector : complexes) {
				std::cout << "\t" << vector[i];
			}
			for (auto vector : splits) {
				std::cout << "\t" << vector[i];
			}
			std::cout << "\n";
		}
	}
	
private:
	int seed;
	std::vector<std::vector<Sample>> realVectors;
	std::vector<std::vector<Sample>> positiveVectors;
	std::vector<std::vector<Complex>> complexVectors;
	std::vector<std::vector<Sample>> splitVectors;
};

template<class Fn>
double runBenchmark(double benchmarkSeconds, Fn &&fn) {
	double benchmarkChunk = benchmarkSeconds/5;

	double seconds = 0;
	size_t rounds = 0, roundStep = 1;
	Stopwatch stopwatch{false};
	while (seconds < benchmarkSeconds) {
		stopwatch.start();
		
		for (size_t r = 0; r < roundStep; ++r) {
			fn();
		}

		double lap = stopwatch.lap();
		if (lap < benchmarkChunk) {
			roundStep *= 2;
		} else {
			seconds += lap;
			rounds += roundStep;
		}
	}
	
	return rounds/seconds;
};

struct RunPlot {
	using Plot = signalsmith::plot::Plot2D;

	std::string name;
	double benchmarkSeconds;
	std::unique_ptr<Plot> plotPtr = nullptr;
	Plot &plot;
	signalsmith::plot::Legend &legend;
	
	RunPlot(const std::string &name, double benchmarkSeconds=0.05) : name(name), benchmarkSeconds(benchmarkSeconds), plotPtr(new Plot(450, 200)), plot(*plotPtr), legend(plot.legend(2, 1)) {
		std::cout << "\n" << name << "\n";
		for (size_t i = 0; i < name.size(); ++i) std::cout << "-";
		std::cout << "\n";
		plot.x.label(name);
	}
	RunPlot(const std::string &name, Plot &plot, double benchmarkSeconds=0.05) : name(name), benchmarkSeconds(benchmarkSeconds), plot(plot), legend(plot.legend(2, 1)) {
		plot.x.label(name);
	}
	~RunPlot() {
		if (benchmarkSeconds) {
			plot.y.major(0); // auto-scaled range includes 0
			plot.y.blankLabels().label("speed"); // values don't matter, only the comparison
			plot.write(name + ".svg");
		}
	}
	
	template<class PrepareAndRun>
	struct Runner {
		Runner(RunPlot &runPlot, std::string name) : runPlot(runPlot), line(runPlot.plot.line()) {
			runPlot.legend.add(line, name);
		}
		
		template<class Data>
		void run(Data data, double refTime, Data *maybeRefData, double errorLimit=1e-4) {
			PrepareAndRun obj;
			obj.prepare(data.size, data.size);
			obj.run(data);
			
			if (maybeRefData) {
				obj.normalise(data);
				double error = data.distance(*maybeRefData);
				if (error > errorLimit) {
					std::cout << "\nsize = " << data.size << ", error = " << error << "\n";
					std::cout << "\nReference:\n";
					maybeRefData->log();
					std::cout << "\noutput:\n";
					data.log();
					abort();
				}
			}
			
			if (runPlot.benchmarkSeconds) {
				double speed = runBenchmark(runPlot.benchmarkSeconds, [&](){
					obj.run(data);
				});
				line.add(data.size, speed*refTime);
			}
		}
	private:
		RunPlot &runPlot;
		signalsmith::plot::Line2D &line;
	};
	
	template<class PrepareAndRun>
	Runner<PrepareAndRun> runner(std::string name) {
		return {*this, name};
	}

	bool firstTick = true;
	void tick(int n, int base=0) {
		std::string nString = std::to_string(n);
		if (base > 0) {
			int log = std::round(std::log(n)/std::log(base));
			int pow = std::round(std::pow(base, log));
			if (pow == n) {
				std::string powString = std::to_string(base) + "^" + std::to_string(log);
				if (powString.size() + 1 < nString.size()) nString = powString;
			}
		}
		tick(n, nString);
	}
	void tick(int n, std::string nString) {
		if (firstTick) {
			firstTick = false;
			plot.x.major(n, nString);
		} else {
			plot.x.tick(n, nString);
		}
	}
};

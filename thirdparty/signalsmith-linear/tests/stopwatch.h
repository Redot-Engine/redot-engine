#ifndef SIGNALSMITH_STOPWATCH_UTIL_H
#define SIGNALSMITH_STOPWATCH_UTIL_H

#include <limits>
#include <cmath>
#include <atomic>
#include <algorithm>

#ifdef WINDOWS // completely untested!
#	include <windows.h>
class Stopwatch {
	using Time = __int64;
	using Duration = Time;
	inline Time now() {
		LARGE_INTEGER result;
		QueryPerformanceCounter(&result);
		return result.QuadPart;
	}
	static double toSeconds(Duration t) {
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		return t/double(freq);
	}
#else
#	include <chrono>
class Stopwatch {
	using Clock = std::conditional<std::chrono::high_resolution_clock::is_steady, std::chrono::high_resolution_clock, std::chrono::steady_clock>::type;
	using Time = Clock::time_point;
	using Duration = std::chrono::duration<double>;
	
	inline Time now() {
		return Clock::now();
	}
	static double toSeconds(Duration duration) {
		return duration.count();
	}
#endif

	std::atomic<Time> lapStart; // the atomic store/load should act as barriers for reordering operations
	double lapBest, lapTotal, lapTotal2;
	double lapOverhead = 0;
	int lapCount = 0;
	
public:
	Stopwatch(bool compensate=true) {
		if (compensate) {
			start();
			const int repeats = 1000;
			for (int i = 0; i < repeats; ++i) {
				startLap();
				lap();
			}
			lapOverhead = (double)lapTotal/lapCount;
		}
		start();
	}
	// Explicit because std::atomic<> can't be copied/moved
	Stopwatch(const Stopwatch &other) : lapBest(other.lapBest), lapTotal(other.lapTotal), lapTotal2(other.lapTotal2), lapOverhead(other.lapOverhead), lapCount(other.lapCount) {
		lapStart.store(other.lapStart.load());
	}

	void start() {
		lapCount = 0;
		lapTotal = lapTotal2 = 0;
		lapBest = std::numeric_limits<double>::max();
		startLap();
	}
	void startLap() {
		lapStart.store(now());
	}
	double lap() {
		double diff = toSeconds(now() - lapStart.load());

		if (diff < lapBest) lapBest = diff;
		lapCount++;
		lapTotal += diff;
		lapTotal2 += diff*diff;

		startLap();
		return diff;
	}
	double total() const {
		return std::max(0.0, lapTotal - lapCount*lapOverhead);
	}
	double mean() const {
		return total()/lapCount;
	}
	double var() const {
		double m = (double)lapTotal/lapCount, m2 = (double)lapTotal2/lapCount;
		return std::max(0.0, m2 - m*m);
	}
	double std() const {
		return sqrt(var());
	}
	double best() const {
		return std::max(0.0, lapBest - lapOverhead);
	}
	double optimistic(double deviations=1) const {
		return std::max(best(), mean() - std()*deviations);
	}
};

#endif // include guard

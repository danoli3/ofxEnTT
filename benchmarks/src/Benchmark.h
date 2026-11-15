#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace bench {
inline volatile double sink = 0;
inline void require(bool condition, const char * message) {
	if (!condition) throw std::runtime_error(message);
}
template <typename Function>
double measure(Function && function, int iterations) {
	auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < iterations; ++i) sink = function();
	return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / iterations;
}
template <typename Before, typename After>
void compare(const char * label, Before before, After after, int iterations = 20) {
	for (int i = 0; i < 3; ++i) { sink = before(); sink = after(); }
	std::array<double, 11> a, b;
	for (size_t i = 0; i < a.size(); ++i) {
		if (i % 2) { b[i] = measure(after, iterations); a[i] = measure(before, iterations); }
		else { a[i] = measure(before, iterations); b[i] = measure(after, iterations); }
	}
	std::sort(a.begin(), a.end());
	std::sort(b.begin(), b.end());
	std::printf("%-36s before %9.2f us  after %9.2f us  time %+.1f%%\n",
		label, a[5], b[5], 100.0 * (b[5] / a[5] - 1.0));
	std::fflush(stdout);
}
}

#pragma once
#include <cstddef>
#include <functional>
#include <memory>

namespace ofxEnTT {

/// Synchronous range jobs on persistent workers. One thread (the caller) by
/// default. Configuration/submission are not reentrant. Callbacks must only
/// write disjoint data; defer registry structural edits until all ranges finish.
class ParallelExecutor {
public:
	ParallelExecutor();
	~ParallelExecutor();
	ParallelExecutor(const ParallelExecutor &) = delete;
	ParallelExecutor & operator=(const ParallelExecutor &) = delete;
	/// Total threads including the caller. 0 means 1. Threadless WebAssembly uses 1.
	void setThreadCount(size_t count);
	size_t getThreadCount() const;
	/// Returns after every worker completes; rethrows a callback exception on the
	/// caller. Small inputs use the caller. The pool is reusable after exceptions.
	void forEachRange(size_t count, const std::function<void(size_t, size_t)> & function,
		size_t minItemsPerThread = 16384);

private:
	struct State;
	std::unique_ptr<State> state;
};
} // namespace ofxEnTT

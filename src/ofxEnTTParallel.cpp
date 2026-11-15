#include "ofxEnTTParallel.h"
#include <algorithm>
#include <stdexcept>

#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace ofxEnTT {
struct ParallelExecutor::State {
	std::mutex mutex;
	std::condition_variable start, done;
	std::vector<std::thread> workers;
	std::function<void(size_t, size_t)> job;
	std::exception_ptr error;
	size_t generation = 0, completed = 0, count = 0, chunks = 1;
	bool stopping = false, running = false;

	void runSlot(size_t slot) {
		if (slot >= chunks) return;
		const auto base = count / chunks, extra = count % chunks;
		const auto begin = base * slot + std::min(slot, extra);
		const auto end = begin + base + (slot < extra ? 1 : 0);
		try { job(begin, end); }
		catch (...) {
			std::lock_guard lock(mutex);
			if (!error) error = std::current_exception();
		}
	}
	void worker(size_t slot) {
		size_t seen = 0;
		std::unique_lock lock(mutex);
		for (;;) {
			start.wait(lock, [&] { return stopping || generation != seen; });
			if (stopping) return;
			seen = generation;
			lock.unlock();
			runSlot(slot);
			lock.lock();
			if (++completed == workers.size()) done.notify_one();
		}
	}
	void stop() {
		{ std::lock_guard lock(mutex); stopping = true; }
		start.notify_all();
		for (auto & worker : workers) worker.join();
		workers.clear();
		stopping = false;
		generation = 0;
	}
};

ParallelExecutor::ParallelExecutor() : state(std::make_unique<State>()) { }
ParallelExecutor::~ParallelExecutor() { state->stop(); }
size_t ParallelExecutor::getThreadCount() const { return state->workers.size() + 1; }

void ParallelExecutor::setThreadCount(size_t count) {
	{
		std::lock_guard lock(state->mutex);
		if (state->running) throw std::logic_error("ParallelExecutor cannot be configured inside a job");
	}
	count = std::max<size_t>(count, 1);
	if (count == getThreadCount()) return;
	state->stop();
	try {
		for (size_t slot = 1; slot < count; ++slot)
			state->workers.emplace_back([this, slot] { state->worker(slot); });
	} catch (...) { state->stop(); throw; }
}

void ParallelExecutor::forEachRange(size_t count, const std::function<void(size_t, size_t)> & function,
	size_t minItemsPerThread) {
	if (count == 0) return;
	std::unique_lock lock(state->mutex);
	if (state->running) throw std::logic_error("ParallelExecutor jobs cannot be nested or submitted concurrently");
	const auto chunks = std::min(getThreadCount(), std::max<size_t>(1, count / std::max<size_t>(1, minItemsPerThread)));
	if (chunks == 1) {
		state->running = true;
		lock.unlock();
		try { function(0, count); }
		catch (...) { std::lock_guard reset(state->mutex); state->running = false; throw; }
		lock.lock();
		state->running = false;
		return;
	}
	state->job = function;
	state->error = nullptr;
	state->count = count; state->chunks = chunks; state->completed = 0;
	state->running = true;
	++state->generation;
	lock.unlock();
	state->start.notify_all();
	state->runSlot(0);
	lock.lock();
	state->done.wait(lock, [&] { return state->completed == state->workers.size(); });
	auto error = state->error;
	state->job = {};
	state->running = false;
	lock.unlock();
	if (error) std::rethrow_exception(error);
}
} // namespace ofxEnTT

#else
// Keep the addon usable in ordinary WebAssembly builds without pthread support.
namespace ofxEnTT {
struct ParallelExecutor::State { bool running = false; };
ParallelExecutor::ParallelExecutor() : state(std::make_unique<State>()) { }
ParallelExecutor::~ParallelExecutor() = default;
size_t ParallelExecutor::getThreadCount() const { return 1; }
void ParallelExecutor::setThreadCount(size_t) {
	if (state->running) throw std::logic_error("ParallelExecutor cannot be configured inside a job");
}
void ParallelExecutor::forEachRange(size_t count, const std::function<void(size_t, size_t)> & function, size_t) {
	if (count == 0) return;
	if (state->running) throw std::logic_error("ParallelExecutor jobs cannot be nested");
	state->running = true;
	try { function(0, count); }
	catch (...) { state->running = false; throw; }
	state->running = false;
}
} // namespace ofxEnTT
#endif

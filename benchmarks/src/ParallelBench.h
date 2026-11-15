#pragma once
#include "Benchmark.h"
#include "MotionBench.h"

namespace bench {
inline void parallelBenchmark() {
	using namespace ofxEnTT;
	for (size_t count : { 20000, 200000 }) for (bool angular : { false, true }) {
		World before, after;
		populateMotion(before, count, angular); populateMotion(after, count, angular);
		before.enableVelocityGroup(); after.enableVelocityGroup();
		after.workers.setThreadCount(4);
		auto entities = before.getEntities();
		char label[96]; std::snprintf(label, sizeof(label), "parallel World / %zu / %s", count, angular ? "rotating" : "linear");
		compare(label, [&] {
			before.update(1.f / 60.f);
			return double(before.registry.get<WorldTransform>(entities[0]).matrix[3].x);
		}, [&] {
			after.update(1.f / 60.f);
			return double(after.registry.get<WorldTransform>(entities[0]).matrix[3].x);
		}, count == 20000 ? 20 : 5);
		for (auto e : entities) {
			const auto & a = before.registry.get<Transform>(e);
			const auto & b = after.registry.get<Transform>(e);
			require(glm::all(glm::lessThanEqual(glm::abs(a.position - b.position), glm::vec3(1e-4f)))
				&& glm::all(glm::lessThanEqual(glm::abs(glm::vec4(a.orientation.x, a.orientation.y, a.orientation.z, a.orientation.w)
					- glm::vec4(b.orientation.x, b.orientation.y, b.orientation.z, b.orientation.w)), glm::vec4(1e-5f))), "parallel world motion mismatch");
		}
	}
	ParallelExecutor executor; executor.setThreadCount(4);
	for (size_t count : { 20000, 200000, 2000000 }) {
		LinearMotionBatch before, after;
		before.reserve(count); after.reserve(count);
		for (size_t i = 0; i < count; ++i) {
			glm::vec3 p(float(i % 97), .5f, 1.f), v(.1f, float(i % 17) * .02f, -.3f);
			before.add(p, v); after.add(p, v);
		}
		char label[80]; std::snprintf(label, sizeof(label), "parallel SoA / %zu", count);
		compare(label, [&] { before.update(1.f / 60.f); return double(before.position(0).x + before.position(count - 1).y); },
			[&] { after.update(1.f / 60.f, executor); return double(after.position(0).x + after.position(count - 1).y); }, count == 20000 ? 20 : 5);
		for (size_t i = 0; i < count; ++i) require(glm::all(glm::lessThanEqual(
			glm::abs(before.position(i) - after.position(i)), glm::vec3(1e-4f))), "parallel bulk motion mismatch");
	}
}
}

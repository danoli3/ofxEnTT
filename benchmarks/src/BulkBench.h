#pragma once
#include "Benchmark.h"
#include "MotionBench.h"

namespace bench {
inline void bulkBenchmark() {
	using namespace ofxEnTT;
	struct Particle { glm::vec3 position, velocity; };
	for (size_t count : { 20000, 200000, 2000000 }) {
		std::vector<Particle> before;
		before.reserve(count);
		LinearMotionBatch after;
		after.reserve(count);
		for (size_t i = 0; i < count; ++i) {
			glm::vec3 position(float(i % 97), .5f, -1.f), velocity(.3f, float(i % 17) * .02f, -.1f);
			before.push_back({ position, velocity }); after.add(position, velocity);
		}
		char label[80]; std::snprintf(label, sizeof(label), "SoA kernel / %zu", count);
		compare(label, [&] {
			for (auto & p : before) p.position += p.velocity * (1.f / 60.f);
			return double(before.front().position.x + before.back().position.y);
		}, [&] {
			after.update(1.f / 60.f);
			return double(after.position(0).x + after.position(count - 1).y);
		}, count == 20000 ? 20 : 5);
		for (size_t i = 0; i < count; ++i)
			require(glm::all(glm::lessThanEqual(glm::abs(before[i].position - after.position(i)), glm::vec3(1e-4f))), "SoA/AoS integration mismatch");
	}
	for (size_t count : { 20000, 200000 }) {
		World before, after;
		populateMotion(before, count, false); populateMotion(after, count, false);
		auto entities = after.getEntities();
		LinearMotionBatch batch; batch.reserve(count);
		for (auto e : entities) batch.add(after.registry.get<Transform>(e).position, after.registry.get<Velocity>(e).linear, e);
		char label[80]; std::snprintf(label, sizeof(label), "SoA + ECS scatter / %zu", count);
		compare(label, [&] {
			detail::forEachVelocity(before.registry, false, [](Transform & t, const Velocity & v) { detail::integrateVelocity(t, v, 1.f / 60.f); });
			return double(before.registry.get<Transform>(entities[0]).position.x);
		}, [&] {
			batch.update(1.f / 60.f); batch.syncTo(after);
			return double(after.registry.get<Transform>(entities[0]).position.x);
		}, count == 20000 ? 20 : 5);
		for (auto e : entities) require(glm::all(glm::lessThanEqual(glm::abs(
			before.registry.get<Transform>(e).position - after.registry.get<Transform>(e).position), glm::vec3(1e-4f))), "SoA scatter mismatch");
	}
}
}

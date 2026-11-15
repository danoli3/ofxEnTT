#pragma once
#include "Benchmark.h"
#include "detail/ofxEnTTMotion.h"

namespace bench {
inline void populateMotion(ofxEnTT::World & world, size_t count, bool angular) {
	auto entities = world.create(count);
	for (size_t i = 0; i < count; ++i) {
		world.registry.emplace<ofxEnTT::Velocity>(entities[i],
			glm::vec3(.1f, float(i % 17) * .02f, -.3f), angular ? glm::vec3(.1f, .2f, .3f) : glm::vec3(0.f));
	}
	for (size_t i = 0; i < count; i += 7) world.registry.remove<ofxEnTT::Velocity>(entities[i]);
	for (size_t i = 0; i < count; i += 7) world.registry.emplace<ofxEnTT::Velocity>(entities[i], glm::vec3(.5f), glm::vec3(0.f));
}
inline double motionFrame(ofxEnTT::World & world) {
	double sum = 0;
	ofxEnTT::detail::forEachVelocity(world.registry, world.isVelocityGroupEnabled(),
		[&](ofxEnTT::Transform & t, const ofxEnTT::Velocity & v) {
			ofxEnTT::detail::integrateVelocity(t, v, 1.f / 60.f);
			sum += t.position.x + t.orientation.w;
		});
	return sum;
}
inline void groupsBenchmark() {
	using namespace ofxEnTT;
	for (size_t count : { 20000, 200000 }) for (bool angular : { false, true }) {
		World before, after;
		populateMotion(before, count, angular);
		populateMotion(after, count, angular);
		after.enableVelocityGroup(); // Cold setup/reordering is deliberately outside timing.
		char label[80];
		std::snprintf(label, sizeof(label), "owning group / %zu / %s", count, angular ? "rotating" : "linear");
		compare(label, [&] { return motionFrame(before); }, [&] { return motionFrame(after); }, count == 20000 ? 20 : 5);
		for (auto entity : before.registry.view<Transform>()) {
			const auto & a = before.registry.get<Transform>(entity);
			const auto & b = after.registry.get<Transform>(entity);
			require(a.position == b.position && a.orientation == b.orientation, "group/view motion mismatch");
		}
	}
}
}

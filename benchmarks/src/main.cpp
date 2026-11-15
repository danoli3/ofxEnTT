#include "ofxEnTT.h"
#include "Benchmark.h"
#include "RenderBench.h"
#include "MotionBench.h"
#include "BulkBench.h"
#include "ParallelBench.h"
#include <random>

using namespace ofxEnTT;

namespace {
// Frozen pre-optimization TRS and traversal, including registry notifications.
glm::mat4 originalLocal(const Transform & transform) {
	return glm::translate(glm::mat4(1.f), transform.position)
		* glm::mat4_cast(transform.orientation)
		* glm::scale(glm::mat4(1.f), transform.scale);
}
void originalVisit(Registry & registry, Entity entity, const glm::mat4 & parent) {
	auto world = parent;
	if (auto * t = registry.try_get<Transform>(entity)) world = parent * originalLocal(*t);
	registry.emplace_or_replace<WorldTransform>(entity, world);
	if (auto * children = registry.try_get<Children>(entity)) {
		for (auto child : children->entities) if (registry.valid(child)) originalVisit(registry, child, world);
	}
}
void originalUpdate(Registry & registry) {
	std::vector<Entity> roots;
	for (auto entity : registry.view<Transform>()) {
		auto * parent = registry.try_get<Parent>(entity);
		if (!parent || !registry.valid(parent->entity)) roots.push_back(entity);
	}
	for (auto entity : roots) originalVisit(registry, entity, glm::mat4(1.f));
}
bool near(const glm::mat4 & a, const glm::mat4 & b) {
	for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r)
		if (std::abs(a[c][r] - b[c][r]) > 1e-4f * std::max(1.f, std::abs(a[c][r]))) return false;
	return true;
}
void populate(World & world, size_t count, bool hierarchy) {
	std::mt19937 random(42);
	std::uniform_real_distribution<float> value(-2.f, 2.f);
	auto entities = world.create(count);
	for (size_t i = 0; i < count; ++i) {
		auto & t = world.registry.get<Transform>(entities[i]);
		t.position = { value(random), value(random), value(random) };
		t.orientation = glm::quat(glm::vec3(value(random), value(random), value(random)));
		t.scale = { value(random), value(random), value(random) };
		if (hierarchy && i % 8) world.setParent(entities[i], entities[i - 1]);
	}
}
double checksum(Registry & registry) {
	double result = 0;
	registry.view<const WorldTransform>().each([&](const WorldTransform & t) { result += t.matrix[3].x; });
	return result;
}
void transformsBenchmark() {
	for (bool hierarchy : { false, true }) {
		World before, after;
		populate(before, 20000, hierarchy);
		populate(after, 20000, hierarchy);
		originalUpdate(before.registry);
		after.transforms.update(after.registry);
		for (auto entity : before.registry.view<Transform>()) {
			bench::require(near(before.registry.get<WorldTransform>(entity).matrix,
				after.registry.get<WorldTransform>(entity).matrix), "TRS/hierarchy mismatch");
		}
		bench::compare(hierarchy ? "transforms / 20k / chains of 8" : "transforms / 20k / roots",
			[&] { originalUpdate(before.registry); return checksum(before.registry); },
			[&] { after.transforms.update(after.registry); return checksum(after.registry); });
	}
}
void incrementalBenchmark() {
	for (bool hierarchy : { false, true }) for (int percent : { 0, 1, 100 }) {
		World before, after;
		populate(before, 20000, hierarchy); populate(after, 20000, hierarchy);
		after.transforms.setIncremental(true);
		auto oldEntities = before.getEntities(), newEntities = after.getEntities();
		auto frame = [percent](World & world, const std::vector<Entity> & entities) {
			for (size_t i = 0; i < entities.size() * percent / 100; ++i)
				world.registry.get<Transform>(entities[i]).position.x += .001f;
			world.transforms.update(world.registry);
			return checksum(world.registry);
		};
		char label[96];
		std::snprintf(label, sizeof(label), "incremental / %s / %d%% edits", hierarchy ? "chains" : "roots", percent);
		bench::compare(label, [&] { return frame(before, oldEntities); }, [&] { return frame(after, newEntities); });
		for (auto e : oldEntities) bench::require(near(before.registry.get<WorldTransform>(e).matrix,
			after.registry.get<WorldTransform>(e).matrix), "incremental/full matrix mismatch");
	}
}

}

int main() {
	try {
		transformsBenchmark();
		bench::renderBenchmark();
		bench::groupsBenchmark();
		incrementalBenchmark();
		bench::bulkBenchmark();
		bench::parallelBenchmark();
	} catch (const std::exception & error) {
		std::fprintf(stderr, "%s\n", error.what());
		return 1;
	}
}

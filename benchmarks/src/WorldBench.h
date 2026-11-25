#pragma once
#include "Benchmark.h"
#include "ofxEnTT.h"
#include <algorithm>

namespace bench {

// Pre-fix subtree destroy. Copies the children list and recursively destroys;
// each recursive call's detach pattern does a linear erase from the parent's
// still-populated siblings vector, which turns N-sibling teardown into O(N^2).
inline void slowDestroy(ofxEnTT::World & world, ofxEnTT::Entity entity) {
	using namespace ofxEnTT;
	auto & registry = world.registry;
	if (!registry.valid(entity)) return;
	if (auto * children = registry.try_get<Children>(entity)) {
		auto list = children->entities; // copy (pre-fix)
		for (auto child : list) {
			slowDestroy(world, child);
		}
	}
	if (auto * parent = registry.try_get<Parent>(entity)) {
		if (registry.valid(parent->entity)) {
			if (auto * siblings = registry.try_get<Children>(parent->entity)) {
				auto & sl = siblings->entities;
				sl.erase(std::remove(sl.begin(), sl.end(), entity), sl.end());
			}
		}
		registry.remove<Parent>(entity);
	}
	registry.destroy(entity);
}

// Builds `siblings` children of a fresh root. Returns the root entity.
inline ofxEnTT::Entity buildFanOut(ofxEnTT::World & world, size_t siblings) {
	auto root = world.create("root");
	for (size_t i = 0; i < siblings; ++i) {
		auto child = world.create();
		world.setParent(child, root);
	}
	return root;
}

inline void subtreeDestroyBenchmark() {
	using namespace ofxEnTT;
	for (size_t siblings : { size_t(500), size_t(2000), size_t(8000) }) {
		char label[80];
		std::snprintf(label, sizeof(label), "subtree destroy / root + %zu siblings", siblings);
		// Each iteration pays the same setup cost on both sides, so the delta
		// is the destroy itself; checksums confirm both paths destroyed the tree.
		auto before = [&] {
			World world;
			auto root = buildFanOut(world, siblings);
			slowDestroy(world, root);
			return double(world.size());
		};
		auto after = [&] {
			World world;
			auto root = buildFanOut(world, siblings);
			world.destroy(root);
			return double(world.size());
		};
		// Correctness check: both reach 0 entities.
		require(before() == 0 && after() == 0, "destroy must empty the world");
		// Fewer iterations for the bigger scenes; the O(N^2) side gets slow.
		int iterations = siblings <= 2000 ? 20 : 5;
		compare(label, before, after, iterations);
	}
}

// Pre-fix incremental-cache cleanup: scan the entire cache every frame to erase
// entries whose lastSeen != frame. Post-fix: skip the scan when every entry was
// visited this frame (visited == cache.size()), which is the steady state.
// We model the saved work on the same dense_map type the TransformSystem uses,
// filled with N entries all marked with the current frame tag.
inline void transformCacheCleanupBenchmark() {
	for (size_t count : { size_t(20000), size_t(200000) }) {
		entt::dense_map<ofxEnTT::Entity, size_t> cache;
		for (size_t i = 0; i < count; ++i) {
			cache.try_emplace(ofxEnTT::Entity(entt::entity(i)), size_t(1));
		}
		const size_t frame = 1;
		// Pre-fix: always scan and consider erasing.
		auto before = [&] {
			size_t survived = 0;
			for (auto it = cache.begin(); it != cache.end();) {
				if (it->second != frame) it = cache.erase(it);
				else { ++survived; ++it; }
			}
			return double(survived);
		};
		// Post-fix: steady state visits every entry, so visited == cache.size()
		// and the scan is skipped. We still need to compute `visited` to match
		// what TransformSystem does (stats.updated + stats.skipped).
		auto after = [&] {
			const size_t visited = count; // all entries visited this frame
			if (visited != cache.size()) {
				for (auto it = cache.begin(); it != cache.end();) {
					if (it->second != frame) it = cache.erase(it);
					else ++it;
				}
			}
			return double(visited);
		};
		char label[96];
		std::snprintf(label, sizeof(label), "transform cache stale scan / %zu / stable", count);
		compare(label, before, after, count <= 20000 ? 50 : 10);
	}
}

inline void worldBenchmark() {
	subtreeDestroyBenchmark();
	transformCacheCleanupBenchmark();
}

} // namespace bench

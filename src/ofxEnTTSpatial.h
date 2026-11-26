#pragma once

#include "ofxEnTTComponents.h"

#include <entt/entt.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ofxEnTT {

/// Uniform-grid broadphase for entities with LocalBounds + SpatialMember.
///
/// Opt in per entity (add SpatialMember) and per scene (the hash is never
/// rebuilt unless you call rebuild). The grid stores entity ids per cell;
/// queries return ids, not pointers, so the cache survives entity churn.
///
///     SpatialHash hash;
///     hash.setCellSize(10.f);
///     hash.rebuild(world.registry);
///     hash.queryRadius(point, 25.f, [&](Entity e) { ... });
///
/// Best for scenes with many small bounded things to query (voxels, boids,
/// pickables). For a handful of entities, or when every entity is queried
/// every frame anyway, a plain view is probably faster.
class SpatialHash {
public:
	using Callback = void(Entity);

	SpatialHash() = default;

	void setCellSize(float cellSize) { this->cellSize = cellSize > 0.f ? cellSize : 1.f; }
	float getCellSize() const { return cellSize; }

	/// Clears and rebuilds the index from every entity with LocalBounds,
	/// WorldTransform and SpatialMember. Call once per frame after transforms
	/// are up to date (world.update() has run, or you have called
	/// transforms.update(registry) yourself).
	void rebuild(Registry & registry);

	/// Empties every cell but keeps their capacities.
	void clear();

	/// Calls fn(Entity) once for each entity whose cell intersects [min, max].
	/// A single entity may be reported more than once if its bounds span
	/// several cells; dedup at the call site when that matters.
	template <typename Fn>
	void queryBounds(const glm::vec3 & min, const glm::vec3 & max, Fn && fn) const {
		const auto lo = toCell(min);
		const auto hi = toCell(max);
		for (int32_t z = lo.z; z <= hi.z; ++z) {
			for (int32_t y = lo.y; y <= hi.y; ++y) {
				for (int32_t x = lo.x; x <= hi.x; ++x) {
					auto it = cells.find(Key { x, y, z });
					if (it == cells.end()) continue;
					for (Entity entity : it->second) fn(entity);
				}
			}
		}
	}

	/// queryBounds on an axis-aligned sphere's bounding cube. The callback
	/// still runs per cell hit, so filter by distance inside it if needed.
	template <typename Fn>
	void queryRadius(const glm::vec3 & center, float radius, Fn && fn) const {
		const glm::vec3 r(radius);
		queryBounds(center - r, center + r, std::forward<Fn>(fn));
	}

	/// Diagnostics.
	struct Stats {
		size_t entities = 0;     ///< entities inserted on the last rebuild
		size_t occupiedCells = 0;///< non-empty buckets
		size_t totalInsertions = 0;///< including entities spanning cells more than once
	};
	const Stats & getStats() const { return stats; }

private:
	struct Key {
		int32_t x, y, z;
		bool operator==(const Key &) const = default;
	};
	struct KeyHash {
		size_t operator()(const Key & key) const {
			size_t h = static_cast<size_t>(key.x) * 73856093u;
			h ^= static_cast<size_t>(key.y) * 19349663u;
			h ^= static_cast<size_t>(key.z) * 83492791u;
			return h;
		}
	};
	using Bucket = std::vector<Entity>;
	using Grid = std::unordered_map<Key, Bucket, KeyHash>;

	Key toCell(const glm::vec3 & p) const;

	float cellSize = 1.f;
	Grid cells;
	Stats stats;
};

} // namespace ofxEnTT

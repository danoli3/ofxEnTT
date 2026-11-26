#include "ofxEnTTSpatial.h"

#include <cmath>

namespace ofxEnTT {

SpatialHash::Key SpatialHash::toCell(const glm::vec3 & p) const {
	return {
		static_cast<int32_t>(std::floor(p.x / cellSize)),
		static_cast<int32_t>(std::floor(p.y / cellSize)),
		static_cast<int32_t>(std::floor(p.z / cellSize)),
	};
}

void SpatialHash::clear() {
	for (auto & [key, bucket] : cells) bucket.clear();
	stats = {};
}

void SpatialHash::rebuild(Registry & registry) {
	clear();
	auto view = registry.view<const LocalBounds, const WorldTransform, const SpatialMember>();
	for (auto [entity, bounds, world] : view.each()) {
		// world-space AABB: transform the 8 corners of the local bounds.
		glm::vec3 lo(std::numeric_limits<float>::max());
		glm::vec3 hi(-std::numeric_limits<float>::max());
		for (int i = 0; i < 8; ++i) {
			glm::vec3 corner {
				(i & 1) ? bounds.max.x : bounds.min.x,
				(i & 2) ? bounds.max.y : bounds.min.y,
				(i & 4) ? bounds.max.z : bounds.min.z,
			};
			glm::vec3 p = glm::vec3(world.matrix * glm::vec4(corner, 1.f));
			lo = glm::min(lo, p);
			hi = glm::max(hi, p);
		}
		const auto cellLo = toCell(lo);
		const auto cellHi = toCell(hi);
		++stats.entities;
		for (int32_t z = cellLo.z; z <= cellHi.z; ++z) {
			for (int32_t y = cellLo.y; y <= cellHi.y; ++y) {
				for (int32_t x = cellLo.x; x <= cellHi.x; ++x) {
					cells[Key { x, y, z }].push_back(entity);
					++stats.totalInsertions;
				}
			}
		}
	}
	stats.occupiedCells = 0;
	for (const auto & [key, bucket] : cells) {
		if (!bucket.empty()) ++stats.occupiedCells;
	}
}

} // namespace ofxEnTT

#include "ofxEnTTCulling.h"

#include "ofCamera.h"

#include <glm/geometric.hpp>

#include <cmath>

namespace ofxEnTT {

namespace {

CullingSystem::Plane planeFromRow(const glm::vec4 & row) {
	CullingSystem::Plane p;
	p.normal = glm::vec3(row);
	p.distance = row.w;
	const float length = glm::length(p.normal);
	if (length > 0.f) {
		p.normal /= length;
		p.distance /= length;
	}
	return p;
}

} // namespace

void CullingSystem::rebuildPlanes(const glm::mat4 & vp) {
	// Row-extraction of the six frustum planes from a column-major view*proj.
	// GLM stores m[col][row]; the row vector we need is (m[0][r], m[1][r], m[2][r], m[3][r]).
	const glm::vec4 rows[4] = {
		{ vp[0][0], vp[1][0], vp[2][0], vp[3][0] },
		{ vp[0][1], vp[1][1], vp[2][1], vp[3][1] },
		{ vp[0][2], vp[1][2], vp[2][2], vp[3][2] },
		{ vp[0][3], vp[1][3], vp[2][3], vp[3][3] },
	};
	planes[0] = planeFromRow(rows[3] + rows[0]); // left
	planes[1] = planeFromRow(rows[3] - rows[0]); // right
	planes[2] = planeFromRow(rows[3] + rows[1]); // bottom
	planes[3] = planeFromRow(rows[3] - rows[1]); // top
	planes[4] = planeFromRow(rows[3] + rows[2]); // near
	planes[5] = planeFromRow(rows[3] - rows[2]); // far
}

bool CullingSystem::intersects(const glm::vec3 & min, const glm::vec3 & max) const {
	for (const auto & p : planes) {
		// positive-vertex (p-vertex) test: the AABB corner furthest along the
		// plane normal. If it is behind the plane, the whole box is outside.
		glm::vec3 pv {
			p.normal.x >= 0.f ? max.x : min.x,
			p.normal.y >= 0.f ? max.y : min.y,
			p.normal.z >= 0.f ? max.z : min.z,
		};
		if (glm::dot(p.normal, pv) + p.distance < 0.f) return false;
	}
	return true;
}

void CullingSystem::update(Registry & registry, const ofCamera & camera) {
	const auto vp = camera.getProjectionMatrix() * camera.getModelViewMatrix();
	update(registry, vp);
}

void CullingSystem::update(Registry & registry, const glm::mat4 & vp) {
	rebuildPlanes(vp);
	stats = {};
	auto view = registry.view<const LocalBounds, const WorldTransform>();
	for (auto [entity, bounds, world] : view.each()) {
		// world-space AABB from the eight transformed corners
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
		++stats.tested;
		const bool visible = intersects(lo, hi);
		const bool wasCulled = registry.all_of<Culled>(entity);
		if (!visible) {
			++stats.culled;
			if (!wasCulled) registry.emplace<Culled>(entity);
		} else if (wasCulled) {
			registry.remove<Culled>(entity);
		}
	}
}

void CullingSystem::clearTags(Registry & registry) {
	registry.clear<Culled>();
	stats = {};
}

} // namespace ofxEnTT

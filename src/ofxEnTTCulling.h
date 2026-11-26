#pragma once

#include "ofxEnTTComponents.h"

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>

class ofCamera;

namespace ofxEnTT {

/// Opt-in frustum culling for entities that have LocalBounds.
///
/// An entity is tagged Culled when its world-space AABB falls outside the
/// camera frustum; the RenderSystem's batch view excludes Culled, so culled
/// entities are skipped at no per-draw cost. Entities without LocalBounds are
/// never touched: they stay visible, as before this system existed.
///
///     CullingSystem culling;
///     culling.update(world.registry, cam); // after world.update()
///     world.draw();                        // Culled is excluded from batches
///
/// Or wire it into the frame as a system:
///
///     world.addSystem("culling", [&](World & w, float) {
///         culling.update(w.registry, cam);
///     });
///
/// Not a win for every scene: a handful of entities, or scenes where most
/// entities are always on screen, pay the AABB test without saving any draws.
/// Measure before leaving it on.
class CullingSystem {
public:
	struct Plane {
		glm::vec3 normal { 0.f };
		float distance { 0.f };
	};

	struct Stats {
		size_t tested = 0;
		size_t culled = 0;
	};

	CullingSystem() = default;

	/// Fills the six frustum planes from view * projection and runs the test.
	/// Reads WorldTransform and LocalBounds; writes / removes the Culled tag.
	void update(Registry & registry, const ofCamera & camera);
	void update(Registry & registry, const glm::mat4 & viewProjection);

	/// Removes the Culled tag from every entity. Call when turning culling
	/// off at runtime so hidden entities come back.
	void clearTags(Registry & registry);

	const Stats & getStats() const { return stats; }
	const std::array<Plane, 6> & getPlanes() const { return planes; }

private:
	void rebuildPlanes(const glm::mat4 & viewProjection);
	bool intersects(const glm::vec3 & boundsMin, const glm::vec3 & boundsMax) const;

	std::array<Plane, 6> planes;
	Stats stats;
};

} // namespace ofxEnTT

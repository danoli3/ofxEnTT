#pragma once

#include "ofxEnTTComponents.h"
#include <span>

namespace ofxEnTT {
class World;

/// Optional structure-of-arrays storage for independent linear particles.
/// Positions/velocities live here; syncTo() explicitly copies positions to bound
/// entities. Do not also give those entities a built-in Velocity unless you want
/// a second integration step. No global GLM layout or CPU-specific flags needed.
class LinearMotionBatch {
public:
	void reserve(size_t count);
	size_t add(const glm::vec3 & position, const glm::vec3 & velocity, Entity entity = Null);
	/// Swap-removes an item, moving the last item into its index.
	void remove(size_t index);
	void clear();
	size_t size() const { return entities.size(); }
	Entity entity(size_t index) const { return entities.at(index); }
	glm::vec3 position(size_t index) const;
	glm::vec3 velocity(size_t index) const;
	void setPosition(size_t index, const glm::vec3 & value);
	void setVelocity(size_t index, const glm::vec3 & value);

	std::span<float> positionX() { return px; }
	std::span<float> positionY() { return py; }
	std::span<float> positionZ() { return pz; }
	std::span<float> velocityX() { return vx; }
	std::span<float> velocityY() { return vy; }
	std::span<float> velocityZ() { return vz; }

	void update(float deltaSeconds);
	/// Serial scatter. Skips deleted/unbound entities and entities without Transform.
	/// Does not create components, rotate entities, or update world matrices.
	void syncTo(World & world) const;

private:
	std::vector<float> px, py, pz, vx, vy, vz;
	std::vector<Entity> entities;
	void integrate(size_t begin, size_t end, float deltaSeconds);
};
} // namespace ofxEnTT

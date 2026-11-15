#include "ofxEnTTLinearMotion.h"
#include "ofxEnTTWorld.h"
#include <algorithm>
#include <stdexcept>

namespace ofxEnTT {

void LinearMotionBatch::reserve(size_t count) {
	for (auto * values : { &px, &py, &pz, &vx, &vy, &vz }) values->reserve(count);
	entities.reserve(count);
}

size_t LinearMotionBatch::add(const glm::vec3 & position, const glm::vec3 & velocity, Entity entity) {
	const auto index = size();
	// Reserve all arrays before changing any sizes, so failed allocation leaves
	// a valid batch. The following scalar pushes cannot allocate or throw.
	if (index == entities.capacity()) reserve(std::max<size_t>(16, index * 2));
	px.push_back(position.x); py.push_back(position.y); pz.push_back(position.z);
	vx.push_back(velocity.x); vy.push_back(velocity.y); vz.push_back(velocity.z);
	entities.push_back(entity);
	return index;
}

void LinearMotionBatch::remove(size_t index) {
	if (index >= size()) throw std::out_of_range("LinearMotionBatch::remove");
	for (auto * values : { &px, &py, &pz, &vx, &vy, &vz }) {
		(*values)[index] = values->back(); values->pop_back();
	}
	entities[index] = entities.back(); entities.pop_back();
}

void LinearMotionBatch::clear() {
	for (auto * values : { &px, &py, &pz, &vx, &vy, &vz }) values->clear();
	entities.clear();
}

glm::vec3 LinearMotionBatch::position(size_t index) const { return { px.at(index), py.at(index), pz.at(index) }; }
glm::vec3 LinearMotionBatch::velocity(size_t index) const { return { vx.at(index), vy.at(index), vz.at(index) }; }
void LinearMotionBatch::setPosition(size_t index, const glm::vec3 & value) {
	px.at(index) = value.x; py.at(index) = value.y; pz.at(index) = value.z;
}
void LinearMotionBatch::setVelocity(size_t index, const glm::vec3 & value) {
	vx.at(index) = value.x; vy.at(index) = value.y; vz.at(index) = value.z;
}

void LinearMotionBatch::integrate(size_t begin, size_t end, float dt) {
	// Independent unit-stride loops let the compiler vectorize across entities.
	for (size_t i = begin; i < end; ++i) px[i] += vx[i] * dt;
	for (size_t i = begin; i < end; ++i) py[i] += vy[i] * dt;
	for (size_t i = begin; i < end; ++i) pz[i] += vz[i] * dt;
}

void LinearMotionBatch::update(float dt) { integrate(0, size(), dt); }

void LinearMotionBatch::syncTo(World & world) const {
	for (size_t i = 0; i < size(); ++i) {
		if (!world.registry.valid(entities[i])) continue;
		if (auto * transform = world.registry.try_get<Transform>(entities[i])) {
			transform->position = { px[i], py[i], pz[i] };
		}
	}
}

} // namespace ofxEnTT

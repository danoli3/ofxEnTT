#pragma once
#include "ofxEnTTComponents.h"

namespace ofxEnTT::detail {
inline void integrateVelocity(Transform & transform, const Velocity & velocity, float dt) {
	transform.position += velocity.linear * dt;
	if (glm::length(velocity.angular) > 0.f) {
		transform.orientation = glm::normalize(glm::quat(velocity.angular * dt) * transform.orientation);
	}
}

template <typename Function>
void forEachVelocity(Registry & registry, bool grouped, Function function) {
	if (grouped) registry.group<Transform, Velocity>().each(function);
	else registry.view<Transform, const Velocity>().each(function);
}
} // namespace ofxEnTT::detail

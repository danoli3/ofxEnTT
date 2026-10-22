#include "ofxEnTTTransformSystem.h"

namespace ofxEnTT {

void TransformSystem::visit(Registry & registry, Entity entity, const glm::mat4 & parentMatrix) {
	glm::mat4 world = parentMatrix;
	if (auto * transform = registry.try_get<Transform>(entity)) {
		world = parentMatrix * transform->localMatrix();
	}
	registry.emplace_or_replace<WorldTransform>(entity, world);

	if (auto * children = registry.try_get<Children>(entity)) {
		for (auto child : children->entities) {
			if (registry.valid(child)) {
				visit(registry, child, world);
			}
		}
	}
}

void TransformSystem::update(Registry & registry) {
	// roots: anything with a Transform and no (valid) Parent
	std::vector<Entity> roots;
	for (auto entity : registry.view<Transform>()) {
		auto * parent = registry.try_get<Parent>(entity);
		if (!parent || !registry.valid(parent->entity)) {
			roots.push_back(entity);
		}
	}
	for (auto entity : roots) {
		visit(registry, entity, glm::mat4(1.f));
	}
}

} // namespace ofxEnTT

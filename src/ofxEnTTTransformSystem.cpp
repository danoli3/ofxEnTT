#include "ofxEnTTTransformSystem.h"

namespace ofxEnTT {

void TransformSystem::setIncremental(bool enabled) {
	if (incremental != enabled) {
		incremental = enabled;
		invalidate();
	}
}

void TransformSystem::invalidate() {
	cache.clear();
}

void TransformSystem::markDirty(Entity entity) {
	if (auto found = cache.find(entity); found != cache.end()) {
		found->second.dirty = true;
	}
}

void TransformSystem::visit(Registry & registry, Entity entity, const glm::mat4 * parentMatrix,
	Entity parent, bool parentChanged) {
	auto * transform = registry.try_get<Transform>(entity);
	auto * previous = incremental ? registry.try_get<WorldTransform>(entity) : nullptr;
	bool changed = true;
	if (incremental) {
		auto [entry, inserted] = cache.try_emplace(entity);
		auto & cached = entry->second;
		changed = inserted || cached.dirty || parentChanged || !previous
			|| cached.parent != parent || cached.hasTransform != (transform != nullptr)
			|| (transform && (cached.local.position != transform->position
				|| cached.local.orientation != transform->orientation || cached.local.scale != transform->scale));
		if (changed && transform) cached.local = *transform;
		cached.parent = parent;
		cached.hasTransform = transform != nullptr;
		cached.dirty = false;
		cached.lastSeen = frame;
	}

	glm::mat4 world;
	if (changed) {
		if (transform) {
			world = transform->localMatrix();
			if (parentMatrix) world = *parentMatrix * world;
		} else {
			world = parentMatrix ? *parentMatrix : glm::mat4(1.f);
		}
		registry.emplace_or_replace<WorldTransform>(entity, world);
		stats.updated++;
	} else {
		world = previous->matrix;
		stats.skipped++;
	}

	if (auto * children = registry.try_get<Children>(entity)) {
		for (auto child : children->entities) {
			if (registry.valid(child)) {
				visit(registry, child, &world, entity, changed);
			}
		}
	}
}

void TransformSystem::update(Registry & registry) {
	stats = {};
	if (lastRegistry != &registry) {
		invalidate();
		lastRegistry = &registry;
	}
	if (++frame == 0) invalidate();
	// roots: anything with a Transform and no (valid) Parent
	roots.clear();
	for (auto entity : registry.view<Transform>()) {
		auto * parent = registry.try_get<Parent>(entity);
		if (!parent || !registry.valid(parent->entity)) {
			roots.push_back(entity);
		}
	}
	for (auto entity : roots) {
		visit(registry, entity, nullptr, Null, false);
	}
	if (incremental) {
		for (auto it = cache.begin(); it != cache.end();) {
			if (it->second.lastSeen != frame) it = cache.erase(it);
			else ++it;
		}
	}
}

} // namespace ofxEnTT

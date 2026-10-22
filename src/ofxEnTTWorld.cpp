#include "ofxEnTTWorld.h"

#include "ofUtils.h"
#include "ofAppRunner.h"

#include <algorithm>
#include <limits>

namespace ofxEnTT {

Entity World::create(const std::string & name) {
	auto entity = registry.create();
	registry.emplace<Transform>(entity);
	if (!name.empty()) {
		registry.emplace<Name>(entity, name);
	}
	return entity;
}

void World::detach(Entity entity) {
	auto * parent = registry.try_get<Parent>(entity);
	if (!parent) {
		return;
	}
	if (registry.valid(parent->entity)) {
		if (auto * siblings = registry.try_get<Children>(parent->entity)) {
			auto & list = siblings->entities;
			list.erase(std::remove(list.begin(), list.end(), entity), list.end());
		}
	}
	registry.remove<Parent>(entity);
}

void World::destroy(Entity entity) {
	if (!registry.valid(entity)) {
		return;
	}
	if (auto * children = registry.try_get<Children>(entity)) {
		auto list = children->entities;
		for (auto child : list) {
			destroy(child);
		}
	}
	detach(entity);
	registry.destroy(entity);
}

void World::clear() {
	registry.clear();
}

bool World::isValid(Entity entity) const {
	return registry.valid(entity);
}

size_t World::size() const {
	return registry.view<Entity>().size();
}

bool World::setParent(Entity child, Entity parent) {
	if (!registry.valid(child) || child == parent) {
		return false;
	}
	if (parent != Null) {
		if (!registry.valid(parent)) {
			return false;
		}
		// no cycles: parent can't be child or one of its descendants
		for (auto p = parent; p != Null; p = getParent(p)) {
			if (p == child) {
				return false;
			}
		}
	}
	detach(child);
	if (parent != Null) {
		registry.emplace_or_replace<Parent>(child, parent);
		registry.get_or_emplace<Children>(parent).entities.push_back(child);
	}
	return true;
}

Entity World::getParent(Entity entity) const {
	if (auto * parent = registry.try_get<Parent>(entity)) {
		return parent->entity;
	}
	return Null;
}

std::vector<Entity> World::getChildren(Entity entity) const {
	if (auto * children = registry.try_get<Children>(entity)) {
		return children->entities;
	}
	return {};
}

std::vector<Entity> World::getEntities() const {
	std::vector<Entity> result;
	for (auto entity : registry.view<Entity>()) {
		result.push_back(entity);
	}
	// storage order is most recent first; creation order reads better
	std::reverse(result.begin(), result.end());
	return result;
}

std::vector<Entity> World::getRoots() const {
	auto result = getEntities();
	result.erase(std::remove_if(result.begin(), result.end(),
		[this](Entity e) { return registry.all_of<Parent>(e); }), result.end());
	return result;
}

Entity World::findByName(const std::string & name) const {
	for (auto entity : getEntities()) {
		if (auto * n = registry.try_get<Name>(entity); n && n->value == name) {
			return entity;
		}
	}
	return Null;
}

std::string World::getName(Entity entity) const {
	if (auto * n = registry.try_get<Name>(entity); n && !n->value.empty()) {
		return n->value;
	}
	return "entity " + ofToString(entt::to_integral(entity));
}

bool World::getBounds(Entity entity, glm::vec3 & min, glm::vec3 & max) const {
	bool found = false;
	std::vector<Entity> stack { entity };
	while (!stack.empty()) {
		auto e = stack.back();
		stack.pop_back();
		if (!registry.valid(e)) continue;
		auto * renderer = registry.try_get<MeshRenderer>(e);
		auto * world = registry.try_get<WorldTransform>(e);
		if (renderer && world && renderer->mesh && renderer->mesh->getNumVertices() > 0) {
			// the mesh's own box, then its 8 corners in world space
			glm::vec3 lo(std::numeric_limits<float>::max()), hi(-std::numeric_limits<float>::max());
			for (const auto & v : renderer->mesh->getVertices()) {
				lo = glm::min(lo, v);
				hi = glm::max(hi, v);
			}
			for (int i = 0; i < 8; i++) {
				glm::vec3 corner { (i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z };
				glm::vec3 p = glm::vec3(world->matrix * glm::vec4(corner, 1.f));
				min = found ? glm::min(min, p) : p;
				max = found ? glm::max(max, p) : p;
				found = true;
			}
		}
		for (auto child : getChildren(e)) {
			stack.push_back(child);
		}
	}
	return found;
}

Entity World::clone(Entity entity, Entity parent) {
	if (!registry.valid(entity)) {
		return Null;
	}
	auto copy = registry.create();
	if (auto * c = registry.try_get<Name>(entity)) registry.emplace<Name>(copy, *c);
	if (auto * c = registry.try_get<Transform>(entity)) registry.emplace<Transform>(copy, *c);
	if (auto * c = registry.try_get<Velocity>(entity)) registry.emplace<Velocity>(copy, *c);
	if (auto * c = registry.try_get<Lifetime>(entity)) registry.emplace<Lifetime>(copy, *c);
	if (auto * c = registry.try_get<MeshRenderer>(entity)) registry.emplace<MeshRenderer>(copy, *c);
	if (registry.all_of<Hidden>(entity)) registry.emplace<Hidden>(copy);
	if (parent != Null) {
		setParent(copy, parent);
	}
	for (auto child : getChildren(entity)) {
		clone(child, copy);
	}
	return copy;
}

void World::addSystem(const std::string & name, System system) {
	for (auto & s : systems) {
		if (s.name == name) {
			s.system = std::move(system);
			return;
		}
	}
	systems.push_back({ name, std::move(system) });
}

void World::removeSystem(const std::string & name) {
	systems.erase(std::remove_if(systems.begin(), systems.end(),
		[&](const NamedSystem & s) { return s.name == name; }), systems.end());
}

bool World::hasSystem(const std::string & name) const {
	return std::any_of(systems.begin(), systems.end(),
		[&](const NamedSystem & s) { return s.name == name; });
}

void World::updateVelocity(float dt) {
	for (auto [entity, transform, velocity] : registry.view<Transform, Velocity>().each()) {
		transform.position += velocity.linear * dt;
		if (glm::length(velocity.angular) > 0.f) {
			transform.orientation = glm::normalize(glm::quat(velocity.angular * dt) * transform.orientation);
		}
	}
}

void World::updateLifetime(float dt) {
	std::vector<Entity> expired;
	for (auto [entity, lifetime] : registry.view<Lifetime>().each()) {
		lifetime.remaining -= dt;
		if (lifetime.remaining <= 0.f) {
			expired.push_back(entity);
		}
	}
	for (auto entity : expired) {
		destroy(entity);
	}
}

void World::update() {
	update(static_cast<float>(ofGetLastFrameTime()));
}

void World::update(float dt) {
	if (builtInSystems) {
		updateVelocity(dt);
		updateLifetime(dt);
	}
	for (auto & s : systems) {
		s.system(*this, dt);
	}
	transforms.update(registry);
}

void World::draw() {
	renderer.draw(registry);
}

void World::setAutoUpdate(bool autoUpdate) {
	if (autoUpdate) {
		updateListener = ofEvents().update.newListener([this](ofEventArgs &) { update(); });
	} else {
		updateListener.unsubscribe();
	}
}

void World::setAutoDraw(bool autoDraw) {
	if (autoDraw) {
		drawListener = ofEvents().draw.newListener([this](ofEventArgs &) { draw(); }, OF_EVENT_ORDER_BEFORE_APP);
	} else {
		drawListener.unsubscribe();
	}
}

} // namespace ofxEnTT

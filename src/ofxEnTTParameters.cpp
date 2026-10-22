#include "ofxEnTTParameters.h"

namespace ofxEnTT {

EntityParameters::EntityParameters() {
	group.setName("entity");

	listeners.push(name.newListener([this](std::string & v) {
		if (pulling) return;
		if (auto * c = component<Name>()) c->value = v;
		else if (world && world->isValid(entity)) world->registry.emplace<Name>(entity, v);
	}));
	listeners.push(position.newListener([this](glm::vec3 & v) {
		if (auto * c = component<Transform>(); c && !pulling) c->position = v;
	}));
	listeners.push(rotation.newListener([this](glm::vec3 & v) {
		if (auto * c = component<Transform>(); c && !pulling) c->setOrientationEulerDeg(v);
	}));
	listeners.push(scale.newListener([this](glm::vec3 & v) {
		if (auto * c = component<Transform>(); c && !pulling) c->scale = v;
	}));
	listeners.push(color.newListener([this](ofFloatColor & v) {
		if (auto * c = component<MeshRenderer>(); c && !pulling) c->color = v;
	}));
	listeners.push(visible.newListener([this](bool & v) {
		if (pulling || !world || !world->isValid(entity)) return;
		if (v) world->registry.remove<Hidden>(entity);
		else world->registry.emplace_or_replace<Hidden>(entity);
	}));
	listeners.push(linearVelocity.newListener([this](glm::vec3 & v) {
		if (auto * c = component<Velocity>(); c && !pulling) c->linear = v;
	}));
	listeners.push(angularVelocity.newListener([this](glm::vec3 & v) {
		if (auto * c = component<Velocity>(); c && !pulling) c->angular = v;
	}));
}

void EntityParameters::setup(World & w) {
	world = &w;
	rebuild();
}

void EntityParameters::setEntity(Entity e) {
	entity = e;
	rebuild();
}

std::string EntityParameters::currentLayout() const {
	if (!world || !world->isValid(entity)) {
		return "";
	}
	const auto & r = world->registry;
	std::string l = "e";
	if (r.all_of<Transform>(entity)) l += "t";
	if (r.all_of<MeshRenderer>(entity)) l += "m";
	if (r.all_of<Velocity>(entity)) l += "v";
	return l;
}

void EntityParameters::rebuild() {
	group.clear();
	layout = currentLayout();
	if (!layout.empty()) {
		group.setName(world->getName(entity));
		group.add(name);
		if (layout.find('t') != std::string::npos) {
			group.add(position, rotation, scale);
		}
		if (layout.find('m') != std::string::npos) {
			group.add(color, visible);
		}
		if (layout.find('v') != std::string::npos) {
			group.add(linearVelocity, angularVelocity);
		}
	} else {
		group.setName("no entity");
	}
	pull();
	ofNotifyEvent(groupChanged, this);
}

void EntityParameters::pull() {
	if (currentLayout() != layout) {
		rebuild();
		return;
	}
	if (layout.empty()) {
		return;
	}
	pulling = true;
	const auto & r = world->registry;
	auto * n = r.try_get<Name>(entity);
	if (!n && name.get() != "") name = "";
	if (n && name.get() != n->value) name = n->value;
	if (auto * t = r.try_get<Transform>(entity)) {
		if (position.get() != t->position) position = t->position;
		auto euler = t->getOrientationEulerDeg();
		if (glm::any(glm::greaterThan(glm::abs(rotation.get() - euler), glm::vec3(0.01f)))) rotation = euler;
		if (scale.get() != t->scale) scale = t->scale;
	}
	if (auto * m = r.try_get<MeshRenderer>(entity)) {
		if (color.get() != m->color) color = m->color;
		bool shown = !r.all_of<Hidden>(entity);
		if (visible.get() != shown) visible = shown;
	}
	if (auto * v = r.try_get<Velocity>(entity)) {
		if (linearVelocity.get() != v->linear) linearVelocity = v->linear;
		if (angularVelocity.get() != v->angular) angularVelocity = v->angular;
	}
	pulling = false;
}

} // namespace ofxEnTT

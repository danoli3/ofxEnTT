#include "ofxEnTTSerialize.h"

#include <unordered_map>

namespace ofxEnTT {

namespace {

ofJson vec3ToJson(const glm::vec3 & v) { return { v.x, v.y, v.z }; }
glm::vec3 jsonToVec3(const ofJson & j) { return { j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>() }; }

} // namespace

ofJson toJson(const World & world) {
	const auto & registry = world.registry;
	auto entities = world.getEntities();

	std::unordered_map<entt::id_type, size_t> indexOf;
	for (size_t i = 0; i < entities.size(); i++) {
		indexOf[entt::to_integral(entities[i])] = i;
	}

	ofJson list = ofJson::array();
	for (auto entity : entities) {
		ofJson e;
		if (auto * c = registry.try_get<Name>(entity)) {
			e["name"] = c->value;
		}
		if (auto * c = registry.try_get<Transform>(entity)) {
			e["transform"] = {
				{ "position", vec3ToJson(c->position) },
				{ "orientation", { c->orientation.w, c->orientation.x, c->orientation.y, c->orientation.z } },
				{ "scale", vec3ToJson(c->scale) }
			};
		}
		if (auto * c = registry.try_get<Parent>(entity); c && registry.valid(c->entity)) {
			e["parent"] = indexOf.at(entt::to_integral(c->entity));
		}
		if (auto * c = registry.try_get<Velocity>(entity)) {
			e["velocity"] = { { "linear", vec3ToJson(c->linear) }, { "angular", vec3ToJson(c->angular) } };
		}
		if (auto * c = registry.try_get<Lifetime>(entity)) {
			e["lifetime"] = c->remaining;
		}
		if (registry.all_of<Hidden>(entity)) {
			e["hidden"] = true;
		}
		if (auto * c = registry.try_get<MeshRenderer>(entity)) {
			e["color"] = { c->color.r, c->color.g, c->color.b, c->color.a };
		}
		list.push_back(e);
	}
	return { { "ofxEnTT", 1 }, { "entities", list } };
}

std::vector<Entity> fromJson(World & world, const ofJson & json, bool clear) {
	if (clear) {
		world.clear();
	}
	auto & registry = world.registry;
	std::vector<Entity> created;
	if (!json.contains("entities")) {
		return created;
	}
	const auto & list = json.at("entities");
	for (size_t i = 0; i < list.size(); i++) {
		created.push_back(world.create(list[i].value("name", std::string())));
	}
	for (size_t i = 0; i < list.size(); i++) {
		const auto & e = list[i];
		auto entity = created[i];
		if (e.contains("transform")) {
			auto & t = registry.get<Transform>(entity);
			const auto & j = e.at("transform");
			t.position = jsonToVec3(j.at("position"));
			const auto & q = j.at("orientation");
			t.orientation = glm::quat(q.at(0).get<float>(), q.at(1).get<float>(), q.at(2).get<float>(), q.at(3).get<float>());
			t.scale = jsonToVec3(j.at("scale"));
		}
		if (e.contains("parent")) {
			auto parent = e.at("parent").get<size_t>();
			if (parent < created.size()) {
				world.setParent(entity, created[parent]);
			}
		}
		if (e.contains("velocity")) {
			registry.emplace_or_replace<Velocity>(entity,
				jsonToVec3(e.at("velocity").at("linear")), jsonToVec3(e.at("velocity").at("angular")));
		}
		if (e.contains("lifetime")) {
			registry.emplace_or_replace<Lifetime>(entity, e.at("lifetime").get<float>());
		}
		if (e.value("hidden", false)) {
			registry.emplace_or_replace<Hidden>(entity);
		}
		if (e.contains("color")) {
			const auto & c = e.at("color");
			registry.get_or_emplace<MeshRenderer>(entity).color
				= ofFloatColor(c.at(0).get<float>(), c.at(1).get<float>(), c.at(2).get<float>(), c.at(3).get<float>());
		}
	}
	return created;
}

bool saveJson(const World & world, const of::filesystem::path & path) {
	return ofSaveJson(path, toJson(world));
}

std::vector<Entity> loadJson(World & world, const of::filesystem::path & path, bool clear) {
	return fromJson(world, ofLoadJson(path), clear);
}

} // namespace ofxEnTT

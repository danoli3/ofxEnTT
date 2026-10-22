#pragma once

// Dear ImGui inspector for an ofxEnTT::World. Needs the ofxImGui addon
// (https://github.com/jvcleave/ofxImGui).
//
//     ofxImGui::Gui gui;  ofxEnTT::Entity selected = ofxEnTT::Null;
//     gui.setup();                                          // setup
//     gui.begin();                                          // draw
//     ofxEnTT::imgui::inspector(world, selected);
//     gui.end();

#include "ofxEnTT.h"
#include "ofxImGui.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstdio>

namespace ofxEnTT {
namespace imgui {

/// One entity and its children as tree nodes; clicking one selects it.
inline void entityTree(World & world, Entity entity, Entity & selected) {
	auto children = world.getChildren(entity);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
	if (entity == selected) flags |= ImGuiTreeNodeFlags_Selected;
	const auto label = world.getName(entity);
	bool open = ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<intptr_t>(entt::to_integral(entity))), flags, "%s", label.c_str());
	if (ImGui::IsItemClicked()) {
		selected = entity;
	}
	if (open) {
		for (auto child : children) {
			entityTree(world, child, selected);
		}
		ImGui::TreePop();
	}
}

/// Editors for the built-in components of one entity.
inline void entityEditor(World & world, Entity entity) {
	auto & r = world.registry;
	if (!r.valid(entity)) {
		ImGui::TextDisabled("no entity selected");
		return;
	}
	char name[256];
	std::snprintf(name, sizeof(name), "%s", r.all_of<Name>(entity) ? r.get<Name>(entity).value.c_str() : "");
	if (ImGui::InputText("name", name, sizeof(name))) {
		r.emplace_or_replace<Name>(entity, std::string(name));
	}
	if (auto * t = r.try_get<Transform>(entity); t && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("position", glm::value_ptr(t->position), 1.f);
		auto euler = t->getOrientationEulerDeg();
		if (ImGui::DragFloat3("rotation", glm::value_ptr(euler), 0.5f)) t->setOrientationEulerDeg(euler);
		ImGui::DragFloat3("scale", glm::value_ptr(t->scale), 0.01f);
	}
	if (auto * m = r.try_get<MeshRenderer>(entity); m && ImGui::CollapsingHeader("MeshRenderer", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::ColorEdit4("color", &m->color.r);
		bool visible = !r.all_of<Hidden>(entity);
		if (ImGui::Checkbox("visible", &visible)) {
			if (visible) r.remove<Hidden>(entity);
			else r.emplace_or_replace<Hidden>(entity);
		}
		ImGui::Text("%zu vertices%s%s", m->mesh ? m->mesh->getNumVertices() : size_t(0),
			m->texture ? ", texture" : "", m->material ? ", material" : "");
	}
	if (auto * v = r.try_get<Velocity>(entity); v && ImGui::CollapsingHeader("Velocity", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("linear", glm::value_ptr(v->linear), 0.1f);
		ImGui::DragFloat3("angular", glm::value_ptr(v->angular), 0.01f);
	}
	if (auto * l = r.try_get<Lifetime>(entity); l && ImGui::CollapsingHeader("Lifetime", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat("remaining", &l->remaining, 0.1f, 0.f, 1000.f);
	}
	if (auto * w = r.try_get<WorldTransform>(entity)) {
		auto p = w->getPosition();
		ImGui::TextDisabled("world position %.1f %.1f %.1f", p.x, p.y, p.z);
	}
	if (!r.all_of<Velocity>(entity) && ImGui::Button("+ Velocity")) r.emplace<Velocity>(entity);
	if (!r.all_of<Lifetime>(entity)) {
		ImGui::SameLine();
		if (ImGui::Button("+ Lifetime")) r.emplace<Lifetime>(entity, 5.f);
	}
}

/// A window with the entity tree, create / clone / destroy, and the selected
/// entity's components. Returns true while the window is open.
inline bool inspector(World & world, Entity & selected, const char * title = "ofxEnTT") {
	bool open = ImGui::Begin(title);
	if (open) {
		const auto & stats = world.renderer.getStats();
		ImGui::Text("%zu entities, %zu drawn in %zu draw calls (%zu instanced)",
			world.size(), stats.entities, stats.drawCalls, stats.instancedBatches);
		ImGui::Checkbox("instancing", &world.renderer.instancing);
		ImGui::SameLine();
		ImGui::Checkbox("lighting", &world.renderer.lighting);
		if (ImGui::Button("create")) {
			auto entity = world.create("entity");
			if (world.isValid(selected)) world.setParent(entity, selected);
			selected = entity;
		}
		if (world.isValid(selected)) {
			ImGui::SameLine();
			if (ImGui::Button("clone")) selected = world.clone(selected, world.getParent(selected));
			ImGui::SameLine();
			if (ImGui::Button("destroy")) {
				world.destroy(selected);
				selected = Null;
			}
		}
		ImGui::Separator();
		if (ImGui::BeginChild("tree", ImVec2(0, 200), true)) {
			for (auto root : world.getRoots()) {
				entityTree(world, root, selected);
			}
		}
		ImGui::EndChild();
		ImGui::Separator();
		entityEditor(world, selected);
	}
	ImGui::End();
	return open;
}

} // namespace imgui
} // namespace ofxEnTT

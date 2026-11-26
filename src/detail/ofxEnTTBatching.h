#pragma once

#include "ofxEnTTComponents.h"
#include <unordered_map>

namespace ofxEnTT::detail {

struct BatchKey {
	const void * mesh;
	const void * texture;
	const void * material;
	int mode;
	bool operator==(const BatchKey &) const = default;
};
struct BatchKeyHash {
	size_t operator()(const BatchKey & key) const {
		auto hash = std::hash<const void *>();
		return hash(key.mesh) ^ (hash(key.texture) << 1)
			^ (hash(key.material) << 2) ^ (std::hash<int>()(key.mode) << 3);
	}
};
struct BatchSlot {
	size_t batch = 0;
	bool active = false;
};
using BatchIndex = std::unordered_map<BatchKey, BatchSlot, BatchKeyHash>;

struct DrawItem {
	const MeshRenderer * renderer;
	const WorldTransform * world;
};
struct DrawBatch {
	ofVboMesh * mesh = nullptr;
	ofTexture * texture = nullptr;
	ofMaterial * material = nullptr;
	ofPolyRenderMode mode = OF_MESH_FILL;
	std::vector<DrawItem> items;
};

// Internal CPU stage shared with the benchmark. Only the returned prefix is
// active; inactive slots retain their vector capacities for subsequent frames.
template <typename BatchType>
size_t collectBatches(Registry & registry, std::vector<BatchType> & batches, BatchIndex & index) {
	for (auto & batch : batches) {
		if constexpr (requires { batch.entities; }) batch.entities.clear();
		else batch.items.clear();
	}
	for (auto & [key, slot] : index) slot.active = false;
	size_t used = 0;
	auto view = registry.view<const MeshRenderer, const WorldTransform>(entt::exclude<Hidden, Culled>);
	for (auto [entity, renderer, world] : view.each()) {
		if (!renderer.visible || !renderer.mesh || renderer.mesh->getNumVertices() == 0) continue;
		BatchKey key { renderer.mesh.get(), renderer.texture.get(), renderer.material.get(), static_cast<int>(renderer.mode) };
		auto found = index.try_emplace(key).first;
		if (!found->second.active) {
			found->second = { used, true };
			if (used == batches.size()) batches.emplace_back();
			auto & batch = batches[used++];
			batch.mesh = renderer.mesh.get();
			batch.texture = renderer.texture.get();
			batch.material = renderer.material.get();
			batch.mode = renderer.mode;
		}
		if constexpr (requires { batches[0].entities; }) {
			batches[found->second.batch].entities.push_back(entity);
		} else {
			batches[found->second.batch].items.push_back({ &renderer, &world });
		}
	}
	for (auto it = index.begin(); it != index.end();) {
		if (!it->second.active) it = index.erase(it);
		else ++it;
	}
	return used;
}

} // namespace ofxEnTT::detail

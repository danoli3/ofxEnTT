#pragma once
#include "Benchmark.h"
#include "detail/ofxEnTTBatching.h"
#include <unordered_map>

namespace bench {
inline auto originalBatches(ofxEnTT::Registry & registry) {
	using namespace ofxEnTT;
	std::vector<RenderSystem::Batch> batches;
	std::unordered_map<detail::BatchKey, size_t, detail::BatchKeyHash> index;
	for (auto [entity, renderer, world] : registry.view<MeshRenderer, WorldTransform>(entt::exclude<Hidden>).each()) {
		if (!renderer.visible || !renderer.mesh || renderer.mesh->getNumVertices() == 0) continue;
		detail::BatchKey key { renderer.mesh.get(), renderer.texture.get(), renderer.material.get(), static_cast<int>(renderer.mode) };
		auto found = index.find(key);
		if (found == index.end()) {
			found = index.emplace(key, batches.size()).first;
			RenderSystem::Batch batch;
			batch.mesh = renderer.mesh.get(); batch.texture = renderer.texture.get();
			batch.material = renderer.material.get(); batch.mode = renderer.mode;
			batches.push_back(std::move(batch));
		}
		batches[found->second].entities.push_back(entity);
	}
	return batches;
}
struct PackedInstances {
	std::vector<glm::vec4> columns[4], colors;
	template <typename Batch>
	double pack(ofxEnTT::Registry & registry, const Batch & batch) {
		auto count = [&] {
			if constexpr (requires { batch.entities; }) return batch.entities.size();
			else return batch.items.size();
		}();
		for (auto & column : columns) column.resize(count);
		colors.resize(count);
		for (size_t i = 0; i < count; ++i) {
			const ofxEnTT::WorldTransform * world;
			const ofxEnTT::MeshRenderer * renderer;
			if constexpr (requires { batch.entities; }) {
				world = &registry.get<ofxEnTT::WorldTransform>(batch.entities[i]);
				renderer = &registry.get<ofxEnTT::MeshRenderer>(batch.entities[i]);
			} else {
				world = batch.items[i].world; renderer = batch.items[i].renderer;
			}
			for (int c = 0; c < 4; ++c) columns[c][i] = world->matrix[c];
			const auto & color = renderer->color;
			colors[i] = { color.r, color.g, color.b, color.a };
		}
		double sum = 0;
		for (size_t i = 0; i < count; ++i) {
			for (const auto & column : columns) for (int c = 0; c < 4; ++c) sum += column[i][c];
			for (int c = 0; c < 4; ++c) sum += colors[i][c];
		}
		return sum;
	}
};
inline void renderBenchmark() {
	using namespace ofxEnTT;
	for (int meshCount : { 1, 32 }) {
		World world;
		std::vector<std::shared_ptr<ofVboMesh>> meshes;
		for (int i = 0; i < meshCount; ++i) meshes.push_back(std::make_shared<ofVboMesh>(ofMesh::box(1, 1, 1)));
		auto entities = world.create(size_t(20000));
		for (size_t i = 0; i < entities.size(); ++i) {
			world.registry.emplace<MeshRenderer>(entities[i], meshes[i % meshCount]).color.r = float(i % 255) / 255.f;
			world.registry.get<Transform>(entities[i]).position.x = float(i);
		}
		world.update(0.f);
		for (size_t i = 0; i < entities.size(); i += 7) world.registry.remove<MeshRenderer>(entities[i]);
		for (size_t i = 0; i < entities.size(); i += 7) world.registry.emplace<MeshRenderer>(entities[i], meshes[i % meshCount]);
		std::vector<detail::DrawBatch> reusable;
		detail::BatchIndex index;
		PackedInstances oldOutput, newOutput;
		auto before = [&] {
			double sum = 0;
			for (const auto & batch : originalBatches(world.registry)) sum += oldOutput.pack(world.registry, batch);
			return sum;
		};
		auto after = [&] {
			double sum = 0;
			auto used = detail::collectBatches(world.registry, reusable, index);
			for (size_t i = 0; i < used; ++i) {
				sum += newOutput.pack(world.registry, reusable[i]);
				reusable[i].items.clear();
			}
			return sum;
		};
		require(before() == after(), "packed render data mismatch");
		compare(meshCount == 1 ? "render prepare / 20k / 1 batch" : "render prepare / 20k / 32 batches", before, after);
	}
}
}

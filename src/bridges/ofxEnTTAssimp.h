#pragma once

// ofxAssimpModelLoader models as entities. Needs the ofxAssimpModelLoader addon.
//
//     auto model = std::make_shared<ofxAssimpModelLoader>();
//     model->load("druid.gltf");
//     auto druid = ofxEnTT::addModel(world, model, "druid");
//     auto another = world.clone(druid);   // shares the meshes: drawn instanced

#include "ofxEnTT.h"
#include "ofxAssimpModelLoader.h"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/matrix_decompose.hpp>

namespace ofxEnTT {

/// The model and mesh an entity was made from (for animation, see
/// updateModelAnimations).
struct ModelMesh {
	std::shared_ptr<ofxAssimpModelLoader> model;
	unsigned int meshIndex = 0;
};

struct ModelOptions {
	/// Keep each mesh's ofMaterial. Without it (the default) the material's
	/// diffuse colour becomes the MeshRenderer colour, so the meshes can be
	/// drawn instanced with the built-in shader.
	bool materials = false;
	/// Keep the diffuse textures.
	bool textures = true;
	/// ofxAssimpModelLoader turns models 180 degrees about z for OF's y-down
	/// screen space. With a y-up camera (ofEasyCam) that leaves them upside
	/// down, so this takes the turn back out. Set false to keep the loader's.
	bool yUp = true;
};

inline Transform transformFromMatrix(const glm::mat4 & matrix) {
	Transform t;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(matrix, t.scale, t.orientation, t.position, skew, perspective);
	return t;
}

/// A root entity (with the model's matrix) and a child per mesh (with the
/// mesh's node matrix), each with a MeshRenderer. Returns the root.
inline Entity addModel(World & world, const std::shared_ptr<ofxAssimpModelLoader> & model,
	const std::string & name = "model", const ModelOptions & options = {}) {
	auto root = world.create(name);
	auto rootTransform = transformFromMatrix(model->getModelMatrix());
	if (options.yUp) {
		rootTransform.orientation = glm::angleAxis(-glm::pi<float>(), glm::vec3(0.f, 0.f, 1.f)) * rootTransform.orientation;
	}
	world.registry.replace<Transform>(root, rootTransform);

	for (unsigned int i = 0; i < model->getNumMeshes(); i++) {
		auto & helper = model->getMeshHelper(i);
		auto entity = world.create(name + "/mesh " + ofToString(i));
		world.setParent(entity, root);
		world.registry.replace<Transform>(entity, transformFromMatrix(helper.matrix));

		MeshRenderer renderer;
		renderer.mesh = std::make_shared<ofVboMesh>(model->getMesh(i));
		auto material = model->getMaterialForMesh(i);
		if (options.materials) {
			renderer.material = std::make_shared<ofMaterial>(material);
		} else {
			renderer.color = material.getDiffuseColor();
			renderer.color.a = 1.f;
		}
		if (options.textures && helper.hasTexture()) {
			renderer.texture = std::make_shared<ofTexture>(helper.getTextureRef());
		}
		world.registry.emplace<MeshRenderer>(entity, renderer);
		world.registry.emplace<ModelMesh>(entity, model, i);
	}
	return root;
}

/// Advances each model's animation and copies the animated meshes into the
/// entities made from it. Call from update (or add it as a World system).
inline void updateModelAnimations(World & world) {
	std::vector<ofxAssimpModelLoader *> updated;
	for (auto [entity, source, renderer] : world.registry.view<ModelMesh, MeshRenderer>().each()) {
		auto * model = source.model.get();
		if (!model || !model->hasAnimations() || !renderer.mesh) continue;
		if (std::find(updated.begin(), updated.end(), model) == updated.end()) {
			model->update();
			updated.push_back(model);
		}
		*renderer.mesh = model->getCurrentAnimatedMesh(source.meshIndex);
	}
}

} // namespace ofxEnTT

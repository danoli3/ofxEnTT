#pragma once

#include "ofxEnTTComponents.h"

namespace ofxEnTT {

/// Draws every visible entity with a MeshRenderer and a WorldTransform.
///
/// Entities sharing a mesh, texture and draw mode (and with no material) form a
/// batch, drawn with one instanced draw call: per-instance matrices and colours
/// go to vertex attributes 5-9 of the mesh's VBO, and a built-in shader draws
/// them with simple directional lighting. That needs the programmable renderer
/// (ofGLWindowSettings::setGLVersion(3, 3), or GLES 3 / WebGL 2). Otherwise,
/// and for entities with a material, each entity is drawn on its own.
///
/// Textures are sampled with sampler2D: call ofDisableArbTex() before loading them.
class RenderSystem {
public:
	struct Batch {
		ofVboMesh * mesh = nullptr;
		ofTexture * texture = nullptr;
		ofMaterial * material = nullptr;
		ofPolyRenderMode mode = OF_MESH_FILL;
		std::vector<Entity> entities;
	};

	struct Stats {
		size_t entities = 0;       ///< entities drawn
		size_t drawCalls = 0;      ///< draw calls issued
		size_t instancedBatches = 0;
	};

	/// Use instanced draw calls when the renderer supports them.
	bool instancing = true;
	/// Batches smaller than this are drawn per entity.
	size_t minInstances = 2;
	/// Light direction for the built-in instanced shader, in view space.
	glm::vec3 lightDirection { -0.4f, -0.6f, -1.f };
	/// Simple lighting in the built-in instanced shader (needs mesh normals).
	bool lighting = true;

	void draw(Registry & registry);

	/// The batches draw() would use, in a stable order. No GL needed.
	static std::vector<Batch> collectBatches(Registry & registry);

	/// True when instanced draw calls can be used (programmable renderer and the
	/// built-in shader compiled). Compiles the shader on first call.
	bool isInstancingSupported();
	const Stats & getStats() const { return stats; }

private:
	ofShader shader;
	bool shaderTried = false;
	bool shaderReady = false;
	Stats stats;
	std::vector<glm::vec4> columns[4];
	std::vector<glm::vec4> colors;

	void drawInstanced(Registry & registry, const Batch & batch);
	void drawEach(Registry & registry, const Batch & batch);
};

} // namespace ofxEnTT

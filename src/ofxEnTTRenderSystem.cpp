#include "ofxEnTTRenderSystem.h"

#include "ofGraphics.h"
#include "ofGLUtils.h"
#include "ofLog.h"

#include <tuple>
#include <unordered_map>

namespace ofxEnTT {

namespace {

// per-instance attributes, after OF's position / color / normal / texcoord / index (0-4)
constexpr int INSTANCE_MATRIX_LOCATION = 5; // 5, 6, 7, 8: matrix columns
constexpr int INSTANCE_COLOR_LOCATION = 9;

#ifdef TARGET_OPENGLES
const char * GLSL_HEADER = "#version 300 es\nprecision highp float;\n";
#else
const char * GLSL_HEADER = "#version 150\n";
#endif

const char * VERTEX_SHADER = R"(
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform int useVertexColor;

in vec4 position;
in vec4 color;
in vec3 normal;
in vec2 texcoord;
in vec4 instanceMatrix0;
in vec4 instanceMatrix1;
in vec4 instanceMatrix2;
in vec4 instanceMatrix3;
in vec4 instanceColor;

out vec4 vColor;
out vec3 vNormal;
out vec2 vTexcoord;

void main() {
	mat4 modelView = modelViewMatrix * mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);
	vNormal = mat3(modelView) * normal;
	vTexcoord = texcoord;
	vColor = useVertexColor == 1 ? instanceColor * color : instanceColor;
	gl_Position = projectionMatrix * modelView * position;
}
)";

const char * FRAGMENT_SHADER = R"(
uniform sampler2D tex0;
uniform int useTexture;
uniform int useLighting;
uniform vec3 lightDirection;

in vec4 vColor;
in vec3 vNormal;
in vec2 vTexcoord;

out vec4 fragColor;

void main() {
	vec4 c = vColor;
	if (useTexture == 1) {
		c *= texture(tex0, vTexcoord);
	}
	if (useLighting == 1) {
		float diffuse = max(dot(normalize(vNormal), normalize(-lightDirection)), 0.0);
		c.rgb *= 0.35 + 0.65 * diffuse;
	}
	fragColor = c;
}
)";

struct BatchKey {
	const void * mesh;
	const void * texture;
	const void * material;
	int mode;
	bool operator==(const BatchKey & o) const {
		return std::tie(mesh, texture, material, mode) == std::tie(o.mesh, o.texture, o.material, o.mode);
	}
};

struct BatchKeyHash {
	size_t operator()(const BatchKey & k) const {
		auto h = std::hash<const void *>();
		return h(k.mesh) ^ (h(k.texture) << 1) ^ (h(k.material) << 2) ^ (std::hash<int>()(k.mode) << 3);
	}
};

} // namespace

std::vector<RenderSystem::Batch> RenderSystem::collectBatches(Registry & registry) {
	std::vector<Batch> batches;
	std::unordered_map<BatchKey, size_t, BatchKeyHash> index;
	for (auto [entity, renderer, world] : registry.view<MeshRenderer, WorldTransform>(entt::exclude<Hidden>).each()) {
		if (!renderer.mesh || renderer.mesh->getNumVertices() == 0) {
			continue;
		}
		BatchKey key { renderer.mesh.get(), renderer.texture.get(), renderer.material.get(), static_cast<int>(renderer.mode) };
		auto found = index.find(key);
		if (found == index.end()) {
			found = index.emplace(key, batches.size()).first;
			Batch batch;
			batch.mesh = renderer.mesh.get();
			batch.texture = renderer.texture.get();
			batch.material = renderer.material.get();
			batch.mode = renderer.mode;
			batches.push_back(std::move(batch));
		}
		batches[found->second].entities.push_back(entity);
	}
	return batches;
}

bool RenderSystem::isInstancingSupported() {
	if (!shaderTried) {
		shaderTried = true;
		if (ofIsGLProgrammableRenderer()) {
			shader.setupShaderFromSource(GL_VERTEX_SHADER, std::string(GLSL_HEADER) + VERTEX_SHADER);
			shader.setupShaderFromSource(GL_FRAGMENT_SHADER, std::string(GLSL_HEADER) + FRAGMENT_SHADER);
			shader.bindDefaults();
			for (int i = 0; i < 4; i++) {
				shader.bindAttribute(INSTANCE_MATRIX_LOCATION + i, "instanceMatrix" + std::to_string(i));
			}
			shader.bindAttribute(INSTANCE_COLOR_LOCATION, "instanceColor");
			shaderReady = shader.linkProgram();
			if (!shaderReady) {
				ofLogWarning("ofxEnTT") << "instanced shader didn't compile, drawing each entity on its own";
			}
		}
	}
	return shaderReady;
}

void RenderSystem::draw(Registry & registry) {
	stats = {};
	for (const auto & batch : collectBatches(registry)) {
		bool instanced = instancing && !batch.material
			&& batch.entities.size() >= std::max<size_t>(minInstances, 1)
			&& isInstancingSupported();
		if (instanced) {
			drawInstanced(registry, batch);
		} else {
			drawEach(registry, batch);
		}
	}
}

void RenderSystem::drawInstanced(Registry & registry, const Batch & batch) {
	const auto count = batch.entities.size();
	for (auto & column : columns) {
		column.resize(count);
	}
	colors.resize(count);
	for (size_t i = 0; i < count; i++) {
		auto entity = batch.entities[i];
		const auto & matrix = registry.get<WorldTransform>(entity).matrix;
		for (int c = 0; c < 4; c++) {
			columns[c][i] = matrix[c];
		}
		const auto & color = registry.get<MeshRenderer>(entity).color;
		colors[i] = { color.r, color.g, color.b, color.a };
	}

	auto & mesh = *batch.mesh;
	mesh.updateVbo();
	auto & vbo = mesh.getVbo();
	for (int c = 0; c < 4; c++) {
		vbo.setAttributeData(INSTANCE_MATRIX_LOCATION + c, &columns[c][0].x, 4, static_cast<int>(count), GL_STREAM_DRAW);
		vbo.setAttributeDivisor(INSTANCE_MATRIX_LOCATION + c, 1);
	}
	vbo.setAttributeData(INSTANCE_COLOR_LOCATION, &colors[0].x, 4, static_cast<int>(count), GL_STREAM_DRAW);
	vbo.setAttributeDivisor(INSTANCE_COLOR_LOCATION, 1);

	shader.begin();
	shader.setUniform1i("useVertexColor", mesh.hasColors() && mesh.usingColors() ? 1 : 0);
	shader.setUniform1i("useLighting", lighting && mesh.hasNormals() && mesh.usingNormals() ? 1 : 0);
	shader.setUniform3f("lightDirection", lightDirection);
	shader.setUniform1i("useTexture", batch.texture ? 1 : 0);
	if (batch.texture) {
		shader.setUniformTexture("tex0", *batch.texture, 0);
	}
	mesh.drawInstanced(batch.mode, static_cast<int>(count));
	shader.end();

	stats.entities += count;
	stats.drawCalls++;
	stats.instancedBatches++;
}

void RenderSystem::drawEach(Registry & registry, const Batch & batch) {
	for (auto entity : batch.entities) {
		const auto & renderer = registry.get<MeshRenderer>(entity);
		ofPushStyle();
		ofPushMatrix();
		ofMultMatrix(registry.get<WorldTransform>(entity).matrix);
		ofSetColor(renderer.color);
		if (batch.material) batch.material->begin();
		if (batch.texture) batch.texture->bind();
		batch.mesh->draw(batch.mode);
		if (batch.texture) batch.texture->unbind();
		if (batch.material) batch.material->end();
		ofPopMatrix();
		ofPopStyle();
		stats.entities++;
		stats.drawCalls++;
	}
}

} // namespace ofxEnTT

#pragma once

// Shared ofVboMesh helpers for MeshRenderer instancing. One mesh, many entities.

#include "ofVboMesh.h"

#include <memory>

namespace ofxEnTT {

inline std::shared_ptr<ofVboMesh> makeQuad(float width, float height = 0.f) {
	if (height <= 0.f) height = width;
	return std::make_shared<ofVboMesh>(ofMesh::plane(width, height, 2, 2));
}

inline std::shared_ptr<ofVboMesh> makeDisk(float radius, int segments = 24) {
	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_TRIANGLE_FAN);
	mesh.addVertex({ 0.f, 0.f, 0.f });
	mesh.addNormal({ 0.f, 0.f, 1.f });
	for (int i = 0; i <= segments; i++) {
		float a = TWO_PI * (float)i / (float)segments;
		mesh.addVertex({ radius * cosf(a), radius * sinf(a), 0.f });
		mesh.addNormal({ 0.f, 0.f, 1.f });
	}
	return std::make_shared<ofVboMesh>(mesh);
}

inline std::shared_ptr<ofVboMesh> makeBox(float width, float height, float depth) {
	return std::make_shared<ofVboMesh>(ofMesh::box(width, height, depth, 1, 1, 1));
}

} // namespace ofxEnTT

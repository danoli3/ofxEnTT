#include "ofApp.h"

using namespace ofxEnTT;

void ofApp::setup() {
	ofBackground(10);
	sphere = std::make_shared<ofVboMesh>(ofMesh::icosphere(1, 3));
	cam.setDistance(900);
	build();
	inspector.setup(world, "solar system", 10, 10);
	inspector.select(world.findByName("earth"));
}

Entity ofApp::addBody(const std::string & name, Entity parent, float orbitRadius, float orbitSpeed, float size, ofFloatColor color) {
	// pivot (spins on the parent) -> frame (offset by the orbit radius) -> body (scaled mesh).
	// Children attach to the unscaled frame, so they don't inherit the body's size.
	auto pivot = world.create(name + " orbit");
	if (parent != Null) {
		world.setParent(pivot, parent);
	}
	world.registry.emplace<Velocity>(pivot, glm::vec3(0.f), glm::vec3(0.f, orbitSpeed, 0.f));
	auto frame = world.create(name + " frame");
	world.setParent(frame, pivot);
	world.registry.get<Transform>(frame).position = { orbitRadius, 0.f, 0.f };

	auto body = world.create(name);
	world.setParent(body, frame);
	world.registry.get<Transform>(body).scale = glm::vec3(size);
	world.registry.emplace<Velocity>(body, glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
	world.registry.emplace<MeshRenderer>(body, sphere).color = color;
	return frame;
}

void ofApp::build() {
	world.clear();
	auto sun = addBody("sun", Null, 0, 0, 60, ofFloatColor(1.f, 0.85f, 0.3f));
	addBody("mercury", sun, 110, 1.6f, 8, ofFloatColor(0.7f, 0.6f, 0.5f));
	addBody("venus", sun, 170, 1.2f, 14, ofFloatColor(0.9f, 0.7f, 0.4f));
	auto earth = addBody("earth", sun, 250, 0.9f, 15, ofFloatColor(0.3f, 0.5f, 1.f));
	addBody("moon", earth, 30, 3.f, 4, ofFloatColor(0.8f));
	auto mars = addBody("mars", sun, 330, 0.7f, 10, ofFloatColor(0.9f, 0.4f, 0.3f));
	addBody("phobos", mars, 20, 4.f, 2.5f, ofFloatColor(0.6f));
	addBody("deimos", mars, 30, 2.5f, 2, ofFloatColor(0.6f));
}

// meshes aren't saved in the JSON: give the loaded bodies the sphere again
void ofApp::giveMeshes() {
	for (auto [entity, renderer] : world.registry.view<MeshRenderer>().each()) {
		if (!renderer.mesh) renderer.mesh = sphere;
	}
}

void ofApp::update() {
	world.update();
	inspector.update();
}

void ofApp::draw() {
	ofEnableDepthTest();
	cam.begin();
	world.draw();
	cam.end();
	ofDisableDepthTest();
	inspector.draw();
	ofDrawBitmapStringHighlight("s: save   l: load   r: rebuild   " + message, 10, ofGetHeight() - 20);
}

void ofApp::keyPressed(int key) {
	if (key == 's') {
		message = saveJson(world, "solar.json") ? "saved bin/data/solar.json" : "save failed";
	} else if (key == 'l') {
		auto loaded = loadJson(world, "solar.json");
		giveMeshes();
		message = "loaded " + ofToString(loaded.size()) + " entities";
		inspector.select(world.findByName("earth"));
	} else if (key == 'r') {
		build();
		message = "rebuilt";
	}
}

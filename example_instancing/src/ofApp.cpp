#include "ofApp.h"

#include <glm/gtc/random.hpp>

using namespace ofxEnTT;

void ofApp::setup() {
	ofSetVerticalSync(false);
	ofBackground(20);
	cube = std::make_shared<ofVboMesh>(ofMesh::box(10, 10, 10, 1, 1, 1));
	spark = std::make_shared<ofVboMesh>(ofMesh::icosphere(3, 1));
	cam.setFarClip(10000);
	cam.setDistance(2500);

	inspector.setup(world, "entity", 10, 10);
	populate(count);

	settings.setup("settings", "settings.json", 10, ofGetHeight() - 120);
	settings.add(count);
	settings.add(instancing);
	listeners.push(count.newListener([this](int & n) { populate(n); }));
	listeners.push(instancing.newListener([this](bool & on) { world.renderer.instancing = on; }));
}

void ofApp::populate(int n) {
	world.clear();
	for (int i = 0; i < n; i++) {
		auto e = world.create("cube " + ofToString(i));
		auto & t = world.registry.get<Transform>(e);
		t.position = glm::sphericalRand(ofRandom(100, 1500));
		t.scale = glm::vec3(ofRandom(0.5f, 2.f));
		world.registry.emplace<Velocity>(e, glm::vec3(0.f), glm::sphericalRand(ofRandom(0.2f, 2.f)));
		ofFloatColor c = ofFloatColor::fromHsb(ofMap(t.position.y, -1500, 1500, 0.f, 0.8f), 0.7f, 1.f);
		world.registry.emplace<MeshRenderer>(e, cube).color = c;
	}
	inspector.select(world.getEntities().front());
}

void ofApp::burst() {
	for (int i = 0; i < 500; i++) {
		auto e = world.create();
		world.registry.emplace<Velocity>(e, glm::sphericalRand(ofRandom(200, 800)), glm::vec3(0.f));
		world.registry.emplace<Lifetime>(e, ofRandom(1.f, 3.f));
		world.registry.emplace<MeshRenderer>(e, spark).color = ofFloatColor(1.f, 0.8f, 0.3f);
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
	settings.draw();

	const auto & stats = world.renderer.getStats();
	ofDrawBitmapStringHighlight(ofToString(ofGetFrameRate(), 0) + " fps\n"
		+ ofToString(world.size()) + " entities\n"
		+ ofToString(stats.drawCalls) + " draw calls (" + ofToString(stats.instancedBatches) + " instanced)\n"
		+ "instancing " + (world.renderer.isInstancingSupported() ? "available" : "not available") + "\n"
		+ "space: burst of particles   i: toggle instancing",
		ofGetWidth() - 420, 30);
}

void ofApp::keyPressed(int key) {
	if (key == ' ') burst();
	if (key == 'i') instancing = !instancing;
}

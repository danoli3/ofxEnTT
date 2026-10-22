#include "ofApp.h"

using namespace ofxEnTT;

void ofApp::setup() {
	ofBackground(30);
	gui.setup();
	cam.setDistance(800);

	auto box = std::make_shared<ofVboMesh>(ofMesh::box(40, 40, 40));
	auto ball = std::make_shared<ofVboMesh>(ofMesh::icosphere(25, 2));

	// a spinning ring of groups, each with a few children
	auto ring = world.create("ring");
	world.registry.emplace<Velocity>(ring, glm::vec3(0.f), glm::vec3(0.f, 0.3f, 0.f));
	for (int i = 0; i < 8; i++) {
		auto group = world.create("group " + ofToString(i));
		world.setParent(group, ring);
		float angle = glm::two_pi<float>() * i / 8.f;
		world.registry.get<Transform>(group).position = { std::cos(angle) * 250.f, 0.f, std::sin(angle) * 250.f };
		world.registry.emplace<Velocity>(group, glm::vec3(0.f), glm::vec3(0.f, 0.f, 1.f));
		for (int j = 0; j < 3; j++) {
			auto item = world.create((j == 0 ? "box " : "ball ") + ofToString(i) + "." + ofToString(j));
			world.setParent(item, group);
			world.registry.get<Transform>(item).position = { 0.f, (j - 1) * 60.f, 0.f };
			world.registry.emplace<MeshRenderer>(item, j == 0 ? box : ball).color
				= ofFloatColor::fromHsb(i / 8.f, 0.6f, 1.f);
		}
	}
	selected = ring;
}

void ofApp::update() {
	world.update();
}

void ofApp::draw() {
	ofEnableDepthTest();
	cam.begin();
	world.draw();
	cam.end();
	ofDisableDepthTest();

	gui.begin();
	ofxEnTT::imgui::inspector(world, selected);
	gui.end();

	// don't orbit the camera while dragging ImGui widgets
	if (ImGui::GetIO().WantCaptureMouse) cam.disableMouseInput();
	else cam.enableMouseInput();
}

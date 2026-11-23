#include "ofApp.h"

using namespace ofxEnTT;

void ofApp::setup() {
	ofSetVerticalSync(false);
	ofBackground(10, 30, 60);
	cam.setFarClip(8000);
	cam.setDistance(1300);
	cam.setTarget(glm::vec3(0.f));

	auto blueFish = aquarium::makeFishMesh(28.f);
	auto yellowFish = aquarium::makeFishMesh(18.f);
	auto bigFish = aquarium::makeFishMesh(60.f);
	config.half = { 500.f, 180.f, 500.f };
	config.species = {
		{ blueFish, ofFloatColor(0.35f, 0.65f, 1.f), 0.9f, 1.4f, 280 },
		{ yellowFish, ofFloatColor(1.f, 0.85f, 0.25f), 0.7f, 1.1f, 240 },
		{ bigFish, ofFloatColor(0.6f, 0.2f, 0.25f), 0.8f, 1.2f, 18 },
	};

	floorMesh = aquarium::makeFloorMesh(1600.f);
	floorEntity = world.create("floor");
	auto & floorTransform = world.registry.get<Transform>(floorEntity);
	floorTransform.position.y = -config.half.y - 10.f;
	floorTransform.orientation = glm::angleAxis(-glm::half_pi<float>(), glm::vec3(1.f, 0.f, 0.f));
	world.registry.emplace<MeshRenderer>(floorEntity, floorMesh).color = ofFloatColor(0.1f, 0.3f, 0.35f);

	rebuild();

	world.addSystem("swim", [this](World &, float dt) { aquarium::swim(world, config, dt); });
}

void ofApp::rebuild() {
	// keep the floor; replace fish
	for (auto [entity, swim] : world.registry.view<aquarium::Swim>().each()) {
		world.registry.destroy(entity);
	}
	aquarium::build(world, config);
}

void ofApp::update() {
	world.update();
}

void ofApp::draw() {
	ofEnableDepthTest();
	cam.begin();
	world.draw();
	// tank wireframe for scale
	ofPushStyle();
	ofNoFill();
	ofSetColor(80, 160, 220, 120);
	ofDrawBox(glm::vec3(0.f), config.half.x * 2.f, config.half.y * 2.f, config.half.z * 2.f);
	ofPopStyle();
	cam.end();
	ofDisableDepthTest();

	const auto & stats = world.renderer.getStats();
	ofDrawBitmapStringHighlight(ofToString(ofGetFrameRate(), 0) + " fps\n"
		+ ofToString(world.size()) + " entities\n"
		+ ofToString(stats.drawCalls) + " draw calls (" + ofToString(stats.instancedBatches) + " instanced)\n"
		+ "r: rebuild school   i: toggle instancing",
		20, 24);
}

void ofApp::keyPressed(int key) {
	if (key == 'r') rebuild();
	if (key == 'i') world.renderer.instancing = !world.renderer.instancing;
}

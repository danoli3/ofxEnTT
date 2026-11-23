#include "ofApp.h"

using namespace ofxEnTT;

void ofApp::setup() {
	ofSetVerticalSync(false);
	ofBackground(140, 180, 220);
	cube = std::make_shared<ofVboMesh>(ofMesh::box(config.cellSize, config.cellSize, config.cellSize, 1, 1, 1));

	cam.setFarClip(10000);
	cam.setDistance(900);
	cam.setTarget(glm::vec3(0.f, config.maxHeight * config.cellSize * 0.5f, 0.f));

	rebuild();
}

void ofApp::rebuild() {
	world.clear();
	stats = mineent::build(world, cube, config);
}

void ofApp::update() {
	if (spin) rotation += ofGetLastFrameTime() * 10.f;
	world.update();
}

void ofApp::draw() {
	ofEnableDepthTest();
	cam.begin();
	ofPushMatrix();
	ofRotateYDeg(rotation);
	world.draw();
	ofPopMatrix();
	cam.end();
	ofDisableDepthTest();

	const auto & rs = world.renderer.getStats();
	ofDrawBitmapStringHighlight(ofToString(ofGetFrameRate(), 0) + " fps\n"
		+ ofToString(world.size()) + " blocks ("
		+ ofToString(stats.terrainBlocks) + " terrain, "
		+ ofToString(stats.treeBlocks) + " trees)\n"
		+ ofToString(rs.drawCalls) + " draw calls (" + ofToString(rs.instancedBatches) + " instanced)\n"
		+ "r: rebuild   space: pause spin   +/-: radius (now " + ofToString(config.radius) + ")",
		20, 24);
}

void ofApp::keyPressed(int key) {
	if (key == 'r') rebuild();
	if (key == ' ') spin = !spin;
	if (key == '+' || key == '=') { config.radius = std::min(config.radius + 2, 48); rebuild(); }
	if (key == '-' || key == '_') { config.radius = std::max(config.radius - 2, 4); rebuild(); }
}

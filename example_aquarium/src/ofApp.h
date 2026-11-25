#pragma once

#include "ofMain.h"
#include "ofxEnTT.h"
#include "aquariumScene.h"

// A school of fish entities swimming in a bounded volume. Each species shares
// one ofVboMesh, so the RenderSystem draws them with one instanced call per
// species. A custom Swim system steers each fish with a slow wander target
// and a soft inward push at the tank walls.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

	void rebuild();

	ofxEnTT::World world;
	aquarium::SceneConfig config;
	ofEasyCam cam;

	std::shared_ptr<ofVboMesh> floorMesh;
	ofxEnTT::Entity floorEntity = ofxEnTT::Null;
};

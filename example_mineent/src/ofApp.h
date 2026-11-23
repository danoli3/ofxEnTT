#pragma once

#include "ofMain.h"
#include "ofxEnTT.h"
#include "mineentScene.h"

// A voxel world: thousands of cubes, all sharing one ofVboMesh. The
// RenderSystem draws them with one instanced call per color/block type.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

	void rebuild();

	ofxEnTT::World world;
	mineent::SceneConfig config;
	mineent::BuildStats stats;
	std::shared_ptr<ofVboMesh> cube;
	ofEasyCam cam;

	float rotation = 0.f;
	bool spin = true;
};

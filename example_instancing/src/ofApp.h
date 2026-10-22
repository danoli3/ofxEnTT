#pragma once

#include "ofMain.h"
#include "ofxEnTT.h"
#include "ofxEnTTGui.h"

// Tens of thousands of spinning cubes sharing one mesh: the RenderSystem draws
// them in one instanced draw call. Space bursts short-lived particles (Lifetime).
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

	void populate(int count);
	void burst();

	ofxEnTT::World world;
	ofxEnTT::GuiInspector inspector;
	ofxPanel settings;
	ofParameter<int> count { "cubes", 20000, 100, 100000 };
	ofParameter<bool> instancing { "instancing", true };
	ofEventListeners listeners;

	std::shared_ptr<ofVboMesh> cube;
	std::shared_ptr<ofVboMesh> spark;
	ofEasyCam cam;
};

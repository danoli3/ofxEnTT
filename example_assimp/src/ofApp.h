#pragma once

#include "ofMain.h"
#include "ofxEnTT.h"
#include "ofxEnTTGui.h"
#include "ofxEnTTAssimp.h"

// A model loaded with ofxAssimpModelLoader becomes entities (ofxEnTT::addModel),
// then cloned into a grid. The clones share the meshes, so each mesh draws in
// one instanced call. Drop another model file on the window to swap it.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;
	void dragEvent(ofDragInfo info) override;

	void loadModel(const of::filesystem::path & path);

	ofxEnTT::World world;
	ofxEnTT::GuiInspector inspector;
	std::shared_ptr<ofxAssimpModelLoader> model;
	ofEasyCam cam;
	int grid = 6;
	std::string message;
};

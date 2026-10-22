#pragma once

#include "ofMain.h"
#include "ofxEnTT.h"
#include "ofxEnTTImGui.h"

// The ImGui inspector (needs ofxImGui): browse the entity tree, edit
// components, and create / clone / destroy entities while it runs.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;

	ofxEnTT::World world;
	ofxEnTT::Entity selected = ofxEnTT::Null;
	ofxImGui::Gui gui;
	ofEasyCam cam;
};

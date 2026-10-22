#pragma once

#include "ofMain.h"
#include "ofxEnTT.h"
#include "ofxEnTTGui.h"

// A solar system built from parent / child Transforms: each orbit is a pivot
// entity spinning with a Velocity, with the planet as its child (and moons as
// the planet's children). s / l save and load the world as JSON.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

	void build();
	ofxEnTT::Entity addBody(const std::string & name, ofxEnTT::Entity parent, float orbitRadius,
		float orbitSpeed, float size, ofFloatColor color);
	void giveMeshes();

	ofxEnTT::World world;
	ofxEnTT::GuiInspector inspector;
	std::shared_ptr<ofVboMesh> sphere;
	ofEasyCam cam;
	std::string message;
};

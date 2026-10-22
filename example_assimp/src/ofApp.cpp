#include "ofApp.h"

using namespace ofxEnTT;

void ofApp::setup() {
	ofBackground(40);
	ofDisableArbTex(); // the instanced shader samples model textures as sampler2D
	cam.setDistance(1600);
	cam.setFarClip(20000);
	inspector.setup(world, "model", 10, 10);

	// models from openFrameworks' assimp example (bin/data -> OF root is 5 folders up)
	const std::string models = "../../../../../examples/3d/assimp3DModelLoaderExample/bin/data/";
	for (auto candidate : { "Druid/druid.gltf", "Fox/Fox_05.fbx", "Astroboy/astroBoy_walk.dae" }) {
		if (ofFile::doesFileExist(models + candidate)) {
			loadModel(models + candidate);
			break;
		}
	}
	if (!model) {
		message = "drop a model file on the window";
	}
}

void ofApp::loadModel(const of::filesystem::path & path) {
	auto loaded = std::make_shared<ofxAssimpModelLoader>();
	if (!loaded->load(path, ofxAssimpModelLoader::OPTIMIZE_DEFAULT)) {
		message = "couldn't load " + path.filename().string();
		return;
	}
	if (loaded->hasAnimations()) {
		loaded->setLoopStateForAllAnimations(OF_LOOP_NORMAL);
		loaded->playAllAnimations();
	}
	model = loaded;
	world.clear();

	auto original = addModel(world, model, path.stem().string());
	auto & base = world.registry.get<Transform>(original);
	// space the copies by the size the model is drawn at
	world.update(0.f);
	glm::vec3 min, max;
	world.getBounds(original, min, max);
	glm::vec3 size = max - min;
	const float spacing = std::max(size.x, size.z) * 1.5f;

	for (int x = 0; x < grid; x++) {
		for (int z = 0; z < grid; z++) {
			auto copy = (x == 0 && z == 0) ? original : world.clone(original);
			auto & t = world.registry.get<Transform>(copy);
			t.position = base.position + glm::vec3((x - (grid - 1) * 0.5f) * spacing, 0.f, (z - (grid - 1) * 0.5f) * spacing);
			world.registry.emplace_or_replace<Velocity>(copy, glm::vec3(0.f), glm::vec3(0.f, ofRandom(-1.f, 1.f), 0.f));
		}
	}
	// frame the whole grid: its bounds after one update
	world.update(0.f);
	glm::vec3 gridMin, gridMax, rootMin, rootMax;
	bool first = true;
	for (auto root : world.getRoots()) {
		if (world.getBounds(root, rootMin, rootMax)) {
			gridMin = first ? rootMin : glm::min(gridMin, rootMin);
			gridMax = first ? rootMax : glm::max(gridMax, rootMax);
			first = false;
		}
	}
	// the grid is centred where the model loaded (near the origin, where the
	// camera looks); only the distance needs to fit it
	cam.setDistance(glm::length(gridMax - gridMin) * 0.65f);

	message = path.filename().string() + ": " + ofToString(model->getNumMeshes()) + " meshes x " + ofToString(grid * grid) + " copies";
	inspector.select(original);
}

void ofApp::update() {
	updateModelAnimations(world);
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
	const auto & stats = world.renderer.getStats();
	ofDrawBitmapStringHighlight(message + "\n" + ofToString(ofGetFrameRate(), 0) + " fps, "
		+ ofToString(stats.entities) + " meshes drawn in " + ofToString(stats.drawCalls) + " draw calls\n"
		+ "i: toggle instancing", 10, ofGetHeight() - 50);
}

void ofApp::keyPressed(int key) {
	if (key == 'i') world.renderer.instancing = !world.renderer.instancing;
}

void ofApp::dragEvent(ofDragInfo info) {
	if (!info.files.empty()) {
		loadModel(info.files[0]);
	}
}

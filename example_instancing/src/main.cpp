#include "ofMain.h"
#include "ofApp.h"

int main() {
	// instanced drawing needs the programmable renderer
#ifdef TARGET_OPENGLES
	ofGLESWindowSettings settings;
	settings.setGLESVersion(3);
#else
	ofGLWindowSettings settings;
	settings.setGLVersion(3, 3);
#endif
	settings.setSize(1280, 800);
	auto window = ofCreateWindow(settings);
	ofRunApp(window, std::make_shared<ofApp>());
	ofRunMainLoop();
}

#pragma once

// ofxEnTT: the EnTT entity component system (https://github.com/skypjack/entt)
// for openFrameworks.
//
// Bridges, header-only, included only by apps that use them (add the addon too):
//   #include "ofxEnTTGui.h"      ofxGui inspector panel      (ofxGui)
//   #include "ofxEnTTImGui.h"    ImGui inspector window      (ofxImGui)
//   #include "ofxEnTTAssimp.h"   model -> entities           (ofxAssimpModelLoader)

#include <entt/entt.hpp>

#include "ofxEnTTComponents.h"
#include "ofxEnTTWorld.h"
#include "ofxEnTTTransformSystem.h"
#include "ofxEnTTRenderSystem.h"
#include "ofxEnTTSerialize.h"
#include "ofxEnTTParameters.h"

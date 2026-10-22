#pragma once

#include "ofxEnTTWorld.h"
#include "ofJson.h"

namespace ofxEnTT {

/// The world's entities as JSON: Name, Transform, Parent, Velocity, Lifetime,
/// Hidden and the MeshRenderer colour (meshes, textures and materials aren't
/// saved; give them back to the loaded entities by name).
ofJson toJson(const World & world);

/// Adds the entities in json to the world (after clearing it, unless clear is
/// false). Returns the created entities, in the order they were saved.
std::vector<Entity> fromJson(World & world, const ofJson & json, bool clear = true);

bool saveJson(const World & world, const of::filesystem::path & path);
std::vector<Entity> loadJson(World & world, const of::filesystem::path & path, bool clear = true);

} // namespace ofxEnTT

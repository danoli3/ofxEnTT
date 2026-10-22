#pragma once

#include "ofxEnTTComponents.h"
#include "ofxEnTTTransformSystem.h"
#include "ofxEnTTRenderSystem.h"

#include "ofEvents.h"

#include <functional>

namespace ofxEnTT {

/// An EnTT registry plus the systems that run on it each frame.
///
///     ofxEnTT::World world;
///     auto e = world.create("box");
///     world.registry.emplace<ofxEnTT::MeshRenderer>(e, mesh);
///     world.update();   // in ofApp::update
///     world.draw();     // in ofApp::draw (inside a camera)
///
/// update() runs Velocity and Lifetime, then the added systems in the order they
/// were added, then the TransformSystem. The registry is public: use the whole
/// EnTT API on it (views, groups, signals, ...).
class World {
public:
	using System = std::function<void(World &, float)>;

	World() = default;
	World(const World &) = delete;
	World & operator=(const World &) = delete;

	Registry registry;
	TransformSystem transforms;
	RenderSystem renderer;

	/// New entity with a Transform (and a Name when name isn't empty).
	Entity create(const std::string & name = "");
	/// Destroys the entity and its children, and removes it from its parent.
	void destroy(Entity entity);
	/// Destroys every entity.
	void clear();
	bool isValid(Entity entity) const;
	/// Number of live entities.
	size_t size() const;

	/// Makes child's Transform relative to parent (Null detaches). Returns false
	/// if that would make a cycle.
	bool setParent(Entity child, Entity parent);
	Entity getParent(Entity entity) const;
	std::vector<Entity> getChildren(Entity entity) const;
	/// Entities without a parent, in creation order.
	std::vector<Entity> getRoots() const;
	/// Every live entity, in creation order.
	std::vector<Entity> getEntities() const;

	/// First entity with this Name, or Null.
	Entity findByName(const std::string & name) const;
	/// The Name, or "entity <id>".
	std::string getName(Entity entity) const;

	/// World-space bounding box of the meshes of entity and its children, from
	/// their WorldTransforms (as of the last update). False if there are none.
	bool getBounds(Entity entity, glm::vec3 & min, glm::vec3 & max) const;

	/// Copies the entity and its children (Name, Transform, Velocity, Lifetime,
	/// Hidden, MeshRenderer). Meshes, textures and materials are shared, so the
	/// copies draw instanced with the original.
	Entity clone(Entity entity, Entity parent = Null);

	/// Runs a system every update, after Velocity / Lifetime and the systems
	/// added before it. Adding a system with an existing name replaces it.
	void addSystem(const std::string & name, System system);
	void removeSystem(const std::string & name);
	bool hasSystem(const std::string & name) const;

	/// Runs the systems with this frame's time step.
	void update();
	void update(float deltaSeconds);
	/// Draws every MeshRenderer (see RenderSystem).
	void draw();

	/// Calls update() / draw() from ofEvents().update / draw, so ofApp doesn't
	/// have to. draw() then runs before ofApp::draw, outside any camera.
	void setAutoUpdate(bool autoUpdate);
	void setAutoDraw(bool autoDraw);

	/// Velocity and Lifetime (on by default).
	bool builtInSystems = true;

private:
	struct NamedSystem {
		std::string name;
		System system;
	};
	std::vector<NamedSystem> systems;
	ofEventListener updateListener;
	ofEventListener drawListener;

	void updateVelocity(float deltaSeconds);
	void updateLifetime(float deltaSeconds);
	void detach(Entity entity);
};

} // namespace ofxEnTT

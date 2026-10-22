#pragma once

#include "ofxEnTTWorld.h"
#include "ofParameter.h"

namespace ofxEnTT {

/// One entity's components as ofParameters, for ofxGui, ofxImGui's ofParameter
/// helpers, OSC sync or anything else that speaks ofParameter.
///
/// Changing a parameter writes the component; pull() (each frame) reads the
/// components back so the parameters follow systems that move the entity.
/// The group only holds the parameters for components the entity has.
class EntityParameters {
public:
	ofParameterGroup group;
	ofParameter<std::string> name { "name", "" };
	ofParameter<glm::vec3> position { "position", glm::vec3(0.f), glm::vec3(-1000.f), glm::vec3(1000.f) };
	ofParameter<glm::vec3> rotation { "rotation", glm::vec3(0.f), glm::vec3(-180.f), glm::vec3(180.f) };
	ofParameter<glm::vec3> scale { "scale", glm::vec3(1.f), glm::vec3(0.f), glm::vec3(10.f) };
	ofParameter<ofFloatColor> color { "color", ofFloatColor::white, ofFloatColor(0.f, 0.f), ofFloatColor(1.f, 1.f) };
	ofParameter<bool> visible { "visible", true };
	ofParameter<glm::vec3> linearVelocity { "linear velocity", glm::vec3(0.f), glm::vec3(-100.f), glm::vec3(100.f) };
	ofParameter<glm::vec3> angularVelocity { "angular velocity", glm::vec3(0.f), glm::vec3(-10.f), glm::vec3(10.f) };

	/// Notified after the group's parameters change (another entity, or the
	/// entity gained / lost components); rebuild GUIs built from the group.
	ofEvent<void> groupChanged;

	EntityParameters();
	EntityParameters(const EntityParameters &) = delete;
	EntityParameters & operator=(const EntityParameters &) = delete;

	void setup(World & world);
	void setEntity(Entity entity);
	Entity getEntity() const { return entity; }

	/// Reads the components into the parameters (no write back), and rebuilds
	/// the group if the entity's components changed.
	void pull();

private:
	World * world = nullptr;
	Entity entity = Null;
	bool pulling = false;
	std::string layout;
	ofEventListeners listeners;

	std::string currentLayout() const;
	void rebuild();
	template <typename T>
	T * component() {
		return (world && world->isValid(entity)) ? world->registry.try_get<T>(entity) : nullptr;
	}
};

} // namespace ofxEnTT

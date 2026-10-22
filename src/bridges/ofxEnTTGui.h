#pragma once

// ofxGui inspector for an ofxEnTT::World. Needs the ofxGui addon.
//
//     ofxEnTT::GuiInspector inspector;
//     inspector.setup(world);            // setup
//     inspector.update();                // update, after world.update()
//     inspector.draw();                  // draw, outside the camera

#include "ofxEnTT.h"
#include "ofxGui.h"

namespace ofxEnTT {

class GuiInspector {
public:
	ofxPanel panel;
	/// The selected entity's parameters (see EntityParameters).
	EntityParameters entity;
	/// Index of the selected entity in World::getEntities().
	ofParameter<int> selection { "select", 0, 0, 0 };

	void setup(World & w, const std::string & title = "ofxEnTT", float x = 10.f, float y = 10.f) {
		world = &w;
		worldGroup.setName("world");
		worldGroup.add(selection);
		panel.setup(title, "ofxEnTT.json", x, y);
		entity.setup(w);
		groupListener = entity.groupChanged.newListener([this]() { needsRebuild = true; });
		selectionListener = selection.newListener([this](int & index) {
			auto entities = world->getEntities();
			if (index >= 0 && index < static_cast<int>(entities.size())) {
				entity.setEntity(entities[index]);
			}
		});
		auto entities = w.getEntities();
		if (!entities.empty()) {
			entity.setEntity(entities.front());
		}
		rebuildPanel();
	}

	void select(Entity e) {
		if (!world) return; // before setup()
		auto entities = world->getEntities();
		auto it = std::find(entities.begin(), entities.end(), e);
		if (it != entities.end()) {
			selection = static_cast<int>(it - entities.begin());
		}
	}

	Entity getSelected() const { return entity.getEntity(); }

	/// Follows the world: entity count, component changes, values moved by systems.
	void update() {
		if (!world) return;
		int last = std::max(0, static_cast<int>(world->size()) - 1);
		if (selection.getMax() != last) {
			selection.setMax(last);
		}
		if (!world->isValid(entity.getEntity()) && world->size() > 0) {
			selection = std::min(selection.get(), last);
			entity.setEntity(world->getEntities()[selection]);
		}
		entity.pull();
		if (needsRebuild) {
			rebuildPanel();
		}
	}

	void draw() { panel.draw(); }

private:
	World * world = nullptr;
	ofParameterGroup worldGroup;
	ofEventListener groupListener;
	ofEventListener selectionListener;
	bool needsRebuild = false;

	// rebuilt from update(), never from inside a panel callback
	void rebuildPanel() {
		needsRebuild = false;
		auto position = panel.getPosition();
		panel.clear();
		panel.add(worldGroup);
		panel.add(entity.group);
		panel.setPosition(position);
	}
};

} // namespace ofxEnTT

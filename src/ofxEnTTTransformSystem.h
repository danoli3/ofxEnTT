#pragma once

#include "ofxEnTTComponents.h"

namespace ofxEnTT {

/// Writes every entity's WorldTransform from its Transform and Parent chain,
/// visiting each parent before its children (one pass, no matrix recomputed).
class TransformSystem {
public:
	void update(Registry & registry);

private:
	void visit(Registry & registry, Entity entity, const glm::mat4 & parentMatrix);
};

} // namespace ofxEnTT

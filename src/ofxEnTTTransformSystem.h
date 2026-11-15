#pragma once

#include "ofxEnTTComponents.h"

namespace ofxEnTT {

/// Writes every entity's WorldTransform from its Transform and Parent chain,
/// visiting each parent before its children (one pass, no matrix recomputed).
class TransformSystem {
public:
	struct Stats {
		size_t updated = 0;
		size_t skipped = 0;
	};

	void update(Registry & registry);
	/// Opt-in matrix caching. Direct edits to Transform fields are detected too.
	void setIncremental(bool enabled);
	bool isIncremental() const { return incremental; }
	/// Force an entity's matrix (and its descendants) to be refreshed next update.
	void markDirty(Entity entity);
	/// Discard all cached transform state, retaining its allocated capacity.
	void invalidate();
	const Stats & getStats() const { return stats; }

private:
	struct CachedTransform {
		Transform local;
		Entity parent = Null;
		size_t lastSeen = 0;
		bool hasTransform = false;
		bool dirty = true;
	};
	bool incremental = false;
	Registry * lastRegistry = nullptr;
	size_t frame = 0;
	Stats stats;
	std::vector<Entity> roots;
	entt::dense_map<Entity, CachedTransform> cache;
	void visit(Registry & registry, Entity entity, const glm::mat4 * parentMatrix,
		Entity parent, bool parentChanged);
};

} // namespace ofxEnTT

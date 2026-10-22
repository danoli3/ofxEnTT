#pragma once

// Built-in ofxEnTT components. They are plain structs: add, read and change
// them through the registry (world.registry.emplace<Transform>(entity) etc.).

#include "ofVboMesh.h"
#include "ofMaterial.h"
#include "ofTexture.h"
#include "ofShader.h"
#include "ofColor.h"

#include <entt/entt.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <string>
#include <vector>

namespace ofxEnTT {

using Entity = entt::entity;
using Registry = entt::registry;
inline constexpr entt::null_t Null{};

/// A display name, used by the inspectors and the JSON save / load.
struct Name {
	std::string value;
};

/// Local position, orientation and scale, relative to the Parent if there is one.
struct Transform {
	glm::vec3 position { 0.f };
	glm::quat orientation { 1.f, 0.f, 0.f, 0.f };
	glm::vec3 scale { 1.f };

	glm::mat4 localMatrix() const {
		return glm::translate(glm::mat4(1.f), position)
			* glm::mat4_cast(orientation)
			* glm::scale(glm::mat4(1.f), scale);
	}

	/// Orientation as Euler angles in degrees (pitch, yaw, roll), like ofNode.
	glm::vec3 getOrientationEulerDeg() const {
		return glm::degrees(glm::eulerAngles(orientation));
	}
	void setOrientationEulerDeg(const glm::vec3 & degrees) {
		orientation = glm::quat(glm::radians(degrees));
	}
};

/// The entity's matrix in world space. Written by the TransformSystem every
/// update from the Transform and the Parent chain; read it, don't set it.
struct WorldTransform {
	glm::mat4 matrix { 1.f };

	glm::vec3 getPosition() const { return glm::vec3(matrix[3]); }
};

/// Makes the entity's Transform relative to another entity. Use
/// World::setParent so the parent's Children list stays in step.
struct Parent {
	Entity entity { Null };
};

/// The entities whose Parent is this entity (kept by World::setParent).
struct Children {
	std::vector<Entity> entities;
};

/// Draws a mesh at the entity's WorldTransform. Entities sharing the same
/// mesh, texture and shader are drawn in one instanced draw call when the
/// renderer allows it (see RenderSystem).
struct MeshRenderer {
	std::shared_ptr<ofVboMesh> mesh;
	/// Optional. With a material the entity is drawn on its own (ofMaterial
	/// has its own shader), not instanced.
	std::shared_ptr<ofMaterial> material;
	/// Optional texture, bound while drawing.
	std::shared_ptr<ofTexture> texture;
	/// Per-entity colour, also the per-instance colour when instanced.
	ofFloatColor color { 1.f, 1.f, 1.f, 1.f };
	ofPolyRenderMode mode { OF_MESH_FILL };
};

/// Tag: the RenderSystem skips the entity.
struct Hidden {};

/// Moves the Transform every update (units and radians per second).
struct Velocity {
	glm::vec3 linear { 0.f };
	glm::vec3 angular { 0.f };
};

/// The entity is destroyed when this reaches zero (seconds).
struct Lifetime {
	float remaining { 1.f };
};

} // namespace ofxEnTT

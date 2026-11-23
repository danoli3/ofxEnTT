#pragma once

// Fish-tank scene builder for ofxEnTT, inspired by the aquarium demo shared
// with the ofx dawn PR. The scene is a school of fish entities, each with a
// Transform (position + orientation), a shared species mesh (so the renderer
// draws them in one instanced call per species), and a Swim component that a
// tiny custom system integrates every frame: fish drift toward a slowly
// wandering target and are gently steered back if they leave the tank.

#include "ofxEnTT.h"

#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace aquarium {

struct Swim {
	float speed = 40.f;
	float wanderPhase = 0.f;
	float wanderRate = 0.4f;
	glm::vec3 heading { 0.f, 0.f, 1.f };
};

struct Species {
	std::shared_ptr<ofVboMesh> mesh;
	ofFloatColor color;
	float minSize = 1.f;
	float maxSize = 1.f;
	int count = 0;
};

struct SceneConfig {
	glm::vec3 half { 400.f, 180.f, 400.f }; ///< tank half-extents
	std::vector<Species> species;
	uint32_t seed = 42;
};

/// A slightly stretched teardrop. Shared across one species; instanced.
inline std::shared_ptr<ofVboMesh> makeFishMesh(float length = 20.f) {
	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_TRIANGLES);
	const int segments = 10, rings = 5;
	const float halfLength = length * 0.5f;
	for (int r = 0; r <= rings; r++) {
		const float t = float(r) / rings; // 0 nose -> 1 tail
		const float z = halfLength - length * t;
		const float radius = length * 0.18f * std::sin(glm::pi<float>() * t);
		for (int s = 0; s <= segments; s++) {
			const float a = s * glm::two_pi<float>() / segments;
			glm::vec3 v { radius * std::cos(a), radius * std::sin(a), z };
			mesh.addVertex(v);
			mesh.addNormal(glm::normalize(glm::vec3(v.x, v.y, 0.f) + glm::vec3(0.f, 0.f, 0.3f)));
		}
	}
	const int stride = segments + 1;
	for (int r = 0; r < rings; r++) {
		for (int s = 0; s < segments; s++) {
			const int a = r * stride + s;
			const int b = a + stride;
			mesh.addTriangle(a, b, a + 1);
			mesh.addTriangle(a + 1, b, b + 1);
		}
	}
	return std::make_shared<ofVboMesh>(mesh);
}

/// Sea-floor quad.
inline std::shared_ptr<ofVboMesh> makeFloorMesh(float size) {
	auto mesh = std::make_shared<ofVboMesh>(ofMesh::plane(size, size, 2, 2));
	return mesh;
}

/// Populates `world` with a floor plus the configured species. Returns the
/// number of fish created. Uses a seeded PRNG so the setup is reproducible.
inline size_t build(ofxEnTT::World & world, const SceneConfig & config) {
	using namespace ofxEnTT;
	std::mt19937 rng(config.seed);
	std::uniform_real_distribution<float> posX(-config.half.x, config.half.x);
	std::uniform_real_distribution<float> posY(-config.half.y, config.half.y);
	std::uniform_real_distribution<float> posZ(-config.half.z, config.half.z);
	std::uniform_real_distribution<float> unit(0.f, 1.f);
	std::uniform_real_distribution<float> phase(0.f, glm::two_pi<float>());

	size_t total = 0;
	for (size_t s = 0; s < config.species.size(); s++) {
		const auto & species = config.species[s];
		for (int i = 0; i < species.count; i++) {
			auto e = world.create("fish " + std::to_string(s) + ":" + std::to_string(i));
			auto & t = world.registry.get<Transform>(e);
			t.position = { posX(rng), posY(rng), posZ(rng) };
			t.scale = glm::vec3(species.minSize + (species.maxSize - species.minSize) * unit(rng));
			const float yaw = phase(rng);
			t.orientation = glm::angleAxis(yaw, glm::vec3(0.f, 1.f, 0.f));

			Swim swim;
			swim.speed = 25.f + 35.f * unit(rng);
			swim.wanderPhase = phase(rng);
			swim.wanderRate = 0.3f + 0.6f * unit(rng);
			swim.heading = glm::vec3(std::sin(yaw), 0.f, std::cos(yaw));
			world.registry.emplace<Swim>(e, swim);
			world.registry.emplace<MeshRenderer>(e, species.mesh).color = species.color;
			total++;
		}
	}
	return total;
}

/// Minimal steering: fish drift toward a slow wander target, and we add a
/// soft inward push whenever they drift past the tank's half-extents. The
/// Transform is oriented to face the final heading so instances line up.
inline void swim(ofxEnTT::World & world, const SceneConfig & config, float dt) {
	using namespace ofxEnTT;
	const auto half = config.half;
	world.registry.view<Transform, Swim>().each([&](Transform & t, Swim & swim) {
		swim.wanderPhase += swim.wanderRate * dt;
		glm::vec3 wander {
			std::sin(swim.wanderPhase * 1.1f),
			0.3f * std::sin(swim.wanderPhase * 0.7f),
			std::cos(swim.wanderPhase)
		};
		// soft wall avoidance: push back along the overshoot axis
		glm::vec3 avoid { 0.f };
		if (t.position.x >  half.x) avoid.x -= (t.position.x - half.x) / half.x;
		if (t.position.x < -half.x) avoid.x -= (t.position.x + half.x) / half.x;
		if (t.position.y >  half.y) avoid.y -= (t.position.y - half.y) / half.y;
		if (t.position.y < -half.y) avoid.y -= (t.position.y + half.y) / half.y;
		if (t.position.z >  half.z) avoid.z -= (t.position.z - half.z) / half.z;
		if (t.position.z < -half.z) avoid.z -= (t.position.z + half.z) / half.z;
		glm::vec3 desired = glm::normalize(wander + 2.0f * avoid + 0.01f * swim.heading);
		swim.heading = glm::normalize(glm::mix(swim.heading, desired, std::min(1.f, 2.f * dt)));
		t.position += swim.heading * (swim.speed * dt);
		// Orient along heading. Mesh's +Z is "nose"; we align to heading.
		const glm::vec3 forward { 0.f, 0.f, 1.f };
		const float d = glm::dot(forward, swim.heading);
		if (d < 0.9999f) {
			if (d < -0.9999f) {
				t.orientation = glm::angleAxis(glm::pi<float>(), glm::vec3(0.f, 1.f, 0.f));
			} else {
				glm::vec3 axis = glm::normalize(glm::cross(forward, swim.heading));
				t.orientation = glm::angleAxis(std::acos(d), axis);
			}
		}
	});
}

} // namespace aquarium

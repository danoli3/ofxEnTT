#pragma once

// A tiny Minecraft-like voxel world built on ofxEnTT. One shared cube mesh is
// used for every block (so the RenderSystem issues one instanced call per
// block type), and only visible blocks at the top of each column plus the
// edges of the terrain patch are added as entities. The scene builder is
// deterministic from a seed so tests can validate counts.

#include "ofxEnTT.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace mineent {

enum class Block : uint8_t { Air = 0, Grass, Dirt, Stone, Sand, Wood, Leaves, Count };

inline ofFloatColor blockColor(Block b) {
	switch (b) {
		case Block::Grass:  return { 0.35f, 0.72f, 0.28f };
		case Block::Dirt:   return { 0.55f, 0.38f, 0.22f };
		case Block::Stone:  return { 0.55f, 0.55f, 0.55f };
		case Block::Sand:   return { 0.95f, 0.88f, 0.60f };
		case Block::Wood:   return { 0.55f, 0.32f, 0.12f };
		case Block::Leaves: return { 0.25f, 0.55f, 0.20f };
		case Block::Air:    return { 0.f, 0.f, 0.f, 0.f };
		default:            return { 1.f, 0.f, 1.f };
	}
}

struct SceneConfig {
	int radius = 24;       ///< terrain spans [-radius, radius] on x and z
	int maxHeight = 10;
	int seaLevel = 2;
	float cellSize = 10.f; ///< edge length of each cube
	uint32_t seed = 42;
	bool addTrees = true;
	int treeCount = 10;
};

struct BuildStats {
	size_t terrainBlocks = 0;
	size_t treeBlocks = 0;
	size_t columns = 0;
};

/// Smooth fBm-ish heightmap in [0, maxHeight].
inline int heightAt(int x, int z, int maxHeight, uint32_t seed) {
	auto hash = [seed](int a, int b) {
		uint32_t h = seed * 0x9E3779B1u + uint32_t(a) * 0x85EBCA77u + uint32_t(b) * 0xC2B2AE3Du;
		h ^= h >> 15; h *= 0x85EBCA77u; h ^= h >> 13;
		return (h & 0xFFFFu) / float(0xFFFF);
	};
	float h = 0.f, amplitude = 1.f, total = 0.f;
	for (int octave = 0; octave < 3; octave++) {
		const int scale = 1 << octave;
		h += amplitude * hash(x / (8 / scale + 1), z / (8 / scale + 1));
		total += amplitude;
		amplitude *= 0.5f;
	}
	return int(std::round((h / total) * maxHeight));
}

inline Block topBlock(int height, int seaLevel) {
	if (height <= seaLevel) return Block::Sand;
	if (height <= seaLevel + 1) return Block::Grass;
	if (height >= 8) return Block::Stone;
	return Block::Grass;
}

inline void placeBlock(ofxEnTT::World & world, const std::shared_ptr<ofVboMesh> & mesh,
	int gx, int gy, int gz, float cellSize, Block type) {
	using namespace ofxEnTT;
	auto e = world.create();
	auto & t = world.registry.get<Transform>(e);
	t.position = { gx * cellSize, gy * cellSize, gz * cellSize };
	world.registry.emplace<MeshRenderer>(e, mesh).color = blockColor(type);
}

/// Builds a terrain patch plus optional trees. Only renders blocks that are
/// visible (top of each column and the vertical walls at the patch edges),
/// which keeps the entity count close to the surface area, not the volume.
inline BuildStats build(ofxEnTT::World & world,
	const std::shared_ptr<ofVboMesh> & cube, const SceneConfig & config) {
	using namespace ofxEnTT;
	BuildStats stats;
	const int R = config.radius;
	for (int z = -R; z <= R; z++) {
		for (int x = -R; x <= R; x++) {
			const int h = heightAt(x, z, config.maxHeight, config.seed);
			const Block top = topBlock(h, config.seaLevel);
			placeBlock(world, cube, x, h, z, config.cellSize, top);
			stats.terrainBlocks++;
			// fill edges so you can see the cliffs when panning
			if (x == -R || x == R || z == -R || z == R) {
				for (int y = h - 1; y >= 0; y--) {
					placeBlock(world, cube, x, y, z, config.cellSize,
						y > config.seaLevel ? Block::Dirt : Block::Stone);
					stats.terrainBlocks++;
				}
			}
			stats.columns++;
		}
	}

	if (config.addTrees) {
		std::mt19937 rng(config.seed ^ 0xBADFACEu);
		std::uniform_int_distribution<int> coord(-R + 2, R - 2);
		for (int i = 0; i < config.treeCount; i++) {
			const int x = coord(rng), z = coord(rng);
			const int h = heightAt(x, z, config.maxHeight, config.seed);
			if (h < config.seaLevel + 1) continue;
			for (int y = 1; y <= 3; y++) {
				placeBlock(world, cube, x, h + y, z, config.cellSize, Block::Wood);
				stats.treeBlocks++;
			}
			for (int dx = -1; dx <= 1; dx++) {
				for (int dz = -1; dz <= 1; dz++) {
					for (int dy = 0; dy <= 1; dy++) {
						if (dx == 0 && dz == 0 && dy == 0) continue;
						placeBlock(world, cube, x + dx, h + 3 + dy, z + dz, config.cellSize, Block::Leaves);
						stats.treeBlocks++;
					}
				}
			}
			placeBlock(world, cube, x, h + 5, z, config.cellSize, Block::Leaves);
			stats.treeBlocks++;
		}
	}
	return stats;
}

} // namespace mineent

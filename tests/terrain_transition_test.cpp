#include "../src/terrain_transition.h"
#include "support/registry_fixture.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main(int argc, char **argv) {
	const auto registry = test_registry::load(argc, argv);
	const int bed = registry.sea_level + registry.wet_coast_offset;
	const int shelf = registry.sea_level + registry.dry_coast_offset;
	const int top = registry.sea_level;
	float boundary = -1;
	for (const auto &biome : registry.biomes)
		if (biome.surface_fill == "ice") { boundary = biome.climate_max; break; }
	assert(boundary >= 0 && boundary <= 1);
	float low = 1, high = 0;
	for (int z = -64; z <= 64; ++z)
		for (int x = -64; x <= 64; ++x) {
			const float v = voxel::transition_patch(42, x, z);
			assert(v >= 0 && v <= 1);
			assert(v == voxel::transition_patch(42, x, z));
			assert(std::abs(v - voxel::transition_patch(42, x + 1, z)) < 0.19f);
			assert(std::abs(v - voxel::transition_patch(42, x, z + 1)) < 0.19f);
			low = std::min(low, v);
			high = std::max(high, v);
		}
	assert(low < 0.1f && high > 0.9f);
	assert(voxel::transition_patch(42, -16, 16) != voxel::transition_patch(43, -16, 16));
	for (float dry : {0.0f, 0.5f, 1.0f}) {
		float previous = shelf;
		for (int i = 0; i <= 100; ++i) {
			float height = voxel::coast_height(bed, shelf, dry, i / 100.0f);
			assert(height <= previous && height >= bed);
			previous = height;
		}
		assert(previous == bed);
	}
	assert(voxel::coast_height(bed, shelf, 1, 0.4f) == shelf);
	assert(voxel::freezing_weight(boundary - 0.14f, boundary, 0.14f) == 1);
	assert(voxel::freezing_weight(boundary + 0.14f, boundary, 0.14f) == 0);
	assert(std::abs(voxel::freezing_weight(boundary, boundary, 0.14f) - 0.5f) < 0.0001f);
	for (float patch : {0.0f, 0.5f, 1.0f}) {
		assert(voxel::freezing_height(0, patch, bed, top) == bed);
		assert(voxel::freezing_height(1, patch, bed, top) == top - 1);
		int previous = bed;
		for (int i = 0; i <= 100; ++i) {
			const int height = voxel::freezing_height(i / 100.0f, patch, bed, top);
			assert(height >= previous && height <= previous + 1 && height <= top - 1);
			previous = height;
		}
	}
	std::cout << "Terrain transition checks passed.\n";
}

#ifndef BIOME_REGISTRY_H
#define BIOME_REGISTRY_H

#include "chunk_model.h"
#include <godot_cpp/variant/dictionary.hpp>
#include <memory>
#include <string>
#include <vector>

namespace godot {

enum class BiomeKind { LAND,
					   OCEAN,
					   RIVER,
					   BEACH };

enum class ReliefNoise { MOUNTAIN,
						 DUNE,
						 TERRAIN };

struct WeightedPlant {
	uint16_t block = 0;
	int weight	   = 1;
	int min_height = 1;
	int max_height = 1;
};

struct VegetationProfile {
	int patch_size	 = 12;
	int patch_chance = 1000, cluster_radius = 0;
	int coverage_min = 0, coverage_max = 0;
	int flowers_min = 0, flowers_max = 0;
	float climate_min = 0.0f, climate_max = 1.0f;
	std::vector<WeightedPlant> plants, flowers;
};

struct TreeProfile {
	enum class Shape { OAK, PALM, PINE };
	Shape shape = Shape::OAK;
	int max_per_chunk = 0, min_height = 5, max_height = 7, crown_radius = 2;
	float climate_min = 0.0f, climate_max = 1.0f;
	uint16_t trunk = voxel::block_ids::oak_log, leaves = voxel::block_ids::oak_leaves;
};

struct SurfaceOverride {
	float climate_min = 0.0f, climate_max = 1.0f;
	uint16_t surface = 0;
	int soil_depth	 = -1;
};

struct TerrainStratum {
	uint16_t block = 0;
	int min_depth = 1, max_depth = 65535;
	int min_y = WORLD_BEDROCK_Y + 1, max_y = WORLD_MAX_CHUNK_Y * Chunk::SIZE_Y + Chunk::MAX_Y;
};

struct BiomeDefinition {
	uint16_t id = 0;
	std::string name;
	BiomeKind kind	  = BiomeKind::LAND;
	float climate_min = 0.0f, climate_max = 1.0f, height_anchor = 0.0f;
	int priority	   = 0;
	int rarity		   = 1;
	float height_scale = 2.4f, ridge_amplitude = 19.0f, ridge_power = 2.0f, height_bias = -5.0f;
	ReliefNoise relief_noise = ReliefNoise::MOUNTAIN;
	bool dry_coast = false, surface_water = true;
	uint16_t surface_fill = voxel::block_ids::water;
	int selection_height_offset = 2;
	float selection_influence	= 0.5f;
	uint16_t surface = voxel::block_ids::grass, soil = voxel::block_ids::dirt;
	uint16_t rock = voxel::block_ids::stone, deep_rock = voxel::block_ids::deepslate;
	int soil_depth = 15, deep_rock_below_y = -32;
	std::vector<SurfaceOverride> surface_overrides;
	std::vector<TerrainStratum> strata;
	TreeProfile trees;
	VegetationProfile vegetation;
};

struct TerrainNoiseProfile {
	float frequency;
	int octaves;
};

struct TerrainWorldProfile {
	int base_height = 24, sea_level = 24;
	float amplitude		= 9.0f;
	float climate_start = -0.90f, climate_span = 2.0f;
	float coast_start = 0.25f, coast_span = 0.80f;
	float dry_coast_start = 0.05f, dry_coast_span = 0.65f;
	float coast_blend_start = 0.34f, coast_blend_end = 0.68f;
	int wet_coast_offset = -8, dry_coast_offset = 1;
	float dry_coast_clamp = 0.85f, river_width = 0.07f;
	int river_bed_offset = -2;
	TerrainNoiseProfile terrain{ 0.025f, 4 }, climate{ 0.001f, 2 }, dune{ 0.016f, 3 };
	TerrainNoiseProfile mountain{ 0.006f, 3 }, ocean{ 0.0015f, 2 }, river{ 0.008f, 1 };
};

// Loaded/validated on the main thread. Workers share only a const snapshot.

class BiomeRegistry {
public:
	TerrainWorldProfile world;
	std::vector<BiomeDefinition> biomes;
	const BiomeDefinition &land_at(float climate, int64_t seed, int32_t x, int32_t z) const;
	const BiomeDefinition *overlay_at(BiomeKind kind, float climate, int64_t seed, int32_t x, int32_t z) const;
	void relief_pair(float climate, const BiomeDefinition *&a, const BiomeDefinition *&b, float &blend) const;
	const BiomeDefinition &fallback_land(float climate) const;
	int max_tree_candidates() const;
	static std::shared_ptr<const BiomeRegistry> load(const String &path);
	static std::shared_ptr<const BiomeRegistry> defaults();
	static std::shared_ptr<const BiomeRegistry> from_dictionary(const Dictionary &data, String &error);
};
} // namespace godot
#endif

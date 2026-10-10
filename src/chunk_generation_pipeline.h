#ifndef CHUNK_GENERATION_PIPELINE_H
#define CHUNK_GENERATION_PIPELINE_H

#include "chunk_model.h"
#include "biome_registry.h"
#include "godot_cpp/classes/fast_noise_lite.hpp"
#include "godot_cpp/classes/ref.hpp"

#include <memory>
#include <vector>

namespace godot {

class TerrainColumnCache;

struct TerrainSettings {
    std::shared_ptr<TerrainColumnCache> column_cache;
	int terrain_base_height = 24;
	float terrain_amplitude = 9.0f;
	int water_level = 24;
	int64_t world_seed = 0;
	std::shared_ptr<const BiomeRegistry> biome_registry;
	Ref<FastNoiseLite> terrain_noise;
	Ref<FastNoiseLite> biome_noise;
	Ref<FastNoiseLite> dune_noise;
	Ref<FastNoiseLite> mountain_noise;
	Ref<FastNoiseLite> ocean_noise;
	Ref<FastNoiseLite> river_noise;
};

// Generation writes are ordered by ownership. A later layer can replace an
// earlier one, while lower-priority passes cannot overwrite established features.

enum class GenerationLayer : uint8_t {
	TERRAIN = 1,
	CARVING = 2,
	ORE = 3,
	WATER = 4,
	VEGETATION = 5,
	TREE_FOLIAGE = 6,
	TREE_TRUNK = 7
};

// Complete base column sampled once before terrain is filled. The definition
// pointer belongs to settings.biome_registry (or the static fallback registry).

struct ColumnGenerationData {
	const BiomeDefinition *definition = nullptr;
	int deep_rock_below_y = -32;
	bool surface_water = true;
	uint16_t surface_fill = voxel::block_ids::water;
	int32_t solid_fill_height = 255;
	int32_t surface_height = 0;
	int32_t height_offset = 0;
	float height_scale = 1.0f;
	int32_t water_level = 24;
	uint16_t biome_id = 0;
	int32_t subsurface_depth = 15;
	float climate_weight = 0.0f;
	float ocean_weight = 0.0f;
	float river_weight = 0.0f;
	bool trees_allowed = true;
	voxel::Block surface_block = 0;
	voxel::Block subsurface_block = 0;
	voxel::Block stone_block = 0;
	voxel::Block deep_stone_block = 0;
};

struct ChunkGenerationContext {
	Vector3i chunk_position;
	Chunk &chunk;
	const TerrainSettings &settings;
	std::vector<ColumnGenerationData> columns;
	bool columns_ready = false;
	std::vector<uint8_t> block_write_layers;

	ChunkGenerationContext(const Vector3i &p_chunk_position, Chunk &p_chunk, const TerrainSettings &p_settings);
	ColumnGenerationData &column(int x, int z);
	const ColumnGenerationData &column(int x, int z) const;
	ColumnGenerationData sample_column_at(int32_t world_x, int32_t world_z) const;
	int32_t surface_height_at(int32_t world_x, int32_t world_z) const;
	int32_t water_level_at(int32_t world_x, int32_t world_z) const;
	float climate_weight_at(int32_t world_x, int32_t world_z) const;
	bool trees_allowed_at(int32_t world_x, int32_t world_z) const;
	int32_t world_x(int local_x) const;
	int32_t world_y(int local_y) const;
	int32_t world_z(int local_z) const;
	bool write_block(int x, int y, int z, voxel::Block block, GenerationLayer layer);
};

class ChunkGenerationPass {
public:
	virtual ~ChunkGenerationPass() = default;
	virtual void apply(ChunkGenerationContext &context) const = 0;
};

// Passes execute in insertion order. Configure the pipeline before handing it
// to ChunkModelGenerator; generation workers then only read the pass list.

class ChunkGenerationPipeline {
	std::vector<std::shared_ptr<const ChunkGenerationPass>> _passes;

public:
	void add_pass(std::shared_ptr<const ChunkGenerationPass> pass);
	Chunk generate(const Vector3i &chunk_position, const TerrainSettings &settings) const;
	size_t pass_count() const { return _passes.size(); }
};

} // namespace godot

#endif

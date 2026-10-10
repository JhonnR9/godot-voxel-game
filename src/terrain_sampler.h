#ifndef TERRAIN_SAMPLER_H
#define TERRAIN_SAMPLER_H
#include "chunk_generation_pipeline.h"
#include <mutex>
#include <unordered_map>
#include <deque>
namespace godot {
// One cache per world/configuration. Immutable columns are shared across Y.

class TerrainColumnCache {
    std::mutex mutex;
    std::unordered_map<uint64_t, std::shared_ptr<const std::vector<ColumnGenerationData>>> entries;
    std::deque<uint64_t> order;
public:
    std::shared_ptr<const std::vector<ColumnGenerationData>> get(const Vector3i &pos, const TerrainSettings &settings);
};

class TerrainSampler {
public:
	static ColumnGenerationData sample(const TerrainSettings &settings, int32_t x, int32_t z);
	static voxel::Block block_at(const ColumnGenerationData &column, int32_t y);
};
} //namespace godot
#endif

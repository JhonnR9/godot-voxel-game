#ifndef CHUNK_GENERATOR_H
#define CHUNK_GENERATOR_H

#include "chunk_generation_pipeline.h"

namespace godot {

class BiomeSelectionPass final : public ChunkGenerationPass {
public:
	void apply(ChunkGenerationContext &context) const override;
};

class VegetationGenerationPass final : public ChunkGenerationPass {
	int64_t _seed;

public:
	explicit VegetationGenerationPass(int64_t p_seed) : _seed(p_seed) {}
	void apply(ChunkGenerationContext &context) const override;
};

class TerrainSurfacePass final : public ChunkGenerationPass {
public:
	void apply(ChunkGenerationContext &context) const override;
};

class CaveCarvingPass final : public ChunkGenerationPass {
public:
	void apply(ChunkGenerationContext &context) const override;
};

class OreGenerationPass final : public ChunkGenerationPass {
public:
	void apply(ChunkGenerationContext &context) const override;
};

class WaterFillPass final : public ChunkGenerationPass {
public:
	void apply(ChunkGenerationContext &context) const override;
};

// Small entry point retained for callers that want to generate a chunk with a
// configured, reusable pass pipeline.

class ChunkGenerator {
public:
	static Chunk generate(const Vector3i &chunk_pos, const TerrainSettings &settings,
			const ChunkGenerationPipeline &pipeline);
};

} // namespace godot

#endif

#include "spawn_tree_service.h"

namespace godot {

namespace {

inline uint64_t next_rand(uint64_t &state) {

	state += 0x9E3779B97F4A7C15ULL;
	uint64_t z = state;

	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;

	return z ^ (z >> 31);
}

inline uint64_t mix_bits(uint64_t value) {
	value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
	value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;

	return value ^ (value >> 31);
}

inline uint64_t leaf_hash(uint64_t seed, int32_t x, int32_t y, int32_t z) {
	uint64_t value = seed;

	value ^= static_cast<uint64_t>(static_cast<int64_t>(x)) * 0x9E3779B97F4A7C15ULL;
	value ^= static_cast<uint64_t>(static_cast<int64_t>(y)) * 0xC2B2AE3D27D4EB4FULL;
	value ^= static_cast<uint64_t>(static_cast<int64_t>(z)) * 0x165667B19E3779F9ULL;

	return mix_bits(value);
}

} // namespace

TreeGenerationPass::CandidateList TreeGenerationPass::_calculate_chunk_candidates(
		const ChunkGenerationContext &context, const int32_t cx, const int32_t cz) const {

	CandidateList candidates;

	const auto registry = context.settings.biome_registry ? context.settings.biome_registry : BiomeRegistry::defaults();
	const int max_candidates = registry->max_tree_candidates();
	if (max_candidates <= 0) return candidates;

	uint64_t rng = static_cast<uint64_t>(_seed) ^
			(static_cast<uint64_t>(static_cast<int64_t>(cx)) * 0x9E3779B97F4A7C15ULL) ^
			(static_cast<uint64_t>(static_cast<int64_t>(cz)) * 0xC2B2AE3D27D4EB4FULL);

	next_rand(rng);

	const int tree_count = static_cast<int>(next_rand(rng) % (max_candidates + 1));

	for (int i = 0; i < tree_count; ++i) {

		const int lx = static_cast<int>(next_rand(rng) % Chunk::SIZE_X);
		const int lz = static_cast<int>(next_rand(rng) % Chunk::SIZE_Z);

        const uint64_t height_roll=next_rand(rng);
        const int32_t wx=cx*Chunk::SIZE_X+lx, wz=cz*Chunk::SIZE_Z+lz;
        const auto column=context.sample_column_at(wx,wz);
        if(!column.trees_allowed || !column.definition) continue;
        const auto &profile=column.definition->trees;
        if(i>=profile.max_per_chunk) continue;
        const int32_t base_y=column.surface_height+1;
        if(base_y<=column.water_level) continue;
        const int trunk_h=profile.min_height+int(height_roll%(profile.max_height-profile.min_height+1));
        candidates.push_back({wx,wz,base_y,trunk_h,next_rand(rng),profile});

	}

	return candidates;
}

TreeGenerationPass::CandidateList TreeGenerationPass::_get_chunk_candidates(
		const ChunkGenerationContext &context, const int32_t cx, const int32_t cz) const {

	const ChunkKey key{cx, cz};

	{
		std::lock_guard<std::mutex> lock(_cache_mutex);
		const auto found = _candidate_cache.find(key);
		if (found != _candidate_cache.end()) {
			_least_to_most_recent.splice(_least_to_most_recent.end(), _least_to_most_recent, found->second.recency);
			return found->second.candidates;
		}
	}

	// Do expensive noise sampling outside the lock; concurrent requests may race
	// once, but only one immutable result is retained in the cache.
	CandidateList calculated = _calculate_chunk_candidates(context, cx, cz);

	std::lock_guard<std::mutex> lock(_cache_mutex);
	const auto raced = _candidate_cache.find(key);

	if (raced != _candidate_cache.end()) {
		_least_to_most_recent.splice(_least_to_most_recent.end(), _least_to_most_recent, raced->second.recency);
		return raced->second.candidates;
	}

	_least_to_most_recent.push_back(key);

	auto recency = std::prev(_least_to_most_recent.end());

	const auto inserted = _candidate_cache.emplace(key, CacheEntry{std::move(calculated), recency});

	while (_candidate_cache.size() > MAX_CACHED_CHUNKS) {

		const ChunkKey &oldest = _least_to_most_recent.front();
		_candidate_cache.erase(oldest);
		_least_to_most_recent.pop_front();

	}
	return inserted.first->second.candidates;
}

void TreeGenerationPass::apply(ChunkGenerationContext &context) const {
	if (context.settings.biome_registry && context.settings.biome_registry->max_tree_candidates() <= 0) {
		return;
	}

	const int32_t origin_x = context.chunk_position.x * Chunk::SIZE_X;
	const int32_t origin_y = context.chunk_position.y * Chunk::SIZE_Y;
	const int32_t origin_z = context.chunk_position.z * Chunk::SIZE_Z;


	auto put = [&](int32_t wx, int32_t wy, int32_t wz, voxel::Block block, bool replace_leaves, bool replace_ground = false) {
		const int32_t lx = wx - origin_x;
		const int32_t ly = wy - origin_y;
		const int32_t lz = wz - origin_z;
		if (lx < 0 || lx >= Chunk::SIZE_X || ly < 0 || ly >= Chunk::SIZE_Y || lz < 0 || lz >= Chunk::SIZE_Z) {
			return;
		}
		const voxel::Block existing = context.chunk.get_block(lx, ly, lz);
		if (voxel::is_air(existing) ||
				(replace_leaves && voxel::has_flag(voxel::default_block_flags(voxel::type(existing)), voxel::BLOCK_FLAG_CUTOUT)) ||
				(replace_leaves && voxel::has_flag(voxel::default_block_flags(voxel::type(existing)), voxel::BLOCK_FLAG_CROSSED)) ||
				(replace_ground && voxel::is_collidable(existing))) {
			const GenerationLayer layer = replace_leaves
					? GenerationLayer::TREE_TRUNK
					: GenerationLayer::TREE_FOLIAGE;
			context.write_block(lx, ly, lz, block, layer);
		}
	};



	for (int dcx = -1; dcx <= 1; ++dcx) {
		for (int dcz = -1; dcz <= 1; ++dcz) {
			const int32_t cx = context.chunk_position.x + dcx;
			const int32_t cz = context.chunk_position.z + dcz;
			for (const TreeCandidate &candidate : _get_chunk_candidates(context, cx, cz)) {

				const int32_t wx = candidate.x;
				const int32_t wz = candidate.z;
				const int32_t base_y = candidate.base_y;
				const int trunk_h = candidate.trunk_height;
				const uint64_t shape_seed = candidate.shape_seed;
                const int crown=candidate.profile.crown_radius;
                const auto log_block=voxel::make_block(candidate.profile.trunk);
                const auto leaves_block=voxel::make_block(candidate.profile.leaves);

				if (base_y + trunk_h + 2 < origin_y || base_y - 1 > origin_y + Chunk::MAX_Y) {
					continue;
				}

				if (wx + crown < origin_x || wx - crown >= origin_x + Chunk::SIZE_X ||
						wz + crown < origin_z || wz - crown >= origin_z + Chunk::SIZE_Z) {
					continue;
				}

				// Five tapered layers make a rounded crown instead of a flat box.
				if (candidate.profile.shape == TreeProfile::Shape::PINE) {
					// Stacked branch skirts narrow toward a single pointed leader.
					const int first = std::max(2, trunk_h / 3);
					const int layers = trunk_h - first + 2;
					for (int layer = 0; layer < layers; ++layer) {
						const int dy = first + layer;
						int radius = (crown * (layers - 1 - layer) + layers - 2) / (layers - 1);
						if (layer % 3 == 2 && radius > 1) --radius;
						for (int dx = -radius; dx <= radius; ++dx)
							for (int dz = -radius; dz <= radius; ++dz) {
								if (radius > 0 && ABS(dx) + ABS(dz) > radius + radius / 2) continue;
								put(wx + dx, base_y + dy, wz + dz, leaves_block, false);
							}
					}
					for (int dy = -1; dy < trunk_h; ++dy)
						put(wx, base_y + dy, wz, log_block, true, dy == -1);
					continue;
				}

				if (candidate.profile.shape == TreeProfile::Shape::PALM) {
					const int top = base_y + trunk_h - 1;
					put(wx, top + 1, wz, leaves_block, false);
					// Eight connected fronds rise at the crown and droop at the tips.
					for (int dx = -1; dx <= 1; ++dx) {
						for (int dz = -1; dz <= 1; ++dz) {
							if (dx == 0 && dz == 0) continue;
							const int reach = std::max(2, crown - int(leaf_hash(shape_seed, dx, 0, dz) % 2));
							for (int step = 1; step <= reach; ++step) {
								const int y = top + (step <= 2 ? 1 : 0) - (step == reach ? 1 : 0);
								const int px = wx + dx * step, pz = wz + dz * step;
								put(px, y, pz, leaves_block, false);
								if (step == reach) put(px, y + 1, pz, leaves_block, false);
								// Fill diagonal corners so each frond stays connected.
								if (dx && dz) put(px - dx, y, pz, leaves_block, false);
								if (step > 1 && step < reach) {
									put(px + dz, y, pz - dx, leaves_block, false);
									put(px - dz, y, pz + dx, leaves_block, false);
								}
							}
						}
					}
					for (int dy = -1; dy < trunk_h; ++dy)
						put(wx, base_y + dy, wz, log_block, true, dy == -1);
					continue;
				}
				for (int layer = 0; layer < 5; ++layer) {
					const int dy = trunk_h - 2 + layer;
					const int radius = (layer == 1 || layer == 2) ? crown : (layer == 4 ? 0 : std::max(1,crown-1));

					for (int dx = -radius; dx <= radius; ++dx) {

						for (int dz = -radius; dz <= radius; ++dz) {
							const int ax = ABS(dx);
							const int az = ABS(dz);

							if (radius == crown && ax == crown && az == crown) {
								continue;
							}
                            
							// Break up the outer silhouette with sparse, repeatable gaps.
							const bool outer_edge = radius == crown && (ax == crown || az == crown);

							if (outer_edge && (leaf_hash(shape_seed, wx + dx, base_y + dy, wz + dz) % 7 == 0)) {
								continue;
							}
							put(wx + dx, base_y + dy, wz + dz, leaves_block, false);
						}
					}
					// Add a few leaf tufts around the upper shoulder of the crown.
					if (layer == 3) {
						put(wx - crown, base_y + dy, wz, leaves_block, false);
						put(wx + crown, base_y + dy, wz, leaves_block, false);
						put(wx, base_y + dy, wz - crown, leaves_block, false);
						put(wx, base_y + dy, wz + crown, leaves_block, false);
					}
				}


				// Carry the trunk up through the crown, leaving only the top leaf cap above it.
				for (int dy = -1; dy < trunk_h + 2; ++dy) {
					put(wx, base_y + dy, wz, log_block, true, dy == -1);
				}
			}
		}
	}
}

} // namespace godot

#ifndef TREE_DECORATOR_H
#define TREE_DECORATOR_H

#include "chunk_generation_pipeline.h"
#include <cstdint>
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace godot {

// 100% deterministic and stateless generation:
// the trees for a chunk column (cx, cz) depend only on (seed, cx, cz) and the terrain height.
// When decorating chunk C, we calculate the trees for the 9 neighboring columns (3x3)
// and write only the blocks that fall within C. This ensures that trees on the boundary
// are complete and reappear identically if the chunk is unloaded and reloaded.

class TreeGenerationPass final : public ChunkGenerationPass {
public:
	explicit TreeGenerationPass(int64_t p_seed) : _seed(p_seed) {}

	void apply(ChunkGenerationContext &context) const override;

private:
	struct ChunkKey {
		int32_t x = 0;
		int32_t z = 0;
		bool operator==(const ChunkKey &other) const { return x == other.x && z == other.z; }
	};
    
	struct ChunkKeyHash {
		size_t operator()(const ChunkKey &key) const {
			uint64_t value = static_cast<uint32_t>(key.x);
			value = (value << 32) | static_cast<uint32_t>(key.z);
			value ^= value >> 30;
			value *= 0xBF58476D1CE4E5B9ULL;
			value ^= value >> 27;
			return static_cast<size_t>(value);
		}
	};

	struct TreeCandidate {
		int32_t x = 0;
		int32_t z = 0;
		int32_t base_y = 0;
		int trunk_height = 0;
		uint64_t shape_seed = 0;
		TreeProfile profile;
	};

	using CandidateList = std::vector<TreeCandidate>;
	using LruList = std::list<ChunkKey>;

	struct CacheEntry {
		CandidateList candidates;
		LruList::iterator recency;
	};

	CandidateList _calculate_chunk_candidates(const ChunkGenerationContext &context, int32_t cx, int32_t cz) const;
	CandidateList _get_chunk_candidates(const ChunkGenerationContext &context, int32_t cx, int32_t cz) const;

	int64_t _seed = 0;

	mutable std::mutex _cache_mutex;
	mutable std::unordered_map<ChunkKey, CacheEntry, ChunkKeyHash> _candidate_cache;
	mutable LruList _least_to_most_recent;

	static constexpr size_t MAX_CACHED_CHUNKS = 512;
};

} // namespace godot

#endif // TREE_DECORATOR_H

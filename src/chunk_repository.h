#ifndef CHUNK_REPOSITORY_H
#define CHUNK_REPOSITORY_H
#include "ChunkDiskRepository.h"
#include "voxel.h"
#include "chunk_mesh_builder.h"
#include "godot_cpp/templates/hash_map.hpp"
#include "godot_cpp/templates/hash_set.hpp"

#include <godot_cpp/classes/ref_counted.hpp>
#include <memory>
#include <mutex>

namespace godot {

struct Chunk;


class ChunkRepository : public RefCounted {
	GDCLASS(ChunkRepository, RefCounted)
	mutable std::mutex _mutex;
	HashMap<Vector3i, std::shared_ptr<Chunk>> _chunks;
	mutable std::mutex _dirty_chunks_mutex;
	HashSet<Vector3i> _dirty_chunks;

	mutable std::mutex _chunk_versions_mutex;
	HashMap<Vector3i, uint64_t> _chunk_versions;

	mutable std::mutex _edited_blocks_mutex;
	HashMap<Vector3i, HashMap<Vector3i, voxel::Block>> _edited_chunks;

	WorldModel world_model_;

	mutable std::mutex _dirty_regions_mutex;
	HashSet<Vector3i> _dirty_regions;

protected:

	static void _bind_methods();

public:
	void add_chunk(const Vector3i &p_pos, const std::shared_ptr<Chunk> &p_model);
	std::shared_ptr<Chunk> get_chunk(const Vector3i &p_pos);
    ChunkNeighbors get_neighbors_snapshot(const Vector3i &pos);
	bool contains_chunk(const Vector3i &p_pos);
	void remove_chunk(const Vector3i &p_pos);
	Vector<Vector3i> get_keys_snapshot();
	void clear_all();
	void set_block(const Vector3i &world_block_pos, voxel::Block block);
	HashSet<Vector3i> consume_dirty_chunks();
	uint64_t get_chunk_version(const Vector3i &p_pos);
	bool is_chunk_dirty(const Vector3i &p_pos);

	void set_world_model(const WorldModel &p_world);
	WorldModel get_world_model(uint64_t p_id);

	void save_edited_chunks_to_disk(Ref<ChunkDiskRepository> disk_repo);
	HashMap<Vector3i, HashMap<Vector3i, voxel::Block>> get_edited_chunks() const;
	voxel::Region take_region_edits(const Vector3i &region_pos);
	void merge_region_edits(const voxel::Region &region);
	voxel::Region get_edited_region(const Vector3i &p_region_pos) const;
	HashSet<Vector3i> get_dirty_regions();
	HashMap<Vector3i, voxel::Region> get_all_edited_regions() const;
private:
	void _update_dirty_chunks(const Vector3i &p_local_pos, const Vector3i &p_chunk_pos);
	void _apply_edited_blocks(const Vector3i &p_chunk_pos, const std::shared_ptr<Chunk> &p_model);
};
} // godot

#endif //CHUNK_REPOSITORY_H

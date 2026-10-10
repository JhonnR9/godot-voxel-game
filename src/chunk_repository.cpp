#include "chunk_repository.h"
#include "chunk_model.h"
#include "utils.h"

namespace godot {
uint64_t ChunkRepository::get_chunk_version(const Vector3i &p_pos) {
	std::lock_guard lock(_chunk_versions_mutex);
	if (_chunk_versions.has(p_pos)) {
		return _chunk_versions[p_pos];
	}

	return 0;
}

bool ChunkRepository::is_chunk_dirty(const Vector3i &p_pos) {
	std::lock_guard lock(_dirty_chunks_mutex);
	return _dirty_chunks.has(p_pos);
}

void ChunkRepository::_update_dirty_chunks(const Vector3i &p_local_pos, const Vector3i &p_chunk_pos) {
	std::lock_guard lock(_dirty_chunks_mutex);

	// Corner AO also depends on edge/corner-adjacent chunks.
	for (int z = (p_local_pos.z == Chunk::MIN_Z ? -1 : 0); z <= (p_local_pos.z == Chunk::MAX_Z ? 1 : 0); ++z)
		for (int y = (p_local_pos.y == Chunk::MIN_Y ? -1 : 0); y <= (p_local_pos.y == Chunk::MAX_Y ? 1 : 0); ++y)
			for (int x = (p_local_pos.x == Chunk::MIN_X ? -1 : 0); x <= (p_local_pos.x == Chunk::MAX_X ? 1 : 0); ++x)
				_dirty_chunks.insert(p_chunk_pos + Vector3i(x, y, z));

    { std::lock_guard region_lock(_dirty_regions_mutex);
        _dirty_regions.insert(voxel::chunk_to_region_coords(p_chunk_pos)); }
}

void ChunkRepository::_apply_edited_blocks(const Vector3i &p_chunk_pos, const std::shared_ptr<Chunk> &p_model) {
	{
		std::lock_guard lock(_edited_blocks_mutex);
		if (_edited_chunks.has(p_chunk_pos)) {
			const HashMap<Vector3i, voxel::Block> &blocks = _edited_chunks[p_chunk_pos];

			for (const auto &E : blocks) {
				const Vector3i &local_pos = E.key;
				const voxel::Block &block = E.value;
				p_model->set_block(local_pos.x, local_pos.y, local_pos.z, block);
			}
		}
	}
}

HashMap<Vector3i, voxel::Region> ChunkRepository::get_all_edited_regions() const {
	std::lock_guard lock(_edited_blocks_mutex);
	HashMap<Vector3i, voxel::Region> regions;

	for (const auto &E : _edited_chunks) {
		Vector3i chunk_pos = E.key;
		Vector3i region_pos = voxel::chunk_to_region_coords(chunk_pos);

		if (!regions.has(region_pos)) {
			regions[region_pos] = voxel::Region();
		}

		voxel::ChunkDelta delta;
		delta.delta = E.value;
		regions[region_pos].edited_chunks.insert(chunk_pos, delta);
	}

	return regions;
}

HashSet<Vector3i> ChunkRepository::get_dirty_regions() {
	std::lock_guard lock(_dirty_regions_mutex);
	HashSet<Vector3i> dirty = _dirty_regions;
	_dirty_regions.clear();
	return dirty;
}

void ChunkRepository::_bind_methods() {
}

void ChunkRepository::add_chunk(const Vector3i &p_pos, const std::shared_ptr<Chunk> &p_model) {
    _apply_edited_blocks(p_pos, p_model);
	{
		std::lock_guard lock(_mutex);
		_chunks[p_pos] = p_model;
	}


	{
		std::lock_guard lock(_chunk_versions_mutex);
		_chunk_versions[p_pos] = 1;
	}
}

std::shared_ptr<Chunk> ChunkRepository::get_chunk(const Vector3i &p_pos) {
	std::lock_guard lock(_mutex);
	if (_chunks.has(p_pos)) {
		return _chunks[p_pos];
	}

	return nullptr;
}

bool ChunkRepository::contains_chunk(const Vector3i &p_pos) {
	std::lock_guard lock(_mutex);
	return _chunks.has(p_pos);
}

void ChunkRepository::remove_chunk(const Vector3i &p_pos) {
	{
		std::lock_guard lock(_mutex);
		if (_chunks.has(p_pos))
			_chunks.erase(p_pos);
	}
	{
		std::lock_guard lock(_chunk_versions_mutex);
		if (_chunk_versions.has(p_pos)) {
			_chunk_versions.erase(p_pos);
		}
	}
}

Vector<Vector3i> ChunkRepository::get_keys_snapshot() {
	std::lock_guard lock(_mutex);

	Vector<Vector3i> keys;

	for (const auto &E : _chunks) {
		keys.push_back(E.key);
	}
	return keys;
}

void ChunkRepository::clear_all() {
	{
		std::lock_guard lock(_mutex);
		_chunks.clear();
	}

	{
		std::lock_guard lock(_chunk_versions_mutex);
		_chunk_versions.clear();
	}

	{
		std::lock_guard lock(_dirty_chunks_mutex);
		_dirty_chunks.clear();
	}

	{
		std::lock_guard lock(_edited_blocks_mutex);
		_edited_chunks.clear();
	}
	{
		std::lock_guard lock(_dirty_regions_mutex);
		_dirty_regions.clear();
	}
}

void ChunkRepository::set_block(const Vector3i &world_block_pos, voxel::Block block) {
    const Vector3i pos = voxel::block_to_chunk_coords(world_block_pos);
    const Vector3i local = voxel::block_to_chunk_local_block(world_block_pos);
    {
        std::lock_guard lock(_mutex);
        auto *stored = _chunks.getptr(pos);
        if (!stored || (*stored)->get_block(local.x, local.y, local.z) == block) return;
        // Only block data is read by workers; stage/flags remain main-thread state.
        if (stored->use_count() > 1) *stored = std::make_shared<Chunk>(**stored);
        (*stored)->set_block(local.x, local.y, local.z, block);
    }
    { std::lock_guard lock(_edited_blocks_mutex); _edited_chunks[pos][local] = block; }
    _update_dirty_chunks(local, pos);
    { std::lock_guard lock(_chunk_versions_mutex); _chunk_versions[pos]++; }
}

ChunkNeighbors ChunkRepository::get_neighbors_snapshot(const Vector3i &pos) {
    ChunkNeighbors n;
    std::lock_guard lock(_mutex);
    for (int z = -1; z <= 1; ++z)
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x)
                if (auto *chunk = _chunks.getptr(pos + Vector3i(x, y, z)))
                    n.halo[ChunkNeighbors::halo_index(x, y, z)] = *chunk;
    n.center = n.halo[ChunkNeighbors::halo_index(0, 0, 0)];
    n.right = n.halo[ChunkNeighbors::halo_index(1, 0, 0)];
    n.left = n.halo[ChunkNeighbors::halo_index(-1, 0, 0)];
    n.top = n.halo[ChunkNeighbors::halo_index(0, 1, 0)];
    n.bottom = n.halo[ChunkNeighbors::halo_index(0, -1, 0)];
    n.front = n.halo[ChunkNeighbors::halo_index(0, 0, 1)];
    n.back = n.halo[ChunkNeighbors::halo_index(0, 0, -1)];
    return n;
}

HashSet<Vector3i> ChunkRepository::consume_dirty_chunks() {
	std::lock_guard lock(_dirty_chunks_mutex);
	HashSet<Vector3i> dirty = _dirty_chunks;

	_dirty_chunks.clear();

	return dirty;
}

void ChunkRepository::set_world_model(const WorldModel &p_world) {
	world_model_ = p_world;
}

WorldModel ChunkRepository::get_world_model(uint64_t p_id) {
	return world_model_;
}

voxel::Region ChunkRepository::get_edited_region(const Vector3i &p_region_pos) const {
	std::lock_guard lock(_edited_blocks_mutex);
	voxel::Region region;

	for (const auto &E : _edited_chunks) {
		if (voxel::chunk_to_region_coords(E.key) == p_region_pos) {
			voxel::ChunkDelta delta;
			delta.delta = E.value;
			region.edited_chunks.insert(E.key, delta);
		}
	}
	return region;
}

HashMap<Vector3i, HashMap<Vector3i, voxel::Block>> ChunkRepository::get_edited_chunks() const {
	std::lock_guard lock(_edited_blocks_mutex);
	return _edited_chunks;
}

void ChunkRepository::merge_region_edits(const voxel::Region &region) {
	{
		std::lock_guard lock(_edited_blocks_mutex);

		for (const auto &chunk_entry : region.edited_chunks) {
			const Vector3i &chunk_pos = chunk_entry.key;
			for (const auto &block_entry : chunk_entry.value.delta) {
				_edited_chunks[chunk_pos][block_entry.key] = block_entry.value;

			}
		}

	}

	std::lock_guard chunks_lock(_mutex);
	std::lock_guard dirty_lock(_dirty_chunks_mutex);

	for (const auto &chunk_entry : region.edited_chunks) {
		const Vector3i &chunk_pos = chunk_entry.key;
		if (_chunks.has(chunk_pos)) {
			auto chunk = std::make_shared<Chunk>(*_chunks[chunk_pos]);
            _chunks[chunk_pos] = chunk;
			for (const auto &block_entry : chunk_entry.value.delta) {
				const Vector3i &local_pos = block_entry.key;
				const voxel::Block &block = block_entry.value;
				chunk->set_block(local_pos.x, local_pos.y, local_pos.z, block);
			}
            // Region edits invalidate face neighbours and AO diagonals too.
            for (int z = -1; z <= 1; ++z)
                for (int y = -1; y <= 1; ++y)
                    for (int x = -1; x <= 1; ++x)
                        _dirty_chunks.insert(chunk_pos + Vector3i(x,y,z));
            { std::lock_guard version_lock(_chunk_versions_mutex); _chunk_versions[chunk_pos]++; }
		}
	}
}

voxel::Region ChunkRepository::take_region_edits(const Vector3i &region_pos) {
	voxel::Region region;
	Vector<Vector3i> to_remove;

	{
		std::lock_guard lock(_edited_blocks_mutex);
		for (const auto &E : _edited_chunks) {
			if (voxel::chunk_to_region_coords(E.key) == region_pos) {
				voxel::ChunkDelta delta;
				delta.delta = E.value;
				region.edited_chunks.insert(E.key, delta);
				to_remove.push_back(E.key);
			}
		}

		for (const Vector3i &chunk_pos : to_remove) {
			_edited_chunks.erase(chunk_pos);
		}
	}

	return region;
}

void ChunkRepository::save_edited_chunks_to_disk(Ref<ChunkDiskRepository> disk_repo) {
	if (disk_repo.is_null()) return;

	std::lock_guard lock(_edited_blocks_mutex);

	HashMap<Vector3i, voxel::Region> regions_to_save;

	for (const auto &E : _edited_chunks) {
		Vector3i chunk_pos = E.key;
		Vector3i region_pos = voxel::chunk_to_region_coords(chunk_pos);

		if (!regions_to_save.has(region_pos)) {
			regions_to_save[region_pos] = voxel::Region();
		}

		voxel::ChunkDelta delta;
		delta.delta = E.value;
		regions_to_save[region_pos].edited_chunks[chunk_pos] = delta;
	}

	for (const auto &E : regions_to_save) {
		disk_repo->save_region_async(E.key, E.value);
	}
}
} //namespace godot

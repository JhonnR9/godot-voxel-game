
#include "chunk_streaming_manager.h"

#include "utils.h"

namespace godot {

void ChunkStreamingManager::_bind_methods() {
}

void ChunkStreamingManager::shift_chunks(const Vector3i &p_pos_center) {
    const int radius_xz = _stream_settings.world_radius;
    const int radius_y = _stream_settings.world_height;

    HashSet<Vector3i> new_active_chunks;
    const int estimated_size = (2 * radius_xz + 1) * (2 * radius_y + 1) * (2 * radius_xz + 1);
    new_active_chunks.reserve(estimated_size);

    for (int x = -radius_xz; x <= radius_xz; x++) {
       for (int z = -radius_xz; z <= radius_xz; z++) {
          for (int y = -radius_y; y <= radius_y; y++) {
             Vector3i chunk_pos = p_pos_center + Vector3i(x, y, z);

             if (chunk_pos.y < WORLD_MIN_CHUNK_Y || chunk_pos.y > WORLD_MAX_CHUNK_Y) {
                continue;
             }

             if (voxel::is_position_in_cylinder(chunk_pos, p_pos_center, radius_xz, radius_y)) {
                new_active_chunks.insert(chunk_pos);
             }
          }
       }
    }

    std::lock_guard lock(_chunks_mutex);
    Vector<Vector3i> chunks_to_remove;

    for (const Vector3i &pos : _active_chunks) {
       if (!new_active_chunks.has(pos)) {
          chunks_to_remove.push_back(pos);
       }
    }

    for (const Vector3i &pos : chunks_to_remove) {
       _active_chunks.erase(pos);
    }

    {
       std::lock_guard free_lock(_queue_free_chunks_mutex);
       for (const Vector3i &pos : chunks_to_remove) {
          _queue_free_chunks.insert(pos);
       }
    }

    _active_chunks = new_active_chunks;
}

void ChunkStreamingManager::rebuild_all_chunks(const Vector3i &p_pos_center) {
    const int radius_xz = _stream_settings.world_radius;
    const int radius_y = _stream_settings.world_height;

    _active_chunks.clear();

    for (int x = -radius_xz; x <= radius_xz; x++) {
       for (int z = -radius_xz; z <= radius_xz; z++) {
          for (int y = -radius_y; y <= radius_y; y++) {
             Vector3i chunk_pos = p_pos_center + Vector3i(x, y, z);

             if (chunk_pos.y < WORLD_MIN_CHUNK_Y || chunk_pos.y > WORLD_MAX_CHUNK_Y) {
                continue;
             }

             if (voxel::is_position_in_cylinder(chunk_pos, p_pos_center, radius_xz, radius_y)) {
                _active_chunks.insert(chunk_pos);
             }
          }
       }
    }
}

bool ChunkStreamingManager::is_chunk_active(const Vector3i &p_pos) {
	return _active_chunks.has(p_pos);
}

HashSet<Vector3i> ChunkStreamingManager::pop_queue_free_chunks() {
	std::lock_guard lock(_queue_free_chunks_mutex);
	HashSet<Vector3i> to_remove_chunks = _queue_free_chunks;
	_queue_free_chunks.clear();

	return to_remove_chunks;
}

HashSet<Vector3i> ChunkStreamingManager::get_active_chunks_snapshot() {
	std::lock_guard lock(_chunks_mutex);
	HashSet<Vector3i> chunks_snapshot = _active_chunks;

	return chunks_snapshot;
}
} //namespace godot

#ifndef CHUNK_REGION_ASYNC_LOADER_H
#define CHUNK_REGION_ASYNC_LOADER_H

#include "ChunkDiskRepository.h"
#include "godot_cpp/templates/hash_set.hpp"

#include <godot_cpp/classes/ref_counted.hpp>
#include <mutex>
#include <vector>

namespace godot {

struct RegionLoadJob {
	Vector3i pos;
	Ref<ChunkDiskRepository> repo;
	class ChunkRegionAsyncLoader *loader;
};

class ChunkRegionAsyncLoader : public RefCounted {
	GDCLASS(ChunkRegionAsyncLoader, RefCounted)
public:
    ~ChunkRegionAsyncLoader() override { reset(); }
    void reset();
	void set_repository(Ref<ChunkDiskRepository> p_repo) { repository = p_repo; }
	void queue_async_load_region(Vector3i p_pos);
	bool has_loaded_region(Vector3i p_pos);
	voxel::Region consume_loaded_region(Vector3i p_pos);
protected:
	static void _bind_methods();
private:
	Ref<ChunkDiskRepository> repository;
    std::vector<int64_t> tasks;
	std::mutex loaded_regions_mutex;
	HashMap<Vector3i, voxel::Region> loaded_regions;
	std::mutex loading_status_mutex;
	HashSet<Vector3i> loading_regions;
};
}
#endif

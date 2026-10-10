#ifndef CHUNKDISKREPOSITORY_H
#define CHUNKDISKREPOSITORY_H

#include "godot_cpp/templates/hash_set.hpp"
#include "utils.h"
#include "voxel.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <mutex>
#include <vector>

namespace godot {

class ChunkDiskRepository : public RefCounted {
	GDCLASS(ChunkDiskRepository, RefCounted)
protected:
	static void _bind_methods();

public:
    ~ChunkDiskRepository() override { wait_for_saves(); }
    void wait_for_saves();
	void set_current_world(int64_t p_id);
	int64_t get_current_world_id() const { return current_world_id; }

	void save_region(Vector3i region_pos, const voxel::Region &region);
	void save_region_async(Vector3i region_pos, const voxel::Region &region);
	voxel::Region load_region(Vector3i region_pos);
	Vector<Vector3i> get_all_saved_regions() const;

private:
    std::vector<int64_t> save_tasks;
	int64_t current_world_id = 0;
	String get_region_path(Vector3i region_pos) const;

	std::mutex regions_mutex;
	HashMap<Vector3i, voxel::Region> regions;
};
} //namespace godot
#endif // CHUNKDISKREPOSITORY_H

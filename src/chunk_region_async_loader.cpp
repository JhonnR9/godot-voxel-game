#include "chunk_region_async_loader.h"
#include <godot_cpp/classes/worker_thread_pool.hpp>

namespace godot {
void ChunkRegionAsyncLoader::_bind_methods() {
}

void ChunkRegionAsyncLoader::queue_async_load_region(Vector3i p_pos) {
    auto *pool = WorkerThreadPool::get_singleton();
    for (auto it = tasks.begin(); it != tasks.end();) {
        if (pool->is_task_completed(*it)) { pool->wait_for_task_completion(*it); it = tasks.erase(it); }
        else ++it;
    }
	{
		std::lock_guard lock(loading_status_mutex);
		if (loading_regions.has(p_pos)) return;
		loading_regions.insert(p_pos);
	}

	auto *job = new RegionLoadJob{ p_pos, repository, this };

	tasks.push_back(WorkerThreadPool::get_singleton()->add_native_task([](void *data) {
		auto *job = static_cast<RegionLoadJob *>(data);

		voxel::Region result = job->repo->load_region(job->pos);

		{
			std::lock_guard lock(job->loader->loaded_regions_mutex);
			job->loader->loaded_regions.insert(job->pos, result);
		}
		{
			std::lock_guard lock(job->loader->loading_status_mutex);
			job->loader->loading_regions.erase(job->pos);
		}

		delete job;
	}, job));
}

void ChunkRegionAsyncLoader::reset() {
    if (auto *pool = WorkerThreadPool::get_singleton())
        for (auto id : tasks) pool->wait_for_task_completion(id);
    tasks.clear();
    loaded_regions.clear();
    loading_regions.clear();
}

bool ChunkRegionAsyncLoader::has_loaded_region(Vector3i p_pos) {
	std::lock_guard lock(loaded_regions_mutex);
	return loaded_regions.has(p_pos);
}

voxel::Region ChunkRegionAsyncLoader::consume_loaded_region(Vector3i p_pos) {
	std::lock_guard lock(loaded_regions_mutex);
	voxel::Region r = loaded_regions[p_pos];
	loaded_regions.erase(p_pos);
	return r;
}
}

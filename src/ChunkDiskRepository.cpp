#include "ChunkDiskRepository.h"

#include "save_service.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

static constexpr int REGION_FILE_VERSION = 1;

void ChunkDiskRepository::_bind_methods() {}

void ChunkDiskRepository::wait_for_saves() {
    if (auto *pool = WorkerThreadPool::get_singleton())
        for (auto id : save_tasks) pool->wait_for_task_completion(id);
    save_tasks.clear();
}

void ChunkDiskRepository::set_current_world(int64_t p_id) {
    wait_for_saves();
	current_world_id = p_id;
	if (const String dir = SaveService::get_world_dir(p_id) + "/regions"; !DirAccess::dir_exists_absolute(dir)) {
		DirAccess::make_dir_recursive_absolute(dir);
	}
}

String ChunkDiskRepository::get_region_path(const Vector3i region_pos) const {
	return SaveService::get_world_dir(current_world_id) + "/regions/region_" + itos(region_pos.x) + "_" + itos(region_pos.y) + "_" + itos(region_pos.z) + ".json";
}

void ChunkDiskRepository::save_region(Vector3i region_pos, const voxel::Region &region) {
	String path = get_region_path(region_pos);

	Dictionary root;
	root["version"] = REGION_FILE_VERSION;

	Dictionary pos_d;
	pos_d["x"]	= region_pos.x;
	pos_d["y"]	= region_pos.y;
	pos_d["z"]	= region_pos.z;
	root["pos"] = pos_d;

	Array chunks_arr;
	for (const auto &E : region.edited_chunks) {
		const Vector3i &chunk_pos			 = E.key;
		const voxel::ChunkDelta &chunk_delta = E.value;

		Dictionary chunk_d;
		Dictionary c_pos_d;
		c_pos_d["x"]   = chunk_pos.x;
		c_pos_d["y"]   = chunk_pos.y;
		c_pos_d["z"]   = chunk_pos.z;
		chunk_d["pos"] = c_pos_d;

		Array blocks_arr;
		for (const auto &F : chunk_delta.delta) {
			const Vector3i &local_pos = F.key;
			const voxel::Block &block = F.value;

			Dictionary block_d;
			block_d["x"] = local_pos.x;
			block_d["y"] = local_pos.y;
			block_d["z"] = local_pos.z;
			block_d["v"] = (int)block;
			blocks_arr.push_back(block_d);
		}
		chunk_d["blocks"] = blocks_arr;
		chunks_arr.push_back(chunk_d);
	}
	root["chunks"] = chunks_arr;

	String text = JSON::stringify(root, "\t");

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null())
		return;

	file->store_string(text);
	file->flush();
	file->close();
}

void ChunkDiskRepository::save_region_async(Vector3i region_pos, const voxel::Region &region) {
    auto *pool = WorkerThreadPool::get_singleton();
    for (auto it = save_tasks.begin(); it != save_tasks.end();) {
        if (pool->is_task_completed(*it)) { pool->wait_for_task_completion(*it); it = save_tasks.erase(it); }
        else ++it;
    }
	struct RegionSaveJob {
		Vector3i pos;
		voxel::Region region;
		ChunkDiskRepository *repo;
	};

	auto *job = new RegionSaveJob{ region_pos, region, this };
	save_tasks.push_back(WorkerThreadPool::get_singleton()->add_native_task([](void *data) {
		auto *save_job = static_cast<RegionSaveJob *>(data);
		save_job->repo->save_region(save_job->pos, save_job->region);
		delete save_job;
	},
			job));
}

voxel::Region ChunkDiskRepository::load_region(Vector3i region_pos) {
	voxel::Region region;
	String path = get_region_path(region_pos);
	if (!FileAccess::file_exists(path))
		return region;

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null())
		return region;

	String text = file->get_as_text();
	file->close();

	Variant parsed = JSON::parse_string(text);
	if (parsed.get_type() != Variant::DICTIONARY)
		return region;

	Dictionary root = parsed;
	if (!root.has("version") || (int)root["version"] != REGION_FILE_VERSION)
		return region;
	if (!root.has("pos") || !root.has("chunks"))
		return region;

	Dictionary pos_d = root["pos"];
	if (!pos_d.has("x") || !pos_d.has("y") || !pos_d.has("z"))
		return region;

	Vector3i saved_pos((int)pos_d["x"], (int)pos_d["y"], (int)pos_d["z"]);
	if (saved_pos != region_pos)
		return region;

	Array chunks_arr = root["chunks"];
	for (int i = 0; i < chunks_arr.size(); i++) {
		Variant chunk_v = chunks_arr[i];
		if (chunk_v.get_type() != Variant::DICTIONARY)
			continue;
		Dictionary chunk_d = chunk_v;
		if (!chunk_d.has("pos") || !chunk_d.has("blocks"))
			continue;

		Dictionary c_pos_d = chunk_d["pos"];
		if (!c_pos_d.has("x") || !c_pos_d.has("y") || !c_pos_d.has("z"))
			continue;

		Vector3i c_pos((int)c_pos_d["x"], (int)c_pos_d["y"], (int)c_pos_d["z"]);

		voxel::ChunkDelta delta;
		Array blocks_arr = chunk_d["blocks"];
		for (int j = 0; j < blocks_arr.size(); j++) {
			Variant block_v = blocks_arr[j];
			if (block_v.get_type() != Variant::DICTIONARY)
				continue;
			Dictionary block_d = block_v;
			if (!block_d.has("x") || !block_d.has("y") || !block_d.has("z") || !block_d.has("v"))
				continue;

			Vector3i local_pos((int)block_d["x"], (int)block_d["y"], (int)block_d["z"]);
			const voxel::Block value = (voxel::Block)(int)block_d["v"];
			delta.delta.insert(local_pos, value);
		}
		region.edited_chunks.insert(c_pos, delta);
	}

	return region;
}

Vector<Vector3i> ChunkDiskRepository::get_all_saved_regions() const {
	Vector<Vector3i> regions_loaded;
	const String dir_path = SaveService::get_world_dir(current_world_id) + "/regions";

	if (const Ref<DirAccess> dir = DirAccess::open(dir_path); dir.is_valid()) {
		dir->list_dir_begin();

		String file_name = dir->get_next();
		while (!file_name.is_empty()) {
			if (file_name.begins_with("region_") && file_name.ends_with(".json")) {
				PackedStringArray parts = file_name
												  .trim_prefix("region_")
												  .trim_suffix(".json")
												  .split("_");

				if (parts.size() == 3) {
					regions_loaded.push_back(
							Vector3i(
									parts[0].to_int(),
									parts[1].to_int(),
									parts[2].to_int()));
				}
			}

			file_name = dir->get_next();
		}

		dir->list_dir_end();
	}

	return regions_loaded;
}
} //namespace godot

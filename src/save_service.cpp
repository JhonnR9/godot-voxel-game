#include "save_service.h"

#include "godot_cpp/classes/dir_access.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/json.hpp"
#include "godot_cpp/classes/project_settings.hpp"
#include "godot_cpp/classes/resource_uid.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/variant/packed_int64_array.hpp"

using namespace godot;

static constexpr int WORLD_MODEL_VERSION = 2;
static constexpr int OLDEST_SUPPORTED_WORLD_VERSION = 1;

SaveService *SaveService::singleton = nullptr;

SaveService::SaveService() {
	ERR_FAIL_COND_MSG(
		singleton != nullptr,
		"SaveService singleton cannot be two instance!"
	);

	singleton = this;
}

PackedInt64Array SaveService::get_saved_worlds_array() {
	PackedInt64Array result;

	const HashSet<int64_t> worlds = get_saved_worlds();

	result.resize(worlds.size());

	int64_t *data = result.ptrw();
	int index = 0;

	for (const int64_t id : worlds) {
		data[index++] = id;
	}

	return result;
}

Dictionary SaveService::load_world_model_dict(int64_t p_id) {
	const auto [seed, name, id] = load_world_model(p_id);

	Dictionary result;

	if (id != p_id) {
		return result;
	}

	result["id"] = id;
	result["seed"] = seed;
	result["name"] = name;

	return result;
}

SaveService::~SaveService() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

SaveService *SaveService::get_singleton() {
	return singleton;
}


HashSet<int64_t> SaveService::get_saved_worlds() {
	if (world_cache.dirty) {
		init_cache();
	}

	return world_cache.worlds;
}

WorldModel SaveService::load_world_model(int64_t p_id) {
	if (world_cache.dirty) {
		init_cache();
	}

	if (const WorldModel *cached = world_cache.models.getptr(p_id)) {
		return *cached;
	}

	const WorldModel model = load_world_model_from_disk(p_id);

	if (model.id == p_id) {
		world_cache.models.insert(p_id, model);
	}

	return model;
}

void SaveService::delete_world(int64_t p_id) {
	const String world_dir = get_world_dir(p_id);

	if (!DirAccess::dir_exists_absolute(world_dir)) {
		world_cache.clear();
		return;
	}

	const Ref<DirAccess> dir = DirAccess::open(world_dir);

	if (dir.is_null()) {
		ERR_PRINT("Failed to open world directory: " + world_dir);
		return;
	}

	dir->list_dir_begin();

	String file_name = dir->get_next();

	while (!file_name.is_empty()) {
		if (file_name != "." && file_name != "..") {
			const String path = world_dir.path_join(file_name);

			if (dir->current_is_dir()) {
				const Ref<DirAccess> subdir = DirAccess::open(path);

				if (subdir.is_valid()) {
					subdir->list_dir_begin();

					String sub_file = subdir->get_next();

					while (!sub_file.is_empty()) {
						if (sub_file != "." && sub_file != "..") {
							const String sub_path = path.path_join(sub_file);
							DirAccess::remove_absolute(sub_path);
						}

						sub_file = subdir->get_next();
					}

					subdir->list_dir_end();
				}

				DirAccess::remove_absolute(path);
			} else {
				DirAccess::remove_absolute(path);
			}
		}

		file_name = dir->get_next();
	}

	dir->list_dir_end();

	const Error err = DirAccess::remove_absolute(world_dir);

	if (err != OK) {
		ERR_PRINT(
			"Failed to delete world directory: " +
			world_dir +
			" error=" +
			itos(err)
		);
	}

	world_cache.clear();
}

WorldModel SaveService::load_world_model_from_disk(const int64_t p_id) {
	WorldModel model{};

	const String path = get_world_dir(p_id) + "/level.json";

	if (!FileAccess::file_exists(path)) {
		return model;
	}

	const Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);

	if (file.is_null()) {
		return model;
	}

	const String text = file->get_as_text();

	file->close();

	const Variant parsed = JSON::parse_string(text);

	if (parsed.get_type() != Variant::DICTIONARY) {
		return model;
	}

	const Dictionary root = parsed;

	if (!root.has("version") ||
		!root.has("id") ||
		!root.has("seed") ||
		!root.has("name")) {
		return model;
		}

	if (root["version"].get_type() != Variant::FLOAT ||
		root["id"].get_type() != Variant::STRING ||
		root["seed"].get_type() != Variant::FLOAT ||
		root["name"].get_type() != Variant::STRING) {
		return model;
		}

	const int64_t version =
		static_cast<int64_t>(double(root["version"]));

	const int64_t id =
		String(root["id"]).to_int();

	const int64_t seed =
		static_cast<int64_t>(double(root["seed"]));

	const String name = root["name"];

	if (version < OLDEST_SUPPORTED_WORLD_VERSION || version > WORLD_MODEL_VERSION) {
		return model;
	}

	if (id < 0) {
		return model;
	}

	if (seed < INT32_MIN || seed > INT32_MAX) {
		return model;
	}

	model.id = id;
	model.seed = static_cast<int32_t>(seed);
	model.name = name;

	return model;
}

String SaveService::get_world_dir(const int64_t p_id) {
	return "user://voxelcraft/worlds/" + String::num_int64(p_id);
}

Dictionary SaveService::load_world_section(const int64_t p_id, const String &p_section) const {
	Dictionary result;
	if (p_id < 0 || p_section.is_empty()) {
		return result;
	}

	const String path = get_world_dir(p_id) + "/level.json";
	const Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		return result;
	}
	const Variant parsed = JSON::parse_string(file->get_as_text());
	file->close();
	if (parsed.get_type() != Variant::DICTIONARY) {
		return result;
	}

	const Dictionary root = parsed;
	const int version = root.get("version", 0);
	if (version < OLDEST_SUPPORTED_WORLD_VERSION || version > WORLD_MODEL_VERSION) {
		return result;
	}
	const Dictionary sections = root.get("data", Dictionary());
	if (sections.has(p_section) && sections[p_section].get_type() == Variant::DICTIONARY) {
		result = sections[p_section];
	}
	return result;
}

bool SaveService::save_world_section(const int64_t p_id, const String &p_section, const Dictionary &p_values) {
	if (p_id < 0 || p_section.is_empty() || load_world_model(p_id).id != p_id) {
		return false;
	}

	const String path = get_world_dir(p_id) + "/level.json";
	const Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		return false;
	}
	const Variant parsed = JSON::parse_string(file->get_as_text());
	file->close();
	if (parsed.get_type() != Variant::DICTIONARY) {
		return false;
	}

	Dictionary root = parsed;
	Dictionary sections = root.get("data", Dictionary());
	Dictionary section_values = sections.get(p_section, Dictionary());
	section_values.merge(p_values, true);
	sections[p_section] = section_values;
	root["data"] = sections;
	root["version"] = WORLD_MODEL_VERSION;

	const String temp_path = path + String(".tmp");
	const Ref<FileAccess> output = FileAccess::open(temp_path, FileAccess::WRITE);
	if (output.is_null()) {
		return false;
	}
	output->store_string(JSON::stringify(root, "\t"));
	output->close();

	const String absolute_temp = ProjectSettings::get_singleton()->globalize_path(temp_path);
	const String absolute_path = ProjectSettings::get_singleton()->globalize_path(path);
	if (DirAccess::rename_absolute(absolute_temp, absolute_path) != OK) {
		DirAccess::remove_absolute(absolute_temp);
		return false;
	}
	return true;
}

Dictionary SaveService::load_user_settings(const String &p_section) const {
	Dictionary result;
	if (p_section.is_empty()) {
		return result;
	}

	const String path = "user://voxelcraft/settings.json";
	if (!FileAccess::file_exists(path)) {
		return result;
	}
	const Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	if (file.is_null()) {
		return result;
	}
	const Variant parsed = JSON::parse_string(file->get_as_text());
	file->close();
	if (parsed.get_type() != Variant::DICTIONARY) {
		return result;
	}
	const Dictionary sections = Dictionary(parsed).get("data", Dictionary());
	if (sections.has(p_section) && sections[p_section].get_type() == Variant::DICTIONARY) {
		result = sections[p_section];
	}
	return result;
}

bool SaveService::save_user_settings(const String &p_section, const Dictionary &p_values) const {
	if (p_section.is_empty()) {
		return false;
	}

	const String path = "user://voxelcraft/settings.json";
	Dictionary root;
	const Ref<FileAccess> input = FileAccess::file_exists(path)
			? FileAccess::open(path, FileAccess::READ)
			: Ref<FileAccess>();
	if (input.is_valid()) {
		const Variant parsed = JSON::parse_string(input->get_as_text());
		input->close();
		if (parsed.get_type() == Variant::DICTIONARY) {
			root = parsed;
		}
	}

	Dictionary sections = root.get("data", Dictionary());
	Dictionary values = sections.get(p_section, Dictionary());
	values.merge(p_values, true);
	sections[p_section] = values;
	root["version"] = 1;
	root["data"] = sections;

	if (DirAccess::make_dir_recursive_absolute("user://voxelcraft") != OK &&
			!DirAccess::dir_exists_absolute("user://voxelcraft")) {
		return false;
	}
	const String temp_path = path + String(".tmp");
	const Ref<FileAccess> output = FileAccess::open(temp_path, FileAccess::WRITE);
	if (output.is_null()) {
		return false;
	}
	output->store_string(JSON::stringify(root, "\t"));
	output->close();

	const String absolute_temp = ProjectSettings::get_singleton()->globalize_path(temp_path);
	const String absolute_path = ProjectSettings::get_singleton()->globalize_path(path);
	if (DirAccess::rename_absolute(absolute_temp, absolute_path) != OK) {
		DirAccess::remove_absolute(absolute_temp);
		return false;
	}
	return true;
}

int64_t SaveService::create_world(const int32_t p_seed, const String &p_name) {
	const WorldModel world_model{
		.seed = p_seed,
		.name = p_name,
		.id = ResourceUID::get_singleton()->create_id()
	};

	String world_dir = get_world_dir(world_model.id);

	if (DirAccess::make_dir_recursive_absolute(world_dir) != OK) {
		return 0;
	}

	const String path = world_dir + "/level.json";

	Dictionary root;
	root["version"] = WORLD_MODEL_VERSION;
	root["id"] = String::num_int64(world_model.id);
	root["seed"] = world_model.seed;
	root["name"] = world_model.name;
	root["data"] = Dictionary();

	const String text = JSON::stringify(root, "\t");

	const Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);

	if (file.is_null()) {
		return 0;
	}

	file->store_string(text);
	file->close();

	world_cache.clear();

	return world_model.id;
}

void SaveService::init_cache() {
	world_cache.worlds.clear();
	world_cache.models.clear();

	const Ref<DirAccess> dir =
		DirAccess::open("user://voxelcraft/worlds");

	if (dir.is_valid()) {
		dir->list_dir_begin();

		String file_name = dir->get_next();

		while (!file_name.is_empty()) {
			if (dir->current_is_dir() && file_name != "." && file_name != "..") {
				if (const int64_t world_id = file_name.to_int(); world_id >= 0) {
					world_cache.worlds.insert(world_id);
				}
			}

			file_name = dir->get_next();
		}

		dir->list_dir_end();
	}

	world_cache.dirty = false;
}

void SaveService::invalidate_world(int64_t p_id) {
	world_cache.models.erase(p_id);
}

void SaveService::invalidate_world_cache() {
	world_cache.clear();
}

void SaveService::_bind_methods() {
	ClassDB::bind_method(
		D_METHOD("get_saved_worlds"),
		&SaveService::get_saved_worlds_array
	);

	ClassDB::bind_method(
		D_METHOD("load_world_model", "id"),
		&SaveService::load_world_model_dict
	);
	ClassDB::bind_method(D_METHOD("load_world_section", "id", "section"), &SaveService::load_world_section);
	ClassDB::bind_method(D_METHOD("save_world_section", "id", "section", "values"), &SaveService::save_world_section);
	ClassDB::bind_method(D_METHOD("load_user_settings", "section"), &SaveService::load_user_settings);
	ClassDB::bind_method(D_METHOD("save_user_settings", "section", "values"), &SaveService::save_user_settings);

	ClassDB::bind_method(
		D_METHOD("delete_world", "id"),
		&SaveService::delete_world
	);

	ClassDB::bind_static_method(
		SaveService::get_class_static(),
		D_METHOD("get_world_dir", "id"),
		&SaveService::get_world_dir
	);

	ClassDB::bind_method(
		D_METHOD("create_world", "seed", "name"),
		&SaveService::create_world
	);

	ClassDB::bind_method(
		D_METHOD("invalidate_world", "id"),
		&SaveService::invalidate_world
	);

	ClassDB::bind_method(
		D_METHOD("invalidate_world_cache"),
		&SaveService::invalidate_world_cache
	);
}

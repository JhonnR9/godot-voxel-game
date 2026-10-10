#ifndef WORLD_H
#define WORLD_H

#include "chunk_generator.h"
#include "chunk_mesh_async_generator.h"
#include "chunk_model.h"
#include "chunk_model_generator.h"
#include "chunk_node.h"
#include "chunk_pool.h"
#include "chunk_region_async_loader.h"
#include "chunk_repository.h"
#include "chunk_streaming_manager.h"
#include "spawn_tree_service.h"

#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <memory>

namespace godot {

class VoxelAPI : public Node3D {
	GDCLASS(VoxelAPI, Node3D)

public:
	// Godot lifecycle
	void _ready() override;
	void _process(double delta) override;
	void _exit_tree() override;

	void break_block(const Vector3 &world_pos);
	void set_block(const Vector3 &p_world_pos, const voxel::Block &p_block) const;
	bool is_water_at(const Vector3 &p_world_pos) const;
	bool is_ocean_at(const Vector3 &p_world_pos) const;
	int32_t get_block_type_at(const Vector3 &p_world_pos) const;
	void set_focus_node(Node3D *p_node);
	void set_biome_registry_path(const String &path);
	String get_biome_registry_path() const { return _biome_registry_path; }
	Dictionary sample_terrain_column(const Vector2i &position) const;
	static Dictionary validate_biome_registry(const Dictionary &data);
	void set_focus_position(Vector3 p_pos);
	void create_new_world(int32_t p_seed, const String &p_name);
	void start_world(int64_t p_id);
	void start_preview(int32_t p_seed);
	bool is_initial_loading() const { return _is_initializing; }
	int get_initial_loading_total_chunks() const { return _initial_loading_chunks.size(); }
	int get_initial_loading_ready_chunks() const;
	float get_initial_loading_progress() const;
	void set_render_settings(const Dictionary &p_settings);
	Dictionary get_render_settings() const;
	static Dictionary get_default_render_settings();
	Dictionary get_pipeline_stats() const;
    void set_pipeline_settings(const Dictionary &settings);
	static void set_default_render_settings(const Dictionary &p_settings);

protected:
	static void _bind_methods();

private:
	Ref<ChunkPool> _chunk_pool;
	Ref<ChunkRepository> _chunk_repository;
	Ref<ChunkStreamingManager> _chunk_stream_manager;
	Ref<ChunkModelGenerator> _model_generator;
	Ref<ChunkMeshAsyncGenerator> _mesh_generator;
	Ref<ChunkDiskRepository> _disk_repository;
	Ref<ChunkRegionAsyncLoader> _region_loader;
	std::shared_ptr<const ChunkGenerationPipeline> _generation_pipeline;
	int64_t _world_seed = 0;
    std::shared_ptr<TerrainColumnCache> _column_cache;

	void save_world_final() const;
	void _set_day_hour(double hour) const;
	double _get_day_hour() const;

	// Optimization
	int _world_radius = 4;
	int _world_height = 3;
	bool _distance_fog_enabled = true;
	int _distance_fog_start_percent = 65;
	int _cache_radius = _world_radius + 3;
	int _diameter     = (_cache_radius * 2) + 1; //  (2 * R + 1).
	int _prewarm_chunk_pool = (_diameter * _diameter) * _world_height;
    double _mesh_finalize_budget_ms = 2.0;
    double _last_finalize_ms = 0.0;
    double _max_finalize_ms = 0.0;
    bool _streaming_changed = false;
    HashSet<Vector3i> _mesh_candidates;
    double _cleanup_timer = 0.0;

	std::shared_ptr<const BiomeRegistry> _biome_registry;
	String _biome_registry_path = "res://data/biome_registry.json";
	int _water_level = 24;
	TerrainSettings _make_terrain_settings() const;
	// Terrain settings
	int _terrain_base_height = 24;
	float _terrain_amplitude = 9.0f;

	Ref<FastNoiseLite> _terrain_noise;
	Ref<FastNoiseLite> _biome_noise;
	Ref<FastNoiseLite> _dune_noise;
	Ref<FastNoiseLite> _mountain_noise;
	Ref<FastNoiseLite> _ocean_noise;
	Ref<FastNoiseLite> _river_noise;

	// Player position control
	Vector3i _last_focos_position;

	// World management
	void save_world() const;
	void _queue_async_generate_chunk(Vector3i p_pos) const;
	ChunkNeighbors _get_neighbors_for(Vector3i p_pos) const;
	void _setup_noises();
	void _flow_water_into(const Vector3i &p_target) const;
	void _init_chunks();
	void _remove_chunk(ChunkNode *p_chunk_node);
	void _update_visible_chunks();
	void _cleanup_far_chunks() const;
	void _clear_world();

	void _finalize_chunk(const MeshResult &res);
	void _try_build_mesh_with_neighbors(Vector3i p_pos) const;

	void _process_models();
	void _process_meshes(const Vector3i &p_pos);
	void _rebuild_chunk(const Vector3i &pos) const;
	bool _is_high_priority(const Vector3i &pos, bool dirty) const;
	void _ensure_region_loaded_for_chunk(const Vector3i &chunk_pos);

	Node3D *_focus_node       = nullptr;
	Vector3 _focus_manual_pos = Vector3(0, 0, 0);
	bool _use_manual_pos      = true;
	Vector3 _get_current_focus_position() const;

	bool _preview_mode = false;
	// World state
	HashMap<Vector3i, ChunkNode *> _rendered_chunks;
	HashMap<Vector3i, voxel::Region> _region_cache;
	HashSet<Vector3i> _pending_region_loads;
	HashSet<Vector3i> _initial_loading_chunks;
	Vector3i _previous_player_chunk_pos;

	void _queue_region_load(const Vector3i &region_pos);
	void _update_region_streaming(const Vector3i &current_chunk_pos, const Vector3i &previous_chunk_pos);
	void _process_loaded_regions();
	void _unload_region(const Vector3i &region_pos);

	void _setup_generation_pipeline(int64_t p_seed);
	void _apply_render_settings_fields(const Dictionary &p_settings, bool p_refresh_active_chunks);
	void _apply_distance_fog() const;
	static Dictionary _normalize_render_settings(const Dictionary &p_settings);
	static void _apply_vsync_setting(const Dictionary &p_settings);

	bool _is_initializing{false};
	void _update_initial_loading_status();
};
} // namespace godot

#endif // WORLD_H

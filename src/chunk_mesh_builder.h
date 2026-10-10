//
// Created by jhone on 13/05/2026.
//

#ifndef CHUNK_MESH_BUILDER_H
#define CHUNK_MESH_BUILDER_H

#include "chunk_model.h"
#include "voxel_mesher.h"
#include "godot_cpp/templates/hash_map.hpp"

#include <godot_cpp/classes/array_mesh.hpp>
#include <memory>
#include <array>

namespace godot {

struct ChunkNeighbors {
	// Full one-chunk halo for corner AO (including diagonal neighbours).
	std::array<std::shared_ptr<Chunk>, 27> halo{};
	static constexpr int halo_index(int x, int y, int z) {
		return (x + 1) + (y + 1) * 3 + (z + 1) * 9;
	}
	std::shared_ptr<Chunk> center;

	std::shared_ptr<Chunk> right;
	std::shared_ptr<Chunk> left;

	std::shared_ptr<Chunk> top;
	std::shared_ptr<Chunk> bottom;

	std::shared_ptr<Chunk> front;
	std::shared_ptr<Chunk> back;
};

struct ChunkMeshMetadata {
    std::array<std::array<int, 6>, 1024> layers{};
    std::array<Color, 1024> tints;
    ChunkMeshMetadata() { tints.fill(Color(1, 1, 1, 1)); }
};

struct ChunkMeshData {
    Array opaque_arrays;
    Array transparent_arrays;
    bool has_opaque = false;
    bool has_transparent = false;
};

class ChunkMeshBuilder {
	VoxelMesher opaque_mesher;
    std::vector<uint8_t> mask, visited;
    std::vector<std::array<float, 4>> ao;
	VoxelMesher transparent_mesher;
	PackedVector3Array light_positions;
	PackedVector3Array selection_positions;
	VoxelMesher &_get_mesher(voxel::Block block);
	void _add_faces(const ChunkNeighbors &neighbors, CubeFace face);
	void _add_crossed_plant_faces(const ChunkNeighbors &neighbors);
	int _get_tex_layer(const CubeFace &face, uint16_t type);
	Color _get_block_tint(uint16_t type) const;

    std::shared_ptr<const ChunkMeshMetadata> metadata;

	static voxel::Block _get_block(const ChunkNeighbors &neighbors, int x, int y, int z);
	static bool _is_face_visible(const ChunkNeighbors &neighbors, int x, int y, int z, voxel::Block current_block);
	static bool _is_crossed_plant(voxel::Block block);

public:
	explicit ChunkMeshBuilder(std::shared_ptr<const ChunkMeshMetadata> p_metadata);
    static std::shared_ptr<const ChunkMeshMetadata> load_metadata();
	ChunkMeshData build(const ChunkNeighbors &neighbors);
	PackedVector3Array get_selection_positions() const { return selection_positions; }
	PackedVector3Array get_light_positions() const { return light_positions; }

	static bool _is_air(const ChunkNeighbors &n, int x, int y, int z);

	PackedVector3Array get_last_collision_faces() const {
		return opaque_mesher.get_collision_faces();
	}
};
} // namespace godot

#endif

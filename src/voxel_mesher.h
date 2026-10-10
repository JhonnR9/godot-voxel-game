#ifndef VOXEL_MESHER_H
#define VOXEL_MESHER_H

#include "voxel_types.h"
#include <array>

#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>

namespace godot {

struct ChunkNeighbors;

class VoxelMesher {
public:

	void clear();

	void add_quad(
			const Vector3 &v0,
			const Vector3 &v1,
			const Vector3 &v2,
			const Vector3 &v3,
			const Vector3 &normal,
			int tex_layer,
			const Vector2 &tile_scale,
			bool swap_uvs = false,
			bool collidable = true,
			const Color &tint = Color(1.0f, 1.0f, 1.0f, 1.0f),
			const std::array<float, 4> &ao = {1.0f, 1.0f, 1.0f, 1.0f});

	[[nodiscard]] Array build_arrays() const;
	[[nodiscard]] PackedVector3Array get_collision_faces() const;

private:
	PackedVector3Array _vertices;
	PackedVector3Array _normals;
	PackedVector2Array _uvs;
	PackedInt32Array _indices;
	PackedFloat32Array _tex_layer;
	PackedColorArray _colors;
	PackedVector3Array _collision_faces;
};
} // namespace godot

#endif

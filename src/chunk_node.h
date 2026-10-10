#ifndef CHUNK_H
#define CHUNK_H

#include "chunk_mesh_builder.h"
#include <godot_cpp/classes/box_shape3d.hpp>

#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <vector>
#include <godot_cpp/variant/packed_vector3_array.hpp>

namespace godot {

class World;

class ChunkNode : public MeshInstance3D {
	GDCLASS(ChunkNode, MeshInstance3D)

public:

	void  _enter_tree() override;

protected:
	static void _bind_methods();

private:
	Vector3i pos;
	StaticBody3D *_selection_body = nullptr;
	PackedVector3Array _selection_positions;
	Ref<BoxShape3D> _selection_box;
	StaticBody3D *_static_body = nullptr;
	CollisionShape3D *_collision_shape = nullptr;

	Ref<ConcavePolygonShape3D> _shape;
	Ref<Material> _material;
	Ref<Material> _water_material;
	std::vector<OmniLight3D *> _lights;
    PackedVector3Array _light_positions;

	void _setup();

public:
	void set_collision_faces(const PackedVector3Array&collision_faces);
	void set_selection_positions(const PackedVector3Array &positions);
	void set_light_positions(const PackedVector3Array &positions);
	Ref<Material> get_material() {
		return _material;
	}
	Ref<Material> get_water_material() {
		return _water_material;
	}
	void disable();
	void enable();
};

}

#endif

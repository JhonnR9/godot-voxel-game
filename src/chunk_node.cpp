#include "chunk_node.h"

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture2d_array.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>


namespace godot {

void ChunkNode::_setup() {
    if (_shape.is_null()) {
        _shape.instantiate();
    }

    if (!_collision_shape) {
        _collision_shape = memnew(CollisionShape3D);
        _collision_shape->set_name("CollisionShape3D");
    }

    if (!_static_body) {
        _static_body = memnew(StaticBody3D);
        _static_body->set_name("StaticBody3D");

        _static_body->add_child(_collision_shape);
        add_child(_static_body);
    }


    // Pool reuse and collision updates retain already configured materials.
    if (_material.is_valid() && _water_material.is_valid()) return;

    Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://shaders/chunk.gdshader");

    Ref<Material> override_material;

    if (shader.is_valid()) {
        Ref<ShaderMaterial> mat;
        mat.instantiate();
        mat->set_shader(shader);

        Ref<Texture2DArray> tex_array = ResourceLoader::get_singleton()->load(
            "res://textures/block_array.tres"
        );

        if (tex_array.is_valid()) {
            mat->set_shader_parameter("albedo_array", tex_array);
            // Generated lookup follows atlas reordering when textures are added.
            mat->set_shader_parameter("iron_ore_layer", voxel::texture_layer_from_name("iron_ore"));
            mat->set_shader_parameter("diamond_ore_layer", voxel::texture_layer_from_name("diamond_ore"));
            mat->set_shader_parameter("torch_layer", voxel::texture_layer_from_name("torch"));
            override_material = mat;
        } else {
            ERR_PRINT("Error: mismatched or invalid atlas array in res://textures/block_array.tres");

            Ref<StandardMaterial3D> fallback_mat;
            fallback_mat.instantiate();
            fallback_mat->set_albedo(Color(0.5f, 0.75f, 1.0f));
            override_material = fallback_mat;
        }
    } else {
        ERR_PRINT("Error: main shader not found");
        Ref<StandardMaterial3D> fallback_mat;
        fallback_mat.instantiate();
        fallback_mat->set_albedo(Color(0.5f, 0.75f, 1.0f));
        override_material = fallback_mat;
    }

    _material = override_material;

    Ref<Shader> water_shader = ResourceLoader::get_singleton()->load("res://shaders/water.gdshader");
    if (water_shader.is_valid()) {
        Ref<ShaderMaterial> water_mat;
        water_mat.instantiate();
        water_mat->set_shader(water_shader);
        _water_material = water_mat;
    } else {
        ERR_PRINT("Error: water shader not found");
        _water_material = _material;
    }

    // Surface materials are selected per mesh surface; a global override would
    // force opaque terrain and cutout foliage through the water material too.
    set_material_override(Ref<Material>());
}

void ChunkNode::set_collision_faces( const PackedVector3Array &collision_faces) {
    _setup();
    _shape->set_faces(collision_faces);

    if (_collision_shape->get_shape() != _shape) {
        _collision_shape->set_shape(_shape);
        _collision_shape->set_debug_color(Color(235, 0, 38));
    }
}

// One selection body per chunk, with shared shapes and no per-object nodes.
// Layer 2 is queried by interaction rays; movement uses terrain layer 1.
void ChunkNode::set_selection_positions(const PackedVector3Array &positions) {
    if (positions == _selection_positions) return;
    _selection_positions = positions;
    if (!_selection_body) {
        _selection_body = memnew(StaticBody3D);
        _selection_body->set_name("ObjectSelection");
        _selection_body->set_collision_layer(2);
        _selection_body->set_collision_mask(0);
        add_child(_selection_body);
        _selection_box.instantiate();
        _selection_box->set_size(Vector3(1, 1, 1));
    }
    const PackedInt32Array owners = _selection_body->get_shape_owners();
    for (int i = 0; i < owners.size(); ++i) _selection_body->remove_shape_owner(owners[i]);
    for (int i = 0; i < positions.size(); ++i) {
        const uint32_t owner = _selection_body->create_shape_owner(_selection_body);
        _selection_body->shape_owner_set_transform(owner, Transform3D(Basis(), positions[i] + Vector3(0.5, 0.5, 0.5)));
        _selection_body->shape_owner_add_shape(owner, _selection_box);
    }
    // Ray hit shape indices match this ordered list of local voxel origins.
    _selection_body->set_meta("voxel_selection_positions", positions);
}

void ChunkNode::set_light_positions(const PackedVector3Array &positions) {
    if (positions == _light_positions) return;
    _light_positions = positions;
    for (OmniLight3D *light : _lights) {
        remove_child(light);
        memdelete(light);
    }
    _lights.clear();
    for (int i = 0; i < positions.size(); ++i) {
        OmniLight3D *light = memnew(OmniLight3D);
        light->set_name("Light");
        light->set_position(positions[i]);
		light->set_color(Color(1.0f, 1.0f, 1.0f));
		light->set_param(Light3D::PARAM_ENERGY, 1.0f);
        light->set_param(Light3D::PARAM_RANGE, 8.0f);
        light->set_shadow(true);
        light->set_enable_distance_fade(true);
        light->set_distance_fade_begin(32.0f);
        light->set_distance_fade_length(8.0f);
        light->set_distance_fade_shadow(24.0f);
        add_child(light);
        _lights.push_back(light);
    }
}

void ChunkNode::disable() {
    set_light_positions(PackedVector3Array());
    set_selection_positions(PackedVector3Array());
    set_mesh(Ref<Mesh>());

    if (_shape.is_valid()) {
        _shape->set_faces(PackedVector3Array());
    }

    set_visible(false);
    set_process(false);
    set_position(Vector3());
}

void ChunkNode::enable() {
    set_visible(true);
    set_process(true);
}

void ChunkNode::_enter_tree() {
    MeshInstance3D::_enter_tree();
    _setup();
}

void ChunkNode::_bind_methods() {
}

} // namespace godot

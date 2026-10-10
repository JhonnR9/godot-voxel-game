#ifndef PLAYER_H
#define PLAYER_H

#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>

namespace godot {

class VoxelAPI;

class Player : public CharacterBody3D {
	GDCLASS(Player, CharacterBody3D)

private:
	Camera3D *_camera = nullptr;
	Node3D *_head = nullptr;
	VoxelAPI* _world{nullptr};

	float _speed = 10.0f;
	float _mouse_sensitivity = 0.002f;

	float _pitch = 0.0f;
	float _yaw = 0.0f;

	float _jump_force = 9.0f;
	enum MoveMode { WALK,
		FLY,
		NOCLIP };
	MoveMode _current_mode = WALK;
	float _double_tap_timer = 0.0f;
	const float DOUBLE_TAP_TIME = 0.3f;
	float _fly_speed = 15.0f;
	float _noclip_speed = 25.0f;
	int _selected_block_id= 0;

protected:
	static void _bind_methods();

public:
	Player() = default;

	void _ready() override;
	void _physics_process(double delta) override;
	void _unhandled_input(const Ref<InputEvent> &event) override;
	Dictionary raycast_block(float distance);
};

} // namespace godot

#endif

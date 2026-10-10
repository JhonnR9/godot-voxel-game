extends Node

const SETTINGS_PATH := "user://audio_settings.cfg"
const DEFAULT_VOLUME := 100.0
var volumes := {
	"Master": DEFAULT_VOLUME,
	"Music": DEFAULT_VOLUME,
	"SFX": DEFAULT_VOLUME,
}
var save_timer: Timer
var has_unsaved_changes := false

func _ready() -> void:
	_ensure_bus("Music")
	_ensure_bus("SFX")
	save_timer = Timer.new()
	save_timer.one_shot = true
	save_timer.wait_time = 0.4
	save_timer.process_mode = Node.PROCESS_MODE_ALWAYS
	save_timer.timeout.connect(_save_settings)
	add_child(save_timer)
	_load_settings()

	for bus_name in volumes:
		_apply_volume(bus_name)

func _ensure_bus(bus_name: String) -> void:
	if AudioServer.get_bus_index(bus_name) >= 0:
		return
	AudioServer.add_bus()
	var bus_index := AudioServer.bus_count - 1
	AudioServer.set_bus_name(bus_index, bus_name)
	AudioServer.set_bus_send(bus_index, "Master")

func get_volume(bus_name: String) -> float:
	return float(volumes.get(bus_name, DEFAULT_VOLUME))

func set_volume(bus_name: String, value: float) -> void:
	if not volumes.has(bus_name):
		return
	volumes[bus_name] = clampf(value, 0.0, 100.0)
	_apply_volume(bus_name)
	has_unsaved_changes = true
	save_timer.start()

func _apply_volume(bus_name: String) -> void:
	var bus_index := AudioServer.get_bus_index(bus_name)
	if bus_index < 0:
		return
	var amount := get_volume(bus_name) / 100.0
	AudioServer.set_bus_mute(bus_index, amount <= 0.0)
	if amount > 0.0:
		AudioServer.set_bus_volume_db(bus_index, linear_to_db(amount))

func _load_settings() -> void:
	var config := ConfigFile.new()
	if config.load(SETTINGS_PATH) != OK:
		return

	for bus_name in volumes:
		volumes[bus_name] = clampf(float(config.get_value("audio", bus_name, DEFAULT_VOLUME)), 0.0, 100.0)

func _save_settings() -> void:
	if not has_unsaved_changes:
		return
	var config := ConfigFile.new()

	for bus_name in volumes:
		config.set_value("audio", bus_name, volumes[bus_name])
	var error := config.save(SETTINGS_PATH)
	if error != OK:
		push_warning("Could not save audio settings: %s" % error_string(error))
	else:
		has_unsaved_changes = false

func _exit_tree() -> void:
	_save_settings()

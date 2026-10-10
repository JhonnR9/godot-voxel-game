extends Node

const SETTINGS_PATH := "user://display_settings.cfg"
const STANDARD_RESOLUTIONS: Array[Vector2i] = [
	Vector2i(800, 600),
	Vector2i(1024, 576),
	Vector2i(1024, 768),
	Vector2i(1152, 864),
	Vector2i(1280, 720),
	Vector2i(1280, 800),
	Vector2i(1280, 1024),
	Vector2i(1366, 768),
	Vector2i(1440, 900),
	Vector2i(1600, 900),
	Vector2i(1600, 1200),
	Vector2i(1680, 1050),
	Vector2i(1920, 1080),
	Vector2i(1920, 1200),
	Vector2i(2560, 1080),
	Vector2i(2560, 1440),
	Vector2i(2560, 1600),
	Vector2i(3440, 1440),
	Vector2i(3840, 2160),
]

var graphics = preload("res://scripts/graphics_settings.gd").new()

var windowed_resolution := Vector2i(1280, 720)
var window_mode := DisplayServer.WINDOW_MODE_FULLSCREEN

func _ready() -> void:
	add_child(graphics)
	window_mode = DisplayServer.window_get_mode()
	if not is_fullscreen():
		windowed_resolution = DisplayServer.window_get_size()
	_load_settings()
	var available := get_available_resolutions()
	if not available.has(windowed_resolution) and not available.is_empty():
		windowed_resolution = available.back()
	call_deferred("_apply_saved_settings")

func get_available_resolutions() -> Array[Vector2i]:
	var screen := DisplayServer.window_get_current_screen()
	var screen_size := DisplayServer.screen_get_size(screen)
	if screen_size.x <= 0 or screen_size.y <= 0:
		screen_size = Vector2i(1920, 1080)

	var resolutions: Array[Vector2i] = []

	for size in STANDARD_RESOLUTIONS:
		if size.x <= screen_size.x and size.y <= screen_size.y:
			resolutions.append(size)
	if screen_size not in resolutions:
		resolutions.append(screen_size)
	if windowed_resolution.x > 0 and windowed_resolution.y > 0 and windowed_resolution not in resolutions:
		if windowed_resolution.x <= screen_size.x and windowed_resolution.y <= screen_size.y:
			resolutions.append(windowed_resolution)
	resolutions.sort_custom(func(a: Vector2i, b: Vector2i) -> bool:
		if a.x * a.y == b.x * b.y:
			return a.x < b.x
		return a.x * a.y < b.x * b.y
	)
	return resolutions

func set_windowed_resolution(size: Vector2i) -> void:
	if size.x <= 0 or size.y <= 0:
		return
	windowed_resolution = size
	if not is_fullscreen():
		_apply_windowed_resolution()
	_save_settings()

func set_fullscreen(enabled: bool) -> void:
	window_mode = DisplayServer.WINDOW_MODE_FULLSCREEN if enabled else DisplayServer.WINDOW_MODE_WINDOWED
	DisplayServer.window_set_mode(window_mode)
	if not enabled:
		DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, false)
		_apply_windowed_resolution()
	_save_settings()

func is_fullscreen() -> bool:
	return window_mode == DisplayServer.WINDOW_MODE_FULLSCREEN or window_mode == DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN

func _apply_saved_settings() -> void:
	DisplayServer.window_set_mode(window_mode)
	if not is_fullscreen():
		DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, false)
		_apply_windowed_resolution()

func _apply_windowed_resolution() -> void:
	DisplayServer.window_set_size(windowed_resolution)
	var screen := DisplayServer.window_get_current_screen()
	var screen_position := DisplayServer.screen_get_position(screen)
	var screen_size := DisplayServer.screen_get_size(screen)
	if screen_size.x > 0 and screen_size.y > 0:
		var centered_offset := Vector2i(Vector2(screen_size - windowed_resolution) * 0.5)
		DisplayServer.window_set_position(screen_position + centered_offset)

func _load_settings() -> void:
	var config := ConfigFile.new()
	if config.load(SETTINGS_PATH) != OK:
		return
	var saved_resolution: Vector2i = config.get_value("display", "windowed_resolution", windowed_resolution)
	if saved_resolution.x > 0 and saved_resolution.y > 0:
		windowed_resolution = saved_resolution
	var screen_size := DisplayServer.screen_get_size(DisplayServer.window_get_current_screen())
	if screen_size.x > 0 and screen_size.y > 0 and (
			windowed_resolution.x > screen_size.x or windowed_resolution.y > screen_size.y):
		windowed_resolution = screen_size
	var saved_fullscreen := bool(config.get_value("display", "fullscreen", is_fullscreen()))
	window_mode = DisplayServer.WINDOW_MODE_FULLSCREEN if saved_fullscreen else DisplayServer.WINDOW_MODE_WINDOWED

func _save_settings() -> void:
	var config := ConfigFile.new()
	config.set_value("display", "windowed_resolution", windowed_resolution)
	config.set_value("display", "fullscreen", is_fullscreen())
	var error := config.save(SETTINGS_PATH)
	if error != OK:
		push_warning("Could not save display settings: %s" % error_string(error))

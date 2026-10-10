extends Control

const GraphicsOptions = preload("res://scripts/graphics_settings.gd")

signal close_requested

@export var world_api: VoxelAPI

@onready var render_distance: HSlider = $Center/Panel/Margin/Content/Categories/Graphics/Options/RenderDistanceRow/RenderDistance
@onready var render_distance_value: Label = $Center/Panel/Margin/Content/Categories/Graphics/Options/RenderDistanceRow/RenderDistanceValue
@onready var vertical_distance: HSlider = $Center/Panel/Margin/Content/Categories/Graphics/Options/VerticalDistanceRow/VerticalDistance
@onready var vertical_distance_value: Label = $Center/Panel/Margin/Content/Categories/Graphics/Options/VerticalDistanceRow/VerticalDistanceValue
@onready var distance_fog: CheckButton = $Center/Panel/Margin/Content/Categories/Graphics/Options/DistanceFogRow/DistanceFog
@onready var fog_start: HSlider = $Center/Panel/Margin/Content/Categories/Graphics/Options/FogStartRow/FogStart
@onready var fog_start_value: Label = $Center/Panel/Margin/Content/Categories/Graphics/Options/FogStartRow/FogStartValue
@onready var vsync: OptionButton = $Center/Panel/Margin/Content/Categories/Display/Options/VSyncRow/VSync
@onready var graphics_hint: Label = $Center/Panel/Margin/Content/Categories/Graphics/Options/GraphicsHint
@onready var antialiasing: OptionButton = $Center/Panel/Margin/Content/Categories/Graphics/Options/AntialiasingRow/Antialiasing
@onready var upscaling: OptionButton = $Center/Panel/Margin/Content/Categories/Graphics/Options/UpscalingRow/Upscaling
@onready var ssao: CheckButton = $Center/Panel/Margin/Content/Categories/Graphics/Options/SsaoRow/Ssao
@onready var master_volume: HSlider = $Center/Panel/Margin/Content/Categories/Audio/Options/MasterVolumeRow/MasterVolume
@onready var music_volume: HSlider = $Center/Panel/Margin/Content/Categories/Audio/Options/MusicVolumeRow/MusicVolume
@onready var sfx_volume: HSlider = $Center/Panel/Margin/Content/Categories/Audio/Options/SfxVolumeRow/SfxVolume
@onready var window_mode: OptionButton = $Center/Panel/Margin/Content/Categories/Display/Options/WindowModeRow/WindowMode
@onready var resolution: OptionButton = $Center/Panel/Margin/Content/Categories/Display/Options/ResolutionRow/Resolution

var available_resolutions: Array[Vector2i] = []
var _caller: Control
var _return_focus: Control

func open_from(caller: Control, api: VoxelAPI = null, return_focus: Control = null) -> void:
	_caller = caller
	_return_focus = return_focus
	configure(api)
	if is_instance_valid(_caller):
		_caller.hide()
	show()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	$Center/Panel/Margin/Content/Categories.get_tab_bar().grab_focus()

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	vsync.add_item("Off", 0)
	vsync.add_item("On", 1)
	antialiasing.add_item("Off", GraphicsOptions.AA_OFF)
	if DisplaySettings.graphics.supports_fxaa():
		antialiasing.add_item("FXAA", GraphicsOptions.AA_FXAA)
	antialiasing.add_item("MSAA 2×", GraphicsOptions.AA_MSAA_2X)
	antialiasing.add_item("MSAA 4×", GraphicsOptions.AA_MSAA_4X)
	upscaling.add_item("Native", GraphicsOptions.UPSCALE_NATIVE)
	if DisplaySettings.graphics.supports_fsr1():
		upscaling.add_item("FSR 1 Quality", GraphicsOptions.UPSCALE_FSR1_QUALITY)
		upscaling.add_item("FSR 1 Balanced", GraphicsOptions.UPSCALE_FSR1_BALANCED)
		upscaling.add_item("FSR 1 Performance", GraphicsOptions.UPSCALE_FSR1_PERFORMANCE)
	if DisplaySettings.graphics.supports_fsr2():
		upscaling.add_item("FSR 2 Quality", GraphicsOptions.UPSCALE_FSR2_QUALITY)
		upscaling.add_item("FSR 2 Balanced", GraphicsOptions.UPSCALE_FSR2_BALANCED)
		upscaling.add_item("FSR 2 Performance", GraphicsOptions.UPSCALE_FSR2_PERFORMANCE)
	graphics_hint.visible = not DisplaySettings.graphics.supports_fsr2()
	upscaling.disabled = not DisplaySettings.graphics.supports_fsr1()
	if upscaling.disabled:
		upscaling.tooltip_text = "FSR 1 and FSR 2 need the Forward+ renderer."
	ssao.disabled = not DisplaySettings.graphics.supports_ssao()
	if ssao.disabled:
		ssao.tooltip_text = "SSAO requires Forward+ or Compatibility; voxel AO remains active."
	window_mode.add_item("Windowed", 0)
	window_mode.add_item("Fullscreen", 1)
	render_distance.value_changed.connect(_on_render_slider_changed)
	vertical_distance.value_changed.connect(_on_vertical_slider_changed)
	distance_fog.toggled.connect(_on_fog_toggled)
	fog_start.value_changed.connect(_on_fog_start_changed)
	vsync.item_selected.connect(_apply_settings)
	antialiasing.item_selected.connect(func(_index: int): DisplaySettings.graphics.set_aa_mode(antialiasing.get_selected_id()))
	upscaling.item_selected.connect(func(_index: int): DisplaySettings.graphics.set_upscale_mode(upscaling.get_selected_id()))
	ssao.toggled.connect(DisplaySettings.graphics.set_ssao_enabled)
	master_volume.value_changed.connect(_on_master_volume_changed)
	music_volume.value_changed.connect(_on_music_volume_changed)
	sfx_volume.value_changed.connect(_on_sfx_volume_changed)
	window_mode.item_selected.connect(_on_window_mode_selected)
	resolution.item_selected.connect(_on_resolution_selected)
	$Center/Panel/Margin/Content/Buttons/Back.pressed.connect(_on_back_pressed)
	_load_settings()

func configure(api: VoxelAPI = null) -> void:
	world_api = api
	if is_node_ready():
		_load_settings()

func _load_settings() -> void:
	var settings: Dictionary
	if world_api and is_instance_valid(world_api):
		settings = world_api.get_render_settings()
	else:
		settings = VoxelAPI.get_default_render_settings()
	render_distance.set_block_signals(true)
	vertical_distance.set_block_signals(true)
	render_distance.value = int(settings.get("render_distance", 4))
	vertical_distance.value = int(settings.get("vertical_render_distance", 3))
	distance_fog.set_pressed_no_signal(bool(settings.get("distance_fog_enabled", true)))
	fog_start.set_block_signals(true)
	fog_start.value = int(settings.get("distance_fog_start_percent", 65))
	fog_start.set_block_signals(false)
	fog_start.editable = distance_fog.button_pressed
	render_distance.set_block_signals(false)
	vertical_distance.set_block_signals(false)
	_update_value_labels()
	_select_id(vsync, 1 if bool(settings.get("vsync", true)) else 0)
	_select_id(antialiasing, DisplaySettings.graphics.aa_mode)
	_select_id(upscaling, DisplaySettings.graphics.upscale_mode)
	ssao.set_pressed_no_signal(DisplaySettings.graphics.ssao_enabled and DisplaySettings.graphics.supports_ssao())
	master_volume.set_block_signals(true)
	music_volume.set_block_signals(true)
	sfx_volume.set_block_signals(true)
	master_volume.value = AudioSettings.get_volume("Master")
	music_volume.value = AudioSettings.get_volume("Music")
	sfx_volume.value = AudioSettings.get_volume("SFX")
	master_volume.set_block_signals(false)
	music_volume.set_block_signals(false)
	sfx_volume.set_block_signals(false)
	_update_audio_value_labels()
	_load_display_settings()

func _select_id(option: OptionButton, value: int) -> void:
	for index in range(option.item_count):
		if option.get_item_id(index) == value:
			option.select(index)
			return
	option.select(0)

func _on_render_slider_changed(_value: float) -> void:
	_update_value_labels()
	_apply_settings()

func _on_vertical_slider_changed(_value: float) -> void:
	_update_value_labels()
	_apply_settings()

func _on_fog_toggled(_enabled: bool) -> void:
	fog_start.editable = distance_fog.button_pressed
	_apply_settings()

func _on_fog_start_changed(_value: float) -> void:
	_update_value_labels()
	_apply_settings()

func _update_value_labels() -> void:
	render_distance_value.text = "%d chunks" % int(render_distance.value)
	vertical_distance_value.text = "%d chunks" % int(vertical_distance.value)
	fog_start_value.text = "%d%%" % int(fog_start.value)

func _on_master_volume_changed(value: float) -> void:
	AudioSettings.set_volume("Master", value)
	_update_audio_value_labels()

func _on_music_volume_changed(value: float) -> void:
	AudioSettings.set_volume("Music", value)
	_update_audio_value_labels()

func _on_sfx_volume_changed(value: float) -> void:
	AudioSettings.set_volume("SFX", value)
	_update_audio_value_labels()

func _update_audio_value_labels() -> void:
	$Center/Panel/Margin/Content/Categories/Audio/Options/MasterVolumeRow/Value.text = "%d%%" % int(master_volume.value)
	$Center/Panel/Margin/Content/Categories/Audio/Options/MusicVolumeRow/Value.text = "%d%%" % int(music_volume.value)
	$Center/Panel/Margin/Content/Categories/Audio/Options/SfxVolumeRow/Value.text = "%d%%" % int(sfx_volume.value)

func _load_display_settings() -> void:
	available_resolutions = DisplaySettings.get_available_resolutions()
	resolution.clear()
	var selected_index := 0

	for index in range(available_resolutions.size()):
		var resolution_size := available_resolutions[index]
		resolution.add_item("%d × %d" % [resolution_size.x, resolution_size.y], index)
		if resolution_size == DisplaySettings.windowed_resolution:
			selected_index = index
	resolution.select(selected_index)
	resolution.disabled = DisplaySettings.is_fullscreen()
	resolution.tooltip_text = "Fullscreen uses the monitor's native resolution."
	window_mode.select(1 if DisplaySettings.is_fullscreen() else 0)

func _on_window_mode_selected(id: int) -> void:
	DisplaySettings.set_fullscreen(id == 1)
	resolution.disabled = id == 1

func _on_resolution_selected(index: int) -> void:
	if index < 0 or index >= available_resolutions.size():
		return
	DisplaySettings.set_windowed_resolution(available_resolutions[index])

func _apply_settings(_index: int = -1) -> void:
	var settings := {
		"render_distance": int(render_distance.value),
		"vertical_render_distance": int(vertical_distance.value),
		"distance_fog_enabled": distance_fog.button_pressed,
		"distance_fog_start_percent": int(fog_start.value),
		"vsync": vsync.get_selected_id() == 1
	}
	if world_api and is_instance_valid(world_api):
		world_api.set_render_settings(settings)
	else:
		VoxelAPI.set_default_render_settings(settings)

func _input(event: InputEvent) -> void:
	if is_visible_in_tree() and event.is_action_pressed("ui_cancel") and not event.is_echo():
		get_viewport().set_input_as_handled()
		_on_back_pressed()

func _on_back_pressed() -> void:
	hide()
	if is_instance_valid(_caller):
		_caller.show()
	if is_instance_valid(_return_focus) and _return_focus.is_visible_in_tree():
		_return_focus.grab_focus()
	_caller = null
	_return_focus = null
	close_requested.emit()

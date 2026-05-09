extends Control

var scroll_speed: float = 40.0
var current_dir: Vector2 = Vector2.ZERO
var target_dir: Vector2 = Vector2.ZERO

@onready var parallax_2d: Parallax2D = $Parallax2D

@onready var master_slider = $VBoxContainer/MasterSlider
@onready var music_slider = $VBoxContainer/MusicSlider
@onready var sfx_slider = $VBoxContainer/SfxSlider
@onready var back_button = $HBoxContainer/BackButton

@onready var sfx_hover = preload("res://ui/menu_hover.mp3")
@onready var sfx_click = preload("res://ui/menu_click.mp3")

var _base_button_scales: Dictionary = {}
var _target_button_scales: Dictionary = {}

func _ready() -> void:
	master_slider.value = SettingsManager.master_volume
	music_slider.value = SettingsManager.music_volume
	sfx_slider.value = SettingsManager.sfx_volume
	
	master_slider.value_changed.connect(_on_master_changed)
	music_slider.value_changed.connect(_on_music_changed)
	sfx_slider.value_changed.connect(_on_sfx_changed)
	back_button.pressed.connect(_on_back_pressed)

	_setup_button_animations()

	target_dir = Vector2(randf_range(-1.0, 1.0), randf_range(-1.0, 1.0)).normalized()
	current_dir = target_dir
	
	var drift_timer := Timer.new()
	drift_timer.wait_time = 4.0
	drift_timer.autostart = true
	drift_timer.timeout.connect(_pick_new_direction)
	add_child(drift_timer)

func _setup_button_animations() -> void:
	for button in [back_button]:
		_base_button_scales[button.name] = button.scale
		_target_button_scales[button.name] = button.scale
		
		if not button.mouse_entered.is_connected(_on_button_hover):
			button.mouse_entered.connect(_on_button_hover.bind(button))
			
		if not button.mouse_exited.is_connected(_on_button_exit):
			button.mouse_exited.connect(_on_button_exit.bind(button))
			
		if not button.pressed.is_connected(_on_button_pressed_animate):
			button.pressed.connect(_on_button_pressed_animate.bind(button))

func _on_button_hover(button: BaseButton) -> void:
	button.pivot_offset = button.size / 2.0
	var tween = create_tween().set_trans(Tween.TRANS_QUINT).set_ease(Tween.EASE_OUT)
	_target_button_scales[button.name] = _base_button_scales[button.name] * 1.1
	tween.tween_property(button, "scale", _target_button_scales[button.name], 0.15)
	
	_play_sfx(sfx_hover)

func _on_button_exit(button: BaseButton) -> void:
	button.pivot_offset = button.size / 2.0
	var tween = create_tween().set_trans(Tween.TRANS_QUINT).set_ease(Tween.EASE_OUT)
	_target_button_scales[button.name] = _base_button_scales[button.name]
	tween.tween_property(button, "scale", _target_button_scales[button.name], 0.15)

func _on_button_pressed_animate(button: BaseButton) -> void:
	button.pivot_offset = button.size / 2.0
	var tween = create_tween().set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tween.tween_property(button, "scale", _base_button_scales[button.name] * 0.9, 0.05)
	tween.tween_property(button, "scale", _target_button_scales[button.name], 0.15)
	
	_play_sfx(sfx_click)

func _play_sfx(stream: AudioStream, start_offset: float = 0.62) -> void:
	var player = AudioStreamPlayer.new()
	player.stream = stream
	player.bus = "SFX"
	player.process_mode = Node.PROCESS_MODE_ALWAYS 
	add_child(player)
	player.play(start_offset)
	player.finished.connect(player.queue_free)

func _process(delta: float) -> void:
	if not visible:
		return
		
	current_dir = current_dir.lerp(target_dir, delta * 0.5)
	
	var step = current_dir * scroll_speed * delta
	parallax_2d.scroll_offset.x = wrapf(parallax_2d.scroll_offset.x + step.x, 0.0, parallax_2d.repeat_size.x)
	parallax_2d.scroll_offset.y = wrapf(parallax_2d.scroll_offset.y + step.y, 0.0, parallax_2d.repeat_size.y)

func _pick_new_direction() -> void:
	target_dir = Vector2(randf_range(-1.0, 1.0), randf_range(-1.0, 1.0)).normalized()

func _on_master_changed(value: float) -> void:
	SettingsManager.update_volume("Master", value)

func _on_music_changed(value: float) -> void:
	SettingsManager.update_volume("Music", value)

func _on_sfx_changed(value: float) -> void:
	SettingsManager.update_volume("SFX", value) 

func _on_back_pressed() -> void:
	self.hide()

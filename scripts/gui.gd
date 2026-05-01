extends Control

var scroll_speed: float = 40.0
var current_dir: Vector2 = Vector2.ZERO
var target_dir: Vector2 = Vector2.ZERO

@onready var parallax_2d: Parallax2D = $Parallax2D

@onready var start_button = $VBoxContainer/StartButton
@onready var options_button = $VBoxContainer/OptionsButton
@onready var quit_button = $VBoxContainer/QuitButton
@onready var options_menu = $Options
@onready var title_sprite = $VBoxContainer/Convallaria

@onready var sfx_hover = preload("res://audio/menu_hover.mp3")
@onready var sfx_click = preload("res://audio/menu_click.mp3")

var _base_button_scales: Dictionary = {}
var _target_button_scales: Dictionary = {}

func _ready() -> void:
	get_tree().paused = false
	var menu_music = load(Data.MUSIC["menu"])
	AudioManager.play_music(menu_music)

	_setup_button_animations()
	
	if title_sprite:
		_setup_title_animation()

	target_dir = Vector2(randf_range(-1.0, 1.0), randf_range(-1.0, 1.0)).normalized()
	current_dir = target_dir
	
	var drift_timer := Timer.new()
	drift_timer.wait_time = 4.0
	drift_timer.autostart = true
	drift_timer.timeout.connect(_pick_new_direction)
	add_child(drift_timer)

func _setup_title_animation() -> void:
	var shadow = Sprite2D.new()
	shadow.texture = title_sprite.texture
	shadow.modulate = Color(0, 0, 0, 0.5)
	shadow.position = Vector2(1, 2)
	shadow.show_behind_parent = true
	title_sprite.add_child(shadow)
	
	# 2. Create the subtle floating animation
	var start_y = title_sprite.position.y
	var tween = create_tween().set_loops().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	tween.tween_property(title_sprite, "position:y", start_y - 10.0, 1.5)
	tween.tween_property(title_sprite, "position:y", start_y, 1.5)

func _setup_button_animations() -> void:
	for button in [start_button, options_button, quit_button]:
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
	add_child(player)
	player.play(start_offset)
	player.finished.connect(player.queue_free)

func _process(delta: float) -> void:
	current_dir = current_dir.lerp(target_dir, delta * 0.5)
	
	var step = current_dir * scroll_speed * delta
	parallax_2d.scroll_offset.x = wrapf(parallax_2d.scroll_offset.x + step.x, 0.0, parallax_2d.repeat_size.x)
	parallax_2d.scroll_offset.y = wrapf(parallax_2d.scroll_offset.y + step.y, 0.0, parallax_2d.repeat_size.y)

func _pick_new_direction() -> void:
	target_dir = Vector2(randf_range(-1.0, 1.0), randf_range(-1.0, 1.0)).normalized()

func _on_start_button_pressed() -> void:
	TransitionManager.change_scene("res://scenes/world.tscn")

func _on_options_button_pressed() -> void:
	options_menu.show()

func _on_quit_button_pressed() -> void:
	get_tree().quit()

extends CanvasLayer

var scroll_speed: float = 40.0
var current_dir: Vector2 = Vector2.ZERO
var target_dir: Vector2 = Vector2.ZERO

@onready var stats_container = %StatsCointainer
@onready var return_button = %ReturnButton
@onready var parallax = $Parallax2D

@onready var sfx_hover = preload("res://ui/menu_hover.mp3")
@onready var sfx_click = preload("res://ui/menu_click.mp3")
@onready var sfx_win = preload("res://ui/win.mp3")
@onready var pixel_font = preload("res://ui/fonts/PixelifySans-VariableFont_wght.ttf")

var _base_button_scales: Dictionary = {}
var _target_button_scales: Dictionary = {}

func _ready() -> void:
	if AudioManager.has_method("stop_music"):
		AudioManager.stop_music()
	_play_sfx(sfx_win, 0.0) 

	if return_button and not return_button.pressed.is_connected(_on_return_button_pressed):
		return_button.pressed.connect(_on_return_button_pressed)

	_setup_button_animations()

	var victory_bonus = 1000
	Data.coins += victory_bonus
	
	var kills = Data.player_data.get("kill_count", 0)
	var time = Data.player_data.get("time_survived", 0.0)
	
	var mins = int(time) / 60
	var secs = int(time) % 60
	var time_string = "%02d:%02d" % [mins, secs]

	for child in stats_container.get_children():
		child.queue_free()
		
	_add_stat_row("Time Survived:", time_string, Color.WHITE)
	_add_stat_row("Enemies Slain:", str(kills), Color.WHITE)
	_add_stat_row("Victory Bonus:", "+ " + str(victory_bonus) + " Gold", Color(1.0, 0.84, 0.0))
	
	target_dir = Vector2(randf_range(-1.0, 1.0), randf_range(-1.0, 1.0)).normalized()
	current_dir = target_dir
	
	var drift_timer := Timer.new()
	drift_timer.wait_time = 4.0
	drift_timer.autostart = true
	drift_timer.timeout.connect(_pick_new_direction)
	add_child(drift_timer)
	
func _add_stat_row(title: String, value: String, value_color: Color = Color.WHITE) -> void:
	var dark_green_outline = Color(0.06, 0.2, 0.1)
	
	var title_label = Label.new()
	title_label.text = title
	title_label.add_theme_font_override("font", pixel_font)
	title_label.add_theme_font_size_override("font_size", 24)
	title_label.add_theme_color_override("font_color", Color(0.7, 0.95, 0.7)) 
	title_label.add_theme_color_override("font_outline_color", dark_green_outline)
	title_label.add_theme_constant_override("outline_size", 6)
	stats_container.add_child(title_label)
	
	var value_label = Label.new()
	value_label.text = value
	value_label.add_theme_font_override("font", pixel_font)
	value_label.add_theme_font_size_override("font_size", 24)
	value_label.add_theme_color_override("font_color", value_color)
	value_label.add_theme_color_override("font_outline_color", dark_green_outline)
	value_label.add_theme_constant_override("outline_size", 6)
	stats_container.add_child(value_label)

func _process(delta: float) -> void:
	if parallax:
		current_dir = current_dir.lerp(target_dir, delta * 0.5)
		var step = current_dir * scroll_speed * delta
		
		if parallax.repeat_size != Vector2.ZERO:
			parallax.scroll_offset.x = wrapf(parallax.scroll_offset.x + step.x, 0.0, parallax.repeat_size.x)
			parallax.scroll_offset.y = wrapf(parallax.scroll_offset.y + step.y, 0.0, parallax.repeat_size.y)
		else:
			parallax.scroll_offset += step

func _pick_new_direction() -> void:
	target_dir = Vector2(randf_range(-1.0, 1.0), randf_range(-1.0, 1.0)).normalized()

func _setup_button_animations() -> void:
	if return_button:
		_base_button_scales[return_button.name] = return_button.scale
		_target_button_scales[return_button.name] = return_button.scale
		
		if not return_button.mouse_entered.is_connected(_on_button_hover):
			return_button.mouse_entered.connect(_on_button_hover.bind(return_button))
			
		if not return_button.mouse_exited.is_connected(_on_button_exit):
			return_button.mouse_exited.connect(_on_button_exit.bind(return_button))
			
		if not return_button.pressed.is_connected(_on_button_pressed_animate):
			return_button.pressed.connect(_on_button_pressed_animate.bind(return_button))

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

func _on_return_button_pressed() -> void:
	Data.player_data.clear()
	Data.silver = 0
	Data.current_floor = 1
	
	TransitionManager.change_scene("res://ui/gui.tscn")

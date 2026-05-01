extends CanvasLayer

signal upgrade_selected(upgrade: Dictionary)

var current_options: Array = []

@onready var pause_resume_btn = %PauseOverlay.get_node("VBoxContainer/ResumeButton")
@onready var pause_options_btn = %PauseOverlay.get_node("VBoxContainer/OptionsButton")
@onready var pause_quit_btn = %PauseOverlay.get_node("VBoxContainer/QuitButton")
@onready var mobile_pause_btn = $PauseBox/PauseButton
@onready var options_menu = $Options
@onready var title_sprite = $PauseOverlay/HBoxContainer/Convallaria

@onready var sfx_hover = preload("res://audio/menu_hover.mp3")
@onready var sfx_click = preload("res://audio/menu_click.mp3")

var _base_button_scales: Dictionary = {}
var _target_button_scales: Dictionary = {}

func _ready() -> void:
	%PauseOverlay.hide()
	
	%TryAgain.pressed.connect(_on_try_again_pressed)
	%Exit.pressed.connect(_on_exit_pressed)
	
	%Upgrade1.pressed.connect(_on_upgrade_pressed.bind(0))
	%Upgrade2.pressed.connect(_on_upgrade_pressed.bind(1))
	%Upgrade3.pressed.connect(_on_upgrade_pressed.bind(2))
	
	pause_resume_btn.pressed.connect(_on_pause_start_pressed)
	pause_options_btn.pressed.connect(_on_pause_options_pressed)
	pause_quit_btn.pressed.connect(_on_pause_quit_pressed)
	mobile_pause_btn.pressed.connect(_toggle_pause)
	
	_setup_button_animations()
	
	if title_sprite:
		_setup_title_animation()

func _setup_title_animation() -> void:
	var shadow = Sprite2D.new()
	shadow.texture = title_sprite.texture
	shadow.modulate = Color(0, 0, 0, 0.5)
	shadow.position = Vector2(1, 2)
	shadow.show_behind_parent = true
	title_sprite.add_child(shadow)

	var start_y = title_sprite.position.y
	var tween = create_tween().set_loops().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	tween.bind_node(title_sprite)
	tween.set_pause_mode(Tween.TWEEN_PAUSE_PROCESS) 
	
	tween.tween_property(title_sprite, "position:y", start_y - 10.0, 1.5)
	tween.tween_property(title_sprite, "position:y", start_y, 1.5)

func _setup_button_animations() -> void:
	var pause_buttons = [pause_resume_btn, pause_options_btn, pause_quit_btn, mobile_pause_btn]
	
	for button in pause_buttons:
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
	player.process_mode = Node.PROCESS_MODE_ALWAYS
	add_child(player)
	player.play(start_offset)
	player.finished.connect(player.queue_free)

func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_cancel"):
		if %GameOverScreen.visible or %LevelUpScreen.visible:
			return
		_toggle_pause()

func _toggle_pause() -> void:
	var is_paused = get_tree().paused
	get_tree().paused = !is_paused
	%PauseOverlay.visible = !is_paused

func _on_pause_start_pressed() -> void:
	_toggle_pause()

func _on_pause_options_pressed() -> void:
	options_menu.show()

func _on_pause_quit_pressed() -> void:
	get_tree().paused = false
	TransitionManager.change_scene("res://scenes/gui.tscn")

func update_health(current: float, maximum: float) -> void:
	%HealthBar.max_value = maximum
	%HealthBar.value = current
	%HealthLabel.text = str(int(current), "/", int(maximum))
	
	if current / maximum <= 0.3:
		%LowHPWarning.visible = true
	else:
		%LowHPWarning.visible = false

func update_exp(current: int, maximum: int) -> void:
	%ExpBar.max_value = maximum
	%ExpBar.value = current
	%ExpLabel.text = str(current, "/", maximum)

func update_level(level: int) -> void:
	%LevelLabel.text = "Level: " + str(level)

func show_game_over() -> void:
	%GameOverScreen.visible = true

func _on_try_again_pressed() -> void:
	get_tree().paused = false
	TransitionManager.change_scene("res://scenes/world.tscn")

func _on_exit_pressed() -> void:
	get_tree().paused = false
	TransitionManager.change_scene("res://scenes/gui.tscn") 

func show_level_up(options: Array) -> void:
	current_options = options
	var buttons = [%Upgrade1, %Upgrade2, %Upgrade3]
	
	for i in range(buttons.size()):
		if i < options.size():
			buttons[i].text = options[i]["text"]
			buttons[i].add_theme_color_override("font_color", options[i]["color"])
			buttons[i].show()
		else:
			buttons[i].hide()
			
	%LevelUpScreen.visible = true
	
func update_weapon_slots(weapon_ids: Array) -> void:
	var slots = [%Slot1, %Slot2, %Slot3]
	
	for i in range(slots.size()):
		var icon = slots[i].get_node("Icon")
		
		if i < weapon_ids.size():
			var w_id = weapon_ids[i]
			var icon_path = Data.WEAPONS[w_id]["icon"]
			icon.texture = load(icon_path)
		else:
			icon.texture = null

func _on_upgrade_pressed(index: int) -> void:
	%LevelUpScreen.visible = false
	get_tree().paused = false
	upgrade_selected.emit(current_options[index])

func update_time(minutes: int, seconds: int) -> void:
	%TimeLabel.text = "%02d:%02d" % [minutes, seconds]

func update_kills(kills: int) -> void:
	%KillLabel.text = "Kills: " + str(kills)

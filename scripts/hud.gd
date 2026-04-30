extends CanvasLayer

signal upgrade_selected(upgrade: Dictionary)

var current_options: Array = []

func _ready() -> void:
	%TryAgain.pressed.connect(_on_try_again_pressed)
	%Exit.pressed.connect(_on_exit_pressed)
	
	%Upgrade1.pressed.connect(_on_upgrade_pressed.bind(0))
	%Upgrade2.pressed.connect(_on_upgrade_pressed.bind(1))
	%Upgrade3.pressed.connect(_on_upgrade_pressed.bind(2))

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

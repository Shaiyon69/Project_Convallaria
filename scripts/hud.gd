extends CanvasLayer

signal upgrade_selected(upgrade_name: String)

var upgrade_pool: Array = [
	{"id": "max_hp", "text": "+50 Max HP"},
	{"id": "speed", "text": "+25 Speed"},
	{"id": "damage", "text": "+15 Damage"},
	{"id": "pickup_range", "text": "+25 Pickup Range"},
	{"id": "fire_rate", "text": "+10% Fire Rate"}
]
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
	# Unpause FIRST so the transition animation is allowed to play
	get_tree().paused = false
	print("Button was clicked!")
	TransitionManager.change_scene("res://scenes/world.tscn")

func _on_exit_pressed() -> void:
	# Unpause FIRST so the transition animation is allowed to play
	get_tree().paused = false
	print("Button was clicked!")
	# Swap this path to match exactly where your Main Menu scene is saved
	TransitionManager.change_scene("res://scenes/gui.tscn") 

func show_level_up() -> void:
	current_options.clear()
	var pool_copy = upgrade_pool.duplicate()
	pool_copy.shuffle()
	
	for i in range(3):
		current_options.append(pool_copy[i])
		
	%Upgrade1.text = current_options[0]["text"]
	%Upgrade2.text = current_options[1]["text"]
	%Upgrade3.text = current_options[2]["text"]
	
	%LevelUpScreen.visible = true

func _on_upgrade_pressed(index: int) -> void:
	%LevelUpScreen.visible = false
	get_tree().paused = false
	upgrade_selected.emit(current_options[index]["id"])

func update_time(minutes: int, seconds: int) -> void:
	%TimeLabel.text = "%02d:%02d" % [minutes, seconds]

func update_kills(kills: int) -> void:
	%KillLabel.text = "💀: " + str(kills)

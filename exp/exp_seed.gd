extends Area2D

# 0 = Normal, 1 = Magnet, 2 = Speed
@export_enum("Normal", "Magnet", "Speed") var seed_type: int = 0
@export var exp_amount: int = 1

var is_magnetic: bool = false
var player: Node2D = null
var current_speed: float = 0.0
var max_speed: float = 600.0
var acceleration: float = 1200.0

func _ready() -> void:
	if not body_entered.is_connected(_on_body_entered):
		body_entered.connect(_on_body_entered)
	_apply_visuals()

func _apply_visuals() -> void:
	match seed_type:
		0: # Normal
			modulate = Color(0.0, 0.733, 0.0, 1.0) 
			scale = Vector2(1, 1)
		1: # Magnet
			modulate = Color(1, 0.2, 0.2)
			scale = Vector2(1.5, 1.5)
		2: # Speed
			modulate = Color(0.2, 0.5, 1)
			scale = Vector2(1.3, 1.3)

func _physics_process(delta: float) -> void:
	if seed_type == 0 and is_magnetic and player:
		current_speed = move_toward(current_speed, max_speed, acceleration * delta)
		var direction = global_position.direction_to(player.global_position)
		global_position += direction * current_speed * delta

func pull_to_player(target: Node2D) -> void:
	if seed_type == 0:
		player = target
		is_magnetic = true

func _on_body_entered(body: Node2D) -> void:
	if body.is_in_group("player"):
		match seed_type:
			0: # Normal
				if body.has_method("gain_experience"):
					body.gain_experience(exp_amount)
			1: # Magnet
				if body.has_method("activate_magnet_powerup"):
					body.activate_magnet_powerup()
			2: # Speed
				if body.has_method("activate_speed_powerup"):
					body.activate_speed_powerup()
		
		queue_free()

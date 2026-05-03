extends Area2D

@export_enum("Normal", "Magnet", "Speed", "Bomb", "Coin") var seed_type: int = 0
@export var exp_amount: int = 1

var is_magnetic: bool = false
var player: Node2D = null
var current_speed: float = 0.0
var max_speed: float = 900.0
var acceleration: float = 2000.0

func _ready() -> void:
	add_to_group("exp_seed") 
	
	if not body_entered.is_connected(_on_body_entered):
		body_entered.connect(_on_body_entered)
		
	_apply_visuals()
	set_physics_process(false) 

func _apply_visuals() -> void:
	match seed_type:
		0: # Normal EXP
			modulate = Color(1, 1, 1)
			scale = Vector2(1, 1)
		1: # Magnet
			modulate = Color(1, 0.2, 0.2)
			scale = Vector2(1.5, 1.5)
		2: # Speed
			modulate = Color(0.2, 0.5, 1)
			scale = Vector2(1.3, 1.3)
		3: # Bomb
			modulate = Color(0.1, 0.1, 0.1)
			scale = Vector2(1.4, 1.4)
		4: # Golden Seed (Coin)
			modulate = Color(1.0, 0.8, 0.1)
			scale = Vector2(1.2, 1.2)

func pull_to_player(target: Node2D) -> void:
	if (seed_type == 0 or seed_type == 4) and not is_magnetic:
		player = target
		is_magnetic = true
		set_physics_process(true) 

func _physics_process(delta: float) -> void:
	if is_magnetic and player:
		current_speed = move_toward(current_speed, max_speed, acceleration * delta)
		var direction = global_position.direction_to(player.global_position)
		global_position += direction * current_speed * delta

func _on_body_entered(body: Node2D) -> void:
	if body.is_in_group("player"):
		match seed_type:
			0:
				if body.has_method("gain_experience"):
					body.gain_experience(exp_amount)
			1:
				if body.has_method("activate_magnet_powerup"):
					body.activate_magnet_powerup()
			2:
				if body.has_method("activate_speed_powerup"):
					body.activate_speed_powerup()
			3:
				if body.has_method("activate_bomb_powerup"):
					body.activate_bomb_powerup(global_position)
			4:
				if body.has_method("collect_coin"):
					body.collect_coin(exp_amount)
		queue_free()

extends Area2D

@export_enum("Normal", "Magnet", "Speed", "Bomb", "Gold", "Silver", "Heal") var seed_type: int = 0
@export var exp_amount: int = 1
@export var despawn_time: float = -1.0
@export var warning_time: float = -1.0

var is_magnetic: bool = false
var player: Node2D = null
var current_speed: float = 0.0
var max_speed: float = 900.0
var acceleration: float = 2000.0
var glimmer_tween: Tween
var warning_tween: Tween

func _ready() -> void:
	add_to_group("exp_seed") 
	
	if not body_entered.is_connected(_on_body_entered):
		body_entered.connect(_on_body_entered)

	_apply_visuals()
	_start_despawn_timer()
	set_physics_process(false) 

func _apply_visuals() -> void:
	match seed_type:
		0: 
			modulate = Color(1, 1, 1)
			scale = Vector2(1.2, 1.2)
		1: 
			modulate = Color(1, 0.2, 0.2)
			scale = Vector2(1.8, 1.8)
		2: 
			modulate = Color(0.2, 0.5, 1)
			scale = Vector2(1.6, 1.6)
		3: 
			modulate = Color(0.1, 0.1, 0.1)
			scale = Vector2(1.7, 1.7)
		4: 
			modulate = Color(1.0, 0.8, 0.1)
			scale = Vector2(1.5, 1.5)
		5: 
			modulate = Color(0.8, 0.8, 0.85)
			scale = Vector2(1.4, 1.4)
		6:
			modulate = Color(0.7, 0.25, 1.0)
			scale = Vector2(1.6, 1.6)

	glimmer_tween = create_tween().set_loops().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	var glimmer_speed = randf_range(0.4, 0.7)
	glimmer_tween.tween_property(self, "modulate:a", 0.5, glimmer_speed)
	glimmer_tween.tween_property(self, "modulate:a", 1.0, glimmer_speed)

func _start_despawn_timer() -> void:
	if despawn_time < 0.0:
		despawn_time = float(Data.PICKUP_SETTINGS.get("despawn_time", 25.0))
	if warning_time < 0.0:
		warning_time = float(Data.PICKUP_SETTINGS.get("warning_time", 5.0))
	if despawn_time <= 0.0:
		return
	var warn_delay = max(0.0, despawn_time - warning_time)
	if warning_time > 0.0 and warn_delay > 0.0:
		get_tree().create_timer(warn_delay).timeout.connect(_start_warning_animation)
	get_tree().create_timer(despawn_time).timeout.connect(_despawn)

func _start_warning_animation() -> void:
	if not is_inside_tree() or is_magnetic:
		return
	if glimmer_tween and glimmer_tween.is_valid():
		glimmer_tween.kill()
	warning_tween = create_tween().set_loops().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	warning_tween.tween_property(self, "modulate:a", 0.2, 0.15)
	warning_tween.tween_property(self, "modulate:a", 1.0, 0.15)

func _despawn() -> void:
	if not is_inside_tree() or is_magnetic:
		return
	queue_free()

func pull_to_player(target: Node2D) -> void:
	if (seed_type == 0 or seed_type == 4 or seed_type == 5 or seed_type == 6) and not is_magnetic:
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
					body.collect_coin(exp_amount, true)
			5:
				if body.has_method("collect_coin"):
					body.collect_coin(exp_amount, false)
			6:
				if body.has_method("heal_from_pickup"):
					body.heal_from_pickup(exp_amount)
		queue_free()

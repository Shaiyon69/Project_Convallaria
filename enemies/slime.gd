extends CharacterBody2D

@export var seed_scene: PackedScene = preload("res://drops/exp/exp_seed.tscn")
@export var damage_scene: PackedScene = preload("res://enemies/damage_number.tscn")
@export var projectile_scene: PackedScene 

var slime_sfx = preload("res://enemies/slime.ogg")

var speed: float
var health: int
var max_health: int
var exp_value: int
var attack_damage: int
var base_pitch: float = 1.0
var drop_tier: int = 1
var is_boss: bool = false

var player: Node2D
var despawn_distance: float = 3000.0
var active_radius: float = 1000.0

var original_speed: float
var is_burning: bool = false
var is_slowed: bool = false
var base_color: Color = Color.WHITE

var is_dying: bool = false
var is_hurt: bool = false
var facing: String = "down"

var direction: Vector2 = Vector2.ZERO
var push_vector: Vector2 = Vector2.ZERO
var logic_timer: float = 0.0
var sfx_timer: float = 0.0

var is_shooter: bool = false
var stop_distance: float = 350.0

# --- NEW: Randomized shooting intervals ---
var shoot_cooldown_min: float = 3.5 
var shoot_cooldown_max: float = 6.0 
var shoot_timer: float = 0.0

@onready var soft_collision = $SoftCollision
@onready var health_bar = $HealthBar

func _ready() -> void:
	add_to_group("enemy")
	player = get_tree().get_first_node_in_group("player")
	logic_timer = randf_range(0.0, 0.2)
	sfx_timer = randf_range(0.0, 2.0)
	
	# NEW: Initial random delay before the first shot
	shoot_timer = randf_range(2.0, 4.0) 

func apply_stats(stats: Dictionary) -> void:
	health = stats["health"]
	max_health = health
	speed = stats["speed"]
	original_speed = speed
	exp_value = stats.get("exp_value", 1)
	attack_damage = stats["damage"]
	scale = Vector2(stats["scale"], stats["scale"])
	base_color = stats["color"]
	$AnimatedSprite2D.modulate = base_color
	health_bar.max_value = health
	health_bar.value = health

	if stats.has("drop_tier"):
		drop_tier = stats["drop_tier"]

	if stats.has("base_pitch"):
		base_pitch = stats["base_pitch"]
		if base_pitch <= 0.2:
			is_boss = true
			var boss_track = load(Data.MUSIC["boss"])
			AudioManager.play_music(boss_track, -10.0)
			AudioManager.set_music_speed(1.0)

	if stats.has("is_shooter") and stats["is_shooter"]:
		is_shooter = true

	if stats.has("is_death_slime") and stats["is_death_slime"]:
		if player and player.time_survived > 600.0:
			var extra_minutes = (player.time_survived - 600.0) / 60.0
			var power_multiplier = 1.0 + (extra_minutes * 0.5)
			var speed_multiplier = 1.0 + (extra_minutes * 0.2)
			var size_multiplier = 1.0 + (extra_minutes * 0.1)
			
			health = int(health * power_multiplier)
			max_health = health
			attack_damage = int(attack_damage * power_multiplier)
			
			speed = speed * speed_multiplier
			original_speed = speed
			
			scale *= size_multiplier

func _physics_process(delta: float) -> void:
	if is_dying:
		return

	if player:
		logic_timer -= delta
		if logic_timer <= 0.0:
			logic_timer = 0.2 + randf_range(-0.05, 0.05)
			_update_expensive_logic()

		if is_hurt:
			velocity = velocity * 0.95
		else:
			var distance = global_position.distance_to(player.global_position)
			
			if distance > active_radius:
				set_collision_mask_value(2, false)
				velocity = direction * speed
			else:
				set_collision_mask_value(2, true)
				
				var desired_velocity = Vector2.ZERO
				
				if is_shooter and distance <= stop_distance:
					shoot_timer -= delta
					if shoot_timer <= 0.0:
						_shoot()
						# NEW: Pick a random cooldown for the next shot
						shoot_timer = randf_range(shoot_cooldown_min, shoot_cooldown_max)
				else:
					var steer_direction = _get_whisker_steering()
					if steer_direction != Vector2.ZERO:
						desired_velocity = (steer_direction * speed) + (push_vector * 15.0)
					else:
						desired_velocity = (direction * speed) + (push_vector * 20.0)

					if desired_velocity.length() > speed:
						desired_velocity = desired_velocity.normalized() * speed

				velocity = desired_velocity

		if velocity.length() > 0 and not is_hurt:
			sfx_timer -= delta
			if sfx_timer <= 0.0:
				sfx_timer = randf_range(1.5, 3.0)
				if slime_sfx:
					AudioManager.play_sfx_2d(slime_sfx, global_position, -20.0, base_pitch, "move")

		move_and_slide()
		_update_animations()

func _shoot() -> void:
	if not projectile_scene or is_dying: return
	
	var proj = projectile_scene.instantiate()
	proj.global_position = global_position
	proj.direction = direction
	proj.damage = attack_damage
	
	get_tree().current_scene.add_child(proj)
	
	var tween = create_tween()
	tween.tween_property($AnimatedSprite2D, "scale", Vector2(scale.x * 1.2, scale.y * 0.8), 0.1)
	tween.tween_property($AnimatedSprite2D, "scale", scale, 0.1)

func _update_expensive_logic() -> void:
	var distance = global_position.distance_to(player.global_position)
	if distance > despawn_distance:
		queue_free()
		return

	direction = global_position.direction_to(player.global_position)

	push_vector = Vector2.ZERO

	if distance <= active_radius:
		if soft_collision.has_overlapping_areas():
			var areas = soft_collision.get_overlapping_areas()
			var max_checks = min(areas.size(), 3)
			for i in range(max_checks):
				push_vector += areas[i].global_position.direction_to(global_position)
			push_vector = push_vector.normalized()

func _get_whisker_steering() -> Vector2:
	var space_state = get_world_2d().direct_space_state
	var ray_length = 50.0 
	
	var forward = direction
	var left = direction.rotated(-PI/3) 
	var right = direction.rotated(PI/3) 

	var q_f = PhysicsRayQueryParameters2D.create(global_position, global_position + (forward * ray_length), 2)
	var hit_f = space_state.intersect_ray(q_f)

	if hit_f:
		var q_l = PhysicsRayQueryParameters2D.create(global_position, global_position + (left * ray_length), 2)
		var q_r = PhysicsRayQueryParameters2D.create(global_position, global_position + (right * ray_length), 2)
		
		var hit_l = space_state.intersect_ray(q_l)
		var hit_r = space_state.intersect_ray(q_r)
		
		if not hit_l: return left 
		elif not hit_r: return right 
		else: return -forward 

	return Vector2.ZERO 

func _update_animations() -> void:
	if is_hurt or is_dying:
		return

	if velocity.length() > 0:
		if abs(velocity.x) > abs(velocity.y):
			facing = "right" if velocity.x > 0 else "left"
		else:
			facing = "down" if velocity.y > 0 else "up"

		$AnimatedSprite2D.play(facing)
	else:
		$AnimatedSprite2D.stop()

func apply_burn(burn_damage: float) -> void:
	if is_burning or is_dying:
		return

	is_burning = true
	$AnimatedSprite2D.modulate = Color(1.0, 0.4, 0.1)

	for i in range(4):
		if is_dying:
			break
		await get_tree().create_timer(0.5).timeout
		take_damage(int(burn_damage))

	if not is_dying and not is_slowed:
		$AnimatedSprite2D.modulate = base_color

	is_burning = false

func apply_slow(slow_multiplier: float) -> void:
	if is_slowed or is_dying:
		return

	is_slowed = true
	speed = original_speed * slow_multiplier
	$AnimatedSprite2D.modulate = Color(0.3, 0.8, 1.0)

	await get_tree().create_timer(3.0).timeout

	if not is_dying:
		speed = original_speed
		if not is_burning:
			$AnimatedSprite2D.modulate = base_color

	is_slowed = false

func take_damage(amount: int) -> void:
	if is_dying:
		return

	health -= amount
	health_bar.value = health

	if is_boss:
		var hp_percent = float(health) / float(max_health)
		var dynamic_bpm = 1.0 + ((1.0 - hp_percent) * 0.5)
		AudioManager.set_music_speed(dynamic_bpm)

	var floating_text = damage_scene.instantiate()
	floating_text.global_position = global_position
	get_tree().current_scene.add_child(floating_text)
	floating_text.setup(amount)

	if health <= 0:
		_die()
	else:
		_play_hurt()

func _play_hurt() -> void:
	if is_hurt:
		return

	is_hurt = true
	if slime_sfx:
		AudioManager.play_sfx_2d(slime_sfx, global_position, -5.0, base_pitch, "hit")

	$AnimatedSprite2D.play("hurt_" + facing)
	await $AnimatedSprite2D.animation_finished
	is_hurt = false

func _die() -> void:
	is_dying = true
	health_bar.hide()

	if is_boss:
		AudioManager.stop_music()
		AudioManager.set_music_speed(1.0)

	if slime_sfx:
		AudioManager.play_sfx_2d(slime_sfx, global_position, -10.0, base_pitch, "death")

	$AnimatedSprite2D.play("death")
	await $AnimatedSprite2D.animation_finished

	if player and player.has_method("add_kill"):
		player.add_kill()

	var new_seed = seed_scene.instantiate()
	new_seed.global_position = global_position
	var roll = randf()

	var active_powerups = 0
	for node in get_tree().get_nodes_in_group("exp_seed"):
		if node.get("seed_type") in [1, 2, 3]:
			active_powerups += 1

	var p_chance = 0.01 * drop_tier
	var c_chance = 0.05 * drop_tier

	if active_powerups < 3 and roll <= p_chance:
		new_seed.seed_type = 1
	elif active_powerups < 3 and roll <= p_chance * 2:
		new_seed.seed_type = 2
	elif active_powerups < 3 and roll <= p_chance * 3:
		new_seed.seed_type = 3
	elif roll <= (p_chance * 3) + c_chance:
		new_seed.seed_type = 4
		new_seed.exp_amount = 1 * drop_tier
	elif roll <= (p_chance * 3) + (c_chance * 2):
		new_seed.seed_type = 5
		new_seed.exp_amount = 1 * drop_tier
	else:
		new_seed.seed_type = 0
		new_seed.exp_amount = exp_value

	get_parent().call_deferred("add_child", new_seed)
	queue_free()

extends CharacterBody2D

@export var seed_scene: PackedScene = preload("res://exp/exp_seed.tscn")
@export var damage_scene: PackedScene = preload("res://enemy/damage_number.tscn")

var slime_sfx = preload("res://audio/slime.ogg") 

var speed: float
var health: int
var attack_damage: int
var exp_value: int = 10
var base_pitch: float = 1.0
var player: Node2D
var despawn_distance: float = 1500.0
var magnet_scale

var base_color: Color = Color.WHITE
var original_speed: float
var is_burning: bool = false
var is_slowed: bool = false

var is_dasher: bool = false
var is_dashing: bool = false
var dash_direction: Vector2 = Vector2.ZERO

var facing: String = "down"
var is_hurt: bool = false
var is_dying: bool = false

@onready var soft_collision = $SoftCollision
@onready var health_bar = %HealthBar
@onready var dash_timer = $DashTimer

func _ready() -> void:
	add_to_group("enemy")
	player = get_tree().get_first_node_in_group("player")

func apply_stats(stats: Dictionary) -> void:
	health = stats["health"]
	speed = stats["speed"]
	original_speed = speed 
	attack_damage = stats["damage"]
	scale = Vector2(stats["scale"], stats["scale"])
	
	base_color = stats["color"] 
	$AnimatedSprite2D.modulate = base_color
	
	if stats.has("exp"):
		exp_value = stats["exp"]
		
	if stats.has("base_pitch"):
		base_pitch = stats["base_pitch"]
	
	health_bar.max_value = health
	health_bar.value = health
	
	if stats.has("is_dasher") and stats["is_dasher"]:
		is_dasher = true
		dash_timer.start(randf_range(2.0, 4.0))

func _physics_process(delta: float) -> void:
	if is_dying:
		return

	if player:
		var distance = global_position.distance_to(player.global_position)
		if distance > despawn_distance:
			queue_free()
			return
			
		var push_vector = Vector2.ZERO
		if soft_collision.has_overlapping_areas():
			var areas = soft_collision.get_overlapping_areas()
			for area in areas:
				push_vector += area.global_position.direction_to(global_position)
				
		if is_hurt:
			velocity = velocity * 0.95
		elif is_dashing:
			velocity = dash_direction * (speed * 5.0)
		else:
			var direction = global_position.direction_to(player.global_position).normalized()
			velocity = (direction * speed) + (push_vector * 20.0)
		if velocity.length() > 0 and not is_hurt:
			if slime_sfx:
				AudioManager.play_sfx_2d(slime_sfx, global_position, -24.0, base_pitch * 0.8, "move")
			
		move_and_slide()
		_update_animations()

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
		AudioManager.play_sfx_2d(slime_sfx, global_position, -15.0, base_pitch, "hit")
		
	$AnimatedSprite2D.play("hurt_" + facing)
	await $AnimatedSprite2D.animation_finished
	is_hurt = false

func _die() -> void:
	is_dying = true
	health_bar.hide()
	
	if slime_sfx:
		AudioManager.play_sfx_2d(slime_sfx, global_position, -10.0, base_pitch, "death") 
	
	$AnimatedSprite2D.play("death")
	await $AnimatedSprite2D.animation_finished
	
	if player and player.has_method("add_kill"):
		player.add_kill()
		
	var new_seed = seed_scene.instantiate()
	new_seed.global_position = global_position
	
	var roll = randf()
	if roll <= 0.03:
		new_seed.seed_type = 1 
	elif roll <= 0.06:
		new_seed.seed_type = 2
	else:
		new_seed.seed_type = 0
		new_seed.exp_amount = exp_value 
		
	get_parent().call_deferred("add_child", new_seed)
	queue_free()

func _on_dash_timer_timeout() -> void:
	if not is_dasher or not player or is_dying:
		return
		
	is_dashing = true
	dash_direction = global_position.direction_to(player.global_position)
	
	await get_tree().create_timer(0.4).timeout
	
	is_dashing = false
	dash_timer.start(3.0)
	
func activate_magnet_powerup() -> void:

	%MagnetZone.scale = Vector2(100.0, 100.0) 
	await get_tree().create_timer(0.6).timeout
	if is_inside_tree():
		%MagnetZone.scale = Vector2(magnet_scale, magnet_scale)

func activate_speed_powerup() -> void:
	var boost_amount = speed
	speed += boost_amount

	%AnimatedSprite2D.modulate = Color(0, 2, 5)
	
	await get_tree().create_timer(4.0).timeout
	
	if is_inside_tree():
		speed -= boost_amount
		%AnimatedSprite2D.modulate = Color(1, 1, 1)

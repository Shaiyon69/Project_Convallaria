extends CharacterBody2D

@export var seed_scene: PackedScene = preload("res://exp/exp_seed.tscn")
@export var damage_scene: PackedScene = preload("res://enemy/damage_number.tscn")

var slime_sfx = preload("res://audio/slime.ogg")

var speed: float
var health: int
var max_health: int
var exp_value: int
var attack_damage: int
var base_pitch: float = 1.0
var is_boss: bool = false

var player: Node2D
var despawn_distance: float = 3000.0

var original_speed: float
var is_burning: bool = false
var is_slowed: bool = false
var base_color: Color = Color.WHITE

var is_dying: bool = false
var is_hurt: bool = false
var facing: String = "down"

@onready var soft_collision = $SoftCollision
@onready var health_bar = $HealthBar

func _ready() -> void:
	add_to_group("enemy")
	player = get_tree().get_first_node_in_group("player")
	
func apply_stats(stats: Dictionary) -> void:
	health = stats["health"]
	max_health = health
	speed = stats["speed"]
	original_speed = speed
	
	# Safe fallback to prevent crashes on startup
	exp_value = stats.get("exp_value", 1)
	
	attack_damage = stats["damage"]
	scale = Vector2(stats["scale"], stats["scale"])
	
	base_color = stats["color"]
	$AnimatedSprite2D.modulate = base_color
	
	health_bar.max_value = health
	health_bar.value = health
	
	if stats.has("base_pitch"):
		base_pitch = stats["base_pitch"]
		if base_pitch <= 0.2: 
			is_boss = true
			var boss_track = load(Data.MUSIC["boss"])
			AudioManager.play_music(boss_track, -10.0)
			AudioManager.set_music_speed(1.0)

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
				
			push_vector = push_vector.normalized()
				
		if is_hurt:
			velocity = velocity * 0.95
		else:
			var direction = global_position.direction_to(player.global_position)
			velocity = (direction * speed) + (push_vector * 20.0)
		
		if velocity.length() > 0 and not is_hurt:
			if slime_sfx:
				AudioManager.play_sfx_2d(slime_sfx, global_position, -20.0, base_pitch, "move")
			
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
	
	if roll <= 0.01:
		new_seed.seed_type = 1 # 1% Chance: Magnet
	elif roll <= 0.02:
		new_seed.seed_type = 2 # 1% Chance: Speed
	elif roll <= 0.03:
		new_seed.seed_type = 3 # 1% Chance: Bomb
	else:
		new_seed.seed_type = 0 # 97% Chance: Normal EXP
		new_seed.exp_amount = exp_value 
		
	get_parent().call_deferred("add_child", new_seed)
	queue_free()

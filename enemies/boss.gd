extends CharacterBody2D

@export var seed_scene: PackedScene = preload("res://drops/exp/exp_seed.tscn")
@export var damage_scene: PackedScene = preload("res://enemies/damage_number.tscn")

var boss_sfx = preload("res://enemies/slime.ogg")

var speed: float
var health: int
var max_health: int
var attack_damage: int
var player: Node2D
var despawn_distance: float = 3000.0

var is_dasher: bool = false
var is_dashing: bool = false
var dash_direction: Vector2 = Vector2.ZERO

var base_color: Color = Color.WHITE
var original_speed: float
var is_burning: bool = false
var is_slowed: bool = false

var facing: String = "down"
var is_hurt: bool = false
var is_dying: bool = false

@onready var soft_collision = $SoftCollision
@onready var health_bar = $BossUI/BossHealthBar
@onready var dash_timer = $DashTimer
@onready var nav_agent = $NavigationAgent2D
@onready var path_timer = $PathTimer
@onready var bulldozer_zone = $BulldozerZone
@onready var active_sprite: AnimatedSprite2D = $Sprites/AnimatedSprite2D

func _ready() -> void:
	add_to_group("enemy")
	player = get_tree().get_first_node_in_group("player")
	if player:
		nav_agent.target_position = player.global_position
		
	active_sprite.show()
	z_index = 100 
		
	var boss_track = load("res://enemies/bob.mp3")
	AudioManager.play_music(boss_track, -10.0)
	AudioManager.set_music_speed(1.0)
	
	set_collision_mask_value(2, false)

func _on_bulldozer_zone_body_entered(body: Node2D) -> void:
	if is_dying: return
	
	if body.has_method("destroy"):
		body.destroy()

func apply_stats(stats: Dictionary) -> void:
	health = stats["health"]
	max_health = health
	speed = stats["speed"]
	original_speed = speed
	attack_damage = stats["damage"]
	scale = Vector2(stats["scale"], stats["scale"])
	
	base_color = stats["color"]
	active_sprite.modulate = base_color
	
	health_bar.max_value = health
	health_bar.value = health
	
	if stats.has("is_dasher") and stats["is_dasher"]:
		is_dasher = true
		dash_timer.start(randf_range(2.0, 4.0))

func _on_path_timer_timeout() -> void:
	if player and not is_dying:
		nav_agent.target_position = player.global_position

func _physics_process(_delta: float) -> void:
	if is_dying:
		return

	if player:
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
			if boss_sfx:
				AudioManager.play_sfx_2d(boss_sfx, global_position, -20.0, 0.3, "move")
			
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
			
		active_sprite.play(facing)
	else:
		active_sprite.stop()

func apply_burn(burn_damage: float) -> void:
	if is_burning or is_dying:
		return
		
	is_burning = true
	active_sprite.modulate = Color(1.0, 0.4, 0.1)
	
	for i in range(4):
		if is_dying:
			break
		await get_tree().create_timer(0.5).timeout
		take_damage(int(burn_damage))
		
	if not is_dying and not is_slowed:
		active_sprite.modulate = base_color
		
	is_burning = false

func apply_slow(slow_multiplier: float) -> void:
	if is_slowed or is_dying:
		return
		
	is_slowed = true
	speed = original_speed * slow_multiplier
	active_sprite.modulate = Color(0.3, 0.8, 1.0)
	
	await get_tree().create_timer(3.0).timeout
	
	if not is_dying:
		speed = original_speed
		if not is_burning:
			active_sprite.modulate = base_color
			
	is_slowed = false

func take_damage(amount: int) -> void:
	if is_dying:
		return
		
	health -= amount
	health_bar.value = health
	
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
	
	if boss_sfx:
		AudioManager.play_sfx_2d(boss_sfx, global_position, -5.0, 0.3, "hit")
		
	active_sprite.play("hurt_" + facing)
	await active_sprite.animation_finished
	is_hurt = false

func _die() -> void:
	is_dying = true
	health_bar.hide()
	var spawner = get_tree().current_scene.get_node_or_null("Spawner")
	
	AudioManager.stop_music()
	AudioManager.set_music_speed(1.0)
	if spawner and "boss_defeated" in spawner:
		spawner.boss_defeated = true
	
	if boss_sfx:
		AudioManager.play_sfx_2d(boss_sfx, global_position, 0.0, 0.2, "death")
		
	active_sprite.play("death")
	await active_sprite.animation_finished
	
	if player and player.has_method("add_kill"):
		player.add_kill()
		
	var seed_drop_count = 15
	for i in range(seed_drop_count):
		var new_seed = seed_scene.instantiate()
		new_seed.global_position = global_position + Vector2(randf_range(-60, 60), randf_range(-60, 60))
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

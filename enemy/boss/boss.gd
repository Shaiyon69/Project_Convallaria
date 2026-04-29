extends CharacterBody2D

@export var seed_scene: PackedScene = preload("res://exp/exp_seed.tscn")
@export var damage_scene: PackedScene = preload("res://enemy/damage_number.tscn")

var speed: float
var health: int
var attack_damage: int
var player: Node2D
var despawn_distance: float = 3000.0

var is_dasher: bool = false
var is_dashing: bool = false
var dash_direction: Vector2 = Vector2.ZERO

var facing: String = "down"
var is_hurt: bool = false
var is_dying: bool = false

@onready var soft_collision = $SoftCollision
@onready var health_bar = $BossUI/BossHealthBar
@onready var dash_timer = $DashTimer
@onready var nav_agent = $NavigationAgent2D
@onready var path_timer = $PathTimer

@onready var move_sound = $MoveSound
@onready var hurt_sound = $HurtSound
@onready var death_sound = $DeathSound

@onready var sprite_container = $Sprites
var active_sprite: AnimatedSprite2D

func _ready() -> void:
	add_to_group("enemy")
	player = get_tree().get_first_node_in_group("player")
	if player:
		nav_agent.target_position = player.global_position
		
	_randomize_sprite()

func _randomize_sprite() -> void:
	var all_sprites = sprite_container.get_children()
	for sprite in all_sprites:
		sprite.hide()
		
	var random_index = randi() % all_sprites.size()
	active_sprite = all_sprites[random_index]
	active_sprite.show()

func apply_stats(stats: Dictionary) -> void:
	health = stats["health"]
	speed = stats["speed"]
	attack_damage = stats["damage"]
	scale = Vector2(stats["scale"], stats["scale"])
	active_sprite.modulate = stats["color"]
	
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
		move_sound.stop()
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
			var next_path_pos = nav_agent.get_next_path_position()
			var direction = global_position.direction_to(next_path_pos).normalized()
			velocity = (direction * speed) + (push_vector * 20.0)
		
		if velocity.length() > 0 and not is_hurt:
			if not move_sound.playing:
				move_sound.play()
		else:
			move_sound.stop()
			
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
	hurt_sound.play()
	active_sprite.play("hurt_" + facing)
	await active_sprite.animation_finished
	is_hurt = false

func _die() -> void:
	is_dying = true
	health_bar.hide()
	move_sound.stop()
	death_sound.play()
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

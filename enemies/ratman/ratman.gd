extends CharacterBody2D

@export var max_health: int = 20
@export var movement_speed: float = 100.0
@export var attack_damage: int = 10
@export var is_shooter: bool = false
@export var exp_value: int = 1

@export var seed_scene: PackedScene
@export var damage_scene: PackedScene
@export var projectile_scene: PackedScene

var current_health: int
var is_dying: bool = false
var sfx_timer: float = 0.0 
var base_pitch: float = 1.0

@onready var anim = $AnimatedSprite2D
@onready var player = get_tree().get_first_node_in_group("player")

var audio_player: AudioStreamPlayer2D
var sfx_stream: AudioStream = preload("res://enemies/ratman/ratman.ogg")

func _ready() -> void:
	current_health = max_health
	add_to_group("enemy")
	sfx_timer = randf_range(0.0, 2.0)
	
	audio_player = AudioStreamPlayer2D.new()
	audio_player.stream = sfx_stream
	audio_player.max_distance = 400.0 
	audio_player.volume_db = -28.0 
	add_child(audio_player)

func _physics_process(delta: float) -> void:
	if is_dying or not player:
		if is_dying and audio_player and audio_player.playing:
			if audio_player.get_playback_position() >= 0.93:
				audio_player.stop()
		return
		
	var direction = global_position.direction_to(player.global_position)
	velocity = direction * movement_speed
	
	_update_animation(direction)
	move_and_slide()
	
	if velocity.length() > 0:
		sfx_timer -= delta
		if sfx_timer <= 0.0:
			sfx_timer = randf_range(2.0, 4.0) 
			
			if AudioManager._can_play_sfx("ratman_move", AudioManager.MOVE_COOLDOWN):
				audio_player.pitch_scale = base_pitch * randf_range(0.9, 1.1)
				audio_player.play(0.0)
				
		if audio_player.playing and audio_player.get_playback_position() >= 0.54:
			audio_player.stop()
	else:
		if audio_player.playing and audio_player.get_playback_position() < 0.70:
			audio_player.stop()

func _update_animation(dir: Vector2) -> void:
	if abs(dir.x) > abs(dir.y):
		if dir.x > 0:
			anim.play("walk_right")
		else:
			anim.play("walk_left")
	else:
		if dir.y > 0:
			anim.play("walk_down")
		else:
			anim.play("walk_up")

func take_damage(amount: int) -> void:
	if is_dying:
		return
		
	current_health -= amount
	
	if damage_scene:
		var dmg_num = damage_scene.instantiate()
		dmg_num.global_position = global_position
		get_tree().current_scene.add_child(dmg_num)
		dmg_num.setup(amount)
		
	if current_health <= 0:
		die()

func die() -> void:
	is_dying = true
	collision_layer = 0
	collision_mask = 0
	anim.play("death")
	
	if audio_player and AudioManager._can_play_sfx("ratman_death", AudioManager.DEATH_COOLDOWN):
		audio_player.volume_db = -15.0 
		audio_player.pitch_scale = base_pitch * randf_range(0.9, 1.1)
		audio_player.play(0.70)
	
	if seed_scene:
		var seed_inst = seed_scene.instantiate()
		seed_inst.global_position = global_position
		seed_inst.exp_amount = exp_value
		get_tree().current_scene.call_deferred("add_child", seed_inst)
		
	if player and player.has_method("add_kill"):
		player.add_kill()
		
	await anim.animation_finished
	queue_free()

func apply_stats(stats: Dictionary) -> void:
	if stats.has("health"):
		max_health = stats["health"]
		current_health = max_health
	if stats.has("speed"):
		movement_speed = stats["speed"]
	if stats.has("damage"):
		attack_damage = stats["damage"]
	if stats.has("exp"):
		exp_value = stats["exp"]
	if stats.has("is_shooter"):
		is_shooter = stats["is_shooter"]
	if stats.has("scale"):
		scale = Vector2(stats["scale"], stats["scale"])
	if stats.has("color"):
		anim.modulate = stats["color"]
	
	if stats.has("base_pitch"):
		base_pitch = stats["base_pitch"]
		if audio_player:
			audio_player.pitch_scale = base_pitch

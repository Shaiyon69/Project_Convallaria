extends CharacterBody2D

@export var seed_scene: PackedScene = preload("res://drops/exp/exp_seed.tscn")
@export var damage_scene: PackedScene = preload("res://enemies/damage_number.tscn")
@export var projectile_scene: PackedScene
@export var minion_scene: PackedScene = preload("res://enemies/slime.tscn")

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

var is_boss: bool = false
var is_enraged: bool = false
var is_casting: bool = false
var special_timer: Timer
var phase: int = 1

var default_sprite_scale: Vector2 = Vector2(1.0, 1.0)
var pulse_tween: Tween

@onready var soft_collision = $SoftCollision
@onready var dash_timer = $DashTimer
@onready var nav_agent = $NavigationAgent2D
@onready var path_timer = $PathTimer
@onready var bulldozer_zone = $BulldozerZone
@onready var active_sprite: AnimatedSprite2D = $Sprites/AnimatedSprite2D

@onready var boss_ui = get_node_or_null("BossUI")
@onready var health_bar = get_node_or_null("BossUI/BossHealthBar")

func _ready() -> void:
	add_to_group("enemy")
	player = get_tree().get_first_node_in_group("player")
	
	if player:
		nav_agent.target_position = player.global_position
		
	default_sprite_scale = active_sprite.scale
	active_sprite.show()
	y_sort_enabled = true
	
	if boss_ui:
		boss_ui.hide()
		
	set_collision_mask_value(2, false)

func apply_stats(stats: Dictionary) -> void:
	health = stats["health"]
	max_health = health
	speed = stats["speed"]
	original_speed = speed
	attack_damage = stats["damage"]
	
	var new_scale = Vector2(stats["scale"], stats["scale"])
	scale = new_scale

	if boss_ui and not boss_ui is CanvasLayer:
		boss_ui.scale = Vector2(1.0, 1.0) / new_scale
	
	base_color = stats["color"]
	active_sprite.modulate = base_color
	
	if health >= 1000 or (stats.has("is_boss") and stats["is_boss"]):
		_setup_as_boss()
	
	if stats.has("is_dasher") and stats["is_dasher"]:
		is_dasher = true
		dash_timer.start(randf_range(2.0, 4.0))

func _setup_as_boss() -> void:
	is_boss = true
	
	var current_floor = Data.current_floor if "current_floor" in Data else 1
	var hp_multiplier = 1.0 + ((current_floor - 1) * 1.5)
	var dmg_multiplier = 1.0 + ((current_floor - 1) * 0.3)
	
	max_health = int(max_health * hp_multiplier)
	health = max_health
	attack_damage = int(attack_damage * dmg_multiplier)
	speed = speed + ((current_floor - 1) * 20.0)
	original_speed = speed
	
	if boss_ui:
		boss_ui.show()
		
	if health_bar:
		health_bar.max_value = health
		health_bar.value = health
		
	var boss_track = ""

	if current_floor == 1:
		boss_track = "res://world/level1.mp3"
		base_color = Color(0.6, 0.5, 0.4)
	elif current_floor == 2:
		boss_track = "res://world/level2.wav"
		base_color = Color(0.4, 0.2, 0.6)
	else:
		boss_track = "res://world/level3.mp3"
		base_color = Color(0.8, 0.2, 0.2)

	active_sprite.modulate = base_color

	if AudioManager.has_method("play_music"):
		var stream = load(boss_track)
		if stream:
			AudioManager.play_music(stream, -10.0)
			AudioManager.set_music_speed(1.0)
	
	special_timer = Timer.new()
	special_timer.wait_time = 6.0
	special_timer.autostart = true
	special_timer.timeout.connect(_on_special_attack)
	add_child(special_timer)

func _on_bulldozer_zone_body_entered(body: Node2D) -> void:
	if is_dying:
		return
	
	if body.has_method("destroy"):
		body.destroy()

func _physics_process(_delta: float) -> void:
	if is_dying or is_casting:
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
			velocity = dash_direction * (original_speed * 4.0)
		else:
			var direction = global_position.direction_to(player.global_position).normalized()
			velocity = (direction * speed) + (push_vector * 20.0)
		
		if velocity.length() > 0 and not is_hurt:
			if boss_sfx and AudioManager.has_method("play_sfx_2d"):
				AudioManager.play_sfx_2d(boss_sfx, global_position, -20.0, 0.3, "move")
			
		move_and_slide()
		_update_animations()

func _update_animations() -> void:
	if is_hurt or is_dying or is_casting:
		return

	if velocity.length() > 0:
		if abs(velocity.x) > abs(velocity.y):
			if velocity.x > 0:
				facing = "right"
			else:
				facing = "left"
		else:
			if velocity.y > 0:
				facing = "down"
			else:
				facing = "up"
			
		active_sprite.play(facing)
	else:
		active_sprite.stop()

func _on_special_attack() -> void:
	if is_dying or not player or is_hurt:
		return
		
	if not is_inside_tree():
		return
		
	is_casting = true
	velocity = Vector2.ZERO
	active_sprite.stop()
	
	if pulse_tween and pulse_tween.is_valid():
		pulse_tween.pause()
	
	var tween = create_tween().set_parallel(true)
	var squash_scale = Vector2(default_sprite_scale.x * 1.3, default_sprite_scale.y * 0.75)
	tween.tween_property(active_sprite, "scale", squash_scale, 0.4).set_trans(Tween.TRANS_QUAD)
	tween.tween_property(active_sprite, "modulate", Color(0.3, 0.3, 0.3), 0.4)
	
	await get_tree().create_timer(0.4).timeout
	
	if is_dying or not is_inside_tree():
		return
	
	var dash_tween = create_tween().set_parallel(true)
	var stretch_scale = Vector2(default_sprite_scale.x * 0.85, default_sprite_scale.y * 1.2)
	dash_tween.tween_property(active_sprite, "scale", stretch_scale, 0.1)
	
	if pulse_tween and pulse_tween.is_valid():
		pulse_tween.play()
	else:
		active_sprite.modulate = base_color

	var proj_count = 8 if is_enraged else 4
	var angle_step = TAU / proj_count
	for i in range(proj_count):
		_shoot_projectile(Vector2.RIGHT.rotated(i * angle_step))
		
	if minion_scene:
		var minion_count = 2 if is_enraged else 1
		for i in range(minion_count):
			var minion = minion_scene.instantiate()
			var spawn_offset = Vector2(randf_range(-100, 100), randf_range(-100, 100))
			minion.global_position = global_position + spawn_offset
			
			var minion_stats = {
				"health": int(max_health * 0.03) + 10,
				"speed": original_speed * 1.1,
				"damage": int(attack_damage * 0.3),
				"scale": 0.6,
				"color": base_color,
				"exp": 0
			}
			get_parent().call_deferred("add_child", minion)
			minion.call_deferred("apply_stats", minion_stats)
		
	is_casting = false
	is_dashing = true
	dash_direction = global_position.direction_to(player.global_position)
	
	await get_tree().create_timer(0.35).timeout
	
	if is_dying or not is_inside_tree():
		return
		
	is_dashing = false
	
	var recover_tween = create_tween()
	recover_tween.tween_property(active_sprite, "scale", default_sprite_scale, 0.2).set_trans(Tween.TRANS_BOUNCE)

func _shoot_projectile(dir: Vector2) -> void:
	if not projectile_scene or is_dying: return
	
	var proj = projectile_scene.instantiate()
	proj.global_position = global_position
	proj.direction = dir
	proj.damage = int(attack_damage * 0.4)
	
	proj.modulate = base_color
	if is_enraged:
		proj.scale = Vector2(1.2, 1.2)
		
	get_tree().current_scene.add_child(proj)

func apply_burn(burn_damage: float) -> void:
	if is_burning or is_dying:
		return
		
	is_burning = true
	
	if not is_enraged:
		active_sprite.modulate = Color(1.0, 0.4, 0.1)
	
	for i in range(4):
		if is_dying:
			break
		await get_tree().create_timer(0.5).timeout
		take_damage(int(burn_damage))
		
	if not is_dying and not is_slowed and not is_casting and not is_enraged:
		active_sprite.modulate = base_color
		
	is_burning = false

func apply_slow(slow_multiplier: float) -> void:
	if is_slowed or is_dying:
		return
		
	is_slowed = true
	speed = original_speed * slow_multiplier
	
	if not is_enraged:
		active_sprite.modulate = Color(0.3, 0.8, 1.0)
	
	await get_tree().create_timer(3.0).timeout
	
	if not is_dying:
		speed = original_speed
		if not is_burning and not is_casting and not is_enraged:
			active_sprite.modulate = base_color
			
	is_slowed = false

func take_damage(amount: int) -> void:
	if is_dying:
		return
		
	health -= amount
	
	if health_bar and is_boss:
		health_bar.value = health
		
	if is_boss and phase == 1 and health <= max_health / 2:
		_transform_phase_two()
	
	var hp_percent = float(health) / float(max_health)
	
	if is_boss and AudioManager.has_method("set_music_speed"):
		var dynamic_bpm = 1.0 + ((1.0 - hp_percent) * 0.5)
		AudioManager.set_music_speed(dynamic_bpm)
	
	if damage_scene:
		var floating_text = damage_scene.instantiate()
		floating_text.global_position = global_position
		get_tree().current_scene.add_child(floating_text)
		if floating_text.has_method("setup"):
			floating_text.setup(amount)
	
	if health <= 0:
		_die()
	else:
		_play_hurt()

func _transform_phase_two() -> void:
	phase = 2
	is_enraged = true
	is_casting = true
	velocity = Vector2.ZERO
	active_sprite.stop()
	
	active_sprite.scale = default_sprite_scale
	
	var current_floor = Data.current_floor if "current_floor" in Data else 1
	var new_color = base_color
	var glow_color = Color(1.0, 0.0, 0.0)

	if current_floor == 1:
		new_color = Color(0.8, 0.7, 0.2)
		glow_color = Color(1.0, 0.9, 0.4)
		speed = original_speed * 1.3
		attack_damage = int(attack_damage * 1.2)
	elif current_floor == 2:
		new_color = Color(0.8, 0.2, 0.8)
		glow_color = Color(1.0, 0.4, 1.0)
		speed = original_speed * 1.5
		attack_damage = int(attack_damage * 1.3)
		special_timer.wait_time = 4.5
	else:
		new_color = Color(1.0, 0.0, 0.0)
		glow_color = Color(1.0, 0.4, 0.2)
		speed = original_speed * 1.8
		attack_damage = int(attack_damage * 1.4)
		special_timer.wait_time = 3.5

	var target_scale = scale * 1.3
	var tween = create_tween().set_parallel(true)
	tween.tween_property(self, "scale", target_scale, 1.0).set_trans(Tween.TRANS_BOUNCE)
	tween.tween_property(active_sprite, "modulate", new_color, 1.0)
	
	if boss_ui and not boss_ui is CanvasLayer:
		tween.tween_property(boss_ui, "scale", Vector2(1.0, 1.0) / target_scale, 1.0)
	
	base_color = new_color
	original_speed = speed
	
	await get_tree().create_timer(1.2).timeout
	if not is_inside_tree() or is_dying:
		return
		
	pulse_tween = create_tween().set_loops().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	pulse_tween.tween_property(active_sprite, "modulate", glow_color, 0.5)
	pulse_tween.tween_property(active_sprite, "modulate", base_color, 0.5)
		
	is_casting = false

func _play_hurt() -> void:
	if is_hurt or is_casting or is_dying:
		return
		
	is_hurt = true
	
	if boss_sfx and AudioManager.has_method("play_sfx_2d"):
		AudioManager.play_sfx_2d(boss_sfx, global_position, -5.0, 0.3, "hit")
		
	if pulse_tween and pulse_tween.is_valid():
		pulse_tween.pause()
		
	active_sprite.modulate = Color(3.0, 3.0, 3.0)
	active_sprite.play("hurt_" + facing)
	
	await get_tree().create_timer(0.15).timeout

	if is_dying or not is_inside_tree() or not active_sprite:
		return
		
	if pulse_tween and pulse_tween.is_valid():
		pulse_tween.play()
	else:
		active_sprite.modulate = base_color
		
	is_hurt = false

func _die() -> void:
	is_dying = true
	
	if pulse_tween and pulse_tween.is_valid():
		pulse_tween.kill()
	
	if boss_ui:
		boss_ui.hide()
		
	active_sprite.modulate = base_color
		
	var spawner = get_tree().current_scene.get_node_or_null("EnemySpawner")
	
	if is_boss:
		if AudioManager.has_method("stop_music"):
			AudioManager.stop_music()
		if AudioManager.has_method("set_music_speed"):
			AudioManager.set_music_speed(1.0)
		if spawner and spawner.has_method("notify_boss_defeated"):
			spawner.notify_boss_defeated()
	
	if boss_sfx and AudioManager.has_method("play_sfx_2d"):
		AudioManager.play_sfx_2d(boss_sfx, global_position, 0.0, 0.2, "death")
		
	active_sprite.play("death")
	await active_sprite.animation_finished
	
	if not is_inside_tree():
		return
	
	if player and player.has_method("add_kill"):
		player.add_kill()
		
	if seed_scene:
		var seed_drop_count = 1
		if is_boss:
			seed_drop_count = 50
			
		for i in range(seed_drop_count):
			var new_seed = seed_scene.instantiate()
			new_seed.global_position = global_position + Vector2(randf_range(-80, 80), randf_range(-80, 80))
			get_parent().call_deferred("add_child", new_seed)
		
	queue_free()

func _on_dash_timer_timeout() -> void:
	if not is_dasher or not player or is_dying or is_casting:
		return
		
	is_dashing = true
	dash_direction = global_position.direction_to(player.global_position)
	
	await get_tree().create_timer(0.4).timeout
	
	is_dashing = false
	dash_timer.start(3.0)

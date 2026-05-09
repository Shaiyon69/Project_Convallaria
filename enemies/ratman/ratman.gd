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

var is_boss: bool = false
var is_minion: bool = false
var is_casting: bool = false
var is_enraged: bool = false
var phase: int = 1

var shoot_timer: float = 0.0
var special_timer: Timer

var default_sprite_scale: Vector2 = Vector2(1.0, 1.0)
var pulse_tween: Tween
var base_color: Color = Color.WHITE

const SHADER_CODE = """
shader_type canvas_item;

uniform bool enraged = false;
uniform vec4 enraged_color : source_color = vec4(1.0, 0.0, 0.0, 1.0);
uniform float pulse_speed = 5.0;
uniform bool hit_flash = false;

void fragment() {
	vec4 tex_color = texture(TEXTURE, UV);
	
	if (hit_flash && tex_color.a > 0.0) {
		COLOR = vec4(1.0, 1.0, 1.0, tex_color.a);
	} else if (enraged && tex_color.a > 0.0) {
		float pulse = (sin(TIME * pulse_speed) + 1.0) * 0.5;
		vec4 glow = mix(tex_color, enraged_color, pulse * 0.55);
		COLOR = glow;
	} else {
		COLOR = tex_color;
	}
}
"""

@onready var anim = $AnimatedSprite2D
@onready var player = get_tree().get_first_node_in_group("player")
@onready var boss_ui = get_node_or_null("BossUI")
@onready var health_bar = get_node_or_null("BossUI/BossHealthBar")

var audio_player: AudioStreamPlayer2D
var sfx_stream: AudioStream = preload("res://enemies/ratman/ratman.ogg")

func _ready() -> void:
	current_health = max_health
	add_to_group("enemy")
	sfx_timer = randf_range(0.0, 2.0)
	shoot_timer = randf_range(2.0, 3.5)
	
	audio_player = AudioStreamPlayer2D.new()
	audio_player.stream = sfx_stream
	audio_player.max_distance = 400.0 
	audio_player.volume_db = -28.0 
	add_child(audio_player)
	
	default_sprite_scale = anim.scale
	
	var mat = ShaderMaterial.new()
	var shader = Shader.new()
	shader.code = SHADER_CODE
	mat.shader = shader
	anim.material = mat
	
	if boss_ui:
		boss_ui.hide()

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
	if stats.has("is_minion"):
		is_minion = stats["is_minion"]
	if stats.has("scale"):
		var new_scale = Vector2(stats["scale"], stats["scale"])
		scale = new_scale
		if boss_ui and not boss_ui is CanvasLayer:
			boss_ui.scale = Vector2(1.0, 1.0) / new_scale
	if stats.has("color"):
		base_color = stats["color"]
		anim.modulate = base_color
	if stats.has("base_pitch"):
		base_pitch = stats["base_pitch"]
		if audio_player:
			audio_player.pitch_scale = base_pitch
			
	if current_health >= 1000 or (stats.has("is_boss") and stats["is_boss"]):
		_setup_as_boss()

func _setup_as_boss() -> void:
	is_boss = true
	is_shooter = true 
	
	var minutes_survived = 0.0
	if player and "time_survived" in player:
		minutes_survived = player.time_survived / 60.0

	var hp_mult = 1.0 + (minutes_survived * 0.40)
	var dmg_mult = 1.0 + (minutes_survived * 0.15)
	
	# Lower base stats, but heavily scaled by time survived
	max_health = int((max_health * 0.35) * hp_mult)
	current_health = max_health
	attack_damage = int((attack_damage * 0.40) * dmg_mult)
	movement_speed += (minutes_survived * 2.0)
	
	if boss_ui:
		boss_ui.show()
	if health_bar:
		health_bar.max_value = max_health
		health_bar.value = current_health
		
	var boss_track = load("res://world/finalboss.ogg")
	if AudioManager.has_method("play_music") and boss_track:
		AudioManager.play_music(boss_track, -8.0)
		AudioManager.set_music_speed(1.0)
		
	special_timer = Timer.new()
	special_timer.wait_time = 7.0
	special_timer.autostart = true
	special_timer.timeout.connect(_on_special_attack)
	add_child(special_timer)

func _physics_process(delta: float) -> void:
	if is_dying or is_casting or not player:
		if is_dying and audio_player and audio_player.playing:
			if audio_player.get_playback_position() >= 0.93:
				audio_player.stop()
		return
		
	var direction = global_position.direction_to(player.global_position)
	var current_speed = movement_speed
	
	var missing_hp = 0.0
	if is_boss:
		missing_hp = 1.0 - (float(current_health) / float(max_health))
		current_speed += (missing_hp * 80.0) # Moves faster as he loses HP
		
	velocity = direction * current_speed
	
	# Shooting logic
	if is_shooter and global_position.distance_to(player.global_position) <= 400.0 + (missing_hp * 200.0):
		shoot_timer -= delta
		if shoot_timer <= 0.0:
			var p_scale = 1.0 + (missing_hp * 0.8) # Passive bullets get up to 80% bigger
			_shoot_projectile(direction, p_scale)
			
			var next_shot = randf_range(2.0, 3.5) - (missing_hp * 2.0) # Shoots much faster as HP drops
			if is_enraged: next_shot *= 0.5
			shoot_timer = max(0.3, next_shot)
	
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

func _shoot_projectile(dir: Vector2, scale_mult: float = 1.0) -> void:
	if not projectile_scene or is_dying: return
	
	var proj = projectile_scene.instantiate()
	proj.global_position = global_position
	proj.direction = dir
	
	var final_damage = attack_damage
	if is_boss:
		final_damage = int(attack_damage * (1.0 + scale_mult * 0.4))
	proj.damage = final_damage
	
	if is_minion:
		proj.modulate = Color(0.1, 0.1, 0.1)
		proj.scale = Vector2(0.8, 0.8)
	elif is_boss:
		if is_enraged:
			proj.modulate = Color(1.0, 0.2, 0.2)
		else:
			proj.modulate = Color(0.8, 0.0, 0.8)
		proj.scale = Vector2(scale_mult, scale_mult)
		
	get_tree().current_scene.add_child(proj)
	
	if is_inside_tree() and anim:
		var tween = create_tween()
		tween.tween_property(anim, "scale", Vector2(default_sprite_scale.x * 1.2, default_sprite_scale.y * 0.8), 0.1)
		tween.tween_property(anim, "scale", default_sprite_scale, 0.1)

func _on_special_attack() -> void:
	if is_dying or not player or not is_inside_tree(): return
	
	is_casting = true
	anim.stop()
	velocity = Vector2.ZERO
	
	if pulse_tween and pulse_tween.is_valid():
		pulse_tween.pause()
		
	var tween = create_tween().set_parallel(true)
	var squash_scale = Vector2(default_sprite_scale.x * 1.3, default_sprite_scale.y * 0.7)
	tween.tween_property(anim, "scale", squash_scale, 0.5).set_trans(Tween.TRANS_QUAD)
	tween.tween_property(anim, "modulate", Color(0.2, 0.2, 0.2), 0.5)
	
	await get_tree().create_timer(0.5).timeout
	if is_dying or not is_inside_tree(): return
	
	var recover_tween = create_tween().set_parallel(true)
	recover_tween.tween_property(anim, "scale", default_sprite_scale, 0.2).set_trans(Tween.TRANS_BOUNCE)
	
	if pulse_tween and pulse_tween.is_valid():
		pulse_tween.play()
	else:
		anim.modulate = base_color
	
	# Massive Dynamic Special Attack based on Missing HP
	var missing_hp = 1.0 - (float(current_health) / float(max_health))
	
	var base_dir = global_position.direction_to(player.global_position)
	var spread = deg_to_rad(15 + (missing_hp * 25))
	var proj_count = 5 + int(missing_hp * 12) # Up to 17 projectiles!
	if is_enraged: proj_count += 4 # Up to 21 projectiles!
	
	var p_scale = 1.0 + (missing_hp * 1.5) # Up to 2.5x bigger
	if is_enraged: p_scale *= 1.2
	
	for i in range(proj_count):
		var angle_offset = (i - (proj_count - 1) / 2.0) * spread
		_shoot_projectile(base_dir.rotated(angle_offset), p_scale)
		
	var minion_scene = load("res://enemies/ratman/ratman.tscn")
	if minion_scene:
		var minion_count = 3 + int(missing_hp * 6) # Up to 9 minions!
		if is_enraged: minion_count += 3 # Up to 12 minions!
		
		for i in range(minion_count):
			var minion = minion_scene.instantiate()
			var spawn_offset = Vector2(randf_range(-200, 200), randf_range(-200, 200))
			minion.global_position = global_position + spawn_offset
			
			var minion_stats = {
				"health": 80 + (Data.current_floor * 20),
				"speed": movement_speed * 1.1,
				"damage": int(attack_damage * 0.3),
				"scale": 0.8,
				"color": Color(0.3, 0.3, 0.3), 
				"base_pitch": 1.5,
				"is_shooter": true,
				"is_minion": true,
				"exp": 0 
			}
			get_parent().call_deferred("add_child", minion)
			minion.call_deferred("apply_stats", minion_stats)
			
	await get_tree().create_timer(0.5).timeout
	is_casting = false

func take_damage(amount: int) -> void:
	if is_dying:
		return
		
	current_health -= amount
	
	if health_bar and is_boss:
		health_bar.value = current_health
		
	var hp_percent = float(current_health) / float(max_health)
	
	if is_boss and phase == 1 and current_health <= max_health / 2:
		_transform_phase_two()
		
	if is_boss and special_timer:
		# Cooldown drastically drops as his HP lowers (From 7s down to 1.5s)
		special_timer.wait_time = max(1.5, 7.0 - ((1.0 - hp_percent) * 5.5))
		
	if is_boss and AudioManager.has_method("set_music_speed"):
		var dynamic_bpm = 1.0 + ((1.0 - hp_percent) * 0.6)
		AudioManager.set_music_speed(dynamic_bpm)
	
	if damage_scene:
		var dmg_num = damage_scene.instantiate()
		dmg_num.global_position = global_position
		get_tree().current_scene.add_child(dmg_num)
		dmg_num.setup(amount)
		
	if current_health <= 0:
		die()
	else:
		_play_hurt()

func _transform_phase_two() -> void:
	phase = 2
	is_enraged = true
	is_casting = true
	velocity = Vector2.ZERO
	anim.stop()
	
	anim.scale = default_sprite_scale
	
	var glow_color = Color(1.0, 0.2, 0.0)
	base_color = Color(0.8, 0.1, 0.1)
	
	movement_speed *= 1.3
	attack_damage = int(attack_damage * 1.3)
	
	var target_scale = scale * 1.25
	var tween = create_tween().set_parallel(true)
	tween.tween_property(self, "scale", target_scale, 1.0).set_trans(Tween.TRANS_BOUNCE)
	tween.tween_property(anim, "modulate", base_color, 1.0)
	
	if boss_ui and not boss_ui is CanvasLayer:
		tween.tween_property(boss_ui, "scale", Vector2(1.0, 1.0) / target_scale, 1.0)
		
	anim.material.set_shader_parameter("enraged", true)
	anim.material.set_shader_parameter("enraged_color", glow_color)
	
	await get_tree().create_timer(1.2).timeout
	if not is_inside_tree() or is_dying: return
	
	pulse_tween = create_tween().set_loops().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	pulse_tween.tween_property(anim, "modulate", glow_color, 0.5)
	pulse_tween.tween_property(anim, "modulate", base_color, 0.5)
	
	is_casting = false

func _play_hurt() -> void:
	if is_dying: return
	
	anim.material.set_shader_parameter("hit_flash", true)
	
	if pulse_tween and pulse_tween.is_valid():
		pulse_tween.pause()
		
	anim.modulate = Color(3.0, 3.0, 3.0)
	
	await get_tree().create_timer(0.15).timeout
	
	if is_inside_tree() and anim and anim.material and not is_dying:
		anim.material.set_shader_parameter("hit_flash", false)
		if pulse_tween and pulse_tween.is_valid():
			pulse_tween.play()
		else:
			anim.modulate = base_color

func die() -> void:
	is_dying = true
	collision_layer = 0
	collision_mask = 0
	anim.play("death")
	
	if pulse_tween and pulse_tween.is_valid():
		pulse_tween.kill()
		
	if anim and anim.material:
		anim.material.set_shader_parameter("hit_flash", false)
		anim.material.set_shader_parameter("enraged", false)
		anim.modulate = base_color
		
	if boss_ui:
		boss_ui.hide()
		
	if is_boss:
		var spawner = get_tree().current_scene.get_node_or_null("EnemySpawner")
		if AudioManager.has_method("stop_music"):
			AudioManager.stop_music()
		if AudioManager.has_method("set_music_speed"):
			AudioManager.set_music_speed(1.0)
		if spawner and spawner.has_method("notify_boss_defeated"):
			spawner.notify_boss_defeated()
	
	if audio_player and AudioManager._can_play_sfx("ratman_death", AudioManager.DEATH_COOLDOWN):
		audio_player.volume_db = -10.0 if is_boss else -15.0
		audio_player.pitch_scale = base_pitch * randf_range(0.9, 1.1)
		audio_player.play(0.70)
	
	if player and player.has_method("add_kill"):
		player.add_kill()
		
	await anim.animation_finished
	if not is_inside_tree(): return
	
	if seed_scene:
		var drop_count = 50 if is_boss else 1
		for i in range(drop_count):
			var seed_inst = seed_scene.instantiate()
			if is_boss:
				seed_inst.global_position = global_position + Vector2(randf_range(-100, 100), randf_range(-100, 100))
			else:
				seed_inst.global_position = global_position
			seed_inst.exp_amount = exp_value
			get_tree().current_scene.call_deferred("add_child", seed_inst)
			
	queue_free()

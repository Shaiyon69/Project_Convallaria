extends Node2D

@export var weapon_id: String = "wand"
@export var projectile_scene: PackedScene = preload("res://weapons/wand/wand_projectile.tscn")

@onready var attack_timer = $AttackTimer
@onready var player = get_parent().get_parent() 

func _ready() -> void:
	attack_timer.timeout.connect(_on_attack_timer_timeout)
	attack_timer.start()

func _process(_delta: float) -> void:
	if player and player.owned_weapons.has(weapon_id):
		var w_data = player.owned_weapons[weapon_id]
		var w_level = w_data["level"]
		
		# Safely clamp the level so we don't crash if they upgrade it past max base stats
		var safe_level = w_level
		if Data.WEAPONS.has(weapon_id) and Data.WEAPONS[weapon_id].has("max_level"):
			safe_level = min(w_level, Data.WEAPONS[weapon_id]["max_level"])
			
		var w_stats = Data.WEAPONS[weapon_id]["levels"][safe_level]
		var base_wait = w_stats["wait_time"]
		
		# Combine global fire rate (from shop) with specific weapon fire rate (from level ups)
		var global_fr = player.fire_rate_multiplier if "fire_rate_multiplier" in player else 1.0
		var new_wait_time = (base_wait * global_fr) / w_data["fire_rate"]
		
		new_wait_time = max(0.05, new_wait_time)
			
		if attack_timer.wait_time != new_wait_time:
			attack_timer.wait_time = new_wait_time

func _on_attack_timer_timeout() -> void:
	if not player or not player.owned_weapons.has(weapon_id):
		return
		
	var w_data = player.owned_weapons[weapon_id]
	var w_level = w_data["level"]
	var safe_level = min(w_level, Data.WEAPONS[weapon_id].get("max_level", 1))
	var w_stats = Data.WEAPONS[weapon_id]["levels"][safe_level]
	
	var target = _get_nearest_enemy()
	if target:
		var proj_count = w_stats["projectiles"]
		_shoot(target, proj_count, w_stats, w_data)

func _shoot(target: Node2D, count: int, base_stats: Dictionary, custom_stats: Dictionary) -> void:
	# Base damage * Global Player Damage * Specific Weapon Damage = Final Damage!
	var total_damage: float = float(base_stats["base_damage"])
	if "base_damage_multiplier" in player:
		total_damage *= player.base_damage_multiplier
	total_damage *= custom_stats["damage"] 
	
	var final_damage: int = round(total_damage)
	var base_dir = global_position.direction_to(target.global_position)
	
	for i in range(count):
		var proj = projectile_scene.instantiate()
		proj.global_position = global_position
		
		var spread_angle = deg_to_rad(15) 
		var angle_offset = (i - (count - 1) / 2.0) * spread_angle
		proj.direction = base_dir.rotated(angle_offset)
		
		proj.speed = base_stats["speed"]
		proj.player_ref = player

		# --- NEW: Apply the custom specific stats! ---
		proj.size_multiplier = custom_stats["size"]
		proj.pierce_count = custom_stats["pierce"]
		proj.ricochet_count = custom_stats["ricochet"]
		
		var crit_chance = 0.05
		if "base_crit_chance" in player:
			crit_chance += player.base_crit_chance
			
		if randf() <= crit_chance:
			proj.damage = final_damage * 2
			proj.modulate = Color(1.0, 0.8, 0.1)
			# Scale crit by size multiplier too!
			proj.scale = Vector2(1.5 * custom_stats["size"], 1.5 * custom_stats["size"])
		else:
			proj.damage = final_damage
			proj.scale = Vector2(custom_stats["size"], custom_stats["size"])
		
		if "imbue_fire" in player: proj.imbue_fire = player.imbue_fire
		if "imbue_frost" in player: proj.imbue_frost = player.imbue_frost
		
		get_tree().current_scene.add_child(proj)

func _get_nearest_enemy() -> Node2D:
	var enemies = get_tree().get_nodes_in_group("enemy")
	var nearest = null
	var min_dist = 800.0 
	
	for enemy in enemies:
		if enemy.get("is_dying") == true:
			continue
		var dist = global_position.distance_to(enemy.global_position)
		if dist < min_dist:
			min_dist = dist
			nearest = enemy
			
	return nearest

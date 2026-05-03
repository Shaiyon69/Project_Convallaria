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
		var w_level = player.owned_weapons[weapon_id]
		var w_stats = Data.WEAPONS[weapon_id]["levels"][w_level]
		
		var speed_buff = 0.0
		if "bonus_attacks" in player:
			speed_buff = player.bonus_attacks * 0.05
			
		var base_wait = w_stats["wait_time"]
		var new_wait_time = base_wait
		
		if "fire_rate_multiplier" in player:
			new_wait_time = (base_wait * player.fire_rate_multiplier) - speed_buff
		else:
			new_wait_time = base_wait - speed_buff
			
		new_wait_time = max(0.05, new_wait_time)
			
		if attack_timer.wait_time != new_wait_time:
			attack_timer.wait_time = new_wait_time

func _on_attack_timer_timeout() -> void:
	if not player or not player.owned_weapons.has(weapon_id):
		return
		
	var w_level = player.owned_weapons[weapon_id]
	var w_stats = Data.WEAPONS[weapon_id]["levels"][w_level]
	
	var target = _get_nearest_enemy()
	if target:
		var proj_count = w_stats["projectiles"]
		_shoot(target, proj_count, w_stats)

func _shoot(target: Node2D, count: int, stats: Dictionary) -> void:
	var total_damage: float = float(stats["base_damage"])
	if "damage_multiplier" in player:
		total_damage *= player.damage_multiplier
	var final_damage: int = round(total_damage)
	
	var base_dir = global_position.direction_to(target.global_position)
	
	for i in range(count):
		var proj = projectile_scene.instantiate()
		proj.global_position = global_position
		
		var spread_angle = deg_to_rad(15) 
		var angle_offset = (i - (count - 1) / 2.0) * spread_angle
		proj.direction = base_dir.rotated(angle_offset)
		
		proj.speed = stats["speed"]
		proj.damage = final_damage
		
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

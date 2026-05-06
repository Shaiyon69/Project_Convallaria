extends Area2D

@export var weapon_id: String = "poison_aura"

@onready var attack_timer = $AttackTimer
@onready var player = get_parent().get_parent()

func _ready() -> void:
	attack_timer.start()

func _process(_delta: float) -> void:
	if player and player.owned_weapons.has(weapon_id):

		var w_data = player.owned_weapons[weapon_id]
		var w_level = w_data["level"]

		var safe_level = w_level
		if Data.WEAPONS.has(weapon_id) and Data.WEAPONS[weapon_id].has("max_level"):
			safe_level = min(w_level, Data.WEAPONS[weapon_id]["max_level"])
			
		var w_stats = Data.WEAPONS[weapon_id]["levels"][safe_level]
		var current_w_scale = w_stats["scale"]
		var global_aoe = player.aoe_multiplier if "aoe_multiplier" in player else 1.0
		var custom_size = w_data["size"]
		
		var final_scale = current_w_scale * global_aoe * custom_size
		scale = Vector2(final_scale, final_scale)

		var base_wait = w_stats["wait_time"]
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
	var safe_level = w_level
	if Data.WEAPONS.has(weapon_id) and Data.WEAPONS[weapon_id].has("max_level"):
		safe_level = min(w_level, Data.WEAPONS[weapon_id]["max_level"])
		
	var w_stats = Data.WEAPONS[weapon_id]["levels"][safe_level]
	var base_dmg = w_stats["base_damage"]
	
	var enemies = get_overlapping_bodies()
	$AnimationPlayer.play("poison")

	var total_damage: float = float(base_dmg)

	if "base_damage_multiplier" in player:
		total_damage *= player.base_damage_multiplier

	total_damage *= w_data["damage"]
		
	var final_damage: int = round(total_damage)
	
	for enemy in enemies:
		if enemy.is_in_group("enemy") and enemy.has_method("take_damage"):
			enemy.take_damage(final_damage)
			
			if "imbue_fire" in player and player.imbue_fire and enemy.has_method("apply_burn"):
				enemy.apply_burn(final_damage * 0.2)
			if "imbue_frost" in player and player.imbue_frost and enemy.has_method("apply_slow"):
				enemy.apply_slow(0.5)

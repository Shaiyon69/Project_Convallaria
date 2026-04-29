extends Area2D

@export var weapon_icon: Texture2D
@export var damage: int = 15
@export var base_wait_time: float = 0.2

@onready var attack_timer = $AttackTimer
@onready var player = get_parent().get_parent()

func _ready() -> void:
	attack_timer.wait_time = base_wait_time
	attack_timer.start()

func _process(_delta: float) -> void:
	if player:
		if "aoe_multiplier" in player:
			scale = Vector2(player.aoe_multiplier, player.aoe_multiplier)
			
		var speed_buff = 0.0
		if "bonus_attacks" in player:
			speed_buff = player.bonus_attacks * 0.05
			
		var new_wait_time = base_wait_time
		if "fire_rate_multiplier" in player:
			new_wait_time = base_wait_time * player.fire_rate_multiplier - speed_buff
		else:
			new_wait_time = base_wait_time - speed_buff
			
		if new_wait_time < 0.05:
			new_wait_time = 0.05
			
		if attack_timer.wait_time != new_wait_time:
			attack_timer.wait_time = new_wait_time

func _on_attack_timer_timeout() -> void:
	var enemies = get_overlapping_bodies()
	$AnimationPlayer.play("poison")
	
	var total_damage: float = float(damage)
	
	if "damage_multiplier" in player:
		total_damage = damage * player.damage_multiplier
		
	var final_damage: int = round(total_damage)
	
	for enemy in enemies:
		if enemy.is_in_group("enemy") and enemy.has_method("take_damage"):
			enemy.take_damage(final_damage)
			
			if "imbue_fire" in player and player.imbue_fire and enemy.has_method("apply_burn"):
				enemy.apply_burn(final_damage * 0.2)
			if "imbue_frost" in player and player.imbue_frost and enemy.has_method("apply_slow"):
				enemy.apply_slow(0.5)

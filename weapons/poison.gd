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
	if "fire_rate_multiplier" in player:
		attack_timer.wait_time = base_wait_time / player.fire_rate_multiplier

func _on_attack_timer_timeout() -> void:
	var enemies = get_overlapping_bodies()
	$AnimationPlayer.play("poison")
	
	var total_damage: float = float(damage)
	
	if "damage_multiplier" in player:
		total_damage = damage * player.damage_multiplier
		
	var final_damage: int = round(total_damage)
	
	for enemy in enemies:
		if enemy.has_method("take_damage"):
			enemy.take_damage(final_damage)

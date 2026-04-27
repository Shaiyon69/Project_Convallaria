extends Area2D

@export var damage: int = 2
@export var base_wait_time: float = 0.2

@onready var attack_timer = $AttackTimer

func _process(_delta: float) -> void:
	var player = get_parent()
	if "fire_rate_multiplier" in player:
		attack_timer.wait_time = base_wait_time / player.fire_rate_multiplier

func _on_attack_timer_timeout() -> void:
	var enemies = get_overlapping_bodies()
	$AnimationPlayer.play("poison")
	
	var total_damage = damage
	var player = get_parent()
	if "bonus_damage" in player:
		total_damage += player.bonus_damage
	
	for enemy in enemies:
		if enemy.has_method("take_damage"):
			enemy.take_damage(total_damage)

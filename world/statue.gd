extends Area2D

var is_activated: bool = false

@onready var sprite = $Sprite2D

func _process(_delta: float) -> void:
	if not is_activated and has_overlapping_bodies() and Input.is_action_just_pressed("ui_accept"):
		for body in get_overlapping_bodies():
			if body.is_in_group("player"):
				_activate(body)

func _activate(player: Node2D) -> void:
	is_activated = true
	if sprite:
		sprite.modulate = Color(0.5, 1.0, 0.5) 
	
	player.current_health = player.max_health
	if player.get("hud") and player.hud.has_method("update_health"):
		player.hud.update_health(player.current_health, player.max_health)

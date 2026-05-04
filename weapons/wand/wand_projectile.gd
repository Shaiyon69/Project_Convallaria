extends Area2D

var speed: float = 400.0
var damage: int = 20
var direction: Vector2 = Vector2.ZERO

var imbue_fire: bool = false
var imbue_frost: bool = false

var player_ref: Node2D = null

func _ready() -> void:
	body_entered.connect(_on_body_entered)
	if has_node("VisibleOnScreenNotifier2D"):
		$VisibleOnScreenNotifier2D.screen_exited.connect(queue_free)

func _physics_process(delta: float) -> void:
	global_position += direction * speed * delta
	rotation = direction.angle()

func _on_body_entered(body: Node2D) -> void:
	if body.is_in_group("enemy") and body.has_method("take_damage"):

		var was_full_hp = false
		if "health" in body and "max_health" in body:
			was_full_hp = (body.health >= body.max_health)

		body.take_damage(damage)

		if was_full_hp and "health" in body and body.health <= 0:
			if player_ref and "base_crit_chance" in player_ref:
				player_ref.base_crit_chance += 0.001
				
		if imbue_fire and body.has_method("apply_burn"):
			body.apply_burn(damage * 0.2)
		if imbue_frost and body.has_method("apply_slow"):
			body.apply_slow(0.5)
			
		queue_free()

	elif not body.is_in_group("enemy") and not body.is_in_group("exp_seed"):
		queue_free()

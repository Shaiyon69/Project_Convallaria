extends Area2D

@export var exp_amount: int = 1

var is_magnetic: bool = false
var player: Node2D = null
var current_speed: float = 0.0
var max_speed: float = 600.0
var acceleration: float = 1200.0

func _ready() -> void:
	if not body_entered.is_connected(_on_body_entered):
		body_entered.connect(_on_body_entered)

func _physics_process(delta: float) -> void:
	if is_magnetic and player:
		current_speed = move_toward(current_speed, max_speed, acceleration * delta)
		var direction = global_position.direction_to(player.global_position)
		global_position += direction * current_speed * delta

func pull_to_player(target: Node2D) -> void:
	player = target
	is_magnetic = true

func _on_body_entered(body: Node2D) -> void:
	if body.is_in_group("player"):
		if body.has_method("gain_experience"):
			body.gain_experience(exp_amount)
		queue_free()

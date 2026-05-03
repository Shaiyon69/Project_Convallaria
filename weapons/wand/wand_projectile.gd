extends Area2D

var speed: float = 400.0
var damage: int = 20
var direction: Vector2 = Vector2.ZERO

var imbue_fire: bool = false
var imbue_frost: bool = false

func _ready() -> void:
	body_entered.connect(_on_body_entered)
	if $VisibleOnScreenNotifier2D:
		$VisibleOnScreenNotifier2D.screen_exited.connect(queue_free)

func _physics_process(delta: float) -> void:
	global_position += direction * speed * delta
	rotation = direction.angle()

func _on_body_entered(body: Node2D) -> void:
	if body.is_in_group("enemy") and body.has_method("take_damage"):
		body.take_damage(damage)
		
		if imbue_fire and body.has_method("apply_burn"):
			body.apply_burn(damage * 0.2)
		if imbue_frost and body.has_method("apply_slow"):
			body.apply_slow(0.5)
			
		queue_free()

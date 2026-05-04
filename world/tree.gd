extends StaticBody2D

@onready var sprite = $Tree1
@onready var transparency_zone = $TransparencyZone
@onready var collision_shape = $CollisionShape2D

var is_destroyed: bool = false

func _ready() -> void:
	transparency_zone.body_entered.connect(_on_zone_entered)
	transparency_zone.body_exited.connect(_on_zone_exited)

func _on_zone_entered(body: Node2D) -> void:
	if is_destroyed: return
	if body.is_in_group("player"):
		var tween = create_tween()
		tween.tween_property(sprite, "modulate:a", 0.4, 0.2)

func _on_zone_exited(body: Node2D) -> void:
	if is_destroyed: return
	if body.is_in_group("player"):
		var tween = create_tween()
		tween.tween_property(sprite, "modulate:a", 1.0, 0.2)

func destroy() -> void:
	if is_destroyed:
		return
		
	is_destroyed = true

	collision_shape.set_deferred("disabled", true)
	transparency_zone.monitoring = false
	var tween = create_tween()
	tween.set_parallel(true)
	tween.tween_property(sprite, "modulate", Color(0.3, 0.3, 0.3, 0.2), 0.5)
	tween.tween_property(sprite, "scale", Vector2(1.1, 0.2), 0.5)

extends CanvasLayer

@onready var interact_container = $Interact 

func _ready() -> void:
	if interact_container:
		interact_container.hide()

func show_interact_button() -> void:
	if interact_container and not interact_container.visible:
		interact_container.show()
		interact_container.scale = Vector2(0, 0)
		var tween = create_tween().set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
		tween.tween_property(interact_container, "scale", Vector2(1, 1), 0.2)

func hide_interact_button() -> void:
	if interact_container and interact_container.visible:
		var tween = create_tween().set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_IN)
		tween.tween_property(interact_container, "scale", Vector2(0, 0), 0.15)
		tween.tween_callback(interact_container.hide)

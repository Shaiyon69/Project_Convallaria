extends CanvasLayer

@onready var interact_container: Control = $Interact

func _ready() -> void:
	if interact_container:
		interact_container.hide()

func _input(event: InputEvent) -> void:
	if not interact_container or not interact_container.visible:
		return

	if event is InputEventScreenTouch and event.pressed:
		if interact_container.get_global_rect().has_point(event.position):
			_press_interact()

	if event is InputEventMouseButton and event.pressed:
		if interact_container.get_global_rect().has_point(event.position):
			_press_interact()

func _press_interact() -> void:
	print("MOBILE INTERACT PRESSED")

	Input.action_press("interact")
	await get_tree().process_frame
	Input.action_release("interact")

func show_interact_button() -> void:
	if interact_container and not interact_container.visible:
		interact_container.show()
		interact_container.scale = Vector2(0, 0)
		var tween := create_tween().set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
		tween.tween_property(interact_container, "scale", Vector2(1, 1), 0.2)

func hide_interact_button() -> void:
	if interact_container and interact_container.visible:
		var tween := create_tween().set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_IN)
		tween.tween_property(interact_container, "scale", Vector2(0, 0), 0.15)
		tween.tween_callback(interact_container.hide)

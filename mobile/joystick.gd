extends Control

@onready var knob = $Knob
var max_distance: float = 50.0

var center: Vector2
var touched: bool = false
var touch_index: int = -1

func _ready() -> void:
	if not OS.has_feature("mobile") and not OS.has_feature("editor"):
		get_parent().hide()
		set_process_input(false)
		return

	center = size / 2.0
	knob.position = center

func _input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		if event.pressed and get_global_rect().has_point(event.position):
			touched = true
			touch_index = event.index
		elif not event.pressed and event.index == touch_index:
			touched = false
			touch_index = -1
			knob.position = center
			_update_virtual_keys(Vector2.ZERO)

	if event is InputEventScreenDrag and touched and event.index == touch_index:
		var local_pos = event.position - global_position
		var direction = (local_pos - center).limit_length(max_distance)
		knob.position = center + direction

		_update_virtual_keys(direction / max_distance)

func _update_virtual_keys(vector: Vector2) -> void:
	# Horizontal Movement
	if vector.x > 0:
		Input.action_press("move_right", vector.x)
		Input.action_release("move_left")
	elif vector.x < 0:
		Input.action_press("move_left", abs(vector.x))
		Input.action_release("move_right")
	else:
		Input.action_release("move_left")
		Input.action_release("move_right")

	# Vertical Movement
	if vector.y > 0:
		Input.action_press("move_down", vector.y)
		Input.action_release("move_up")
	elif vector.y < 0:
		Input.action_press("move_up", abs(vector.y))
		Input.action_release("move_down")
	else:
		Input.action_release("move_up")
		Input.action_release("move_down")

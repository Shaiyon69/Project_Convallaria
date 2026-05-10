extends Control

var action_name: String = "interact"

@onready var interact_btn = $HBoxContainer/InteractButton
@onready var sfx_click = preload("res://ui/menu_click.mp3")

var _base_scale: Vector2
var _active_tween: Tween
var _touch_id: int = -1

func _ready() -> void:
	_base_scale = interact_btn.scale
	interact_btn.pivot_offset = interact_btn.size / 2.0
	interact_btn.gui_input.connect(_on_gui_input)

func _on_gui_input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		if event.pressed and _touch_id == -1:
			_touch_id = event.index
			_press_button()
		elif not event.pressed and event.index == _touch_id:
			_touch_id = -1
			_release_button()
	elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
		if event.pressed:
			_press_button()
		else:
			_release_button()

func _press_button() -> void:
	var event = InputEventAction.new()
	event.action = action_name
	event.pressed = true
	Input.parse_input_event(event)
	
	if _active_tween and _active_tween.is_valid():
		_active_tween.kill()
		
	_active_tween = create_tween().set_parallel(true)
	_active_tween.tween_property(interact_btn, "scale", _base_scale * 0.8, 0.05).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	_active_tween.tween_property(interact_btn, "modulate", Color(0.7, 0.7, 0.7), 0.05)
	
	_play_sfx(sfx_click)

func _release_button() -> void:
	var event = InputEventAction.new()
	event.action = action_name
	event.pressed = false
	Input.parse_input_event(event)
		
	if _active_tween and _active_tween.is_valid():
		_active_tween.kill()
			
	_active_tween = create_tween().set_parallel(true)
	_active_tween.tween_property(interact_btn, "scale", _base_scale, 0.4).set_trans(Tween.TRANS_ELASTIC).set_ease(Tween.EASE_OUT)
	_active_tween.tween_property(interact_btn, "modulate", Color.WHITE, 0.2)

func _play_sfx(stream: AudioStream, start_offset: float = 0.62) -> void:
	var player = AudioStreamPlayer.new()
	player.stream = stream
	player.process_mode = Node.PROCESS_MODE_ALWAYS
	add_child(player)
	player.play(start_offset)
	player.finished.connect(player.queue_free)

extends Control

var action_name: String = "interact"

@onready var interact_btn = $HBoxContainer/InteractButton
@onready var sfx_click = preload("res://ui/menu_click.mp3")

var _base_scale: Vector2
var _active_tween: Tween

func _ready() -> void:
	_base_scale = interact_btn.scale
	interact_btn.pivot_offset = interact_btn.size / 2.0
	
	interact_btn.button_down.connect(_on_button_down)
	interact_btn.button_up.connect(_on_button_up)
	interact_btn.mouse_exited.connect(_on_button_up)

func _on_button_down() -> void:
	Input.action_press(action_name)
	
	if _active_tween and _active_tween.is_valid():
		_active_tween.kill()
		
	_active_tween = create_tween().set_parallel(true)
	_active_tween.tween_property(interact_btn, "scale", _base_scale * 0.8, 0.05).set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	_active_tween.tween_property(interact_btn, "modulate", Color(0.7, 0.7, 0.7), 0.05)
	
	_play_sfx(sfx_click)

func _on_button_up() -> void:
	if Input.is_action_pressed(action_name):
		Input.action_release(action_name)
		
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

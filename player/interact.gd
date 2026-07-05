extends Control

@export var action_name: String = "interact"

@onready var interact_btn: BaseButton = $HBoxContainer/InteractButton
@onready var sfx_hover: AudioStream = preload("res://ui/menu_hover.mp3")
@onready var sfx_click: AudioStream = preload("res://ui/menu_click.mp3")

var _base_scale: Vector2
var _target_scale: Vector2

func _ready() -> void:
	interact_btn.disabled = false
	interact_btn.mouse_filter = Control.MOUSE_FILTER_STOP
	interact_btn.action_mode = BaseButton.ACTION_MODE_BUTTON_PRESS

	_base_scale = interact_btn.scale
	_target_scale = _base_scale
	interact_btn.pivot_offset = interact_btn.size / 2.0

	interact_btn.mouse_entered.connect(_on_button_hover)
	interact_btn.mouse_exited.connect(_on_button_exit)
	interact_btn.pressed.connect(_on_button_pressed)

func _on_button_hover() -> void:
	print("Interact hover")
	interact_btn.pivot_offset = interact_btn.size / 2.0
	_target_scale = _base_scale * 1.1

	var tween := create_tween().set_trans(Tween.TRANS_QUINT).set_ease(Tween.EASE_OUT)
	tween.tween_property(interact_btn, "scale", _target_scale, 0.15)

	_play_sfx(sfx_hover)

func _on_button_exit() -> void:
	interact_btn.pivot_offset = interact_btn.size / 2.0
	_target_scale = _base_scale

	var tween := create_tween().set_trans(Tween.TRANS_QUINT).set_ease(Tween.EASE_OUT)
	tween.tween_property(interact_btn, "scale", _target_scale, 0.15)

func _on_button_pressed() -> void:
	print("MOBILE INTERACT PRESSED")

	Input.action_press(action_name)

	var tween := create_tween().set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tween.tween_property(interact_btn, "scale", _base_scale * 0.9, 0.05)
	tween.tween_property(interact_btn, "scale", _target_scale, 0.15)

	_play_sfx(sfx_click)

	await get_tree().process_frame
	Input.action_release(action_name)

func _play_sfx(stream: AudioStream, start_offset: float = 0.62) -> void:
	var player := AudioStreamPlayer.new()
	player.stream = stream
	player.process_mode = Node.PROCESS_MODE_ALWAYS
	add_child(player)
	player.play(start_offset)
	player.finished.connect(player.queue_free)

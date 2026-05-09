extends Area2D

var speed: float = 250.0
var damage: int = 5
var direction: Vector2 = Vector2.ZERO

var sfx_shoot = preload("res://enemies/shooting.mp3")

func _ready() -> void:
	_play_shoot_sound()
	body_entered.connect(_on_body_entered)
	
	if has_node("VisibleOnScreenNotifier2D"):
		$VisibleOnScreenNotifier2D.screen_exited.connect(queue_free)

func _physics_process(delta: float) -> void:
	global_position += direction * speed * delta
	rotation = direction.angle()

func _on_body_entered(body: Node2D) -> void:
	if body.is_in_group("player") and body.has_method("take_damage"):
		body.take_damage(damage)
		queue_free()

	elif not body.is_in_group("enemy") and not body.is_in_group("exp_seed"):
		queue_free()

func _play_shoot_sound() -> void:
	var audio_player = AudioStreamPlayer2D.new()
	audio_player.stream = sfx_shoot
	audio_player.volume_db = -9.0
	audio_player.global_position = global_position
	
	get_tree().current_scene.call_deferred("add_child", audio_player)
	audio_player.call_deferred("play", 0.52)
	
	await get_tree().create_timer(0.1).timeout
	if is_instance_valid(audio_player):
		audio_player.queue_free()

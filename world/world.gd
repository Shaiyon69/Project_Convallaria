extends Node2D

@export var slime_scene: PackedScene = preload("res://enemies/slime.tscn")
@onready var player = $Player 

func _on_spawn_timer_timeout() -> void:
	var spawn_path = player.get_node("Path2D/PathFollow2D")
	spawn_path.progress_ratio = randf()

	var new_slime = slime_scene.instantiate()
	new_slime.global_position = spawn_path.global_position

	add_child(new_slime)

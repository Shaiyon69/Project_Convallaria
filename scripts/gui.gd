extends Control

func _ready() -> void:
	get_tree().paused = false

func _on_start_button_pressed() -> void:
	TransitionManager.change_scene("res://scenes/world.tscn")

func _on_quit_button_pressed() -> void:
	get_tree().quit()

func _on_options_button_pressed() -> void:
	pass

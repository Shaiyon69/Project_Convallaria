extends CanvasLayer

@onready var color_rect = $ColorRect
@onready var animation_player = $AnimationPlayer

func _ready() -> void:
	color_rect.color.a = 0.0
	color_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE

func change_scene(target_scene: String) -> void:
	color_rect.mouse_filter = Control.MOUSE_FILTER_STOP
	
	if animation_player.has_animation("fade_in"):
		animation_player.play("fade_in")
		await animation_player.animation_finished
	
	get_tree().change_scene_to_file(target_scene)
	
	if animation_player.has_animation("fade_out"):
		animation_player.play("fade_out")
		await animation_player.animation_finished
	
	color_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE

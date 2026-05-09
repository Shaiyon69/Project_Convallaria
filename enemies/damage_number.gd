extends Node2D

func setup(amount: int) -> void:
	$Label.text = str(amount)

	var tween = create_tween()
	
	tween.set_parallel(true)
	tween.tween_property(self, "position", position + Vector2(0, -50), 0.5).set_trans(Tween.TRANS_QUART).set_ease(Tween.EASE_OUT)
	tween.tween_property(self, "modulate:a", 0.0, 0.5)
	tween.chain().tween_callback(queue_free)

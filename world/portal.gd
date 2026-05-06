extends Area2D

@export var boss_scene: PackedScene
@export var next_level_path: String = "res://world/world.tscn"

var player_in_range: bool = false
var current_state: String = "corrupted" 

@onready var animated_sprite = $AnimatedSprite2D
@onready var interact_label = $InteractLabel

func _ready() -> void:
	animated_sprite.play("corrupted")
	interact_label.hide()
	body_entered.connect(_on_body_entered)
	body_exited.connect(_on_body_exited)

func _process(_delta: float) -> void:
	if player_in_range and Input.is_action_just_pressed("interact"):
		if current_state == "corrupted":
			_summon_boss()
		elif current_state == "purified":
			_teleport_player()

func _on_body_entered(body: Node2D) -> void:
	if body.is_in_group("player"):
		player_in_range = true
		_update_label_visibility()

func _on_body_exited(body: Node2D) -> void:
	if body.is_in_group("player"):
		player_in_range = false
		_update_label_visibility()

func _update_label_visibility() -> void:
	var tree = get_tree()
	if tree == null:
		return

	if player_in_range and (current_state == "corrupted" or current_state == "purified"):
		interact_label.show()
		
		if OS.has_feature("mobile") or OS.has_feature("editor"):
			var mobile_ui = tree.get_first_node_in_group("mobile_ui")
			if mobile_ui and mobile_ui.has_method("show_interact_button"):
				mobile_ui.show_interact_button()
	else:
		interact_label.hide()
		
		if OS.has_feature("mobile") or OS.has_feature("editor"):
			var mobile_ui = tree.get_first_node_in_group("mobile_ui")
			if mobile_ui and mobile_ui.has_method("hide_interact_button"):
				mobile_ui.hide_interact_button()

func _summon_boss() -> void:
	current_state = "combat"
	_update_label_visibility()
	
	if boss_scene:
		var boss = boss_scene.instantiate()
		get_parent().add_child(boss)
		boss.global_position = _calculate_spawn_position()
		
		if boss.has_method("apply_stats"):
			boss.apply_stats(Data.ENEMIES["boss"])
		
		boss.tree_exited.connect(_on_boss_defeated)
	else:
		print("ERROR: Boss Scene is not assigned in the Portal Inspector!")

func _calculate_spawn_position() -> Vector2:
	var grass_layer = get_parent().get_node_or_null("GrassLayer")
	var tree = get_tree()
	if tree == null:
		return global_position
		
	var player = tree.get_first_node_in_group("player")
	
	if not grass_layer or not player:
		return global_position
		
	var portal_cell = grass_layer.local_to_map(global_position)
	var valid_positions = []
	
	for x in range(-8, 9):
		for y in range(-8, 9):
			var check_cell = portal_cell + Vector2i(x, y)
			if grass_layer.get_cell_source_id(check_cell) != -1:
				var world_pos = grass_layer.map_to_local(check_cell)
				if world_pos.distance_to(player.global_position) > 150.0:
					valid_positions.append(world_pos)
					
	if valid_positions.size() > 0:
		var random_index = randi() % valid_positions.size()
		return valid_positions[random_index]
		
	return global_position

func _on_boss_defeated() -> void:
	if not is_inside_tree():
		return

	current_state = "purified"
	animated_sprite.play("purified")
	_update_label_visibility() 

func _teleport_player() -> void:
	current_state = "teleporting" 
	_update_label_visibility() 
	get_tree().paused = false
	
	var tree = get_tree()
	if tree != null:
		var player = tree.get_first_node_in_group("player")
		if player and player.has_method("save_data"):
			player.save_data()
			
	Data.current_floor += 1
	
	TransitionManager.change_scene(next_level_path)

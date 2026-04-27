extends Node2D

@export var slime_scene: PackedScene = preload("res://enemy/slime/slime.tscn")
@export var spawn_radius: float = 800.0

@onready var grass_layer: TileMapLayer = $"../Map/GrassLayer"

var player: Node2D
var min_wait_time: float = 0.5
var spawn_count: int = 1
var boss_spawned: bool = false
var max_enemies: int = 300

func _ready() -> void:
	player = get_tree().get_first_node_in_group("player")

func _on_spawn_timer_timeout() -> void:
	if not player or not grass_layer:
		return
		
	if get_tree().get_node_count_in_group("enemy") >= max_enemies:
		return
		
	for i in range(spawn_count):
		var valid_spawn = false
		var spawn_pos = Vector2.ZERO
		var attempts = 0
		
		while not valid_spawn and attempts < 50:
			var angle = randf_range(0.0, TAU)
			var spawn_offset = Vector2.RIGHT.rotated(angle) * spawn_radius
			var raw_pos = player.global_position + spawn_offset
			
			var map_coords = grass_layer.local_to_map(raw_pos)
			
			if grass_layer.get_cell_tile_data(map_coords) != null:
				spawn_pos = NavigationServer2D.map_get_closest_point(get_world_2d().navigation_map, raw_pos)
				valid_spawn = true
			
			attempts += 1
		
		if valid_spawn:
			var new_slime = slime_scene.instantiate()
			new_slime.global_position = spawn_pos
			
			var enemy_types = _get_allowed_enemies(player.time_survived)
			var random_type = enemy_types[randi() % enemy_types.size()]
			
			get_parent().add_child(new_slime)
			new_slime.apply_stats(Data.ENEMIES[random_type])

func _on_difficulty_timer_timeout() -> void:
	if not player:
		return

	if player.time_survived >= 300.0 and not boss_spawned:
		_spawn_boss()
		return

	if $SpawnTimer.wait_time > min_wait_time:
		$SpawnTimer.wait_time -= 0.05
	else:
		spawn_count += 1

func _spawn_boss() -> void:
	boss_spawned = true
	
	var valid_spawn = false
	var spawn_pos = Vector2.ZERO
	
	while not valid_spawn:
		var angle = randf_range(0.0, TAU)
		var spawn_offset = Vector2.RIGHT.rotated(angle) * spawn_radius
		var raw_pos = player.global_position + spawn_offset
		
		var map_coords = grass_layer.local_to_map(raw_pos)
		if grass_layer.get_cell_tile_data(map_coords) != null:
			spawn_pos = NavigationServer2D.map_get_closest_point(get_world_2d().navigation_map, raw_pos)
			valid_spawn = true
	
	var boss_slime = slime_scene.instantiate()
	boss_slime.global_position = spawn_pos
	
	get_parent().add_child(boss_slime)
	boss_slime.apply_stats(Data.ENEMIES["boss"])

func _get_allowed_enemies(time: float) -> Array:
	if time < 60.0:
		return ["basic"]
	elif time < 120.0:
		return ["basic", "basic", "basic", "runner", "swarm"]
	elif time < 180.0:
		return ["basic", "basic", "runner", "swarm", "swarm", "brute", "dasher"]
	else:
		return ["basic", "basic", "runner", "swarm", "brute", "tank", "dasher", "dasher"]

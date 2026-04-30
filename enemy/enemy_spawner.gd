extends Node2D

@export var slime_scene: PackedScene = preload("res://enemy/slime/slime.tscn")
@export var portal_scene: PackedScene
@export var spawn_radius: float = 800.0

@onready var grass_layer: TileMapLayer = $"../Map/GrassLayer"

var player: Node2D
var min_wait_time: float = 0.5
var spawn_count: int = 1
var boss_spawned: bool = false
var boss_defeated: bool = false
var max_enemies: int = 300
var last_processed_second: int = -1

var horde_events: Dictionary = {
	60: {"type": "swarm", "amount": 30},
	120: {"type": "brute", "amount": 15},
	180: {"type": "dasher", "amount": 40}
}

func _ready() -> void:
	player = get_tree().get_first_node_in_group("player")

func _process(_delta: float) -> void:
	if not player:
		return
		
	var current_second = int(player.time_survived)
	if current_second != last_processed_second:
		last_processed_second = current_second
		_check_time_events(current_second)

func _check_time_events(current_second: int) -> void:
	if horde_events.has(current_second):
		_spawn_horde(horde_events[current_second]["type"], horde_events[current_second]["amount"])
		
	if current_second >= 600 and not boss_spawned:
		_spawn_portal()

func _on_spawn_timer_timeout() -> void:
	if not player or not grass_layer:
		return
		
	if get_tree().get_node_count_in_group("enemy") >= max_enemies:
		return
		
	for i in range(spawn_count):
		_spawn_single_enemy()

func _is_safe_spawn_area(center_coords: Vector2i) -> bool:
	for x in range(-1, 2):
		for y in range(-1, 2):
			if grass_layer.get_cell_tile_data(center_coords + Vector2i(x, y)) == null:
				return false
	return true

func _spawn_single_enemy(specific_type: String = "") -> void:
	var valid_spawn = false
	var spawn_pos = Vector2.ZERO
	var attempts = 0
	
	while not valid_spawn and attempts < 50:
		var angle = randf_range(0.0, TAU)
		var spawn_offset = Vector2.RIGHT.rotated(angle) * spawn_radius
		var raw_pos = player.global_position + spawn_offset
		
		var map_coords = grass_layer.local_to_map(raw_pos)

		if _is_safe_spawn_area(map_coords):
			spawn_pos = raw_pos 
			valid_spawn = true
		
		attempts += 1
	
	if valid_spawn:
		var new_slime = slime_scene.instantiate()
		new_slime.global_position = spawn_pos
		get_parent().add_child(new_slime)
		
		var final_stats: Dictionary
		var minutes_survived = int(player.time_survived) / 60
		var spawn_death_slime = (minutes_survived >= 10) or boss_defeated
		
		if spawn_death_slime and specific_type == "":
			final_stats = Data.ENEMIES["death_slime"].duplicate()
			
			var over_time = max(0, minutes_survived - 10)
			if boss_defeated and over_time == 0:
				over_time = 1
				
			var scaling_factor = 1.0 + (over_time * 0.5)
			final_stats["health"] = int(final_stats["health"] * scaling_factor)
			final_stats["damage"] = int(final_stats["damage"] * scaling_factor)
			final_stats["speed"] += over_time * 15.0

			final_stats["scale"] += over_time * 0.25 
			var shade = max(0.0, 0.15 - (over_time * 0.03))
			final_stats["color"] = Color(shade, 0.0, shade)
		else:
			var enemy_type = specific_type
			if enemy_type == "":
				var enemy_types = _get_allowed_enemies(player.time_survived)
				enemy_type = enemy_types[randi() % enemy_types.size()]
				
			final_stats = Data.ENEMIES[enemy_type]
			
		new_slime.apply_stats(final_stats)

func _spawn_horde(enemy_type: String, amount: int) -> void:
	for i in range(amount):
		_spawn_single_enemy(enemy_type)

func _on_difficulty_timer_timeout() -> void:
	if not player:
		return

	if $SpawnTimer.wait_time > min_wait_time:
		$SpawnTimer.wait_time -= 0.05
	else:
		spawn_count += 1

func _spawn_portal() -> void:
	boss_spawned = true
	if not portal_scene:
		return
		
	var valid_spawn = false
	var spawn_pos = Vector2.ZERO
	
	while not valid_spawn:
		var angle = randf_range(0.0, TAU)
		var spawn_offset = Vector2.RIGHT.rotated(angle) * (spawn_radius * 0.5)
		var raw_pos = player.global_position + spawn_offset
		
		var map_coords = grass_layer.local_to_map(raw_pos)
		if _has_enough_space(map_coords, 3):
			spawn_pos = raw_pos 
			valid_spawn = true
	
	var portal = portal_scene.instantiate()
	portal.global_position = spawn_pos
	get_parent().add_child(portal)

func _has_enough_space(center_cell: Vector2i, tile_radius: int) -> bool:
	for x in range(-tile_radius, tile_radius + 1):
		for y in range(-tile_radius, tile_radius + 1):
			var check_pos = center_cell + Vector2i(x, y)
			if grass_layer.get_cell_source_id(check_pos) == -1:
				return false
	return true

func _get_allowed_enemies(time: float) -> Array:
	if time < 60.0:
		return ["basic"]
	elif time < 120.0:
		return ["basic", "basic", "basic", "runner", "swarm"]
	elif time < 180.0:
		return ["basic", "basic", "runner", "swarm", "swarm", "brute", "dasher"]
	else:
		return ["basic", "basic", "runner", "swarm", "brute", "tank", "dasher", "dasher"]

extends Node2D

@export var slime_scene: PackedScene = preload("res://enemies/slime.tscn")
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
var is_end_times: bool = false
var level_duration: int = 600 

var horde_events: Dictionary = {
	60: {"type": "swarm", "amount": 30},
	120: {"type": "brute", "amount": 15},
	180: {"type": "dasher", "amount": 40},
	300: {"type": "tank", "amount": 5},
	450: {"type": "swarm", "amount": 60}
}

func _ready() -> void:
	player = get_tree().get_first_node_in_group("player")
	
	if Data.current_floor == Data.MAX_FLOORS:
		if has_node("SpawnTimer"):
			$SpawnTimer.stop()
		if has_node("DifficultyTimer"):
			$DifficultyTimer.stop()

		set_process(false) 
		call_deferred("_spawn_final_boss")

func _process(_delta: float) -> void:
	if not player or is_end_times:
		return
		
	var current_second = int(player.time_survived)
	if current_second != last_processed_second:
		last_processed_second = current_second
		_check_time_events(current_second)
	# Time check for end times start
	if current_second >= level_duration and not is_end_times:
		start_end_times()
		
# Horde events helper func
func _check_time_events(current_second: int) -> void:
	if horde_events.has(current_second):
		_spawn_horde(horde_events[current_second]["type"], horde_events[current_second]["amount"])

func start_end_times() -> void:
	is_end_times = true
	
	if has_node("SpawnTimer"):
		$SpawnTimer.stop()
	if has_node("DifficultyTimer"):
		$DifficultyTimer.stop()
		
	if not boss_spawned:
		_spawn_portal()
		
	_spawn_death_slimes(8)
	
	var death_timer = Timer.new()
	death_timer.wait_time = 1.5
	death_timer.autostart = true
	death_timer.timeout.connect(func(): _spawn_death_slimes(4))
	add_child(death_timer)

func notify_boss_defeated() -> void:
	boss_defeated = true
	if not is_end_times:
		start_end_times()

func _spawn_death_slimes(amount: int) -> void:
	if not player or not grass_layer:
		return
	
	if get_tree().get_node_count_in_group("enemy") >= max_enemies + 50:
		return
		
	for i in range(amount):
		_spawn_single_enemy("death_slime")

func _on_spawn_timer_timeout() -> void:
	if not player or not grass_layer or is_end_times:
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
		var enemy_type = specific_type
		if enemy_type == "":
			enemy_type = _get_weighted_enemy(player.time_survived)
			
		var scene_to_load = slime_scene
		if Data.ENEMIES.has(enemy_type) and Data.ENEMIES[enemy_type].has("scene_path"):
			var custom_path = Data.ENEMIES[enemy_type]["scene_path"]
			var custom_scene = load(custom_path)
			if custom_scene:
				scene_to_load = custom_scene
				
		var new_enemy = scene_to_load.instantiate()
		new_enemy.global_position = spawn_pos
		get_parent().add_child(new_enemy)
		
		var final_stats: Dictionary
		var minutes_survived = int(player.time_survived) / 60
		var spawn_death_slime = (minutes_survived >= 10) or boss_defeated or specific_type == "death_slime"
		var floor_mult = 1.0 + ((Data.current_floor - 1) * 0.3) 
		
		if spawn_death_slime:
			final_stats = Data.ENEMIES["death_slime"].duplicate()
			var over_time = max(0, minutes_survived - 10)
			if boss_defeated and over_time == 0:
				over_time = 1
				
			var scaling_factor = 1.0 + (over_time * 0.5)
			final_stats["health"] = int(final_stats["health"] * scaling_factor * floor_mult)
			final_stats["damage"] = int(final_stats["damage"] * scaling_factor * floor_mult)
			final_stats["speed"] += over_time * 15.0
			final_stats["scale"] += over_time * 0.25 
			
			var shade = max(0.0, 0.15 - (over_time * 0.03))
			final_stats["color"] = Color(shade, 0.0, shade)
		else:
			final_stats = Data.get_scaled_enemy_stats(enemy_type, minutes_survived)
			final_stats["health"] = int(final_stats["health"] * floor_mult)
			final_stats["damage"] = int(final_stats["damage"] * floor_mult)
			if final_stats.has("exp"):
				final_stats["exp"] = int(final_stats["exp"] * floor_mult) 
			
		new_enemy.apply_stats(final_stats)

func _spawn_horde(enemy_type: String, amount: int) -> void:
	for i in range(amount):
		_spawn_single_enemy(enemy_type)

func _on_difficulty_timer_timeout() -> void:
	if not player or is_end_times:
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

# Helper func to fetch spawnable enemy
func _get_allowed_enemies(time: float) -> Array:
	if time < 60.0:
		return ["basic"]
	elif time < 120.0:
		return ["basic", "basic", "basic", "runner", "swarm"]
	elif time < 180.0:
		return ["basic", "basic", "runner", "swarm", "swarm", "brute", "dasher"]
	else:
		return ["basic", "basic", "runner", "swarm", "brute", "tank", "dasher", "dasher"]

# Helper function for handling boss spawn
func _spawn_final_boss() -> void:
	if not slime_scene or not player: 
		return
	
	await get_tree().create_timer(2.5).timeout
	var boss = slime_scene.instantiate()
	boss.global_position = player.global_position 
	
	var final_stats = Data.ENEMIES["boss"].duplicate()
	boss.apply_stats(final_stats)
	get_parent().add_child(boss)

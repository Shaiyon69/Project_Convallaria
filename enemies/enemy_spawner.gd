extends Node2D

@export var slime_scene: PackedScene = preload("res://enemies/slime.tscn")
@export var ratman_scene: PackedScene = preload("res://enemies/ratman/ratman.tscn")
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
		
	if current_second >= level_duration and not is_end_times:
		start_end_times()
		
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
		
	var endgame_timer = Timer.new()
	endgame_timer.wait_time = 5.0
	endgame_timer.autostart = true
	endgame_timer.timeout.connect(_on_endgame_tick.bind(endgame_timer))
	add_child(endgame_timer)

func _on_endgame_tick(timer: Timer) -> void:
	var over_time_sec = max(0.0, player.time_survived - level_duration)
	
	var slimes_to_spawn = 1 + int(over_time_sec / 10.0)
	_spawn_death_slimes(slimes_to_spawn)
	
	timer.wait_time = max(0.1, timer.wait_time * 0.90)

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
		var floor_mult = 1.0 + ((Data.current_floor - 1) * 0.3)
		
		var spawn_death_slime = (player.time_survived >= level_duration) or boss_defeated or specific_type == "death_slime"
		
		if spawn_death_slime:
			final_stats = Data.ENEMIES["basic"].duplicate()
			var over_time_sec = max(0.0, player.time_survived - level_duration)
			
			if boss_defeated and over_time_sec == 0:
				over_time_sec = 30.0
				
			final_stats["health"] = int((30.0 + (over_time_sec * 25.0)) * floor_mult)
			final_stats["damage"] = int((12.0 + (over_time_sec * 1.5)) * floor_mult)
			final_stats["speed"] = 80.0 + (over_time_sec * 2.0)
			final_stats["scale"] = 1.2 + (over_time_sec * 0.015)
			final_stats["exp"] = int((10.0 + (over_time_sec * 0.5)) * floor_mult)
			
			var color_intensity = max(0.0, 0.4 - (over_time_sec * 0.003))
			final_stats["color"] = Color(color_intensity, 0.0, color_intensity)
			final_stats["base_pitch"] = max(0.2, 1.0 - (over_time_sec * 0.01))
		else:
			var base_enemy_data = Data.ENEMIES[enemy_type].duplicate()
			var scaled_math = get_scaled_enemy_stats(base_enemy_data.get("health", 10), base_enemy_data.get("speed", 50.0), base_enemy_data.get("damage", 5))

			final_stats = base_enemy_data
			final_stats["health"] = scaled_math["health"]
			final_stats["speed"] = scaled_math["speed"]
			final_stats["damage"] = scaled_math["damage"]

			if final_stats.has("exp"):
				final_stats["exp"] = int(final_stats["exp"] * floor_mult)
			
			if enemy_type == "ratman":
				var variation_roll = randf()
				if variation_roll > 0.75:
					final_stats["scale"] = final_stats.get("scale", 1.0) * 1.25
					final_stats["health"] = int(final_stats["health"] * 1.5)
					final_stats["speed"] = final_stats.get("speed", 100) * 0.75
					final_stats["color"] = Color(0.7, 0.6, 0.6)
				elif variation_roll < 0.25:
					final_stats["scale"] = final_stats.get("scale", 1.0) * 0.8
					final_stats["health"] = int(final_stats["health"] * 0.6)
					final_stats["speed"] = final_stats.get("speed", 100) * 1.4
					final_stats["color"] = Color(1.2, 1.1, 1.1)
			
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

func _get_weighted_enemy(time: float) -> String:
	var minutes = time / 60.0
	var total_weight = 0.0
	var current_weights = {}
	
	for enemy_id in Data.ENEMY_SPAWN_CHANCES:
		var chance_data = Data.ENEMY_SPAWN_CHANCES[enemy_id]
		var weight = max(0.0, chance_data["base"] + (chance_data["growth"] * minutes))
		current_weights[enemy_id] = weight
		total_weight += weight
		
	var roll = randf() * total_weight
	var accumulator = 0.0
	
	for enemy_id in current_weights:
		accumulator += current_weights[enemy_id]
		if roll <= accumulator:
			return enemy_id
			
	return "basic"

func _spawn_final_boss() -> void:
	if not player:
		return
	
	await get_tree().create_timer(2.5).timeout

	var boss = ratman_scene.instantiate() if ratman_scene else slime_scene.instantiate()
	boss.global_position = player.global_position + (Vector2.UP * 500)
	
	boss.tree_exited.connect(_on_final_boss_died)
	
	get_parent().add_child(boss)
	var final_stats = Data.ENEMIES["boss"].duplicate()
	
	var minutes_survived = int(player.time_survived) / 60
	var boss_multiplier = 1.0 + (minutes_survived * 0.5) + (Data.current_floor * 0.5)

	final_stats["health"] = int(final_stats["health"] * boss_multiplier * 5.0)
	final_stats["damage"] = int(final_stats["damage"] * boss_multiplier * 2.0)
	final_stats["speed"] = final_stats.get("speed", 100) * 1.5
	final_stats["scale"] = 4.5
	final_stats["color"] = Color(0.8, 0.05, 0.1, 1.0)
	final_stats["base_pitch"] = 0.5
	
	boss.apply_stats(final_stats)

func _on_final_boss_died() -> void:
	if not is_inside_tree() or get_tree() == null:
		return
		
	await get_tree().create_timer(4.0).timeout

	if not is_inside_tree() or get_tree() == null:
		return
	
	if player and player.has_method("save_data"):
		player.save_data()
		
	if TransitionManager and TransitionManager.has_method("change_scene"):
		TransitionManager.change_scene("res://ui/victory_screen.tscn")
		
func get_scaled_enemy_stats(base_hp: int, base_speed: float, base_dmg: int) -> Dictionary:
	var minutes_survived = 0.0

	if player and "time_survived" in player:
		minutes_survived = player.time_survived / 60.0
		
	var current_floor = Data.current_floor if "current_floor" in Data else 1
	var floor_mult = 1.0 + ((current_floor - 1) * 0.5)
	
	var time_hp_mult = 1.0 + (minutes_survived * 0.30)
	var time_dmg_mult = 1.0 + (minutes_survived * 0.15)
	
	var final_hp = int(base_hp * floor_mult * time_hp_mult)
	var final_dmg = int(base_dmg * floor_mult * time_dmg_mult)
	var final_speed = (base_speed + (minutes_survived * 3.0)) + ((current_floor - 1) * 10.0)
	
	return {
		"health": final_hp,
		"speed": final_speed,
		"damage": final_dmg
	}

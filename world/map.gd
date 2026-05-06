extends Node2D

@export var map_seed: int = 0
@export var use_random_seed: bool = true
@export var map_radius: int = 100

@export var water_source_id: int = 0
@export var water_atlas_coords: Vector2i = Vector2i(0, 0)

@export var tree_scene: PackedScene 
@export var chest_scene: PackedScene
@export var statue_scene: PackedScene
@export var boss_portal_scene: PackedScene

@onready var water_layer: TileMapLayer = $WaterLayer
@onready var grass_layer: TileMapLayer = $GrassLayer
@onready var soil_layer: TileMapLayer = $SoilLayer

var noise: FastNoiseLite = FastNoiseLite.new()
var valid_spawn_tiles: Array[Vector2i] = []
var occupied_cells: Dictionary = {}

func _ready() -> void:
	# --- CHANGED: Made the boss arena much bigger (60 instead of 25) ---
	if Data.current_floor == Data.MAX_FLOORS:
		map_radius = 60
		
	_initialize_noise()
	_generate_terrain()
	_apply_biome()
	_place_player()
	
	if Data.current_floor < Data.MAX_FLOORS:
		_spawn_boss_portal()
		_spawn_objects()
		_spawn_trees() 
	
	var battle_music = load(Data.MUSIC["battle"])
	if AudioManager.has_method("play_music"):
		AudioManager.play_music(battle_music, 1.1)

func _initialize_noise() -> void:
	if use_random_seed:
		randomize()
		map_seed = randi()
	
	noise.seed = map_seed
	noise.noise_type = FastNoiseLite.TYPE_PERLIN
	noise.fractal_type = FastNoiseLite.FRACTAL_FBM
	noise.fractal_octaves = 4
	noise.fractal_lacunarity = 2.0
	noise.fractal_gain = 0.5
	noise.frequency = 0.02 

func _generate_terrain() -> void:
	var grass_cells: Array[Vector2i] = []
	var soil_cells: Array[Vector2i] = []
	
	var water_bounds = map_radius + 15
	for x in range(-water_bounds, water_bounds):
		for y in range(-water_bounds, water_bounds):
			water_layer.set_cell(Vector2i(x, y), water_source_id, water_atlas_coords)
	
	for x in range(-map_radius, map_radius):
		for y in range(-map_radius, map_radius):
			var cell_pos = Vector2i(x, y)
			
			var dist = Vector2(x, y).length()
			var normalized_dist = dist / float(map_radius)
			
			if normalized_dist > 1.0:
				continue
				
			var noise_val = noise.get_noise_2d(x * 2.0, y * 2.0)
			var falloff = 1.0 - pow(normalized_dist, 2.5)
			var final_val = (noise_val * 0.5 + 0.5) * falloff
			
			if final_val > 0.2:
				grass_cells.append(cell_pos)
				valid_spawn_tiles.append(cell_pos)
				
				if final_val > 0.6:
					soil_cells.append(cell_pos)
					
	if grass_cells.size() > 0:
		grass_layer.set_cells_terrain_connect(grass_cells, 0, 0)
	if soil_cells.size() > 0:
		soil_layer.set_cells_terrain_connect(soil_cells, 0, 0)

func _apply_biome() -> void:
	var floor_index = Data.current_floor
	
	if not Data.BIOME_COLORS.has(floor_index):
		floor_index = ((Data.current_floor - 1) % Data.BIOME_COLORS.size()) + 1
		
	var palette = Data.BIOME_COLORS[floor_index]
	
	grass_layer.modulate = palette["grass"]
	soil_layer.modulate = palette["soil"]
	water_layer.modulate = palette["water"]

func _is_space_free(center_cell: Vector2i, tile_radius: int) -> bool:
	for x in range(-tile_radius, tile_radius + 1):
		for y in range(-tile_radius, tile_radius + 1):
			var check_pos = center_cell + Vector2i(x, y)
			if grass_layer.get_cell_source_id(check_pos) == -1:
				return false
			if occupied_cells.has(check_pos):
				return false
	return true

func _reserve_space(center_cell: Vector2i, tile_radius: int) -> void:
	for x in range(-tile_radius, tile_radius + 1):
		for y in range(-tile_radius, tile_radius + 1):
			occupied_cells[center_cell + Vector2i(x, y)] = true

func _place_player() -> void:
	var player = get_tree().get_first_node_in_group("player")
	if not player or valid_spawn_tiles.is_empty():
		return
		
	var center_coords = Vector2i(0, 0)
	var closest_tile = valid_spawn_tiles[0]
	var min_dist = closest_tile.distance_squared_to(center_coords)
	
	for tile in valid_spawn_tiles:
		var dist = tile.distance_squared_to(center_coords)
		if dist < min_dist:
			min_dist = dist
			closest_tile = tile
			
	_reserve_space(closest_tile, 5)
	player.global_position = grass_layer.map_to_local(closest_tile)

func _spawn_boss_portal() -> void:
	if not boss_portal_scene or valid_spawn_tiles.is_empty():
		return
		
	var rng = RandomNumberGenerator.new()
	rng.seed = map_seed + 1 
	
	var portal_placed = false
	var attempts = 0
	
	while not portal_placed and attempts < 200:
		var random_index = rng.randi_range(0, valid_spawn_tiles.size() - 1)
		var cell_coords = valid_spawn_tiles[random_index]
		
		if _is_space_free(cell_coords, 3):
			_reserve_space(cell_coords, 3)
			var portal = boss_portal_scene.instantiate()
			portal.global_position = grass_layer.map_to_local(cell_coords)
			add_child(portal)
			portal_placed = true
			
		attempts += 1

func _spawn_objects() -> void:
	var rng = RandomNumberGenerator.new()
	rng.seed = map_seed
	
	_place_entities(chest_scene, 15, rng, 1, false)
	_place_entities(statue_scene, 10, rng, 2, true)

func _place_entities(scene: PackedScene, count: int, rng: RandomNumberGenerator, radius: int, apply_tint: bool) -> void:
	if not scene or valid_spawn_tiles.is_empty():
		return
		
	var placed = 0
	var attempts = 0
	
	while placed < count and attempts < count * 20:
		var random_index = rng.randi_range(0, valid_spawn_tiles.size() - 1)
		var cell_coords = valid_spawn_tiles[random_index]
		
		if _is_space_free(cell_coords, radius):
			_reserve_space(cell_coords, radius)
			var entity = scene.instantiate()
			entity.global_position = grass_layer.map_to_local(cell_coords)
			
			if apply_tint:
				entity.modulate = soil_layer.modulate
				
			add_child(entity)
			placed += 1
			
		attempts += 1

func _spawn_trees() -> void:
	if not tree_scene or valid_spawn_tiles.is_empty():
		return

	var rng = RandomNumberGenerator.new()
	rng.seed = map_seed + 2 
	
	var target_tree_count = int(valid_spawn_tiles.size() * 0.015) 
	var placed_trees = 0
	var attempts = 0
	
	while placed_trees < target_tree_count and attempts < target_tree_count * 4:
		var random_index = rng.randi_range(0, valid_spawn_tiles.size() - 1)
		var cell_coords = valid_spawn_tiles[random_index]
		
		if _is_space_free(cell_coords, 1):
			_reserve_space(cell_coords, 1)
			var tree = tree_scene.instantiate()
			
			var offset_x = rng.randf_range(-6.0, 6.0)
			var offset_y = rng.randf_range(-6.0, 6.0)
			
			tree.global_position = grass_layer.map_to_local(cell_coords) + Vector2(offset_x, offset_y)
			tree.modulate = grass_layer.modulate
			add_child(tree)
			
			placed_trees += 1
			
		attempts += 1

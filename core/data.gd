extends Node

const ModLoader = preload("res://core/mod_loader.gd")
const GAME_VERSION: String = "1.0.0"

var coins: int = 0
var silver: int = 0
var current_floor: int = 1
var run_time: float = 0.0
var player_data: Dictionary = {}
var starting_weapon: String = "wand" 

const MAX_FLOORS: int = 4
const MAX_WEAPONS: int = 3

const RARITY = {
	"white": {"color": Color(1.0, 1.0, 1.0), "mult": 1.0, "weight": 60},
	"green": {"color": Color(0.2, 0.8, 0.2), "mult": 1.5, "weight": 25},
	"blue": {"color": Color(0.2, 0.5, 1.0), "mult": 2.0, "weight": 12},
	"gold": {"color": Color(1.0, 0.8, 0.1), "mult": 3.0, "weight": 3}
}

const BIOME_COLORS = {
	1: {"grass": Color(1.0, 1.0, 1.0), "water": Color(1.0, 1.0, 1.0), "soil": Color(1.0, 1.0, 1.0)},
	2: {"grass": Color(0.8, 0.5, 0.8), "water": Color(0.9, 0.2, 0.5), "soil": Color(0.5, 0.3, 0.5)},
	3: {"grass": Color(0.5, 0.8, 1.0), "water": Color(0.2, 0.5, 1.0), "soil": Color(0.3, 0.5, 0.8)},
	4: {"grass": Color(0.9, 0.8, 0.4), "water": Color(0.9, 0.5, 0.2), "soil": Color(0.8, 0.6, 0.3)},
	5: {"grass": Color(0.4, 0.4, 0.4), "water": Color(0.8, 0.1, 0.1), "soil": Color(0.2, 0.2, 0.2)}
}

const MUSIC = {
	"menu": "res://ui/music.mp3",
	"battle": "res://ui/music.mp3",
	"boss": "res://enemies/bob.mp3"
}

var WEAPONS = {
	"poison_aura": {
		"display_name": "Poison Aura",
		"scene_path": "res://weapons/poison/poison.tscn",
		"icon": "res://weapons/poison/poison.png",
		"max_level": 3,
		"levels": {
			1: {"base_damage": 15, "wait_time": 1.0, "scale": 1.0},
			2: {"base_damage": 25, "wait_time": 0.8, "scale": 1.25},
			3: {"base_damage": 40, "wait_time": 0.5, "scale": 1.6}
		}
	},
	"wand": {
		"display_name": "Wand",
		"scene_path": "res://weapons/wand/wand.tscn",
		"icon": "res://weapons/wand/wand.png",
		"max_level": 3,
		"levels": {
			1: {"base_damage": 20, "projectiles": 1, "speed": 400.0, "wait_time": 1.0},
			2: {"base_damage": 35, "projectiles": 1, "speed": 450.0, "wait_time": 0.8},
			3: {"base_damage": 55, "projectiles": 1, "speed": 550.0, "wait_time": 0.5}
		}
	}
}

var STAGE_SETTINGS: Dictionary = {
	"stage_duration": 600,
	"death_slime_base_interval": 5.0,
	"death_slime_min_interval": 0.1,
	"death_slime_interval_multiplier": 0.90,
	"death_slime_spawn_growth_seconds": 10.0,
	"final_boss_uses_stage_timer": false
}

var PICKUP_SETTINGS: Dictionary = {
	"despawn_time": 25.0,
	"warning_time": 5.0,
	"heal_amount": 35,
	"heal_drop_chance": 0.025
}

var STAGES: Dictionary = {}

var ENEMIES = {
	"basic": {"health": 30, "speed": 45.0, "scale": 1.0, "color": Color(1.0, 1.0, 1.0), "damage": 10, "exp": 10, "base_pitch": 1.0, "scene_path": "res://enemies/slime.tscn"},
	"ratman": {"health": 20, "speed": 110.0, "scale": 1.0, "color": Color(1.0, 1.0, 1.0), "damage": 12, "exp": 15, "base_pitch": 1.2, "scene_path": "res://enemies/ratman/ratman.tscn"},
	"shooter": {"health": 20, "speed": 50.0, "scale": 0.9, "color": Color(1.0, 0.831, 0.0, 1.0), "is_shooter": true, "damage": 15, "exp": 20, "base_pitch": 1.4, "scene_path": "res://enemies/slime.tscn"},
	"brute": {"health": 90, "speed": 25.0, "scale": 1.5, "color": Color(1.0, 0.4, 0.4), "damage": 25, "exp": 30, "base_pitch": 0.6, "scene_path": "res://enemies/slime.tscn"},
	"runner": {"health": 15, "speed": 85.0, "scale": 0.8, "color": Color(0.2, 0.9, 0.2), "damage": 5, "exp": 15, "base_pitch": 1.5, "scene_path": "res://enemies/slime.tscn"},
	"swarm": {"health": 5, "speed": 100.0, "scale": 0.5, "color": Color(1.0, 0.5, 0.0), "damage": 2, "exp": 5, "base_pitch": 1.8, "scene_path": "res://enemies/slime.tscn"},
	"tank": {"health": 300, "speed": 15.0, "scale": 2.0, "color": Color(0.2, 0.2, 0.2), "damage": 50, "exp": 100, "base_pitch": 0.4, "scene_path": "res://enemies/slime.tscn"},
	"dasher": {"health": 25, "speed": 40.0, "scale": 0.9, "color": Color(0.1, 0.8, 0.8), "is_dasher": true, "damage": 15, "exp": 25, "base_pitch": 1.3, "scene_path": "res://enemies/slime.tscn"},
	"boss": {"health": 5000, "speed": 35.0, "scale": 2.0, "color": Color(0.8, 0.1, 0.8), "damage": 100, "exp": 1500, "base_pitch": 0.3, "scene_path": "res://enemies/boss.tscn"},
	"death_slime": {"health": 15000, "speed": 250.0, "scale": 3.0, "color": Color(0.367, 0.0, 0.367, 1.0), "damage": 500, "exp": 0, "base_pitch": 0.2, "is_death_slime": true, "scene_path": "res://enemies/slime.tscn"} 
}

var ENEMY_SPAWN_CHANCES = {
	"basic": {"base": 100.0, "growth": 0.0},
	"ratman": {"base": 10.0, "growth": 15.0}, 
	"runner": {"base": 0.0, "growth": 25.0},
	"shooter": {"base": 0.0, "growth": 20.0},  
	"swarm": {"base": 0.0, "growth": 15.0},
	"brute": {"base": 0.0, "growth": 10.0},
	"dasher": {"base": 0.0, "growth": 8.0},
	"tank": {"base": 0.0, "growth": 3.0}
}

var UPGRADES = [
	{"id": "max_hp", "base_text": "+%s%% Max HP", "base_val": 15},
	{"id": "speed", "base_text": "+%s%% Speed", "base_val": 8},
	{"id": "damage", "base_text": "+%s%% Damage", "base_val": 15},
	{"id": "fire_rate", "base_text": "+%s%% Fire Rate", "base_val": 10},
	{"id": "aoe_size", "base_text": "+%s%% Weapon Area", "base_val": 15},
	{"id": "regeneration", "base_text": "+%s HP Regen", "base_val": 2.0},
	{"id": "thorns", "base_text": "Reflect %s%% Damage", "base_val": 50},
	{"id": "evasion", "base_text": "+%s%% Dodge", "base_val": 5},
	{"id": "crit_chance", "base_text": "+%s%% Crit Chance", "base_val": 5},
	
	{"id": "glass_cannon", "base_text": "Glass Cannon: +40% Dmg, -20% HP", "base_val": 40, "unique": true},
	{"id": "heavy_armor", "base_text": "Heavy Armor: +50% Thorns, -15% Speed", "base_val": 50, "unique": true},
	{"id": "berserker", "base_text": "Berserker: +30% Fire Rate, -10% Evasion", "base_val": 30, "unique": true},
	{"id": "vampiric_edge", "base_text": "Vampiric Edge: +%s%% Kill Leech", "base_val": 3, "unique": true},
	{"id": "magnet_training", "base_text": "+%s%% Pickup Range", "base_val": 25, "unique": true},
	{"id": "precision", "base_text": "Precision: +%s%% Crit and Damage", "base_val": 8, "unique": true},
	{"id": "momentum", "base_text": "Momentum: +%s%% Speed and Fire Rate", "base_val": 6, "unique": true},
	{"id": "soul_harvest", "base_text": "Soul Harvest: +%s%% EXP and +1 Regen", "base_val": 15, "unique": true},
	
	{"id": "exp_boost", "base_text": "+%s%% EXP", "base_val": 20},
	{"id": "multi_attack", "base_text": "+%s Attack Strike", "base_val": 1},
	{"id": "fire_imbue", "base_text": "Add Fire Damage (Burn)", "base_val": 0},
	{"id": "frost_imbue", "base_text": "Add Frost Damage (Slow)", "base_val": 0}
]

var ITEMS = {
	"apple": {"name": "Apple", "icon": "res://player/items/apple.png", "type": "vampirism", "value": 0.03, "rarity": "white", "desc": "3% chance on kill to heal 5% Max HP."},
	"sprinkler": {"name": "Sprinkler", "icon": "res://player/items/sprinkler.png", "type": "nova", "value": 20.0, "rarity": "blue", "desc": "Releases a devastating water blast every 20 seconds."},
	"beanie": {"name": "Beanie", "icon": "res://player/items/beanie.png", "type": "shield", "value": 15.0, "rarity": "green", "desc": "Blocks 1 hit and releases a shockwave. Recharges every 15s."},
	"goldfish": {"name": "Goldfish", "icon": "res://player/items/goldfish.png", "type": "greed", "value": 0.05, "rarity": "gold", "desc": "Consumes 5 Silver every 60s for a permanent +5% Damage stack. Runs out if broke!"}
}

func _ready() -> void:
	_load_external_data()

func start_new_run() -> void:
	current_floor = 1
	run_time = 0.0
	silver = 0
	player_data.clear()

func advance_run_time(delta: float) -> void:
	run_time += delta

func get_run_minutes() -> float:
	return run_time / 60.0

func get_current_stage_data() -> Dictionary:
	var floor_key := str(current_floor)
	if STAGES.has(floor_key) and typeof(STAGES[floor_key]) == TYPE_DICTIONARY:
		return STAGES[floor_key]
	var named_floor_key := "floor_" + floor_key
	if STAGES.has(named_floor_key) and typeof(STAGES[named_floor_key]) == TYPE_DICTIONARY:
		return STAGES[named_floor_key]
	if STAGES.has("default") and typeof(STAGES["default"]) == TYPE_DICTIONARY:
		return STAGES["default"]
	return {}

func _load_external_data() -> void:
	_load_json_dictionary("res://data/stage_settings.json", STAGE_SETTINGS)
	_load_json_dictionary("res://data/pickup_settings.json", PICKUP_SETTINGS)
	_load_json_dictionary("res://data/enemies.json", ENEMIES)
	_load_json_dictionary("res://data/weapons.json", WEAPONS)
	_load_json_dictionary("res://data/stages.json", STAGES)
	_load_json_array("res://data/upgrades.json", UPGRADES)
	_load_json_dictionary("res://data/items.json", ITEMS)
	_load_mod_data()
	_normalize_enemy_data()
	_normalize_weapon_data()

func _load_mod_data() -> void:
	var mod_loader = ModLoader.new()
	var loaded_mods: Array = mod_loader.load_mods(GAME_VERSION)
	if loaded_mods.is_empty():
		return

	mod_loader.merge_mod_data(loaded_mods, {
		"upgrades": UPGRADES,
		"enemies": ENEMIES,
		"weapons": WEAPONS,
		"items": ITEMS,
		"stages": STAGES
	}, {
		"enemies": {"scene_path": "res://enemies/slime.tscn"},
		"weapons": {"scene_path": "res://weapons/wand/wand.tscn", "icon": "res://weapons/wand/wand.png"},
		"items": {"icon": "res://player/items/apple.png"},
		"stages": {}
	})

func _load_json_dictionary(path: String, target: Dictionary) -> void:
	if not FileAccess.file_exists(path):
		return

	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		return

	var parsed = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_DICTIONARY:
		return

	for key in parsed:
		target[key] = parsed[key]

func _load_json_array(path: String, target: Array) -> void:
	if not FileAccess.file_exists(path):
		return

	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		return

	var parsed = JSON.parse_string(file.get_as_text())
	if typeof(parsed) != TYPE_ARRAY:
		return

	target.clear()
	for value in parsed:
		target.append(value)

func _normalize_enemy_data() -> void:
	for enemy_id in ENEMIES:
		var enemy_data: Dictionary = ENEMIES[enemy_id]
		if enemy_data.has("color") and typeof(enemy_data["color"]) == TYPE_ARRAY:
			var color_values: Array = enemy_data["color"]
			if color_values.size() >= 3:
				var alpha := float(color_values[3]) if color_values.size() > 3 else 1.0
				enemy_data["color"] = Color(float(color_values[0]), float(color_values[1]), float(color_values[2]), alpha)
		if enemy_data.has("spawn_chance") and typeof(enemy_data["spawn_chance"]) == TYPE_DICTIONARY:
			var spawn_chance: Dictionary = enemy_data["spawn_chance"]
			ENEMY_SPAWN_CHANCES[enemy_id] = {
				"base": float(spawn_chance.get("base", 0.0)),
				"growth": float(spawn_chance.get("growth", 0.0))
			}

func _normalize_weapon_data() -> void:
	for weapon_id in WEAPONS:
		var weapon_data = WEAPONS[weapon_id]
		if typeof(weapon_data) != TYPE_DICTIONARY or not weapon_data.has("levels"):
			continue

		var levels = weapon_data["levels"]
		if typeof(levels) != TYPE_DICTIONARY:
			continue

		var normalized_levels: Dictionary = {}
		for level_key in levels:
			if typeof(level_key) == TYPE_INT:
				normalized_levels[level_key] = levels[level_key]
			else:
				normalized_levels[int(str(level_key))] = levels[level_key]
		weapon_data["levels"] = normalized_levels

var permanent_upgrades = {
	"max_hp": {"level": 0, "max_level": 5, "base_cost": 100, "cost_mult": 1.5, "boost_per_level": 10.0, "name": "Base Health"},
	"damage": {"level": 0, "max_level": 5, "base_cost": 250, "cost_mult": 2.0, "boost_per_level": 0.05, "name": "Base Damage"},
	"speed": {"level": 0, "max_level": 5, "base_cost": 150, "cost_mult": 1.5, "boost_per_level": 15.0, "name": "Movement Speed"},
	"regeneration": {"level": 0, "max_level": 3, "base_cost": 300, "cost_mult": 2.5, "boost_per_level": 0.5, "name": "HP Regen"},
	"armor": {"level": 0, "max_level": 3, "base_cost": 400, "cost_mult": 2.0, "boost_per_level": 0.1, "name": "Thorns Armor"},
	"evasion": {"level": 0, "max_level": 3, "base_cost": 500, "cost_mult": 3.0, "boost_per_level": 0.02, "name": "Dodge Chance"},
	"greed": {"level": 0, "max_level": 3, "base_cost": 500, "cost_mult": 3.0, "boost_per_level": 0.2, "name": "Coin Multiplier"},
	"exp_gain": {"level": 0, "max_level": 5, "base_cost": 200, "cost_mult": 1.8, "boost_per_level": 0.10, "name": "EXP Gain %"}
}

func get_scaled_enemy_stats(enemy_id: String, time_minutes: int) -> Dictionary:
	var scaled_stats = ENEMIES[enemy_id].duplicate() 
	
	var hp_multiplier = 1.0 + (time_minutes * 0.08)
	var dmg_multiplier = 1.0 + (time_minutes * 0.02)
	
	scaled_stats["health"] = int(scaled_stats["health"] * hp_multiplier)
	scaled_stats["damage"] = int(scaled_stats["damage"] * dmg_multiplier)
	scaled_stats["speed"] = scaled_stats["speed"] + (time_minutes * 0.5)
	
	return scaled_stats

func get_upgrade_cost(upgrade_id: String) -> int:
	var upg = permanent_upgrades[upgrade_id]
	if upg["level"] >= upg["max_level"]:
		return -1 
	return int(upg["base_cost"] * pow(upg["cost_mult"], upg["level"]))

func buy_upgrade(upgrade_id: String) -> bool:
	var cost = get_upgrade_cost(upgrade_id)
	if cost > 0 and coins >= cost:
		coins -= cost
		permanent_upgrades[upgrade_id]["level"] += 1
		return true
	return false

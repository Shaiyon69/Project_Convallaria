extends Node

var coins: int = 0
var current_floor: int = 1
var player_data: Dictionary = {}

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
	"menu": "res://audio/music.mp3",
	"battle": "res://audio/music.mp3",
	"boss": "res://audio/bob.mp3"
}

const WEAPONS = {
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
	"vine_whip": {
		"display_name": "Vine Whip",
		"scene_path": "res://weapons/vine/vine_whip.tscn",
		"icon": "res://weapons/vine/icon.png", 
		"max_level": 3,
		"levels": {
			1: {"base_damage": 20, "projectiles": 1, "speed": 400.0},
			2: {"base_damage": 35, "projectiles": 2, "speed": 450.0},
			3: {"base_damage": 55, "projectiles": 3, "speed": 550.0}
		}
	}
}

#ideated entries
#const ENEMIES = {
	#"basic_weed": {"health": 30, "speed": 75.0, "scale": 1.0, "color": Color(0.2, 0.8, 0.2), "damage": 10, "exp": 10, "base_pitch": 1.0, "drop_tier": 1},
	#"locust_runner": {"health": 15, "speed": 140.0, "scale": 0.8, "color": Color(0.8, 0.8, 0.2), "damage": 5, "exp": 15, "base_pitch": 1.5, "drop_tier": 1},
	#"root_brute": {"health": 150, "speed": 40.0, "scale": 1.5, "color": Color(0.5, 0.3, 0.1), "damage": 25, "exp": 45, "base_pitch": 0.6, "drop_tier": 2},
	#"gourd_tank": {"health": 450, "speed": 25.0, "scale": 2.0, "color": Color(0.9, 0.5, 0.1), "damage": 50, "exp": 120, "base_pitch": 0.4, "drop_tier": 3},
	#"blight_shadow": {"health": 15000, "speed": 250.0, "scale": 3.0, "color": Color(0.1, 0.0, 0.1), "damage": 500, "exp": 0, "base_pitch": 0.2, "drop_tier": 0},
	#"boss_convallaria": {"health": 8000, "speed": 50.0, "scale": 2.5, "color": Color(0.9, 0.9, 1.0), "damage": 120, "exp": 2000, "base_pitch": 0.3, "drop_tier": 5}
#}

const ENEMIES = {
	"basic": {"health": 30, "speed": 75.0, "scale": 1.0, "color": Color(1.0, 1.0, 1.0), "damage": 10, "exp": 10, "base_pitch": 1.0},
	"brute": {"health": 90, "speed": 40.0, "scale": 1.5, "color": Color(1.0, 0.4, 0.4), "damage": 25, "exp": 30, "base_pitch": 0.6},
	"runner": {"health": 15, "speed": 130.0, "scale": 0.8, "color": Color(0.4, 0.4, 1.0), "damage": 5, "exp": 15, "base_pitch": 1.5},
	"swarm": {"health": 5, "speed": 160.0, "scale": 0.5, "color": Color(1.0, 1.0, 0.2), "damage": 2, "exp": 5, "base_pitch": 1.8},
	"tank": {"health": 300, "speed": 25.0, "scale": 2.0, "color": Color(0.2, 0.2, 0.2), "damage": 50, "exp": 100, "base_pitch": 0.4},
	"dasher": {"health": 25, "speed": 60.0, "scale": 0.9, "color": Color(0.1, 0.8, 0.8), "is_dasher": true, "damage": 15, "exp": 25, "base_pitch": 1.3},
	"boss": {"health": 5000, "speed": 60.0, "scale": 2.0, "color": Color(0.8, 0.1, 0.8), "damage": 100, "exp": 1500, "base_pitch": 0.3},
	"death_slime": {"health": 15000, "speed": 250.0, "scale": 3.0, "color": Color(0.1, 0.0, 0.1), "damage": 500, "exp": 0, "base_pitch": 0.2}
}

const UPGRADES = [
	{"id": "max_hp", "base_text": "+%s%% Max HP", "base_val": 5},
	{"id": "speed", "base_text": "+%s%% Speed", "base_val": 3},
	{"id": "damage", "base_text": "+%s%% Damage", "base_val": 5},
	{"id": "fire_rate", "base_text": "+%s%% Fire Rate", "base_val": 5},
	{"id": "aoe_size", "base_text": "+%s%% Weapon Area", "base_val": 5},
	{"id": "regeneration", "base_text": "+%s HP Regen", "base_val": 0.5},
	{"id": "thorns", "base_text": "Reflect %s%% Damage", "base_val": 25},
	{"id": "evasion", "base_text": "+%s%% Dodge", "base_val": 3},
	{"id": "exp_boost", "base_text": "+%s%% EXP", "base_val": 10},
	{"id": "multi_attack", "base_text": "+%s Attack Strike", "base_val": 1},
	{"id": "fire_imbue", "base_text": "Add Fire Damage (Burn)", "base_val": 0},
	{"id": "frost_imbue", "base_text": "Add Frost Damage (Slow)", "base_val": 0}
]

func get_scaled_enemy_stats(enemy_id: String, time_minutes: int) -> Dictionary:
	var scaled_stats = ENEMIES[enemy_id].duplicate() 
	
	var hp_multiplier = 1.0 + (time_minutes * 0.15)
	var dmg_multiplier = 1.0 + (time_minutes * 0.05)
	
	scaled_stats["health"] = int(scaled_stats["health"] * hp_multiplier)
	scaled_stats["damage"] = int(scaled_stats["damage"] * dmg_multiplier)
	scaled_stats["speed"] = scaled_stats["speed"] + (time_minutes * 2.0)
	
	return scaled_stats

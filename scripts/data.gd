extends Node

var current_floor: int = 1

const BIOME_COLORS = {
	1: {"grass": Color(1.0, 1.0, 1.0), "water": Color(1.0, 1.0, 1.0), "soil": Color(1.0, 1.0, 1.0)}, 
	2: {"grass": Color(0.8, 0.5, 0.8), "water": Color(0.9, 0.2, 0.5), "soil": Color(0.5, 0.3, 0.5)}, 
	3: {"grass": Color(0.5, 0.8, 1.0), "water": Color(0.2, 0.5, 1.0), "soil": Color(0.3, 0.5, 0.8)}, 
	4: {"grass": Color(0.9, 0.8, 0.4), "water": Color(0.9, 0.5, 0.2), "soil": Color(0.8, 0.6, 0.3)}, 
	5: {"grass": Color(0.4, 0.4, 0.4), "water": Color(0.8, 0.1, 0.1), "soil": Color(0.2, 0.2, 0.2)}  
}

const MUSIC = {
	"menu": "res://audio/music/menu_theme.ogg",
	"battle": "res://audio/music/battle_theme.ogg",
	"boss": "res://audio/music/boss_theme.ogg"
}

const WEAPONS = {
	"poison_aura": {
		"display_name": "Poison Aura",
		"base_damage": 15,
		"scene_path": "res://weapons/poison/poison.tscn"
	},
	"vine_whip": {
		"display_name": "Vine Whip",
		"base_damage": 20,
		"scene_path": "res://weapons/vine/vine_whip.tscn"
	}
}

const ENEMIES = {
	"basic": {"health": 30, "speed": 75.0, "scale": 1.0, "color": Color(1.0, 1.0, 1.0), "damage": 10, "exp": 10},
	"brute": {"health": 90, "speed": 40.0, "scale": 1.5, "color": Color(1.0, 0.4, 0.4), "damage": 25, "exp": 30},
	"runner": {"health": 15, "speed": 130.0, "scale": 0.8, "color": Color(0.4, 0.4, 1.0), "damage": 5, "exp": 15},
	"swarm": {"health": 5, "speed": 160.0, "scale": 0.5, "color": Color(1.0, 1.0, 0.2), "damage": 2, "exp": 5},
	"tank": {"health": 300, "speed": 25.0, "scale": 2.0, "color": Color(0.2, 0.2, 0.2), "damage": 50, "exp": 100},
	"dasher": {"health": 25, "speed": 60.0, "scale": 0.9, "color": Color(0.1, 0.8, 0.8), "is_dasher": true, "damage": 15, "exp": 25},
	"boss": {"health": 5000, "speed": 60.0, "scale": 2.0, "color": Color(0.8, 0.1, 0.8), "damage": 100, "exp": 1500}
}

const UPGRADES = [
	{"id": "max_hp", "text": "+10% Max HP"},
	{"id": "speed", "text": "+5% Speed"},
	{"id": "damage", "text": "+10% Damage"},
	{"id": "pickup_range", "text": "+15% Pickup Range"},
	{"id": "fire_rate", "text": "+10% Fire Rate"},
	{"id": "aoe_size", "text": "+15% Weapon Area"},
	{"id": "multi_attack", "text": "Additional Attack Strike"},
	{"id": "fire_imbue", "text": "Add Fire Damage (Burn)"},
	{"id": "frost_imbue", "text": "Add Frost Damage (Slow)"},
	{"id": "regeneration", "text": "+1 HP Regen Per Second"},
	{"id": "thorns", "text": "Reflect 50% Damage Taken"},
	{"id": "evasion", "text": "+10% Dodge Chance"},
	{"id": "exp_boost", "text": "+25% EXP Gained"}
]

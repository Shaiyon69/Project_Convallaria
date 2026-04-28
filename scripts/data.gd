extends Node

const MUSIC = {
	"menu": "res://audio/music.mp3",
	"battle": "res://audio/battle.mp3",
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
	"basic": {"health": 30, "speed": 75.0, "scale": 1.0, "color": Color(1.0, 1.0, 1.0), "damage": 10},
	"brute": {"health": 90, "speed": 40.0, "scale": 1.5, "color": Color(1.0, 0.4, 0.4), "damage": 25},
	"runner": {"health": 15, "speed": 130.0, "scale": 0.8, "color": Color(0.4, 0.4, 1.0), "damage": 5},
	"swarm": {"health": 5, "speed": 160.0, "scale": 0.5, "color": Color(1.0, 1.0, 0.2), "damage": 2},
	"tank": {"health": 300, "speed": 25.0, "scale": 2.0, "color": Color(0.2, 0.2, 0.2), "damage": 50},
	"dasher": {"health": 25, "speed": 60.0, "scale": 0.9, "color": Color(0.1, 0.8, 0.8), "is_dasher": true, "damage": 15},
	"boss": {"health": 5000, "speed": 60.0, "scale": 4.0, "color": Color(0.8, 0.1, 0.8), "damage": 100}
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
	{"id": "frost_imbue", "text": "Add Frost Damage (Slow)"}
]

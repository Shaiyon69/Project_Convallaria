extends Node

const SETTINGS_FILE_PATH = "user://settings.cfg"
var config = ConfigFile.new()

var master_volume: float = 1.0
var music_volume: float = 1.0
var sfx_volume: float = 1.0

func _ready() -> void:
	load_settings()

func save_settings() -> void:
	config.set_value("audio", "master", master_volume)
	config.set_value("audio", "music", music_volume)
	config.set_value("audio", "sfx", sfx_volume)

	config.save(SETTINGS_FILE_PATH)

func load_settings() -> void:
	var err = config.load(SETTINGS_FILE_PATH)
	if err != OK:
		save_settings()
		return

	master_volume = config.get_value("audio", "master", 1.0)
	music_volume = config.get_value("audio", "music", 1.0)
	sfx_volume = config.get_value("audio", "sfx", 1.0)

	_apply_volume("Master", master_volume)
	_apply_volume("Music", music_volume)
	_apply_volume("SFX", sfx_volume)

func update_volume(bus_name: String, value: float) -> void:
	match bus_name:
		"Master": master_volume = value
		"Music": music_volume = value
		"SFX": sfx_volume = value
		
	_apply_volume(bus_name, value)
	save_settings()

func _apply_volume(bus_name: String, value: float) -> void:
	var bus_idx = AudioServer.get_bus_index(bus_name)
	AudioServer.set_bus_volume_db(bus_idx, linear_to_db(max(value, 0.001)))

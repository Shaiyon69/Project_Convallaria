extends Node

var music_player: AudioStreamPlayer
var sfx_players: Dictionary = {}
var sfx_cooldowns: Dictionary = {}

const MOVE_COOLDOWN = 300
const HIT_COOLDOWN = 100
const DEATH_COOLDOWN = 150

func _ready() -> void:
	music_player = AudioStreamPlayer.new()
	music_player.bus = "Master"
	add_child(music_player)

func play_music(stream: AudioStream, volume: float = -10.0) -> void:
	if music_player.stream == stream and music_player.playing:
		return
	music_player.stream = stream
	music_player.volume_db = volume
	music_player.play()

func stop_music() -> void:
	music_player.stop()

func play_sfx(stream: AudioStream, volume: float = -15.0) -> void:
	if not stream:
		return
		
	var stream_id = stream.resource_path
	if not sfx_players.has(stream_id):
		var new_sfx_player = AudioStreamPlayer.new()
		new_sfx_player.stream = stream
		new_sfx_player.bus = "Master"
		new_sfx_player.max_polyphony = 3 
		add_child(new_sfx_player)
		sfx_players[stream_id] = new_sfx_player
		
	var player = sfx_players[stream_id]
	player.volume_db = volume
	player.play()
func _can_play_sfx(stream_id: String, cooldown_ms: int) -> bool:
	var current_time = Time.get_ticks_msec()
	
	if sfx_cooldowns.has(stream_id):
		if current_time - sfx_cooldowns[stream_id] < cooldown_ms:
			return false
			
	sfx_cooldowns[stream_id] = current_time
	return true

func play_sfx_2d(stream: AudioStream, pos: Vector2, volume: float = -15.0, pitch_center: float = 1.0, type: String = "hit") -> void:
	if not stream:
		return
		
	var stream_id = stream.resource_path
	var cooldown = HIT_COOLDOWN
	
	if type == "move":
		cooldown = MOVE_COOLDOWN
	elif type == "death":
		cooldown = DEATH_COOLDOWN
	if not _can_play_sfx(stream_id + type, cooldown):
		return
		
	var p = AudioStreamPlayer2D.new()
	p.stream = stream
	p.global_position = pos
	p.max_distance = 600.0 
	p.bus = "Master"
	p.volume_db = volume
	p.pitch_scale = pitch_center * randf_range(0.9, 1.1)
		
	add_child(p)
	p.play()
	
	p.finished.connect(p.queue_free)
	
func set_music_speed(speed_scale: float) -> void:
	if music_player:
		music_player.pitch_scale = speed_scale

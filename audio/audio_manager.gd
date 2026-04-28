extends Node

@onready var music_player = $MusicPlayer

func play_music(music_stream: AudioStream, pitch: float = 1.0) -> void:
	if music_player.stream == music_stream and music_player.playing and music_player.pitch_scale == pitch:
		return
		
	music_player.stream = music_stream
	music_player.pitch_scale = pitch
	music_player.play()

func stop_music() -> void:
	music_player.stop()

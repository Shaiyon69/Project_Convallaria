extends CharacterBody2D

@export var speed: float = 150.0
@export var max_health: float = 100.0

var is_invincible: bool = false
var i_frame_duration: float = 0.2

var level: int = 1
var current_exp: int = 0
var exp_to_next_level: int = 5

var current_health: float
var bonus_damage: int = 0
var time_survived: float = 0.0
var kill_count: int = 0
var fire_rate_multiplier: float = 1.0

@onready var hud = $HUD

func _ready() -> void:
	current_health = max_health
	hud.update_health(current_health, max_health)
	hud.update_exp(current_exp, exp_to_next_level)
	hud.update_level(level)
	
	hud.upgrade_selected.connect(_apply_upgrade)
	%MagnetZone.area_entered.connect(_on_magnet_zone_area_entered)

func _on_magnet_zone_area_entered(area: Area2D) -> void:
	if area.has_method("pull_to_player"):
		area.pull_to_player(self)

func _physics_process(delta: float) -> void:
	_timer_calc(delta)
	_movement_handle()
	_handle_damage(delta)

# ------------------------------------------------------ #

func _movement_handle():
	var direction := Input.get_vector("move_left", "move_right", "move_up", "move_down")
	velocity = direction * speed
	_update_animations(direction)
	move_and_slide()
	
func _timer_calc(delta: float):
	time_survived += delta
	var minutes = int(time_survived) / 60
	var seconds = int(time_survived) % 60
	hud.update_time(minutes, seconds)


func add_kill() -> void:
	kill_count += 1
	hud.update_kills(kill_count)
	
func trigger_iframes() -> void:
	is_invincible = true
	
	var tween = create_tween()
	tween.tween_property($AnimatedSprite2D, "modulate:a", 0.3, 0.1)
	tween.tween_property($AnimatedSprite2D, "modulate:a", 1.0, 0.1)
	tween.set_loops(int(i_frame_duration / 0.2))
	
	await get_tree().create_timer(i_frame_duration).timeout
	
	is_invincible = false
	$AnimatedSprite2D.modulate.a = 1.0

func _update_animations(dir: Vector2) -> void:
	if dir.length() > 0:
		if abs(dir.x) > abs(dir.y):
			%AnimatedSprite2D.play("right" if dir.x > 0 else "left")
		else:
			%AnimatedSprite2D.play("down" if dir.y > 0 else "up")
	else:
		%AnimatedSprite2D.stop()

func _handle_damage(_delta: float) -> void:
	if is_invincible:
		return
		
	var overlapping_mobs = %HurtBox.get_overlapping_bodies()
	
	for body in overlapping_mobs:
		if body.is_in_group("enemy"):
			var damage_taken = 10
			
			if "attack_damage" in body:
				damage_taken = body.attack_damage
				
			current_health -= damage_taken
			hud.update_health(current_health, max_health)
			
			if current_health <= 0.0:
				get_tree().paused = true
				hud.show_game_over()
			else:
				trigger_iframes()
			return

func gain_experience(amount: int) -> void:
	current_exp += amount
	if current_exp >= exp_to_next_level:
		level_up()
	hud.update_exp(current_exp, exp_to_next_level)

func level_up() -> void:
	current_exp -= exp_to_next_level
	level += 1
	exp_to_next_level = int(5 * (level ** 1.5)) # Experimental polynomial level rq scaling
	
	current_health = max_health
	
	hud.update_health(current_health, max_health)
	hud.update_exp(current_exp, exp_to_next_level)
	hud.update_level(level)
	
	get_tree().paused = true
	hud.show_level_up()

func _apply_upgrade(upgrade_name: String) -> void:
	if upgrade_name == "max_hp":
		max_health += 50.0
		current_health += 50.0
	elif upgrade_name == "speed":
		speed += 25.0
	elif upgrade_name == "damage":
		bonus_damage += 15
	elif upgrade_name == "pickup_range":
		var shape = %MagnetZone.get_node("CollisionShape2D").shape as CircleShape2D
		shape.radius += 25.0
	elif upgrade_name == "fire_rate":
		fire_rate_multiplier += 0.1
	else:
		_acquire_weapon(upgrade_name)
		
	hud.update_health(current_health, max_health)

func _acquire_weapon(weapon_id: String) -> void:
	if Data.WEAPONS.has(weapon_id):
		var weapon_data = Data.WEAPONS[weapon_id]
		var weapon_scene = load(weapon_data["scene_path"])
		var new_weapon = weapon_scene.instantiate()
		
		add_child(new_weapon)

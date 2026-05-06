extends CharacterBody2D

@export var speed: float = 150.0
@export var max_health: float = 100.0

var is_invincible: bool = false
var i_frame_duration: float = 0.2

var level: int = 1
var current_exp: int = 0
var exp_to_next_level: int = 15 

var current_health: float
var damage_multiplier: float = 1.0
var time_survived: float = 0.0
var kill_count: int = 0
var fire_rate_multiplier: float = 1.0

var bonus_attacks: int = 0
var aoe_multiplier: float = 1.0
var imbue_fire: bool = false
var imbue_frost: bool = false

var hp_regen_rate: float = 0.0
var regen_accumulator: float = 0.0
var thorns_multiplier: float = 0.0
var evasion_chance: float = 0.0
var exp_multiplier: float = 1.0

var magnet_scale: float = 1.0 
var magnet_time_left: float = 0.0
var speed_time_left: float = 0.0
var is_speed_boosted: bool = false
var base_speed_before_boost: float = 0.0

var owned_weapons: Dictionary = {}
var owned_items: Array = [] 

var vampirism_rate: float = 0.0
var greed_multiplier: float = 0.0
var _prev_greed_bonus: float = 0.0
var has_goldfish: bool = false
var goldfish_timer: float = 60.0

var base_crit_chance: float = 0.0 

var has_shield: bool = false
var shield_active: bool = false
var shield_timer: float = 0.0
var shield_cooldown: float = 15.0

var has_nova: bool = false
var nova_timer: float = 0.0
var nova_cooldown: float = 20.0

var end_times_triggered: bool = false 

var sfx_pickup = preload("res://player/orb.mp3")
var sfx_levelup = preload("res://assets/audio/levelup.wav")

var last_sfx_time: int = 0

@onready var hud = $HUD
@onready var step_sound = $StepSound

func _ready() -> void:
	if Data.player_data.is_empty():
		current_health = max_health
		Data.silver = 0
		
		if Data.starting_weapon != "":
			_acquire_weapon(Data.starting_weapon)
		else:
			_acquire_weapon("wand")
	else:
		_load_data()
		
	hud.update_health(current_health, max_health)
	hud.update_exp(current_exp, exp_to_next_level)
	hud.update_level(level)
	
	if hud.has_method("update_coins"):
		hud.update_coins() # FIXED: No arguments needed!
	
	hud.upgrade_selected.connect(_apply_upgrade)
	%MagnetZone.area_entered.connect(_on_magnet_zone_area_entered)

func save_data() -> void:
	Data.player_data = {
		"level": level,
		"current_exp": current_exp,
		"exp_to_next_level": exp_to_next_level,
		"max_health": max_health,
		"current_health": current_health,
		"speed": speed,
		"damage_multiplier": damage_multiplier,
		"fire_rate_multiplier": fire_rate_multiplier,
		"bonus_attacks": bonus_attacks,
		"aoe_multiplier": aoe_multiplier,
		"imbue_fire": imbue_fire,
		"imbue_frost": imbue_frost,
		"hp_regen_rate": hp_regen_rate,
		"thorns_multiplier": thorns_multiplier,
		"evasion_chance": evasion_chance,
		"exp_multiplier": exp_multiplier,
		"kill_count": kill_count,
		"time_survived": time_survived,
		"owned_weapons": owned_weapons,
		"owned_items": owned_items,
		"magnet_scale": magnet_scale
	}

func _load_data() -> void:
	var pd = Data.player_data
	
	level = pd["level"]
	current_exp = pd["current_exp"]
	exp_to_next_level = pd["exp_to_next_level"]
	max_health = pd["max_health"]
	current_health = pd["current_health"]
	speed = pd["speed"]
	damage_multiplier = pd["damage_multiplier"]
	fire_rate_multiplier = pd["fire_rate_multiplier"]
	bonus_attacks = pd["bonus_attacks"]
	aoe_multiplier = pd["aoe_multiplier"]
	imbue_fire = pd["imbue_fire"]
	imbue_frost = pd["imbue_frost"]
	hp_regen_rate = pd["hp_regen_rate"]
	thorns_multiplier = pd["thorns_multiplier"]
	evasion_chance = pd["evasion_chance"]
	exp_multiplier = pd["exp_multiplier"]
	kill_count = pd["kill_count"]
	time_survived = pd["time_survived"]
	
	if pd.has("owned_items"):
		owned_items = pd["owned_items"]
		for item in owned_items:
			_apply_relic_stats(item, false) 
	
	if pd.has("magnet_scale"):
		magnet_scale = pd["magnet_scale"]
		%MagnetZone.scale = Vector2(magnet_scale, magnet_scale)
	
	owned_weapons.clear()
	var saved_weapons = pd["owned_weapons"]
	for weapon_id in saved_weapons:
		owned_weapons[weapon_id] = saved_weapons[weapon_id]
		$WeaponManager.add_weapon(Data.WEAPONS[weapon_id]["scene_path"])
		
	hud.update_weapon_slots(owned_weapons.keys())
	if hud.has_method("update_inventory_display"):
		hud.update_inventory_display(owned_items)

func get_level_up_options() -> Array:
	var valid_pool = []
	
	for upgrade in Data.UPGRADES:
		valid_pool.append({"type": "stat", "data": upgrade})
		
	for weapon_id in Data.WEAPONS:
		if owned_weapons.has(weapon_id):
			var current_lvl = owned_weapons[weapon_id]
			if current_lvl < Data.WEAPONS[weapon_id]["max_level"]:
				valid_pool.append({"type": "weapon", "id": weapon_id, "is_new": false, "level": current_lvl + 1})
		else:
			if owned_weapons.size() < Data.MAX_WEAPONS:
				valid_pool.append({"type": "weapon", "id": weapon_id, "is_new": true})
				
	valid_pool.shuffle()
	
	var options = []
	for i in range(min(3, valid_pool.size())):
		var item = valid_pool[i]
		if item["type"] == "stat":
			var rarity = _roll_rarity()
			var r_data = Data.RARITY[rarity]
			var final_val = item["data"]["base_val"] * r_data["mult"]
			
			var display_text = item["data"]["base_text"]
			if item["data"]["base_val"] > 0:
				display_text = display_text % str(final_val)
				
			options.append({
				"id": item["data"]["id"],
				"type": "stat",
				"text": display_text,
				"color": r_data["color"],
				"value": final_val,
				"rarity": rarity
			})
		else:
			var w_id = item["id"]
			var w_data = Data.WEAPONS[w_id]
			var text = "New Weapon: " + w_data["display_name"] if item["is_new"] else "Upgrade " + w_data["display_name"] + " (Lv " + str(item["level"]) + ")"
			
			options.append({
				"id": w_id,
				"type": "weapon",
				"text": text,
				"color": Data.RARITY["white"]["color"],
				"value": 0,
				"rarity": "white"
			})
			
	return options

func _roll_rarity() -> String:
	var roll = randi() % 100
	var cumulative = 0
	
	for r_key in Data.RARITY:
		cumulative += Data.RARITY[r_key]["weight"]
		if roll < cumulative:
			return r_key
			
	return "white"

func _on_magnet_zone_area_entered(area: Area2D) -> void:
	if area.has_method("pull_to_player"):
		area.pull_to_player(self)

func _physics_process(delta: float) -> void:
	_timer_calc(delta)
	_movement_handle()
	_handle_damage(delta)
	_handle_regeneration(delta)
	_handle_powerups(delta)
	_handle_relics(delta) 

func _movement_handle():
	var direction := Input.get_vector("move_left", "move_right", "move_up", "move_down")
	velocity = direction * speed
	
	if velocity.length() > 0:
		if not step_sound.playing:
			step_sound.play()
	else:
		step_sound.stop()
		
	_update_animations(direction)
	move_and_slide()

func _timer_calc(delta: float):
	time_survived += delta
	var minutes = int(time_survived) / 60
	var seconds = int(time_survived) % 60
	hud.update_time(minutes, seconds)
	
	if time_survived >= 600.0 and not end_times_triggered:
		end_times_triggered = true
		_trigger_end_times()

func _trigger_end_times() -> void:
	var spawner = get_tree().current_scene.get_node_or_null("Spawner")
	if spawner and spawner.has_method("start_end_times"):
		spawner.start_end_times()

func _handle_regeneration(delta: float) -> void:
	if hp_regen_rate > 0.0 and current_health < max_health:
		regen_accumulator += hp_regen_rate * delta
		if regen_accumulator >= 1.0:
			var heal_amount = floor(regen_accumulator)
			current_health = min(current_health + heal_amount, max_health)
			regen_accumulator -= heal_amount
			hud.update_health(current_health, max_health)

func add_kill() -> void:
	kill_count += 1
	hud.update_kills(kill_count)

	if vampirism_rate > 0.0 and current_health < max_health:
		if randf() <= vampirism_rate:
			var heal_amt = max_health * 0.05
			current_health = min(current_health + heal_amt, max_health)
			hud.update_health(current_health, max_health)

func _handle_damage(_delta: float) -> void:
	if is_invincible:
		return
		
	var overlapping_mobs = %HurtBox.get_overlapping_bodies()
	
	for body in overlapping_mobs:
		if body.is_in_group("enemy"):
			if randf() < evasion_chance:
				return

			if shield_active:
				shield_active = false
				shield_timer = shield_cooldown
				_trigger_beanie_shockwave()
				trigger_iframes()
				return
				
			var damage_taken = 10
			
			if "attack_damage" in body:
				damage_taken = body.attack_damage
				
			current_health -= damage_taken
			hud.update_health(current_health, max_health)
			
			if thorns_multiplier > 0.0 and body.has_method("take_damage"):
				body.take_damage(int(damage_taken * thorns_multiplier))
			
			if current_health <= 0.0:
				get_tree().paused = true
				hud.show_game_over()
			else:
				trigger_iframes()
			return

func take_damage(damage_amount: int) -> void:
	if is_invincible:
		return
		
	if randf() < evasion_chance:
		return

	if shield_active:
		shield_active = false
		shield_timer = shield_cooldown
		_trigger_beanie_shockwave()
		trigger_iframes()
		return
		
	current_health -= damage_amount
	hud.update_health(current_health, max_health)
	
	if current_health <= 0.0:
		get_tree().paused = true
		hud.show_game_over()
	else:
		trigger_iframes()

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

func _play_pickup_sfx(pitch: float, volume: float = -10.0, throttle: bool = false) -> void:
	if throttle:
		var current_time = Time.get_ticks_msec()
		if current_time - last_sfx_time < 40: 
			return
		last_sfx_time = current_time

	var player_audio = AudioStreamPlayer.new()
	player_audio.stream = sfx_pickup
	player_audio.pitch_scale = pitch
	player_audio.volume_db = volume
	add_child(player_audio)
	player_audio.play()
	player_audio.finished.connect(player_audio.queue_free)

func gain_experience(amount: int) -> void:
	current_exp += round(amount * exp_multiplier)
	
	_play_pickup_sfx(randf_range(1.4, 1.7), -12.0, true) 
	
	var leveled_up = false
	
	while current_exp >= exp_to_next_level:
		current_exp -= exp_to_next_level
		level += 1
		exp_to_next_level = int(15 + (level * 10) * (level * 0.4))
		
		max_health += 5.0
		damage_multiplier += 0.02
		current_health = max_health
		leveled_up = true
		
	if leveled_up:
		_trigger_level_up_ui()
	else:
		hud.update_exp(current_exp, exp_to_next_level)

func _trigger_level_up_ui() -> void:
	hud.update_health(current_health, max_health)
	hud.update_exp(current_exp, exp_to_next_level)
	hud.update_level(level)
	
	var lvl_audio = AudioStreamPlayer.new()
	lvl_audio.stream = sfx_levelup
	lvl_audio.volume_db = -12.0
	lvl_audio.process_mode = Node.PROCESS_MODE_ALWAYS
	add_child(lvl_audio)
	lvl_audio.play()
	lvl_audio.finished.connect(lvl_audio.queue_free)
	
	get_tree().paused = true
	var options = get_level_up_options()
	hud.show_level_up(options)

func collect_coin(amount: int, is_gold: bool = false) -> void:
	if is_gold:
		Data.coins += amount
	else:
		Data.silver += amount
		
	_play_pickup_sfx(2.0, -5.0, true)
	if hud and hud.has_method("update_coins"):
		hud.update_coins() # FIXED: No arguments needed!

func _apply_upgrade(upgrade: Dictionary) -> void:
	var id = upgrade["id"]
	var val = upgrade["value"]
	
	if upgrade["type"] == "weapon":
		_acquire_weapon(id)
	else:
		if id == "max_hp":
			var hp_increase = max_health * (val / 100.0)
			max_health += hp_increase
			current_health += hp_increase
		elif id == "speed":
			speed += speed * (val / 100.0)
		elif id == "damage":
			damage_multiplier += (val / 100.0)
		elif id == "fire_rate":
			fire_rate_multiplier -= (val / 100.0)
			if fire_rate_multiplier < 0.2:
				fire_rate_multiplier = 0.2
		elif id == "aoe_size":
			aoe_multiplier += (val / 100.0)
		elif id == "multi_attack":
			bonus_attacks += int(val)
		elif id == "fire_imbue":
			imbue_fire = true
		elif id == "frost_imbue":
			imbue_frost = true
		elif id == "regeneration":
			hp_regen_rate += val
		elif id == "thorns":
			thorns_multiplier += (val / 100.0)
		elif id == "evasion":
			evasion_chance += (val / 100.0)
		elif id == "exp_boost":
			exp_multiplier += (val / 100.0)
			
	hud.update_health(current_health, max_health)

func _acquire_weapon(weapon_id: String) -> void:
	if owned_weapons.has(weapon_id):
		var current_lvl = owned_weapons[weapon_id]
		if current_lvl < Data.WEAPONS[weapon_id]["max_level"]:
			owned_weapons[weapon_id] += 1
	else:
		if owned_weapons.size() < Data.MAX_WEAPONS:
			owned_weapons[weapon_id] = 1
			$WeaponManager.add_weapon(Data.WEAPONS[weapon_id]["scene_path"])
			
	hud.update_weapon_slots(owned_weapons.keys())

func _handle_relics(delta: float) -> void:
	if has_shield and not shield_active:
		shield_timer -= delta
		if shield_timer <= 0.0:
			shield_active = true

	if has_nova:
		nova_timer -= delta
		if nova_timer <= 0.0:
			_trigger_sprinkler_nova()
			nova_timer = nova_cooldown
			
	if has_goldfish:
		goldfish_timer -= delta
		if goldfish_timer <= 0.0:
			goldfish_timer = 60.0
			if Data.silver >= 5:
				Data.silver -= 5
				_prev_greed_bonus += greed_multiplier
				damage_multiplier += greed_multiplier
				if hud and hud.has_method("update_coins"):
					hud.update_coins() # FIXED: No arguments needed!
			else:
				damage_multiplier -= _prev_greed_bonus
				_prev_greed_bonus = 0.0

func _trigger_beanie_shockwave() -> void:
	var radius = 200.0 * aoe_multiplier
	var blast_dmg = 50 + (thorns_multiplier * 500)
	
	_play_pickup_sfx(0.4, 2.0) 
	
	var enemies = get_tree().get_nodes_in_group("enemy")
	for e in enemies:
		if global_position.distance_to(e.global_position) <= radius:
			if e.has_method("take_damage"):
				e.take_damage(int(blast_dmg))
			if e.has_method("apply_slow"):
				e.apply_slow(0.2)

func _trigger_sprinkler_nova() -> void:
	var radius = 350.0 * aoe_multiplier
	var base_dmg = 200 + (level * 20)
	var final_dmg = int(base_dmg * damage_multiplier)
	
	_play_pickup_sfx(0.6, 5.0) 
	
	var enemies = get_tree().get_nodes_in_group("enemy")
	for e in enemies:
		if global_position.distance_to(e.global_position) <= radius:
			if e.has_method("take_damage"):
				e.take_damage(final_dmg)
			if e.has_method("apply_slow"):
				e.apply_slow(0.8)

func add_relic_item(item_id: String) -> void:
	owned_items.append(item_id)
	_apply_relic_stats(item_id, true)
	
	if hud.has_method("show_item_get"):
		hud.show_item_get(item_id)
	if hud.has_method("update_inventory_display"):
		hud.update_inventory_display(owned_items)

func _apply_relic_stats(item_id: String, is_new: bool) -> void:
	var item_data = Data.ITEMS[item_id]
	match item_data["type"]:
		"vampirism":
			vampirism_rate += item_data["value"]
		"nova":
			has_nova = true
			nova_cooldown = item_data["value"]
		"shield":
			has_shield = true
			shield_active = true
			shield_cooldown = item_data["value"]
		"greed":
			has_goldfish = true
			greed_multiplier += item_data["value"]

func _handle_powerups(delta: float) -> void:
	if magnet_time_left > 0.0:
		magnet_time_left -= delta
		var all_seeds = get_tree().get_nodes_in_group("exp_seed")
		for exp_seed in all_seeds:
			if exp_seed.has_method("pull_to_player"):
				exp_seed.pull_to_player(self)
				
	if speed_time_left > 0.0:
		speed_time_left -= delta
		if speed_time_left <= 0.0:
			speed = base_speed_before_boost
			is_speed_boosted = false
			%AnimatedSprite2D.modulate = Color(1.0, 1.0, 1.0)

func activate_magnet_powerup() -> void:
	_play_pickup_sfx(1.3, -5.0) 
	magnet_time_left = 5.0

func activate_speed_powerup() -> void:
	_play_pickup_sfx(1.3, -5.0) 
	if not is_speed_boosted:
		base_speed_before_boost = speed
		speed += speed * 0.5
		is_speed_boosted = true
		%AnimatedSprite2D.modulate = Color(0.5, 0.8, 1.0)
	speed_time_left = 5.0

func activate_bomb_powerup(bomb_pos: Vector2) -> void:
	_play_pickup_sfx(1.3, -5.0) 
	var explosion_radius: float = 600.0
	var bomb_damage: int = 150
	var all_enemies = get_tree().get_nodes_in_group("enemy")
	for enemy in all_enemies:
		if enemy.global_position.distance_to(bomb_pos) <= explosion_radius:
			if enemy.has_method("take_damage"):
				enemy.take_damage(bomb_damage)

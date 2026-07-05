extends CharacterBody2D

@export var speed: float = 165.0
@export var max_health: float = 250.0

var is_invincible: bool = false
var i_frame_duration: float = 0.4

var level: int = 1
var current_exp: int = 0
var exp_to_next_level: int = 15

var current_health: float
var base_damage_multiplier: float = 1.0
var damage_multiplier: float = 1.0
var time_survived: float = 0.0
var kill_count: int = 0
var fire_rate_multiplier: float = 1.0

var aoe_multiplier: float = 1.0
var imbue_fire: bool = false
var imbue_frost: bool = false
var exp_multiplier: float = 1.0

var hp_regen_rate: float = 0.0
var regen_accumulator: float = 0.0
var thorns_multiplier: float = 0.0
var evasion_chance: float = 0.0

var magnet_scale: float = 1.0
var magnet_time_left: float = 0.0
var speed_time_left: float = 0.0
var is_speed_boosted: bool = false
var base_speed_before_boost: float = 0.0

var owned_weapons: Dictionary = {}
var owned_items: Array = []
var acquired_unique_upgrades: Array[String] = []

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
var magnet_scan_interval: float = 0.2
var magnet_scan_timer: float = 0.0

var sfx_pickup = preload("res://player/orb.mp3")
var sfx_levelup = preload("res://assets/audio/levelup.wav")
var sfx_hurt = preload("res://player/hurt.mp3")

var last_sfx_time: int = 0

@onready var hud = $HUD
@onready var step_sound = $StepSound
@onready var animated_sprite: AnimatedSprite2D = $AnimatedSprite2D
@onready var hurt_box: Area2D = $HurtBox
@onready var magnet_zone: Area2D = $MagnetZone
@onready var weapon_manager: Node2D = $WeaponManager

func _ready() -> void:
	if Data.player_data.is_empty():
		_apply_permanent_upgrades()
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
		hud.update_coins()
	
	hud.upgrade_selected.connect(_apply_upgrade)
	magnet_zone.area_entered.connect(_on_magnet_zone_area_entered)
	
	if hud.has_method("update_player_stats"):
		hud.update_player_stats(self)

func _apply_permanent_upgrades() -> void:
	var upgrades = Data.permanent_upgrades
	if upgrades.has("max_hp"):
		max_health += upgrades["max_hp"]["level"] * 10.0
	if upgrades.has("damage"):
		base_damage_multiplier += upgrades["damage"]["level"] * 0.05
	if upgrades.has("speed"):
		speed += upgrades["speed"]["level"] * 10.0
	if upgrades.has("regeneration"):
		hp_regen_rate += upgrades["regeneration"]["level"] * 0.5
	if upgrades.has("armor"):
		thorns_multiplier += upgrades["armor"]["level"] * 0.1
		if upgrades.has("evasion"):
			evasion_chance += upgrades["evasion"]["level"] * 0.02
		if upgrades.has("exp_gain"):
			exp_multiplier += upgrades["exp_gain"]["level"] * 0.10

func save_data() -> void:
	Data.player_data = {
		"level": level, "current_exp": current_exp, "exp_to_next_level": exp_to_next_level,
		"max_health": max_health, "current_health": current_health, "speed": speed,
		"base_damage_multiplier": base_damage_multiplier, "fire_rate_multiplier": fire_rate_multiplier,
		"aoe_multiplier": aoe_multiplier, "imbue_fire": imbue_fire, "imbue_frost": imbue_frost,
		"hp_regen_rate": hp_regen_rate, "thorns_multiplier": thorns_multiplier,
		"evasion_chance": evasion_chance, "base_crit_chance": base_crit_chance,
		"exp_multiplier": exp_multiplier,
		"kill_count": kill_count, "time_survived": time_survived, "run_time": Data.run_time,
		"owned_weapons": owned_weapons, "owned_items": owned_items, "magnet_scale": magnet_scale,
		"acquired_unique_upgrades": acquired_unique_upgrades
	}

func _load_data() -> void:
	var pd = Data.player_data
	level = pd["level"]
	current_exp = pd["current_exp"]
	exp_to_next_level = pd["exp_to_next_level"]
	max_health = pd["max_health"]
	current_health = pd["current_health"]
	speed = pd["speed"]
	base_damage_multiplier = pd.get("base_damage_multiplier", 1.0)
	fire_rate_multiplier = pd["fire_rate_multiplier"]
	aoe_multiplier = pd["aoe_multiplier"]
	imbue_fire = pd["imbue_fire"]
	imbue_frost = pd["imbue_frost"]
	hp_regen_rate = pd["hp_regen_rate"]
	thorns_multiplier = pd["thorns_multiplier"]
	evasion_chance = pd["evasion_chance"]
	base_crit_chance = pd.get("base_crit_chance", 0.0)
	exp_multiplier = pd.get("exp_multiplier", 1.0)
	kill_count = pd["kill_count"]
	time_survived = 0.0
	Data.run_time = pd.get("run_time", Data.run_time)
	acquired_unique_upgrades.clear()
	for upgrade_id in pd.get("acquired_unique_upgrades", []):
		acquired_unique_upgrades.append(str(upgrade_id))
	
	if pd.has("owned_items"):
		owned_items = []
		for item in pd["owned_items"]:
			var item_id := str(item)
			if not Data.ITEMS.has(item_id):
				push_warning("[DataEffects] Saved item '%s' is not available; skipping" % item_id)
				continue
			owned_items.append(item_id)
			_apply_relic_stats(item_id, false)
	
	if pd.has("magnet_scale"):
		magnet_scale = pd["magnet_scale"]
		magnet_zone.scale = Vector2(magnet_scale, magnet_scale)
	
	owned_weapons.clear()
	var saved_weapons = pd["owned_weapons"]
	for w_id in saved_weapons:
		var w_data = saved_weapons[w_id]
		if typeof(w_data) == TYPE_INT:
			owned_weapons[w_id] = {"level": w_data, "damage": 1.0, "size": 1.0, "fire_rate": 1.0, "pierce": 0, "ricochet": 0, "projectile": 0}
		else:
			owned_weapons[w_id] = w_data
		weapon_manager.add_weapon(Data.WEAPONS[w_id]["scene_path"], w_id)
		
	hud.update_weapon_slots(owned_weapons.keys())
	if hud.has_method("update_inventory_display"):
		hud.update_inventory_display(owned_items)

func get_level_up_options() -> Array:
	var valid_pool = []
	
	for upgrade in Data.UPGRADES:
		if typeof(upgrade) != TYPE_DICTIONARY:
			push_warning("[DataEffects] Upgrade entry must be an object")
			continue
		var upgrade_id := str(upgrade.get("id", ""))
		if upgrade_id == "":
			push_warning("[DataEffects] Upgrade entry is missing an id")
			continue
		if upgrade.get("unique", false) and acquired_unique_upgrades.has(upgrade_id):
			continue
		valid_pool.append({"type": "stat", "data": upgrade})
		
	if owned_weapons.size() < 1:
		for w_id in Data.WEAPONS:
			valid_pool.append({"type": "weapon_unlock", "id": w_id})
	else:
		var active_weapon = owned_weapons.keys()[0]
		if owned_weapons[active_weapon]["level"] < 99:
			var possible_buffs = [
				{"type": "damage", "base_text": "+%s%% Damage", "val": 0.15},
				{"type": "fire_rate", "base_text": "+%s%% Fire Rate", "val": 0.10}
			]
			
			match active_weapon:
				"poison_aura":
					possible_buffs.append({"type": "size", "base_text": "+%s%% Aura Radius", "val": 0.15})
				"wand":
					possible_buffs.append({"type": "size", "base_text": "+%s%% Splash Size", "val": 0.15})
					possible_buffs.append({"type": "ricochet", "base_text": "+%s Bounce", "val": 1.0})
					possible_buffs.append({"type": "projectile", "base_text": "+%s Projectile", "val": 1.0})
				"sword", "axe":
					possible_buffs.append({"type": "size", "base_text": "+%s%% Reach", "val": 0.15})
					possible_buffs.append({"type": "pierce", "base_text": "+%s Pierce", "val": 1.0})
				"cat":
					possible_buffs.append({"type": "size", "base_text": "+%s%% Size", "val": 0.15})
					possible_buffs.append({"type": "ricochet", "base_text": "+%s Bounce", "val": 1.0})
				_:
					possible_buffs.append({"type": "size", "base_text": "+%s%% Size", "val": 0.15})
					possible_buffs.append({"type": "pierce", "base_text": "+%s Pierce", "val": 1.0})
					possible_buffs.append({"type": "ricochet", "base_text": "+%s Ricochet", "val": 1.0})
			
			for i in range(4):
				var r_buff = possible_buffs.pick_random()
				valid_pool.append({"type": "weapon_buff", "id": active_weapon, "buff": r_buff})

	var options = []
	var used_ids: Dictionary = {}
	var attempts := 0
	valid_pool.shuffle()
	
	while options.size() < 3 and valid_pool.size() > 0 and attempts < 24:
		attempts += 1
		var item: Dictionary = valid_pool.pop_front() as Dictionary
		var option_id: String = str(item.get("id", ""))

		if option_id == "" and item.has("data"):
			option_id = str(item["data"].get("id", ""))
		var unique_key = option_id + ":" + item.get("type", "")
		if item.get("type", "") == "weapon_buff":
			unique_key = option_id + ":" + item["buff"]["type"]
		if used_ids.has(unique_key):
			continue
		used_ids[unique_key] = true
		
		var rarity = _roll_rarity()
		var r_data = Data.RARITY[rarity]
		var rarity_color = r_data["color"]
		
		if item["type"] == "stat":
			var upgrade_data: Dictionary = item["data"]
			var option_source := "upgrade:" + str(upgrade_data.get("id", ""))
			var base_value_result := _get_optional_data_number(upgrade_data, "base_val", 0.0, option_source)
			if not base_value_result["ok"]:
				continue

			var final_val: float = base_value_result["value"] * float(r_data["mult"])
			var display_text := str(upgrade_data.get("base_text", upgrade_data.get("id", "Upgrade")))
			
			if "%s" in display_text:
				if "%%" in display_text or final_val != int(final_val):
					display_text = display_text % str(snapped(final_val, 0.1))
				else:
					display_text = display_text % str(int(final_val))
				
			options.append({
				"id": str(upgrade_data.get("id", "")), "type": "stat", "text": display_text,
				"color": rarity_color, "value": final_val, "rarity": rarity,
				"unique": upgrade_data.get("unique", false),
				"effects": upgrade_data.get("effects", [])
			})
			
		elif item["type"] == "weapon_unlock":
			var w_data = Data.WEAPONS[item["id"]]
			options.append({
				"id": item["id"], "type": "weapon_unlock",
				"text": "New Weapon:\n" + w_data["display_name"],
				"color": Data.RARITY["gold"]["color"], "value": 0, "rarity": "gold"
			})
			
		elif item["type"] == "weapon_buff":
			var w_data = Data.WEAPONS[item["id"]]
			var buff_data = item["buff"]
			var final_val = buff_data["val"] * r_data["mult"]
			
			var display_val = final_val
			if buff_data["type"] in ["damage", "size", "fire_rate"]:
				display_val = final_val * 100.0
				
			var buff_text = buff_data["base_text"] % str(snapped(display_val, 0.1))
			var current_level = owned_weapons[item["id"]]["level"] + 1
			
			options.append({
				"id": item["id"], "type": "weapon_buff", "buff_type": buff_data["type"],
				"text": w_data["display_name"] + " Lv." + str(current_level) + "\n" + buff_text,
				"color": rarity_color,
				"value": final_val, "rarity": rarity
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
		if step_sound.playing:
			step_sound.stop()
	_update_animations(direction)
	move_and_slide()

func _timer_calc(delta: float):
	time_survived += delta
	Data.advance_run_time(delta)
	var minutes = int(time_survived) / 60
	var seconds = int(time_survived) % 60
	hud.update_time(minutes, seconds)

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
			current_health = min(current_health + (max_health * 0.05), max_health)
			hud.update_health(current_health, max_health)

func _handle_damage(_delta: float) -> void:
	if is_invincible:
		return
	var overlapping_mobs = hurt_box.get_overlapping_bodies()
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
			_play_hurt_sfx()
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
	_play_hurt_sfx()
	hud.update_health(current_health, max_health)
	if current_health <= 0.0:
		get_tree().paused = true
		hud.show_game_over()
	else:
		trigger_iframes()

func trigger_iframes() -> void:
	is_invincible = true
	var tween = create_tween()
	tween.tween_property(animated_sprite, "modulate:a", 0.3, 0.1)
	tween.tween_property(animated_sprite, "modulate:a", 1.0, 0.1)
	tween.set_loops(int(i_frame_duration / 0.2))
	await get_tree().create_timer(i_frame_duration).timeout
	is_invincible = false
	animated_sprite.modulate.a = 1.0

func _update_animations(dir: Vector2) -> void:
	if dir.length() > 0:
		if abs(dir.x) > abs(dir.y):
			animated_sprite.play("right" if dir.x > 0 else "left")
		else:
			animated_sprite.play("down" if dir.y > 0 else "up")
	else:
		animated_sprite.stop()

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

func _play_hurt_sfx() -> void:
	var player_audio = AudioStreamPlayer.new()
	player_audio.stream = sfx_hurt
	player_audio.pitch_scale = randf_range(1.4, 1.8)
	player_audio.volume_db = -5.0
	add_child(player_audio)
	player_audio.play()
	player_audio.finished.connect(player_audio.queue_free)

func gain_experience(amount: int) -> void:
	var final_amount = int(amount * exp_multiplier)
	current_exp += final_amount
	_play_pickup_sfx(randf_range(1.4, 1.7), -12.0, true)
	var leveled_up = false
	
	while current_exp >= exp_to_next_level:
		current_exp -= exp_to_next_level
		level += 1
		exp_to_next_level = int(15 + (level * 10) * (level * 0.4))
		max_health += 10.0
		base_damage_multiplier += 0.05
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
		hud.update_coins()

func _apply_upgrade(upgrade: Dictionary) -> void:
	var id = upgrade["id"]
	var val = upgrade["value"]
	
	if upgrade["type"] == "weapon_unlock":
		_acquire_weapon(id)
	elif upgrade["type"] == "weapon_buff":
		_apply_weapon_buff(id, upgrade["buff_type"], val)
	else:
		if id == "max_hp":
			var hp_increase = max_health * (val / 100.0)
			max_health += hp_increase
			current_health += hp_increase
		elif id == "speed":
			speed += speed * (val / 100.0)
		elif id == "damage":
			base_damage_multiplier += (val / 100.0)
		elif id == "fire_rate":
			fire_rate_multiplier = max(0.2, fire_rate_multiplier - (val / 100.0))
		elif id == "aoe_size":
			aoe_multiplier += (val / 100.0)
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
		elif id == "crit_chance":
			base_crit_chance += (val / 100.0)
		elif id == "exp_boost":
			exp_multiplier += (val / 100.0)
		elif id == "multi_attack":
			for w_id in owned_weapons:
				owned_weapons[w_id]["projectile"] += int(val)
				if weapon_manager.has_method("update_weapon_stats"):
					weapon_manager.update_weapon_stats(w_id, owned_weapons[w_id])
		elif id == "glass_cannon":
			base_damage_multiplier += (val / 100.0)
			max_health -= (max_health * 0.20)
			current_health = min(current_health, max_health)
		elif id == "heavy_armor":
			thorns_multiplier += (val / 100.0)
			speed -= (speed * 0.15)
			max_health += (max_health * 0.10)
			current_health += (max_health * 0.10)
		elif id == "berserker":
			fire_rate_multiplier = max(0.2, fire_rate_multiplier - (val / 100.0))
			evasion_chance = max(0.0, evasion_chance - 0.10)
		elif id == "vampiric_edge":
			vampirism_rate += val / 100.0
		elif id == "magnet_training":
			magnet_scale += val / 100.0
			magnet_zone.scale = Vector2(magnet_scale, magnet_scale)
		elif id == "precision":
			base_crit_chance += val / 100.0
			base_damage_multiplier += val / 100.0
		elif id == "momentum":
			speed += speed * (val / 100.0)
			fire_rate_multiplier = max(0.2, fire_rate_multiplier - (val / 100.0))
		elif id == "soul_harvest":
			exp_multiplier += val / 100.0
			hp_regen_rate += 1.0
		elif upgrade.has("effects"):
			_apply_data_effects(upgrade["effects"], val, "upgrade:" + id)
		else:
			push_warning("[DataEffects] Unsupported upgrade id '%s' and no effects provided" % id)

		if upgrade.get("unique", false) and not acquired_unique_upgrades.has(id):
			acquired_unique_upgrades.append(id)
			
	hud.update_health(current_health, max_health)
	
	if hud.has_method("update_player_stats"):
		hud.update_player_stats(self)

func _acquire_weapon(weapon_id: String) -> void:
	if owned_weapons.size() < 1:
		owned_weapons[weapon_id] = {
			"level": 1, "damage": 1.0, "size": 1.0,
			"fire_rate": 1.0, "pierce": 0, "ricochet": 0, "projectile": 0
		}
		weapon_manager.add_weapon(Data.WEAPONS[weapon_id]["scene_path"], weapon_id)
			
	hud.update_weapon_slots(owned_weapons.keys())

func _apply_weapon_buff(weapon_id: String, buff_type: String, val: float) -> void:
	if not owned_weapons.has(weapon_id):
		return
	
	var w_data = owned_weapons[weapon_id]
	w_data["level"] += 1
	
	if buff_type == "damage":
		w_data["damage"] += val
	elif buff_type == "size":
		w_data["size"] += val
	elif buff_type == "fire_rate":
		w_data["fire_rate"] += val
	elif buff_type == "pierce":
		w_data["pierce"] += int(val)
	elif buff_type == "ricochet":
		w_data["ricochet"] += int(val)
	elif buff_type == "projectile":
		w_data["projectile"] += int(val)
	
	if weapon_manager.has_method("update_weapon_stats"):
		weapon_manager.update_weapon_stats(weapon_id, w_data)
		
	if hud.has_method("update_player_stats"):
		hud.update_player_stats(self)

func _on_magnet_zone_area_entered(area: Area2D) -> void:
	if area.has_method("pull_to_player"):
		area.pull_to_player(self)

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
				base_damage_multiplier += greed_multiplier
				if hud and hud.has_method("update_coins"):
					hud.update_coins()
			else:
				base_damage_multiplier -= _prev_greed_bonus
				_prev_greed_bonus = 0.0

func _trigger_beanie_shockwave() -> void:
	var radius = 200.0 * aoe_multiplier
	var radius_sq = radius * radius
	var blast_dmg = 50 + (thorns_multiplier * 500)
	_play_pickup_sfx(0.4, 2.0)
	var enemies = get_tree().get_nodes_in_group("enemy")
	for e in enemies:
		if global_position.distance_squared_to(e.global_position) <= radius_sq:
			if e.has_method("take_damage"):
				e.take_damage(int(blast_dmg))
			if e.has_method("apply_slow"):
				e.apply_slow(0.2)

func _trigger_sprinkler_nova() -> void:
	var radius = 350.0 * aoe_multiplier
	var radius_sq = radius * radius
	var base_dmg = 200 + (level * 20)
	var final_dmg = int(base_dmg * damage_multiplier)
	_play_pickup_sfx(0.6, 5.0)
	var enemies = get_tree().get_nodes_in_group("enemy")
	for e in enemies:
		if global_position.distance_squared_to(e.global_position) <= radius_sq:
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

func _apply_relic_stats(item_id: String, _is_new: bool) -> void:
	var source := "item:" + item_id
	var item_data = Data.ITEMS.get(item_id, null)
	if typeof(item_data) != TYPE_DICTIONARY:
		push_warning("[DataEffects] %s is not available; skipping relic stats" % source)
		return

	var item_value_result := _get_optional_data_number(item_data, "value", 0.0, source)
	if not item_value_result["ok"]:
		return
	var item_value: float = item_value_result["value"]

	if item_data.has("effects"):
		_apply_data_effects(item_data["effects"], item_value, source)
		return

	var item_type := str(item_data.get("type", ""))
	match item_type:
		"vampirism":
			vampirism_rate += item_value
		"nova":
			has_nova = true
			nova_cooldown = item_value
		"shield":
			has_shield = true
			shield_active = true
			shield_cooldown = item_value
		"greed":
			has_goldfish = true
			greed_multiplier += item_value
		_:
			push_warning("[DataEffects] Unsupported item type '%s' for '%s' and no effects provided" % [item_type, item_id])

func _apply_data_effects(effects, scaled_value: float, source: String) -> void:
	if typeof(effects) != TYPE_ARRAY:
		push_warning("[DataEffects] %s effects must be an array" % source)
		return

	for effect in effects:
		if typeof(effect) != TYPE_DICTIONARY:
			push_warning("[DataEffects] %s has a non-object effect" % source)
			continue

		var stat := str(effect.get("stat", ""))
		var op := str(effect.get("op", "add_percent"))
		var amount_result := _get_data_effect_amount(effect, scaled_value, source)
		if not amount_result["ok"]:
			continue
		var amount: float = amount_result["value"]
		_apply_data_effect(stat, op, amount, source)

func _get_data_effect_amount(effect: Dictionary, scaled_value: float, source: String) -> Dictionary:
	if effect.has("value"):
		return _get_data_number(effect["value"], source, "value")
	if effect.has("scale"):
		var scale_result := _get_data_number(effect["scale"], source, "scale")
		if not scale_result["ok"]:
			return scale_result
		return {"ok": true, "value": scaled_value * scale_result["value"]}
	return {"ok": true, "value": scaled_value}

func _get_optional_data_number(data: Dictionary, field: String, default_value: float, source: String) -> Dictionary:
	if not data.has(field):
		return {"ok": true, "value": default_value}
	return _get_data_number(data[field], source, field)

func _get_data_number(value, source: String, field: String) -> Dictionary:
	match typeof(value):
		TYPE_INT, TYPE_FLOAT:
			return {"ok": true, "value": float(value)}
		_:
			push_warning("[DataEffects] %s field '%s' must be a number" % [source, field])
			return {"ok": false, "value": 0.0}

func _apply_data_effect(stat: String, op: String, amount: float, source: String) -> void:
	match stat:
		"max_hp":
			var hp_before := max_health
			max_health = _apply_numeric_effect(max_health, op, amount, source, stat)
			current_health += max(0.0, max_health - hp_before)
		"speed":
			speed = _apply_numeric_effect(speed, op, amount, source, stat)
		"damage":
			base_damage_multiplier = _apply_numeric_effect(base_damage_multiplier, op, amount, source, stat)
		"fire_rate":
			fire_rate_multiplier = max(0.2, _apply_numeric_effect(fire_rate_multiplier, op, amount, source, stat, true))
		"aoe_size":
			aoe_multiplier = _apply_numeric_effect(aoe_multiplier, op, amount, source, stat)
		"regeneration":
			hp_regen_rate = _apply_numeric_effect(hp_regen_rate, op, amount, source, stat)
		"thorns":
			thorns_multiplier = _apply_numeric_effect(thorns_multiplier, op, amount, source, stat)
		"evasion":
			evasion_chance = _apply_numeric_effect(evasion_chance, op, amount, source, stat)
		"crit_chance":
			base_crit_chance = _apply_numeric_effect(base_crit_chance, op, amount, source, stat)
		"exp_gain":
			exp_multiplier = _apply_numeric_effect(exp_multiplier, op, amount, source, stat)
		"pickup_range":
			magnet_scale = _apply_numeric_effect(magnet_scale, op, amount, source, stat)
			magnet_zone.scale = Vector2(magnet_scale, magnet_scale)
		"vampirism":
			vampirism_rate = _apply_numeric_effect(vampirism_rate, op, amount, source, stat)
		"nova_cooldown":
			has_nova = true
			nova_cooldown = _apply_numeric_effect(nova_cooldown, op, amount, source, stat)
		"shield_cooldown":
			has_shield = true
			shield_active = true
			shield_cooldown = _apply_numeric_effect(shield_cooldown, op, amount, source, stat)
		"greed":
			has_goldfish = true
			greed_multiplier = _apply_numeric_effect(greed_multiplier, op, amount, source, stat)
		"imbue_fire":
			imbue_fire = _apply_bool_effect(imbue_fire, op, amount, source, stat)
		"imbue_frost":
			imbue_frost = _apply_bool_effect(imbue_frost, op, amount, source, stat)
		"multi_attack":
			_apply_all_weapon_stat_effect("projectile", op, amount, source)
		"weapon_damage":
			_apply_all_weapon_stat_effect("damage", op, amount, source)
		"weapon_size":
			_apply_all_weapon_stat_effect("size", op, amount, source)
		"weapon_fire_rate":
			_apply_all_weapon_stat_effect("fire_rate", op, amount, source)
		_:
			push_warning("[DataEffects] %s has unsupported stat '%s'" % [source, stat])

func _apply_numeric_effect(current: float, op: String, amount: float, source: String, stat: String, invert_percent: bool = false) -> float:
	match op:
		"add":
			return current + amount
		"add_percent":
			var delta := amount / 100.0
			if invert_percent:
				return current - delta
			return current + delta
		"multiply":
			return current * amount
		"set":
			return amount
		_:
			push_warning("[DataEffects] %s has unsupported op '%s' for '%s'" % [source, op, stat])
			return current

func _apply_bool_effect(current: bool, op: String, amount: float, source: String, stat: String) -> bool:
	match op:
		"set", "enable", "set_true":
			return amount != 0.0
		_:
			push_warning("[DataEffects] %s has unsupported op '%s' for '%s'" % [source, op, stat])
			return current

func _apply_all_weapon_stat_effect(stat: String, op: String, amount: float, source: String) -> void:
	for w_id in owned_weapons:
		var current := float(owned_weapons[w_id].get(stat, 0.0))
		owned_weapons[w_id][stat] = _apply_numeric_effect(current, op, amount, source, "weapon." + stat)
		if stat in ["pierce", "ricochet", "projectile"]:
			owned_weapons[w_id][stat] = int(owned_weapons[w_id][stat])
		if weapon_manager.has_method("update_weapon_stats"):
			weapon_manager.update_weapon_stats(w_id, owned_weapons[w_id])

func _handle_powerups(delta: float) -> void:
	if magnet_time_left > 0.0:
		magnet_time_left -= delta
		magnet_scan_timer -= delta
		if magnet_scan_timer <= 0.0:
			magnet_scan_timer = magnet_scan_interval
			var all_seeds = get_tree().get_nodes_in_group("exp_seed")
			for exp_seed in all_seeds:
				if exp_seed.has_method("pull_to_player"):
					exp_seed.pull_to_player(self)
				
	if speed_time_left > 0.0:
		speed_time_left -= delta
		if speed_time_left <= 0.0:
			speed = base_speed_before_boost
			is_speed_boosted = false
			animated_sprite.modulate = Color(1.0, 1.0, 1.0)

func activate_magnet_powerup() -> void:
	_play_pickup_sfx(1.3, -5.0)
	magnet_time_left = 5.0
	magnet_scan_timer = 0.0

func activate_speed_powerup() -> void:
	_play_pickup_sfx(1.3, -5.0)
	if not is_speed_boosted:
		base_speed_before_boost = speed
		speed += speed * 0.5
		is_speed_boosted = true
		animated_sprite.modulate = Color(0.5, 0.8, 1.0)
	speed_time_left = 5.0

func activate_bomb_powerup(bomb_pos: Vector2) -> void:
	_play_pickup_sfx(1.3, -5.0)
	var explosion_radius: float = 600.0
	var explosion_radius_sq: float = explosion_radius * explosion_radius
	var bomb_damage: int = 150
	var all_enemies = get_tree().get_nodes_in_group("enemy")
	for enemy in all_enemies:
		if enemy.global_position.distance_squared_to(bomb_pos) <= explosion_radius_sq:
			if enemy.has_method("take_damage"):
				enemy.take_damage(bomb_damage)

func heal_from_pickup(amount: int) -> void:
	if current_health >= max_health:
		return
	current_health = min(current_health + amount, max_health)
	hud.update_health(current_health, max_health)
	_play_pickup_sfx(1.8, -6.0, true)

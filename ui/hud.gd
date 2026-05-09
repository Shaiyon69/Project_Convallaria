extends CanvasLayer

signal upgrade_selected(upgrade: Dictionary)

var current_options: Array = []

var _current_level: int = 1
var _exp_display_timer: float = 0.0

var stats_wrapper: Control = null

@onready var gold_label = %GoldLabel if has_node("%GoldLabel") else null
@onready var silver_label = %SilverLabel if has_node("%SilverLabel") else null
@onready var kill_label = %KillLabel if has_node("%KillLabel") else null
@onready var health_bar = %HealthBar if has_node("%HealthBar") else null
@onready var boss_health_bar = %BossHealthBar if has_node("%BossHealthBar") else null

@onready var stat_list = $MarginContainer/VBoxContainer/MainScreen/RightPanel/StatList if has_node("MarginContainer/VBoxContainer/MainScreen/RightPanel/StatList") else find_child("StatList", true, false)

@onready var item_get_popup = %ItemGetPopup if has_node("%ItemGetPopup") else null
@onready var item_name_label = %ItemNameLabel if has_node("%ItemNameLabel") else null
@onready var item_icon_display = %ItemIconDisplay if has_node("%ItemIconDisplay") else null
@onready var item_desc_label = %ItemDescLabel if has_node("%ItemDescLabel") else null
@onready var continue_btn = %ContinueButton if has_node("%ContinueButton") else null

@onready var item_grid = %ItemGrid if has_node("%ItemGrid") else null
@onready var weapon_slots = %WeaponSlots if has_node("%WeaponSlots") else null

@onready var pause_resume_btn = %PauseOverlay.get_node("VBoxContainer/ResumeButton") if has_node("%PauseOverlay") else null
@onready var pause_options_btn = %PauseOverlay.get_node("VBoxContainer/OptionsButton") if has_node("%PauseOverlay") else null
@onready var pause_quit_btn = %PauseOverlay.get_node("VBoxContainer/QuitButton") if has_node("%PauseOverlay") else null
@onready var pause_restart_btn = %PauseOverlay.get_node("RestartButton") if has_node("%PauseOverlay") else null
@onready var mobile_pause_btn = $PauseBox/PauseButton if has_node("PauseBox/PauseButton") else null
@onready var options_menu = $Options if has_node("Options") else null
@onready var title_sprite = %PauseOverlay.get_node("HBoxContainer/Convallaria") if has_node("%PauseOverlay") else null

@onready var sfx_hover = preload("res://ui/menu_hover.mp3")
@onready var sfx_click = preload("res://ui/menu_click.mp3")

var _base_button_scales: Dictionary = {}
var _target_button_scales: Dictionary = {}

func _ready() -> void:
	get_tree().paused = false
	if has_node("%PauseOverlay"): %PauseOverlay.hide()
	if has_node("%GameOverScreen"): %GameOverScreen.hide()
	if has_node("%LevelUpScreen"): %LevelUpScreen.hide()
	if boss_health_bar: boss_health_bar.hide()
	
	var right_panel = find_child("RightPanel", true, false)
	if right_panel and right_panel.get_parent():
		var parent = right_panel.get_parent()
		
		var spacer = MarginContainer.new()
		spacer.add_theme_constant_override("margin_top", 75)
		spacer.size_flags_horizontal = Control.SIZE_SHRINK_END
		spacer.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
		
		var idx = right_panel.get_index()
		parent.remove_child(right_panel)
		parent.add_child(spacer)
		parent.move_child(spacer, idx)
		spacer.add_child(right_panel)
		
		right_panel.size_flags_horizontal = Control.SIZE_FILL
		right_panel.size_flags_vertical = Control.SIZE_FILL
		
		if right_panel is PanelContainer or right_panel is Panel:
			var style = StyleBoxFlat.new()
			style.bg_color = Color(0.1, 0.1, 0.1, 0.85)
			style.corner_radius_top_left = 12
			style.corner_radius_top_right = 12
			style.corner_radius_bottom_left = 12
			style.corner_radius_bottom_right = 12
			style.content_margin_left = 20
			style.content_margin_right = 20
			style.content_margin_top = 15
			style.content_margin_bottom = 15
			right_panel.add_theme_stylebox_override("panel", style)
			
		stats_wrapper = spacer
		stats_wrapper.hide()
	
	if item_get_popup:
		item_get_popup.hide()
		if continue_btn and not continue_btn.pressed.is_connected(_on_continue_pressed):
			continue_btn.pressed.connect(_on_continue_pressed)
	
	if has_node("%TryAgain"): %TryAgain.pressed.connect(_on_try_again_pressed)
	if has_node("%Exit"): %Exit.pressed.connect(_on_exit_pressed)
	
	if has_node("%Upgrade1"): %Upgrade1.pressed.connect(_on_upgrade_pressed.bind(0))
	if has_node("%Upgrade2"): %Upgrade2.pressed.connect(_on_upgrade_pressed.bind(1))
	if has_node("%Upgrade3"): %Upgrade3.pressed.connect(_on_upgrade_pressed.bind(2))
	
	if pause_resume_btn: pause_resume_btn.pressed.connect(_on_pause_start_pressed)
	if pause_options_btn: pause_options_btn.pressed.connect(_on_pause_options_pressed)
	if pause_quit_btn: pause_quit_btn.pressed.connect(_on_pause_quit_pressed)
	if pause_restart_btn: pause_restart_btn.pressed.connect(_on_pause_restart_pressed)
	if mobile_pause_btn: mobile_pause_btn.pressed.connect(_toggle_pause)
	
	add_to_group("hud")
	update_coins()
	
	_setup_button_animations()
	
	if title_sprite:
		_setup_title_animation()

func _process(delta: float) -> void:
	if _exp_display_timer > 0:
		_exp_display_timer -= delta
		if _exp_display_timer <= 0:
			if has_node("%ExpLabel"):
				%ExpLabel.text = "LVL " + str(_current_level)

func update_exp(current: int, maximum: int) -> void:
	if has_node("%ExpBar"):
		%ExpBar.max_value = maximum
		%ExpBar.value = current
		
	if has_node("%ExpLabel"):
		%ExpLabel.text = str(current) + " / " + str(maximum)
		_exp_display_timer = 2.0 

func update_level(level: int) -> void:
	_current_level = level
	
	if has_node("%ExpLabel"):
		if _exp_display_timer <= 0:
			%ExpLabel.text = "LVL " + str(_current_level)

func update_coins(gold_amount: int = 0, silver_amount: int = 0) -> void:
	if gold_label:
		gold_label.text = "Gold: " + str(Data.coins)
	if silver_label:
		silver_label.text = "Silver: " + str(Data.silver)

func update_kills(kills: int) -> void:
	if kill_label:
		kill_label.text = "Kills: " + str(kills)

func show_item_get(item_id: String) -> void:
	if not item_name_label or not item_desc_label:
		return
		
	var item_data = Data.ITEMS[item_id]
	
	item_name_label.text = item_data["name"]
	item_icon_display.texture = load(item_data["icon"])
	item_desc_label.text = item_data["desc"] 
	
	if Data.RARITY.has(item_data["rarity"]):
		item_name_label.add_theme_color_override("font_color", Data.RARITY[item_data["rarity"]]["color"])
	
	get_tree().paused = true
	item_get_popup.show()

func _on_continue_pressed() -> void:
	if item_get_popup:
		item_get_popup.hide()
	get_tree().paused = false

func update_inventory_display(owned_items: Array) -> void:
	if not item_grid: return
	
	for child in item_grid.get_children():
		child.queue_free()
		
	var item_counts = {}
	for item_id in owned_items:
		item_counts[item_id] = item_counts.get(item_id, 0) + 1
		
	for item_id in item_counts:
		var count = item_counts[item_id]
		var tex_rect = TextureRect.new()
		tex_rect.texture = load(Data.ITEMS[item_id]["icon"])
		tex_rect.custom_minimum_size = Vector2(32, 32)
		tex_rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
		
		if count > 1:
			var badge = Label.new()
			badge.text = "x" + str(count)
			badge.add_theme_font_size_override("font_size", 12)
			badge.add_theme_color_override("font_outline_color", Color.BLACK)
			badge.add_theme_constant_override("outline_size", 4)
			badge.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT)
			badge.position = Vector2(16, 16)
			tex_rect.add_child(badge)
			
		item_grid.add_child(tex_rect)

func update_weapon_slots(weapon_ids: Array) -> void:
	if not weapon_slots or weapon_slots.get_child_count() == 0: return
	
	var slot = weapon_slots.get_child(0)
	var icon = slot.get_node("Icon")
	
	if weapon_ids.size() > 0:
		var w_id = weapon_ids[0]
		var icon_path = Data.WEAPONS[w_id]["icon"]
		icon.texture = load(icon_path)
	else:
		icon.texture = null
		
	for i in range(1, weapon_slots.get_child_count()):
		weapon_slots.get_child(i).visible = false

func update_health(current: float, maximum: float) -> void:
	if health_bar:
		health_bar.max_value = maximum
		health_bar.value = current
	if has_node("%HealthLabel"):
		%HealthLabel.text = str(int(current), "/", int(maximum))
	
	if current / maximum <= 0.3:
		if has_node("PauseBox/LowHPWarning"): $PauseBox/LowHPWarning.visible = true
	else:
		if has_node("PauseBox/LowHPWarning"): $PauseBox/LowHPWarning.visible = false

func update_time(minutes: int, seconds: int) -> void:
	if has_node("%TimeLabel"):
		%TimeLabel.text = "%02d:%02d" % [minutes, seconds]
		if minutes >= 10:
			%TimeLabel.add_theme_color_override("font_color", Color(1, 0.2, 0.2))
		else:
			%TimeLabel.add_theme_color_override("font_color", Color(1, 1, 1))

func show_level_up(options: Array) -> void:
	current_options = options
	var buttons = []
	if has_node("%Upgrade1"): buttons.append(%Upgrade1)
	if has_node("%Upgrade2"): buttons.append(%Upgrade2)
	if has_node("%Upgrade3"): buttons.append(%Upgrade3)
	
	for i in range(buttons.size()):
		if i < options.size():
			buttons[i].text = options[i]["text"]
			buttons[i].add_theme_color_override("font_color", options[i]["color"])
			buttons[i].show()
		else:
			buttons[i].hide()
			
	if has_node("%LevelUpScreen"): %LevelUpScreen.visible = true
	if stats_wrapper: stats_wrapper.show()

func _on_upgrade_pressed(index: int) -> void:
	if has_node("%LevelUpScreen"): %LevelUpScreen.visible = false
	if stats_wrapper: stats_wrapper.hide()
	get_tree().paused = false
	upgrade_selected.emit(current_options[index])

func show_game_over() -> void:
	if has_node("%GameOverScreen"): %GameOverScreen.visible = true

func _on_try_again_pressed() -> void:
	get_tree().paused = false
	TransitionManager.change_scene("res://world/world.tscn")

func _on_exit_pressed() -> void:
	get_tree().paused = false
	TransitionManager.change_scene("res://ui/gui.tscn") 

func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_cancel"):
		if (has_node("%GameOverScreen") and %GameOverScreen.visible) or (has_node("%LevelUpScreen") and %LevelUpScreen.visible) or (item_get_popup and item_get_popup.visible):
			return
		_toggle_pause()

func _toggle_pause() -> void:
	var is_paused = get_tree().paused
	get_tree().paused = !is_paused
	if has_node("%PauseOverlay"): %PauseOverlay.visible = !is_paused
	if stats_wrapper: stats_wrapper.visible = !is_paused

func _on_pause_start_pressed() -> void:
	_toggle_pause()

func _on_pause_options_pressed() -> void:
	if options_menu: options_menu.show()

func _on_pause_restart_pressed() -> void:
	get_tree().paused = false
	TransitionManager.change_scene("res://world/world.tscn")

func _on_pause_quit_pressed() -> void:
	get_tree().paused = false
	TransitionManager.change_scene("res://ui/gui.tscn")

func _setup_title_animation() -> void:
	var shadow = Sprite2D.new()
	shadow.texture = title_sprite.texture
	shadow.modulate = Color(0, 0, 0, 0.5)
	shadow.position = Vector2(1, 2)
	shadow.show_behind_parent = true
	title_sprite.add_child(shadow)

	var start_y = title_sprite.position.y
	var tween = create_tween().set_loops().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	tween.bind_node(title_sprite)
	tween.set_pause_mode(Tween.TWEEN_PAUSE_PROCESS) 
	
	tween.tween_property(title_sprite, "position:y", start_y - 10.0, 1.5)
	tween.tween_property(title_sprite, "position:y", start_y, 1.5)

func _setup_button_animations() -> void:
	var animated_buttons = [
		pause_resume_btn, pause_options_btn, pause_quit_btn, pause_restart_btn, mobile_pause_btn,
		%TryAgain if has_node("%TryAgain") else null,
		%Exit if has_node("%Exit") else null,
		%Upgrade1 if has_node("%Upgrade1") else null,
		%Upgrade2 if has_node("%Upgrade2") else null,
		%Upgrade3 if has_node("%Upgrade3") else null,
		continue_btn
	]
	
	for button in animated_buttons:
		if not button: continue
		_base_button_scales[button.name] = button.scale
		_target_button_scales[button.name] = button.scale
		
		if not button.mouse_entered.is_connected(_on_button_hover):
			button.mouse_entered.connect(_on_button_hover.bind(button))
			
		if not button.mouse_exited.is_connected(_on_button_exit):
			button.mouse_exited.connect(_on_button_exit.bind(button))
			
		if not button.pressed.is_connected(_on_button_pressed_animate):
			button.pressed.connect(_on_button_pressed_animate.bind(button))

func _on_button_hover(button: BaseButton) -> void:
	if not button: return
	button.pivot_offset = button.size / 2.0
	var tween = create_tween().set_trans(Tween.TRANS_QUINT).set_ease(Tween.EASE_OUT)
	_target_button_scales[button.name] = _base_button_scales[button.name] * 1.1
	tween.tween_property(button, "scale", _target_button_scales[button.name], 0.15)
	_play_sfx(sfx_hover)

func _on_button_exit(button: BaseButton) -> void:
	if not button: return
	button.pivot_offset = button.size / 2.0
	var tween = create_tween().set_trans(Tween.TRANS_QUINT).set_ease(Tween.EASE_OUT)
	_target_button_scales[button.name] = _base_button_scales[button.name]
	tween.tween_property(button, "scale", _target_button_scales[button.name], 0.15)

func _on_button_pressed_animate(button: BaseButton) -> void:
	if not button: return
	button.pivot_offset = button.size / 2.0
	var tween = create_tween().set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tween.tween_property(button, "scale", _base_button_scales[button.name] * 0.9, 0.05)
	tween.tween_property(button, "scale", _target_button_scales[button.name], 0.15)
	_play_sfx(sfx_click)

func _play_sfx(stream: AudioStream, start_offset: float = 0.62) -> void:
	var player = AudioStreamPlayer.new()
	player.stream = stream
	player.process_mode = Node.PROCESS_MODE_ALWAYS
	add_child(player)
	player.play(start_offset)
	player.finished.connect(player.queue_free)

func show_boss_health(max_hp: int) -> void:
	if boss_health_bar:
		boss_health_bar.max_value = max_hp
		boss_health_bar.value = max_hp
		boss_health_bar.show()

func update_boss_health(current_hp: int) -> void:
	if boss_health_bar:
		boss_health_bar.value = current_hp

func hide_boss_health() -> void:
	if boss_health_bar:
		boss_health_bar.hide()

func update_player_stats(player: Node2D) -> void:
	var stats_grid = find_child("Stats", true, false)
	
	if not stats_grid: 
		return
		
	if stats_grid is GridContainer:
		stats_grid.columns = 2
		
	for child in stats_grid.get_children():
		child.queue_free()
		
	var custom_font = load("res://ui/fonts/PixelifySans-VariableFont_wght.ttf")
	
	var add_header = func(text: String):
		var lbl1 = Label.new()
		lbl1.text = text
		lbl1.add_theme_color_override("font_color", Color("yellow"))
		if custom_font: lbl1.add_theme_font_override("font", custom_font)
		
		var lbl2 = Label.new() 
		
		stats_grid.add_child(lbl1)
		stats_grid.add_child(lbl2)

	var add_stat = func(name: String, value: String, color: Color):
		var lbl_name = Label.new()
		lbl_name.text = name
		lbl_name.add_theme_color_override("font_color", color)
		if custom_font: lbl_name.add_theme_font_override("font", custom_font)
		
		var lbl_val = Label.new()
		lbl_val.text = value
		if custom_font: lbl_val.add_theme_font_override("font", custom_font)
		
		stats_grid.add_child(lbl_name)
		stats_grid.add_child(lbl_val)

	add_header.call("--- PLAYER STATS ---")
	
	add_stat.call("Max HP:", str(int(player.max_health)), Color("lightgreen"))
	add_stat.call("Speed:", str(int(player.speed)), Color("cyan"))
	
	var total_dmg = player.base_damage_multiplier * player.damage_multiplier
	add_stat.call("Damage:", str(snapped(total_dmg * 100.0, 1.0)) + "%", Color("tomato"))
	add_stat.call("Area Size:", str(snapped(player.aoe_multiplier * 100.0, 1.0)) + "%", Color("lightgreen"))
	add_stat.call("Cooldown:", str(snapped(player.fire_rate_multiplier * 100.0, 1.0)) + "%", Color("cyan"))
	
	if player.base_crit_chance > 0:
		add_stat.call("Crit Chance:", str(snapped(player.base_crit_chance * 100.0, 1.0)) + "%", Color("magenta"))
	if player.hp_regen_rate > 0:
		add_stat.call("HP Regen:", str(snapped(player.hp_regen_rate, 0.1)) + "/s", Color("pink"))
	if player.evasion_chance > 0:
		add_stat.call("Evasion:", str(snapped(player.evasion_chance * 100.0, 1.0)) + "%", Color("lightblue"))
	if player.thorns_multiplier > 0:
		add_stat.call("Thorns:", str(snapped(player.thorns_multiplier * 100.0, 1.0)) + "%", Color("orange"))
	if player.vampirism_rate > 0:
		add_stat.call("Vampirism:", str(snapped(player.vampirism_rate * 100.0, 1.0)) + "%", Color("red"))
	if player.greed_multiplier > 0:
		add_stat.call("Greed Bonus:", "+" + str(snapped(player.greed_multiplier * 100.0, 1.0)) + "%", Color("gold"))
		
	add_header.call("--- WEAPON BUFFS ---")
	
	if player.owned_weapons.size() > 0:
		var w_id = player.owned_weapons.keys()[0]
		var w_data = player.owned_weapons[w_id]
		var display_name = Data.WEAPONS[w_id]["display_name"] if Data.WEAPONS.has(w_id) else "Weapon"
		
		add_stat.call("Equipped:", display_name + " Lv." + str(w_data["level"]), Color("orange"))
		
		if w_data["damage"] > 1.0:
			add_stat.call("Bonus Dmg:", "+" + str(snapped((w_data["damage"] - 1.0) * 100.0, 1.0)) + "%", Color("tomato"))
		if w_data["size"] > 1.0:
			add_stat.call("Bonus Size:", "+" + str(snapped((w_data["size"] - 1.0) * 100.0, 1.0)) + "%", Color("lightgreen"))
		if w_data["fire_rate"] > 1.0:
			add_stat.call("Bonus Speed:", "+" + str(snapped((w_data["fire_rate"] - 1.0) * 100.0, 1.0)) + "%", Color("cyan"))
		if w_data.get("pierce", 0) > 0:
			add_stat.call("Pierce:", "+" + str(w_data["pierce"]), Color("yellow"))
		if w_data.get("ricochet", 0) > 0:
			add_stat.call("Ricochet:", "+" + str(w_data["ricochet"]), Color("lightblue"))
		if w_data.get("projectile", 0) > 0:
			add_stat.call("Projectiles:", "+" + str(w_data["projectile"]), Color("yellow"))
	else:
		add_header.call("No Weapon Equipped")

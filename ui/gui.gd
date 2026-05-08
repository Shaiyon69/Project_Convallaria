extends Control

var scroll_speed: float = 40.0
var current_dir: Vector2 = Vector2.ZERO
var target_dir: Vector2 = Vector2.ZERO

var selected_weapon: String = "wand"

@onready var parallax_2d: Parallax2D = $Parallax2D
@onready var main_menu = $VBoxContainer
@onready var main_menu_start_button = $VBoxContainer/StartButton
@onready var options_button = $VBoxContainer/OptionsButton
@onready var quit_button = $VBoxContainer/QuitButton
@onready var options_menu = $Options
@onready var title_sprite = $VBoxContainer/Convallaria

@onready var shop_panel = %ShopPanel
@onready var upgrade_grid = %UpgradeContainer
@onready var weapon_grid = %WeaponGrid
@onready var coin_display = %CoinDisplay
@onready var close_shop_button = %BackButton
@onready var shop_start_button = %StartButton
@onready var respec_button = %RespecButton
@onready var details_label = %Details

@onready var sfx_hover = preload("res://ui/menu_hover.mp3")
@onready var sfx_click = preload("res://ui/menu_click.mp3")
@onready var shop_music = preload("res://ui/shopping.wav")

@onready var custom_font = preload("res://ui/fonts/PixelifySans-VariableFont_wght.ttf")

var _base_button_scales: Dictionary = {}
var _target_button_scales: Dictionary = {}

func _ready() -> void:
	get_tree().paused = false
	var menu_music = load(Data.MUSIC["menu"])
	AudioManager.play_music(menu_music)
	
	shop_panel.hide()

	_bind_button_animation(main_menu_start_button)
	_bind_button_animation(options_button)
	_bind_button_animation(quit_button)
	_bind_button_animation(close_shop_button)
	_bind_button_animation(shop_start_button)
	_bind_button_animation(respec_button)
	
	if not main_menu_start_button.pressed.is_connected(_on_main_menu_start_pressed):
		main_menu_start_button.pressed.connect(_on_main_menu_start_pressed)
	if not options_button.pressed.is_connected(_on_options_button_pressed):
		options_button.pressed.connect(_on_options_button_pressed)
	if not quit_button.pressed.is_connected(_on_quit_button_pressed):
		quit_button.pressed.connect(_on_quit_button_pressed)
	if not close_shop_button.pressed.is_connected(_on_shop_closed):
		close_shop_button.pressed.connect(_on_shop_closed)
	if not shop_start_button.pressed.is_connected(_on_start_run_pressed):
		shop_start_button.pressed.connect(_on_start_run_pressed)
	if not respec_button.pressed.is_connected(_on_respec_pressed):
		respec_button.pressed.connect(_on_respec_pressed)
	
	if title_sprite:
		_setup_title_animation()

	target_dir = Vector2(randf_range(-1.0, 1.0), randf_range(-1.0, 1.0)).normalized()
	current_dir = target_dir
	
	var drift_timer := Timer.new()
	drift_timer.wait_time = 4.0
	drift_timer.autostart = true
	drift_timer.timeout.connect(_pick_new_direction)
	add_child(drift_timer)

func _process(delta: float) -> void:
	current_dir = current_dir.lerp(target_dir, delta * 0.5)
	var step = current_dir * scroll_speed * delta
	parallax_2d.scroll_offset.x = wrapf(parallax_2d.scroll_offset.x + step.x, 0.0, parallax_2d.repeat_size.x)
	parallax_2d.scroll_offset.y = wrapf(parallax_2d.scroll_offset.y + step.y, 0.0, parallax_2d.repeat_size.y)

func _pick_new_direction() -> void:
	target_dir = Vector2(randf_range(-1.0, 1.0), randf_range(-1.0, 1.0)).normalized()

func _on_main_menu_start_pressed() -> void:
	main_menu.hide()
	shop_panel.show()
	details_label.text = "Prepare for your journey."
	
	AudioManager.play_music(shop_music)
	
	if Data.starting_weapon != "":
		selected_weapon = Data.starting_weapon
		
	_refresh_shop_ui()

func _on_options_button_pressed() -> void:
	options_menu.show()

func _on_quit_button_pressed() -> void:
	get_tree().quit()

func _on_shop_closed() -> void:
	shop_panel.hide()
	main_menu.show()
	AudioManager.play_music(load(Data.MUSIC["menu"]))

func _on_start_run_pressed() -> void:
	Data.starting_weapon = selected_weapon
	TransitionManager.change_scene("res://world/world.tscn")

func _on_respec_pressed() -> void:
	var total_refund = 0
	
	for upgrade_id in Data.permanent_upgrades:
		var upg = Data.permanent_upgrades[upgrade_id]
		var current_lvl = upg["level"]
		
		for i in range(current_lvl):
			total_refund += int(upg["base_cost"] * pow(upg["cost_mult"], i))
			
		upg["level"] = 0 
		
	Data.coins += total_refund
	_play_sfx(sfx_click) 
	details_label.text = "Upgrades Reset! Refunded " + str(total_refund) + " Coins."
	_refresh_shop_ui()

func _refresh_shop_ui() -> void:
	coin_display.text = "Total Coins: " + str(Data.coins)
	
	for child in upgrade_grid.get_children():
		child.queue_free()
		
	for upgrade_id in Data.permanent_upgrades:
		var upg_data = Data.permanent_upgrades[upgrade_id]
		var cost = Data.get_upgrade_cost(upgrade_id)
		
		var btn = Button.new()
		btn.custom_minimum_size = Vector2(250, 50)
		
		if custom_font:
			btn.add_theme_font_override("font", custom_font)
			
		_bind_button_animation(btn)
		
		btn.mouse_entered.connect(_show_upgrade_details.bind(upg_data, cost))
		
		if cost == -1:
			btn.text = upg_data["name"] + " (MAX)"
			btn.disabled = true
		else:
			btn.text = "%s (Lv %d) : %d Coins" % [upg_data["name"], upg_data["level"], cost]
			if Data.coins < cost:
				btn.disabled = true 
			else:
				btn.pressed.connect(_on_buy_pressed.bind(upgrade_id))
				
		upgrade_grid.add_child(btn)

	for child in weapon_grid.get_children():
		child.queue_free()
		
	for weapon_id in Data.WEAPONS:
		var w_data = Data.WEAPONS[weapon_id]
		var btn = Button.new()
		btn.custom_minimum_size = Vector2(200, 50)
		
		if custom_font:
			btn.add_theme_font_override("font", custom_font)
			
		_bind_button_animation(btn)
		
		btn.mouse_entered.connect(_show_weapon_details.bind(w_data))
		
		if weapon_id == selected_weapon:
			btn.text = "[ " + w_data["display_name"] + " ]"
			btn.modulate = Color(0.5, 1.0, 0.5) 
		else:
			btn.text = w_data["display_name"]
			btn.pressed.connect(_on_weapon_selected.bind(weapon_id))
			
		weapon_grid.add_child(btn)

func _show_upgrade_details(upg_data: Dictionary, cost: int) -> void:
	if cost == -1:
		details_label.text = "Maximum level reached for " + upg_data["name"] + "."
	else:
		var current_boost = upg_data["level"] * upg_data["boost_per_level"]
		var next_boost = (upg_data["level"] + 1) * upg_data["boost_per_level"]
		details_label.text = "Increases " + upg_data["name"] + ". Current: +" + str(current_boost) + " -> Next: +" + str(next_boost)

func _show_weapon_details(w_data: Dictionary) -> void:
	details_label.text = "Start your run equipped with the " + w_data["display_name"] + "."

func _on_buy_pressed(upgrade_id: String) -> void:
	if Data.buy_upgrade(upgrade_id):
		details_label.text = "Upgrade Purchased!"
		_refresh_shop_ui() 

func _on_weapon_selected(weapon_id: String) -> void:
	selected_weapon = weapon_id
	_refresh_shop_ui()

func _setup_title_animation() -> void:
	var shadow = Sprite2D.new()
	shadow.texture = title_sprite.texture
	shadow.modulate = Color(0, 0, 0, 0.5)
	shadow.position = Vector2(1, 2)
	shadow.show_behind_parent = true
	title_sprite.add_child(shadow)
	
	var start_y = title_sprite.position.y
	var tween = create_tween().set_loops().set_trans(Tween.TRANS_SINE).set_ease(Tween.EASE_IN_OUT)
	tween.tween_property(title_sprite, "position:y", start_y - 10.0, 1.5)
	tween.tween_property(title_sprite, "position:y", start_y, 1.5)

func _bind_button_animation(button: BaseButton) -> void:
	_base_button_scales[button] = button.scale
	_target_button_scales[button] = button.scale
	
	if not button.mouse_entered.is_connected(_on_button_hover):
		button.mouse_entered.connect(_on_button_hover.bind(button))
		
	if not button.mouse_exited.is_connected(_on_button_exit):
		button.mouse_exited.connect(_on_button_exit.bind(button))
		
	if not button.pressed.is_connected(_on_button_pressed_animate):
		button.pressed.connect(_on_button_pressed_animate.bind(button))

func _on_button_hover(button: BaseButton) -> void:
	button.pivot_offset = button.size / 2.0
	var tween = create_tween().set_trans(Tween.TRANS_QUINT).set_ease(Tween.EASE_OUT)
	_target_button_scales[button] = _base_button_scales[button] * 1.1
	tween.tween_property(button, "scale", _target_button_scales[button], 0.15)
	_play_sfx(sfx_hover)

func _on_button_exit(button: BaseButton) -> void:
	button.pivot_offset = button.size / 2.0
	var tween = create_tween().set_trans(Tween.TRANS_QUINT).set_ease(Tween.EASE_OUT)
	_target_button_scales[button] = _base_button_scales[button]
	tween.tween_property(button, "scale", _target_button_scales[button], 0.15)

func _on_button_pressed_animate(button: BaseButton) -> void:
	button.pivot_offset = button.size / 2.0
	var tween = create_tween().set_trans(Tween.TRANS_BACK).set_ease(Tween.EASE_OUT)
	tween.tween_property(button, "scale", _base_button_scales[button] * 0.9, 0.05)
	tween.tween_property(button, "scale", _target_button_scales[button], 0.15)
	_play_sfx(sfx_click)

func _play_sfx(stream: AudioStream, start_offset: float = 0.62) -> void:
	var player = AudioStreamPlayer.new()
	player.stream = stream
	add_child(player)
	player.play(start_offset)
	player.finished.connect(player.queue_free)

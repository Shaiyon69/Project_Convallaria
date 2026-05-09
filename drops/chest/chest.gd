extends Area2D

var is_opened: bool = false
var cost: int = 10 

@onready var sprite = $Sprite2D

func _process(_delta: float) -> void:
	if not is_opened and has_overlapping_bodies() and Input.is_action_just_pressed("ui_accept"):
		for body in get_overlapping_bodies():
			if body.is_in_group("player"):
				_try_open(body)

func _try_open(player: Node2D) -> void:
	if Data.silver >= cost:
		Data.silver -= cost
		
		if player.get("hud") and player.hud.has_method("update_coins"):
			player.hud.update_coins(Data.coins, Data.silver)
			
		is_opened = true
		if sprite:
			sprite.modulate = Color(0.4, 0.4, 0.4) 
		
		var rolled_item = _roll_item_drop()
		if player.has_method("add_relic_item"):
			player.add_relic_item(rolled_item)

func _roll_item_drop() -> String:
	var roll = randi() % 100
	var selected_rarity = "white"
	var cumulative = 0
	
	for r_key in Data.RARITY:
		cumulative += Data.RARITY[r_key]["weight"]
		if roll < cumulative:
			selected_rarity = r_key
			break
			
	var valid_items = []
	for item_id in Data.ITEMS:
		if Data.ITEMS[item_id]["rarity"] == selected_rarity:
			valid_items.append(item_id)
			
	if valid_items.size() > 0:
		valid_items.shuffle()
		return valid_items[0]
		
	return Data.ITEMS.keys()[0]

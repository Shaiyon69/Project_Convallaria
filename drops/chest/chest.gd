extends Area2D

var is_opened: bool = false

func _ready() -> void:
	body_entered.connect(_on_body_entered)

func _on_body_entered(body: Node2D) -> void:
	if is_opened:
		return
		
	if body.name == "Player" and body.has_method("add_relic_item"):
		is_opened = true
		_open_chest(body)

func _open_chest(player_node: Node2D) -> void:
	var rolled_item = _roll_item_drop()
	player_node.add_relic_item(rolled_item)
	
	queue_free()

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

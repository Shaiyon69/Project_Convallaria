extends Node2D

signal weapons_updated(weapon_textures: Array)

var active_weapons: Array = []
var max_weapon_slots: int = 3

func add_weapon(weapon_scene_path: String) -> void:
	if active_weapons.size() >= max_weapon_slots:
		return
		
	var weapon_scene = load(weapon_scene_path)
	var weapon_instance = weapon_scene.instantiate()
	
	add_child(weapon_instance)
	active_weapons.append(weapon_instance)
	_emit_update()

func replace_weapon(slot_index: int, weapon_scene_path: String) -> void:
	if slot_index < 0 or slot_index >= active_weapons.size():
		return
		
	active_weapons[slot_index].queue_free()
	
	var weapon_scene = load(weapon_scene_path)
	var new_weapon = weapon_scene.instantiate()
	
	add_child(new_weapon)
	active_weapons[slot_index] = new_weapon
	_emit_update()

func _emit_update() -> void:
	var textures: Array = []
	for weapon in active_weapons:
		if "weapon_icon" in weapon and weapon.weapon_icon != null:
			textures.append(weapon.weapon_icon)
			
	weapons_updated.emit(textures)

func get_weapon_count() -> int:
	return active_weapons.size()

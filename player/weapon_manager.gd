extends Node2D

signal weapons_updated(weapon_textures: Array)

var active_weapons: Array = []
var active_weapon_ids: Array[String] = []
var max_weapon_slots: int = 3

func add_weapon(weapon_scene_path: String, weapon_id: String = "") -> void:
	if active_weapons.size() >= max_weapon_slots:
		return
		
	var weapon_scene = load(weapon_scene_path)
	if not weapon_scene is PackedScene:
		push_warning("[DataEffects] Could not load weapon scene '%s'" % weapon_scene_path)
		return
	var weapon_instance = weapon_scene.instantiate()
	_assign_weapon_id(weapon_instance, weapon_id)
	
	add_child(weapon_instance)
	active_weapons.append(weapon_instance)
	active_weapon_ids.append(weapon_id)
	_emit_update()

func replace_weapon(slot_index: int, weapon_scene_path: String, weapon_id: String = "") -> void:
	if slot_index < 0 or slot_index >= active_weapons.size():
		return
		
	active_weapons[slot_index].queue_free()
	
	var weapon_scene = load(weapon_scene_path)
	if not weapon_scene is PackedScene:
		push_warning("[DataEffects] Could not load weapon scene '%s'" % weapon_scene_path)
		return
	var new_weapon = weapon_scene.instantiate()
	_assign_weapon_id(new_weapon, weapon_id)
	
	add_child(new_weapon)
	active_weapons[slot_index] = new_weapon
	active_weapon_ids[slot_index] = weapon_id
	_emit_update()

func update_weapon_stats(weapon_id: String, stats: Dictionary) -> void:
	for i in range(active_weapons.size()):
		if active_weapon_ids[i] != weapon_id:
			continue
		var weapon = active_weapons[i]
		if weapon.has_method("apply_weapon_stats"):
			weapon.apply_weapon_stats(stats)

func _assign_weapon_id(weapon: Node, weapon_id: String) -> void:
	if weapon_id == "":
		return
	if "weapon_id" in weapon:
		weapon.weapon_id = weapon_id

func _emit_update() -> void:
	var textures: Array = []
	for weapon in active_weapons:
		if "weapon_icon" in weapon and weapon.weapon_icon != null:
			textures.append(weapon.weapon_icon)
			
	weapons_updated.emit(textures)

func get_weapon_count() -> int:
	return active_weapons.size()

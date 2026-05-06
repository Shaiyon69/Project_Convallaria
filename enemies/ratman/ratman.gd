extends CharacterBody2D

@export var max_health: int = 20
@export var movement_speed: float = 100.0
@export var attack_damage: int = 10
@export var is_shooter: bool = false
@export var exp_value: int = 1

@export var seed_scene: PackedScene
@export var damage_scene: PackedScene
@export var projectile_scene: PackedScene

var current_health: int
var is_dying: bool = false

@onready var anim = $AnimatedSprite2D
@onready var player = get_tree().get_first_node_in_group("player")

func _ready() -> void:
	current_health = max_health
	add_to_group("enemy")

func _physics_process(_delta: float) -> void:
	if is_dying or not player:
		return
		
	var direction = global_position.direction_to(player.global_position)
	velocity = direction * movement_speed
	
	_update_animation(direction)
	move_and_slide()

func _update_animation(dir: Vector2) -> void:
	if abs(dir.x) > abs(dir.y):
		if dir.x > 0:
			anim.play("walk_right")
		else:
			anim.play("walk_left")
	else:
		if dir.y > 0:
			anim.play("walk_down")
		else:
			anim.play("walk_up")

func take_damage(amount: int) -> void:
	if is_dying:
		return
		
	current_health -= amount
	
	if damage_scene:
		var dmg_num = damage_scene.instantiate()
		dmg_num.global_position = global_position
		dmg_num.set_value(amount)
		get_tree().current_scene.add_child(dmg_num)
		
	if current_health <= 0:
		die()

func die() -> void:
	is_dying = true
	collision_layer = 0
	collision_mask = 0
	anim.play("death")
	
	if seed_scene:
		var seed_inst = seed_scene.instantiate()
		seed_inst.global_position = global_position
		seed_inst.exp_amount = exp_value
		get_tree().current_scene.call_deferred("add_child", seed_inst)
		
	if player and player.has_method("add_kill"):
		player.add_kill()
		
	await anim.animation_finished
	queue_free()

func apply_stats(stats: Dictionary) -> void:
	if stats.has("health"):
		max_health = stats["health"]
		current_health = max_health
	if stats.has("speed"):
		movement_speed = stats["speed"]
	if stats.has("damage"):
		attack_damage = stats["damage"]
	if stats.has("exp"):
		exp_value = stats["exp"]
	if stats.has("is_shooter"):
		is_shooter = stats["is_shooter"]
	if stats.has("scale"):
		scale = Vector2(stats["scale"], stats["scale"])
	if stats.has("color"):
		anim.modulate = stats["color"]

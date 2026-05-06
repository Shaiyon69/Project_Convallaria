extends Area2D

var speed: float = 400.0
var damage: int = 20
var direction: Vector2 = Vector2.ZERO
var explosion_radius: float = 35.0

# --- NEW: Custom Buff Stats ---
var size_multiplier: float = 1.0
var pierce_count: int = 0
var ricochet_count: int = 0
var hit_enemies: Array = [] # Tracks who we've hit so we don't multi-hit the same enemy instantly

var imbue_fire: bool = false
var imbue_frost: bool = false

var player_ref: Node2D = null

func _ready() -> void:
	body_entered.connect(_on_body_entered)
	if has_node("VisibleOnScreenNotifier2D"):
		$VisibleOnScreenNotifier2D.screen_exited.connect(queue_free)

func _physics_process(delta: float) -> void:
	global_position += direction * speed * delta
	rotation = direction.angle()

func _on_body_entered(body: Node2D) -> void:
	# Prevent the projectile from triggering on the exact same enemy multiple times
	if hit_enemies.has(body):
		return
		
	if body.is_in_group("enemy"):
		hit_enemies.append(body)
		_trigger_explosion()
		
		# --- NEW: Check for Pierce and Ricochet ---
		if pierce_count > 0:
			pierce_count -= 1
		elif ricochet_count > 0:
			ricochet_count -= 1
			_bounce_to_next_target(body)
		else:
			queue_free() # Out of buffs, destroy it!
			
	# Destroy on walls/obstacles, ignoring the player/exp drops
	elif not body.is_in_group("enemy") and not body.is_in_group("exp_seed"):
		queue_free()

func _bounce_to_next_target(exclude_target: Node2D) -> void:
	var enemies = get_tree().get_nodes_in_group("enemy")
	var nearest = null
	var min_dist = 400.0 * size_multiplier # Bounce range
	
	for enemy in enemies:
		if enemy == exclude_target or enemy.get("is_dying") == true or hit_enemies.has(enemy):
			continue
			
		var dist = global_position.distance_to(enemy.global_position)
		if dist < min_dist:
			min_dist = dist
			nearest = enemy
			
	# If we found a target, redirect the projectile! Otherwise, destroy it.
	if nearest:
		direction = global_position.direction_to(nearest.global_position)
		rotation = direction.angle()
	else:
		queue_free()

func _trigger_explosion() -> void:
	var space_state = get_world_2d().direct_space_state
	var query = PhysicsShapeQueryParameters2D.new()
	
	var circle_shape = CircleShape2D.new()
	# Multiply the explosion radius by the specific size buff!
	circle_shape.radius = explosion_radius * size_multiplier 
	
	query.shape = circle_shape
	query.transform = Transform2D(0, global_position)
	
	var results = space_state.intersect_shape(query)
	
	for result in results:
		var target = result.collider
		
		if target.is_in_group("enemy") and target.has_method("take_damage"):
			var was_full_hp = false
			if "health" in target and "max_health" in target:
				was_full_hp = (target.health >= target.max_health)

			target.take_damage(damage)

			if was_full_hp and "health" in target and target.health <= 0:
				if player_ref and "base_crit_chance" in player_ref:
					player_ref.base_crit_chance += 0.001
					
			if imbue_fire and target.has_method("apply_burn"):
				target.apply_burn(damage * 0.2)
			if imbue_frost and target.has_method("apply_slow"):
				target.apply_slow(0.5)

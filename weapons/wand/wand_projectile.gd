extends Area2D

var speed: float = 400.0
var damage: int = 20
var direction: Vector2 = Vector2.ZERO
var explosion_radius: float = 35.0

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
	# If it hits an enemy, trigger the AoE and destroy the projectile
	if body.is_in_group("enemy"):
		_trigger_explosion()
		queue_free()
	# Destroy on walls/obstacles, ignoring the player/exp drops
	elif not body.is_in_group("enemy") and not body.is_in_group("exp_seed"):
		queue_free()

func _trigger_explosion() -> void:
	var space_state = get_world_2d().direct_space_state
	var query = PhysicsShapeQueryParameters2D.new()
	
	var circle_shape = CircleShape2D.new()
	circle_shape.radius = explosion_radius
	
	query.shape = circle_shape
	query.transform = Transform2D(0, global_position)
	# query.max_results = 32 # Uncomment and increase if you expect more than 32 enemies in one blast
	
	var results = space_state.intersect_shape(query)
	
	for result in results:
		var target = result.collider
		
		# Apply your exact logic to every valid target caught in the radius
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

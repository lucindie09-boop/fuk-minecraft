extends CharacterBody3D

# These are temporary values used for testing, this player system will be gone at some point and replaced with a c++ alternative.
const SPEED = 100.0
const JUMP = 40.0
const GRAVITY = 20.0
const SENSITIVITY = 0.003

var camera: Camera3D
var pitch: float = 0.0
var chunk_manager: Node3D
var block_info_label: Label
var on_floor: bool = false

# Block placing/breaking
var selected_block_type = ChunkManager.BLOCK_GRASS  # Default to grass block

func _ready():
	camera = get_node("Camera3D")
	camera.position = Vector3(0, 1.6, 0)
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	
	# Get reference to ChunkManager node
	chunk_manager = get_node_or_null("/root/Main/ChunkManager")
	
	# Create HUD info label (shows facing direction + selected block)
	block_info_label = Label.new()
	block_info_label.position = Vector2(10, 10)
	block_info_label.add_theme_font_size_override("font_size", 24)
	var hud = get_node_or_null("/root/Main/HUD")
	if hud:
		hud.add_child(block_info_label)
	else:
		add_child(block_info_label)  # fallback
	update_direction_label()

func _input(event):
	if event is InputEventMouseMotion:
		rotate_y(-event.relative.x * SENSITIVITY)
		pitch -= event.relative.y * SENSITIVITY
		pitch = clamp(pitch, -1.4, 1.4)
		camera.rotation.x = pitch
	if event.is_action_pressed("ui_cancel"):
		Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	
	# Block breaking (left click)
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
		if Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
			break_block()
	
	# Block placing (right click)
	if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
		if Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED:
			place_block()
	
	# Block selection with number keys
	if event is InputEventKey and event.pressed:
		match event.keycode:
			KEY_1: selected_block_type = ChunkManager.BLOCK_STONE
			KEY_2: selected_block_type = ChunkManager.BLOCK_DIRT
			KEY_3: selected_block_type = ChunkManager.BLOCK_GRASS
			KEY_4: selected_block_type = ChunkManager.BLOCK_SAND
			KEY_5: selected_block_type = ChunkManager.BLOCK_WOOD
			KEY_6: selected_block_type = ChunkManager.BLOCK_LEAVES
			KEY_7: selected_block_type = ChunkManager.BLOCK_BEDROCK
			KEY_8: selected_block_type = ChunkManager.BLOCK_LIGHT_BLOCK
			KEY_9: selected_block_type = ChunkManager.BLOCK_LIGHT_RED
			KEY_0: selected_block_type = ChunkManager.BLOCK_LIGHT_GREEN
			KEY_MINUS: selected_block_type = ChunkManager.BLOCK_LIGHT_BLUE
		if event.keycode in [KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS]:
			update_direction_label()

func _physics_process(delta):
	if not on_floor:
		velocity.y -= GRAVITY * delta
	var input_dir = Vector3.ZERO
	if Input.is_action_pressed("move_forward"): input_dir -= transform.basis.z
	if Input.is_action_pressed("move_back"): input_dir += transform.basis.z
	if Input.is_action_pressed("move_left"): input_dir -= transform.basis.x
	if Input.is_action_pressed("move_right"): input_dir += transform.basis.x
	input_dir.y = 0
	input_dir = input_dir.normalized()
	velocity.x = input_dir.x * SPEED
	velocity.z = input_dir.z * SPEED
	if Input.is_action_pressed("jump") and on_floor:
		velocity.y = JUMP

	# Custom voxel collision — queries the chunk map directly instead of Godot physics nodes
	var motion = velocity * delta
	if chunk_manager:
		var collision_result = chunk_manager.resolve_voxel_collision(global_position, motion, Vector3(1, 2, 1))
		global_position = collision_result.position
		if collision_result.collided_x:
			velocity.x = 0
		if collision_result.collided_y:
			velocity.y = 0
		if collision_result.collided_z:
			velocity.z = 0
		on_floor = collision_result.on_floor
	else:
		global_position += motion

func update_direction_label():
	var forward = -transform.basis.z  # Player's forward direction
	var direction = ""
	
	# Determine cardinal direction based on forward vector
	if abs(forward.x) > abs(forward.z):
		if forward.x > 0:
			direction = "East (+X)"
		else:
			direction = "West (-X)"
	else:
		if forward.z > 0:
			direction = "South (+Z)"
		else:
			direction = "North (-Z)"
	
	var block_name = "Unknown"
	if chunk_manager:
		block_name = chunk_manager.get_block_name(selected_block_type)
	block_info_label.text = "Facing: " + direction + "\nBlock: " + block_name

func break_block():
	if not chunk_manager:
		return
	
	var raycast_result = chunk_manager.raycast_from_camera(10.0)
	if raycast_result.get("success", false):
		var pos = raycast_result["position"]
		chunk_manager.set_block(pos.x, pos.y, pos.z, ChunkManager.BLOCK_AIR)

func place_block():
	if not chunk_manager:
		return
	
	var raycast_result = chunk_manager.raycast_from_camera(10.0)
	if raycast_result.get("success", false):
		var place_pos = raycast_result["place_position"]
		var block_x = int(floor(place_pos.x))
		var block_y = int(floor(place_pos.y))
		var block_z = int(floor(place_pos.z))
		
		# Don't place block where player is standing (player is 2 blocks tall)
		var player_block_x = int(floor(global_position.x))
		var player_block_y = int(floor(global_position.y))
		var player_block_z = int(floor(global_position.z))
		
		# Check if the block would be inside the player's bounding box
		# Player occupies 2 vertical blocks (feet at player_y, head at player_y + 1)
		if (block_x == player_block_x and block_z == player_block_z and 
			(block_y == player_block_y or block_y == player_block_y + 1)):
			return
		
		chunk_manager.set_block(block_x, block_y, block_z, selected_block_type)

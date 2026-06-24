extends CharacterBody3D

# These are temporary values used for testing, this player system will be gone at some point and replaced with a c++ alternative.
const SPEED = 100.0
const JUMP = 40.0
const GRAVITY = 20.0
const SENSITIVITY = 0.003
const MANUAL_TIME_STEP = 0.125

var camera: Camera3D
var pitch: float = 0.0
var chunk_manager: Node3D
var block_info_label: Label
var crosshair: Control
var on_floor: bool = false
var flight_enabled: bool = false

# Block placing/breaking
var selected_block_type = ChunkManager.BLOCK_GRASS  # Default to grass block

func _ready():
	camera = get_node("Camera3D")
	camera.position = Vector3(0, 1.6, 0)
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)

	# Get reference to ChunkManager node
	chunk_manager = get_node_or_null("/root/Main/ChunkManager")

	# Set up descend action for flight mode
	if not InputMap.has_action("descend"):
		InputMap.add_action("descend")
		var left = InputEventKey.new()
		left.physical_keycode = KEY_SHIFT
		InputMap.action_add_event("descend", left)

	# Set up jump action if not present (Space by default)
	if not InputMap.has_action("jump"):
		InputMap.add_action("jump")
		var jump = InputEventKey.new()
		jump.physical_keycode = KEY_SPACE
		InputMap.action_add_event("jump", jump)

	# Create HUD info label and crosshair
	var hud = get_node_or_null("/root/main/HUD")
	var ui_parent: Node = hud if hud else get_tree().current_scene

	block_info_label = Label.new()
	block_info_label.position = Vector2(10, 10)
	block_info_label.add_theme_font_size_override("font_size", 24)
	ui_parent.add_child.call_deferred(block_info_label)

	crosshair = Control.new()
	crosshair.anchor_left = 0.5
	crosshair.anchor_top = 0.5
	crosshair.anchor_right = 0.5
	crosshair.anchor_bottom = 0.5
	crosshair.offset_left = -8.0
	crosshair.offset_top = -8.0
	crosshair.offset_right = 8.0
	crosshair.offset_bottom = 8.0
	crosshair.mouse_filter = Control.MOUSE_FILTER_IGNORE
	ui_parent.add_child.call_deferred(crosshair)

	var horizontal = ColorRect.new()
	horizontal.position = Vector2(0, 7)
	horizontal.size = Vector2(16, 2)
	horizontal.color = Color(1, 1, 1, 0.95)
	crosshair.add_child.call_deferred(horizontal)

	var vertical = ColorRect.new()
	vertical.position = Vector2(7, 0)
	vertical.size = Vector2(2, 16)
	vertical.color = Color(1, 1, 1, 0.95)
	crosshair.add_child.call_deferred(vertical)

	update_direction_label()

func _input(event):
	if event is InputEventMouseMotion:
		rotate_y(-event.relative.x * SENSITIVITY)
		pitch -= event.relative.y * SENSITIVITY
		pitch = clamp(pitch, -1.4, 1.4)
		camera.rotation.x = pitch
	update_direction_label()
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
	if event is InputEventKey and event.pressed and not event.echo:
		var kc = event.keycode
		var pk = event.physical_keycode
		if kc == KEY_F or pk == KEY_F:
			flight_enabled = not flight_enabled
			on_floor = false
			velocity = Vector3.ZERO
		elif kc == KEY_T or pk == KEY_T:
			if chunk_manager:
				chunk_manager.toggle_day_night_cycle()
		elif kc == KEY_G or pk == KEY_G:
			if chunk_manager:
				var next_time = wrapf(chunk_manager.get_time() + MANUAL_TIME_STEP, 0.0, 1.0)
				chunk_manager.set_time(next_time)
		elif kc == KEY_1 or pk == KEY_1:
			selected_block_type = ChunkManager.BLOCK_STONE
		elif kc == KEY_2 or pk == KEY_2:
			selected_block_type = ChunkManager.BLOCK_DIRT
		elif kc == KEY_3 or pk == KEY_3:
			selected_block_type = ChunkManager.BLOCK_GRASS
		elif kc == KEY_4 or pk == KEY_4:
			selected_block_type = ChunkManager.BLOCK_SAND
		elif kc == KEY_5 or pk == KEY_5:
			selected_block_type = ChunkManager.BLOCK_WOOD
		elif kc == KEY_6 or pk == KEY_6:
			selected_block_type = ChunkManager.BLOCK_LEAVES
		elif kc == KEY_7 or pk == KEY_7:
			selected_block_type = ChunkManager.BLOCK_BEDROCK
		elif kc == KEY_8 or pk == KEY_8:
			selected_block_type = ChunkManager.BLOCK_LIGHT_BLOCK
		elif kc == KEY_9 or pk == KEY_9:
			selected_block_type = ChunkManager.BLOCK_LIGHT_RED
		elif kc == KEY_0 or pk == KEY_0:
			selected_block_type = ChunkManager.BLOCK_LIGHT_GREEN
		elif kc == KEY_MINUS or pk == KEY_MINUS:
			selected_block_type = ChunkManager.BLOCK_LIGHT_BLUE

	update_direction_label()

func _physics_process(delta):
	var input_dir = Vector3.ZERO
	if Input.is_action_pressed("move_forward"): input_dir -= transform.basis.z
	if Input.is_action_pressed("move_back"): input_dir += transform.basis.z
	if Input.is_action_pressed("move_left"): input_dir -= transform.basis.x
	if Input.is_action_pressed("move_right"): input_dir += transform.basis.x
	input_dir.y = 0

	if flight_enabled:
		if Input.is_action_pressed("jump"):
			input_dir += Vector3.UP
		if Input.is_action_pressed("descend"):
			input_dir += Vector3.DOWN
		input_dir = input_dir.normalized()
		velocity = input_dir * SPEED
		global_position += velocity * delta
		on_floor = false
		return

	input_dir = input_dir.normalized()
	velocity.x = input_dir.x * SPEED
	velocity.z = input_dir.z * SPEED
	if Input.is_action_pressed("jump") and on_floor:
		velocity.y = JUMP

	# Apply gravity when not on floor
	velocity.y -= GRAVITY * delta

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
	var cycle_status = "Unknown"
	var time_text = "--:--"
	if chunk_manager:
		block_name = chunk_manager.get_block_name(selected_block_type)
		cycle_status = "running" if chunk_manager.get_day_night_cycle_enabled() else "Paused"
		time_text = format_time_of_day(chunk_manager.get_time())
	var movement_mode = "Flight" if flight_enabled else "Walk"
	block_info_label.text = "Facing: " + direction + "\nBlock: " + block_name + " \nMode: " + movement_mode + "\nCycle: " + cycle_status + " (" + time_text + ")"

func format_time_of_day(time_value: float) -> String:
	var wrapped_time = wrapf(time_value, 0.0, 1.0)
	var total_minutes = int(floor(wrapped_time * 24.0 * 60.0))
	var hours = int(total_minutes / 60.0) % 24
	var minutes = total_minutes % 60
	return "%02d:%02d" % [hours, minutes]

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

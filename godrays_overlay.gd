extends ColorRect

# Drives shaders/godrays.gdshader. Projects the sun's world-space direction
# (read from ChunkManager/DayNightCycle) into screen space every frame and
# fades the effect out smoothly when the sun swings behind the camera or
# below the horizon, so the radial-scatter shader never gets a degenerate
# "sun point" to smear from.

const SUN_DISTANCE := 5000.0
const FRONT_FADE_START := -0.5   # dot(cam_forward, sun_dir) below this = fully hidden
const FRONT_FADE_END := 0.25     # dot(cam_forward, sun_dir) above this = fully visible
const HORIZON_FADE := 0.10       # sun_dir.y below this starts fading rays out

var camera: Camera3D
var chunk_manager: Node

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_preset(Control.PRESET_FULL_RECT)

	if material == null:
		var shader_material := ShaderMaterial.new()
		shader_material.shader = load("res://shaders/godrays.gdshader")
		material = shader_material

func _process(_delta: float) -> void:
	if camera == null:
		camera = get_viewport().get_camera_3d()
	if chunk_manager == null:
		chunk_manager = get_node_or_null("/root/Main/ChunkManager")
	if camera == null or chunk_manager == null or material == null:
		return

	var sun_dir: Vector3 = chunk_manager.get_sun_direction()
	if sun_dir.length_squared() < 0.0001:
		return
	sun_dir = sun_dir.normalized()

	var cam_forward: Vector3 = -camera.global_transform.basis.z
	var viewport_size: Vector2 = get_viewport_rect().size
	if viewport_size.x <= 0.0 or viewport_size.y <= 0.0:
		return

	var shader_material := material as ShaderMaterial

	# --- Sun ---
	var sun_facing: float = cam_forward.dot(sun_dir)
	var sun_facing_fade: float = smoothstep(FRONT_FADE_START, FRONT_FADE_END, sun_facing)
	var sun_horizon_fade: float = smoothstep(-HORIZON_FADE, HORIZON_FADE, sun_dir.y)
	var sun_visibility: float = sun_facing_fade * sun_horizon_fade
	if sun_visibility > 0.0:
		var sun_world_pos: Vector3 = camera.global_position + sun_dir * SUN_DISTANCE
		var screen_pos: Vector2 = camera.unproject_position(sun_world_pos)
		var sun_uv: Vector2 = screen_pos / viewport_size
		shader_material.set_shader_parameter("sun_screen_uv", sun_uv)
		shader_material.set_shader_parameter("sun_visibility", sun_visibility)
	else:
		shader_material.set_shader_parameter("sun_visibility", 0.0)

	# --- Moon (opposite of sun) ---
	var moon_dir: Vector3 = -sun_dir
	var moon_facing: float = cam_forward.dot(moon_dir)
	var moon_facing_fade: float = smoothstep(FRONT_FADE_START, FRONT_FADE_END, moon_facing)
	var moon_horizon_fade: float = smoothstep(-HORIZON_FADE, HORIZON_FADE, moon_dir.y)
	var moon_visibility: float = moon_facing_fade * moon_horizon_fade
	if moon_visibility > 0.0:
		var moon_world_pos: Vector3 = camera.global_position + moon_dir * SUN_DISTANCE
		var screen_pos: Vector2 = camera.unproject_position(moon_world_pos)
		var moon_uv: Vector2 = screen_pos / viewport_size
		shader_material.set_shader_parameter("moon_screen_uv", moon_uv)
		shader_material.set_shader_parameter("moon_visibility", moon_visibility)
	else:
		shader_material.set_shader_parameter("moon_visibility", 0.0)

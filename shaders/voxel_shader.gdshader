shader_type spatial;
render_mode unshaded, cull_back, depth_draw_always;

uniform sampler2DArray texture_array : filter_nearest_mipmap_anisotropic;

// Controls how close to the camera mipmapping kicks in.
uniform float mipmap_bias : hint_range(-4.0, 4.0) = 1;

// Day/night cycle intensity (0.0 = midnight, 1.0 = noon)
uniform float sky_light_intensity : hint_range(0.0, 1.0) = 1.0;
uniform vec3 sky_light_color = vec3(1.0, 1.0, 1.0);

void fragment() {
	// Sample texture from array using UV for coordinates and UV2.x for layer
	vec3 tex_coord = vec3(UV, UV2.x);

	// FIXED: GDSL uses the standard texture() function, passing mipmap_bias as the 3rd argument
	vec4 color = texture(texture_array, tex_coord, mipmap_bias);

	// UV2.y carries per-vertex AO brightness multiplied by face shade (pre-computed on CPU).
	float ao = UV2.y;

	// COLOR.rgb = pre-computed block light brightness (CPU: light_level_to_brightness), per channel.
	vec3 block_brightness = COLOR.rgb;
	// COLOR.a = pre-computed sky light brightness (CPU: light_level_to_brightness, no sky_light_intensity).
	float sky_brightness = COLOR.a * sky_light_intensity;
	vec3 sky_contribution = sky_light_color * sky_brightness;

	// Per-channel max: colored block lights mix naturally, sky fills in white daylight
	vec3 combined_light = max(block_brightness, sky_contribution);

	// Apply lighting: combined_light * ao (ao includes pre-multiplied face shade)
	ALBEDO = color.rgb * combined_light * ao;
	ALPHA = 1.0;
}

shader_type spatial;
render_mode unshaded, cull_back, depth_draw_always;

uniform sampler2DArray texture_array : filter_nearest_mipmap;

uniform float mipmap_bias = 1.0;

// Lighting
uniform float sky_light_intensity : hint_range(0.0,1.0)=1.0;
uniform vec3 sky_light_color=vec3(1.0);
uniform vec3 sun_direction=vec3(0.0,1.0,0.0);

// Player light
uniform vec3 player_light_position=vec3(0.0);
uniform float player_light_radius=8.0;
uniform float player_light_intensity=1.0;
uniform vec3 player_light_color=vec3(1.0,0.9,0.7);

// Fog
uniform float fog_begin=64.0;
uniform float fog_end=896.0;
uniform float fog_density=0.6;
uniform float height_fog_density=0.012;
uniform float sea_level=64.0;

// Color grading
uniform float saturation=1.0;
uniform float contrast=1.0;

// Atmosphere
uniform float fog_scatter=0.0;
uniform vec3 fog_scatter_color=vec3(1.0,0.85,0.55);

uniform vec3 aerial_color=vec3(0.62,0.80,0.96);
uniform float aerial_strength=0.35;

uniform vec3 sky_light_warmth=vec3(1.0,0.95,0.85);

uniform vec3 horizon_color=vec3(0.30,0.52,0.85);
uniform vec3 zenith_color=vec3(0.06,0.20,0.85);

varying vec3 world_position;
varying vec3 world_normal;
varying vec3 view_direction;
varying float fog_distance;
varying vec4 custom0_attr;
varying vec4 custom1_attr;
void vertex(){

    world_position=(MODEL_MATRIX*vec4(VERTEX,1.0)).xyz;
    world_normal=normalize((MODEL_MATRIX*vec4(NORMAL,0.0)).xyz);

    vec3 camera_position=INV_VIEW_MATRIX[3].xyz;

    view_direction=normalize(world_position-camera_position);

    fog_distance=distance(world_position,camera_position);
    custom0_attr = CUSTOM0;
    custom1_attr = CUSTOM1;
}

void fragment(){

    float layer=custom1_attr.r;
    vec2 water_uv = UV + vec2(TIME * 0.02, TIME * 0.01);
    vec2 final_uv = mix(UV, water_uv, step(abs(layer - 11.0), 0.5));
    vec3 tex_coord = vec3(final_uv, layer);
    vec4 color = texture(texture_array, tex_coord, mipmap_bias);

    float ao=custom1_attr.g;

    bool is_water=abs(layer-11.0)<0.5;
    bool is_leaves=false;

    vec3 block_light=custom0_attr.rgb;

    vec3 sun_dir_norm = normalize(sun_direction);
    float sunFacing=max(dot(world_normal, sun_dir_norm),0.0);

    float skyBrightness=
        custom0_attr.a*
        sky_light_intensity*
        mix(0.25,1.0,sunFacing);

    vec3 skyTemp=
        mix(vec3(0.55,0.65,0.85),
            sky_light_warmth,
            sky_light_intensity);

    vec3 skyContribution=
        skyTemp*
        skyBrightness*
        sky_light_color;

    vec3 combinedLight=max(block_light,skyContribution);

    float playerDist=
        distance(world_position,player_light_position);

    float playerLight=
        1.0-clamp(playerDist/player_light_radius,0.0,1.0);

    playerLight*=playerLight;

    combinedLight=max(
        combinedLight,
        player_light_color*
        playerLight*
        player_light_intensity
    );

    vec3 lit_color=color.rgb*combinedLight*ao;

    if(is_leaves){

        float backlit=
            max(dot(world_normal,-sun_dir_norm),0.0);

        float sss=
            backlit*
            backlit*
            sky_light_intensity*
            0.6;

        lit_color+=
            vec3(0.15,0.35,0.05)*
            sss*
            color.g;
    }

    if(is_water){

        float shimmer=
            sin(world_position.x*12.0+
            world_position.z*8.0)*0.5+0.5;

        shimmer*=
            sin(world_position.x*7.0-
            world_position.z*11.0+
            1.5)*0.5+0.5;

        lit_color*=1.0+shimmer*0.15;

        lit_color+=
            sky_light_color*
            sky_light_intensity*
            0.08;

        // Sun glint: smooth specular highlight where the sun's reflection
        // hits the water, strongest low on the horizon at sunrise/sunset.
        // Uses the flat face normal (no per-pixel wave noise), so the
        // highlight shape comes purely from how the view ray sweeps across
        // the surface geometry -- same as a classic flat-plane specular
        // highlight. No noise involved means no aliasing risk.
        vec3 incident=normalize(view_direction);
        vec3 reflect_dir=reflect(incident,world_normal);
        float specAngle=max(dot(reflect_dir,sun_dir_norm),0.0);
        float glint=
            pow(specAngle,90.0)*1.2+
            pow(specAngle,20.0)*0.25;
        glint*=max(sky_light_intensity,0.05);

        lit_color+=vec3(1.0,0.85,0.6)*glint;
    }

    if(sky_light_intensity>0.1){

    vec3 bounce=
        vec3(0.35,0.32,0.22)*
        sky_light_intensity*
        0.15;

    float bounceAmount=
        world_normal.y*0.5+0.5;
    bounceAmount=max(bounceAmount,0.5);

        lit_color+=
            color.rgb*
            bounce*
            bounceAmount*
            ao;
    }

    // ========= FOG =========
	float fog_amount = 0.0;
	float dist_factor = 0.0;

	if (fog_density > 0.001) {
    	float dist = max(fog_distance - fog_begin, 0.0);
    	float fog_range = max(fog_end - fog_begin, 1.0);
    	float x = clamp(dist / fog_range, 0.0, 1.0);
    	fog_amount = x * sqrt(x);
    	fog_amount *= fog_density * 1.5;
    	fog_amount = clamp(fog_amount, 0.0, 1.0);
    	dist_factor = smoothstep(fog_begin, fog_end, fog_distance);
	}

    float belowSea = max(sea_level - world_position.y, 0.0);
    float hf = belowSea * height_fog_density;
    float heightFog = hf / (1.0 + hf);
    heightFog *= dist_factor;
    fog_amount += heightFog * 0.5;

    float horizon = abs(view_direction.y);
    float h3 = (1.0 - horizon);
    h3 = h3 * h3 * h3;
    fog_amount += h3 * 0.18 * dist_factor;
    fog_amount = clamp(fog_amount, 0.0, 1.0);

    fog_amount = clamp(fog_amount, 0.0, 1.0);

    // ========= SUN SCATTERING =========
    vec3 view_norm = normalize(view_direction);
    float sunDot = max(dot(view_norm, sun_dir_norm), 0.0);
    float sd2 = sunDot * sunDot;
    float sd4 = sd2 * sd2;
    float sd8 = sd4 * sd4;
    float sd16 = sd8 * sd8;
    float sd64 = sd16 * sd16;
    sd64 = sd64 * sd64;
    float sunGlow = sd4 * 0.25 + sd16 + sd64 * 0.4;

    vec3 scatter = fog_scatter_color * sunGlow * fog_scatter * sky_light_intensity;
    scatter *= dist_factor;

	// ========= SKY GRADIENT =========
	float up2 = max(view_direction.y, 0.0);
	float skyBlend = 1.0 - (1.0 - up2) * sqrt(1.0 - up2);
	vec3 finalFogColor = mix(horizon_color, zenith_color, skyBlend);

	float sun_elevation = sun_dir_norm.y;
	float sun_vis = smoothstep(-0.08, 0.08, sun_elevation);
	vec3 warmFog = mix(horizon_color, fog_scatter_color, 0.35);
	finalFogColor = mix(finalFogColor, warmFog, sd4 * sd2 * 0.4 * sun_vis);
	finalFogColor += scatter * (1.0 - fog_amount);

    // ========= AERIAL PERSPECTIVE =========
    float aerial = fog_amount * fog_amount * aerial_strength;
    float luma = dot(lit_color, vec3(0.299, 0.587, 0.114));
    float tintStrength = aerial * (1.0 - luma * 0.6);
    float facingSky = clamp(world_normal.y * 0.5 + 0.5, 0.0, 1.0);
    tintStrength *= mix(1.2, 0.8, facingSky);
    vec3 sceneColor = mix(lit_color, finalFogColor, tintStrength);
    sceneColor += vec3(0.05, 0.08, 0.12) * fog_amount * (1.0 - sky_light_intensity);

    // ========= FINAL BLEND =========
    vec3 final_color = mix(sceneColor, finalFogColor, fog_amount);
    final_color += scatter * fog_amount * 0.20;

    float desat = (1.0 - smoothstep(0.95, 1.0, fog_amount)) * 0.12;
    float lum = dot(final_color, vec3(0.299, 0.587, 0.114));
    final_color = mix(final_color, vec3(lum), desat);

    final_color = final_color * (2.51 * final_color + 0.03) / (final_color * (2.43 * final_color + 0.59) + 0.14);
    final_color = clamp(final_color, 0.0, 1.0);

    // Saturation
    float l = dot(final_color, vec3(0.299, 0.587, 0.114));
    final_color = mix(vec3(l), final_color, saturation);

    // Contrast
    final_color = (final_color - 0.5) * contrast + 0.5;
    final_color = clamp(final_color, 0.0, 1.0);

    ALBEDO = final_color;
    ALPHA = 1.0;
}

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

// Atmosphere
uniform float fog_scatter=0.0;
uniform vec3 fog_scatter_color=vec3(1.0,0.85,0.55);

uniform vec3 aerial_color=vec3(0.62,0.80,0.96);
uniform float aerial_strength=0.10;

uniform vec3 sky_light_warmth=vec3(1.0,0.95,0.85);

uniform vec3 horizon_color=vec3(0.30,0.52,0.85);
uniform vec3 zenith_color=vec3(0.06,0.20,0.85);

varying vec3 world_position;
varying vec3 world_normal;
varying vec3 view_direction;
varying float fog_distance;
varying vec4 custom0_attr;
varying vec4 custom1_attr;

vec3 saturate_color(vec3 value, float amount) {
    float lum = dot(value, vec3(0.299, 0.587, 0.114));
    return mix(vec3(lum), value, amount);
}

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

    vec3 tex_coord=vec3(UV,custom1_attr.r);
    vec4 color=texture(texture_array,tex_coord,mipmap_bias);

    float ao=custom1_attr.g;

    float layer=custom1_attr.r;

    bool is_water=abs(layer-11.0)<0.5;
    bool is_leaves=false;

    vec3 block_light=custom0_attr.rgb;

    vec3 sun_dir_norm = normalize(sun_direction);
    vec3 view_norm = normalize(view_direction);
    float sunFacing = max(dot(world_normal, sun_dir_norm), 0.0);
    float skyLevel = custom0_attr.a * sky_light_intensity;

    float skyBand = smoothstep(0.08, 0.65, skyLevel);
    float sunBand = smoothstep(0.18, 0.52, sunFacing);
    float shadeBand = smoothstep(0.02, 0.30, skyLevel * 0.8 + sunFacing * 0.25);

    vec3 skyTint = mix(vec3(0.54, 0.74, 0.96), sky_light_color, 0.35);
    skyTint = saturate_color(skyTint, 1.03);

    vec3 shadowTint = mix(vec3(0.26, 0.30, 0.42), horizon_color, 0.22);
    shadowTint = saturate_color(shadowTint, 1.02);

    vec3 sunTint = mix(vec3(0.98, 0.76, 0.30), sky_light_warmth, 0.68);
    sunTint = saturate_color(sunTint, 1.03);

    vec3 ambientLight = mix(shadowTint * 0.62, skyTint * 0.74, shadeBand);
    ambientLight *= mix(0.22, 0.56, skyBand);

    vec3 directLight = sunTint * mix(0.12, 0.62, sunBand) * mix(0.45, 0.92, skyBand);
    vec3 skyContribution = ambientLight + directLight * (0.40 + sky_light_intensity * 0.42);

    vec3 combinedLight = max(block_light, skyContribution);

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

        lit_color*=1.0+shimmer*0.10;

        lit_color+=
            skyTint*
            sky_light_intensity*
            0.06;
    }

    if(sky_light_intensity>0.1){

        vec3 bounce=
            mix(vec3(0.30,0.24,0.18), vec3(0.68,0.56,0.30), skyBand)*
            sky_light_intensity*
            0.08;

        float bounceAmount=
            world_normal.y*0.5+0.5;
        bounceAmount=max(bounceAmount,0.45);

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
        fog_amount = x * x;
        fog_amount *= fog_density * 1.15;
        fog_amount = clamp(fog_amount, 0.0, 1.0);
        dist_factor = smoothstep(fog_begin, fog_end, fog_distance);
    }

    float belowSea = max(sea_level - world_position.y, 0.0);
    float hf = belowSea * height_fog_density;
    float heightFog = hf / (1.0 + hf);
    heightFog *= dist_factor;
    fog_amount += heightFog * 0.35;

    float horizon = abs(view_direction.y);
    float h3 = (1.0 - horizon);
    h3 = h3 * h3 * h3;
    fog_amount += h3 * 0.12 * dist_factor;
    fog_amount = clamp(fog_amount, 0.0, 1.0);

    // ========= SUN SCATTERING =========
    float sunDot = max(dot(view_norm, sun_dir_norm), 0.0);
    float sd2 = sunDot * sunDot;
    float sd4 = sd2 * sd2;
    float sd8 = sd4 * sd4;
    float sd16 = sd8 * sd8;
    float sd64 = sd16 * sd16;
    sd64 = sd64 * sd64;
    float sunGlow = sd4 * 0.18 + sd16 * 0.9 + sd64 * 0.55;

    vec3 scatter = fog_scatter_color * sunGlow * fog_scatter * sky_light_intensity;
    scatter *= dist_factor;

    // ========= SKY GRADIENT =========
    float up2 = max(view_direction.y, 0.0);
    float skyBlend = smoothstep(0.0, 1.0, up2);
    vec3 finalFogColor = mix(horizon_color, zenith_color, skyBlend);

    float sun_elevation = sun_dir_norm.y;
    float sun_vis = smoothstep(-0.08, 0.08, sun_elevation);
    vec3 warmFog = mix(horizon_color, fog_scatter_color, 0.55);
    finalFogColor = mix(finalFogColor, warmFog, sd8 * 0.45 * sun_vis);
    finalFogColor += scatter * (1.0 - fog_amount);
    finalFogColor = saturate_color(finalFogColor, 1.06);

    // ========= AERIAL PERSPECTIVE =========
    float aerial = fog_amount * fog_amount * aerial_strength;
    float tintStrength = aerial * 0.40;
    vec3 sceneColor = mix(lit_color, mix(finalFogColor, aerial_color, 0.35), tintStrength);
    sceneColor += vec3(0.03, 0.05, 0.08) * fog_amount * (1.0 - sky_light_intensity);

    // ========= FINAL BLEND =========
    vec3 final_color = mix(sceneColor, finalFogColor, fog_amount * 0.62);
    final_color += scatter * fog_amount * 0.10;

    float lum = dot(final_color, vec3(0.299, 0.587, 0.114));
    final_color = mix(vec3(lum), final_color, 1.03);
    final_color = pow(max(final_color, vec3(0.0)) * 0.92, vec3(1.02));
    final_color = final_color * (2.51 * final_color + 0.03) / (final_color * (2.43 * final_color + 0.59) + 0.14);
    final_color = clamp(final_color, 0.0, 1.0);

    ALBEDO = final_color;
    ALPHA = 1.0;
}
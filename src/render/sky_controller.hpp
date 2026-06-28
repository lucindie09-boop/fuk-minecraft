#ifndef FUK_MINECRAFT_SKY_CONTROLLER_HPP
#define FUK_MINECRAFT_SKY_CONTROLLER_HPP
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <algorithm>

namespace VoxelEngine {

static const char* SKY_SHADER_SRC = R"(
shader_type sky;

uniform float blend = 1.0;
uniform vec3 sun_direction = vec3(0.0, 1.0, 0.0);
uniform vec3 sun_color = vec3(1.0, 0.95, 0.7);
uniform float exposure = 1.0;
uniform vec3 horizon_color = vec3(0.30, 0.52, 0.85);
uniform vec3 zenith_color = vec3(0.06, 0.20, 0.85);
uniform float moon_phase = 0.5;
uniform sampler2D sun_texture;

vec3 aces_tonemap(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void sky() {
    vec3 dir = normalize(EYEDIR);
    float up = max(dir.y, 0.0);

    vec3 sun_fwd = normalize(sun_direction);
    float sun_elevation = sun_fwd.y;
    float sun_dot = max(dot(dir, sun_fwd), 0.0);

    float gradient = 1.0 - (1.0 - up) * sqrt(1.0 - up);
    vec3 day_sky = mix(horizon_color, zenith_color, gradient);

    float ng = pow(up, 1.2);
    vec3 night_zenith_c = vec3(0.001, 0.004, 0.015);
    vec3 night_horizon_c = vec3(0.025, 0.045, 0.090);
    vec3 night_mid_c = vec3(0.008, 0.018, 0.050);
    vec3 night_sky = mix(night_horizon_c, night_mid_c, smoothstep(0.0, 0.5, ng));
    night_sky = mix(night_sky, night_zenith_c, smoothstep(0.5, 1.0, ng));

    float dither = fract(sin(dot(dir * 1000.0, vec3(12.9898, 78.233, 45.543))) * 43758.5453) * 0.003;
    night_sky += dither;

    float sunset_fade = 1.0 - smoothstep(0.0, 0.20, abs(sun_elevation));
    vec3 sunset_color = mix(
        vec3(1.00, 0.28, 0.06),
        vec3(0.95, 0.45, 0.15),
        smoothstep(0.0, 0.5, up)
    );
    sunset_color = mix(sunset_color, vec3(0.70, 0.30, 0.50), smoothstep(0.5, 1.0, up));

    vec3 base_sky = mix(night_sky, day_sky, blend);
    base_sky += (sunset_color - day_sky) * sunset_fade * blend;

    float sun_glow = pow(sun_dot, 20.0) * 0.3 + pow(sun_dot, 60.0) * 0.6;
    base_sky += sun_color * sun_glow * smoothstep(-0.08, 0.08, sun_elevation);

    float hg = 1.0 - up;
    hg = hg * hg;
    hg = hg * hg;
    float horizon_glow = hg * hg;
    base_sky += horizon_color * horizon_glow * 0.15 * blend;

    // Sun using texture
    vec3 sun_right = normalize(cross(vec3(0.0, 1.0, 0.0), sun_fwd + vec3(0.001, 0.0, 0.0)));
    vec3 sun_up_vec = cross(sun_fwd, sun_right);
    float sun_vis = smoothstep(-0.08, 0.08, sun_elevation);
    if (dot(dir, sun_fwd) > 0.0 && sun_vis > 0.0) {
        vec2 suv = vec2(dot(dir, sun_right), dot(dir, sun_up_vec)) / dot(dir, sun_fwd);
        vec2 tex_uv = suv / 0.065 + 0.5;
        vec4 texel = texture(sun_texture, tex_uv);
        float alpha = texel.a;
        vec3 sun_tex_col = texel.rgb * sun_color * 2.0;
        float glow = exp(-length(suv) * 8.0) * 0.2;
        base_sky += (sun_tex_col * alpha + glow) * sun_vis;
    }

    // Moon using texture
    vec3 moon_dir = -sun_fwd;
    float moon_dot = max(dot(dir, moon_dir), 0.0);
    float moon_vis = smoothstep(-0.08, 0.08, moon_dir.y);
    float moon_dim = 1.0 - blend * 0.75;
    if (moon_dot > 0.0 && moon_vis > 0.0) {
        vec2 muv = vec2(-dot(dir, sun_right), dot(dir, sun_up_vec)) / dot(dir, moon_dir);
        vec2 tex_uv = muv / 0.065 + 0.5;
        vec4 texel = texture(sun_texture, tex_uv);
        float alpha = texel.a;
        vec3 moon_tint = vec3(0.7, 0.8, 1.0);
        float halo = pow(moon_dot, 80.0) * 0.04;
        base_sky += (texel.rgb * moon_tint * alpha * 1.5 + halo) * moon_vis * moon_dim;
    }

    base_sky = aces_tonemap(base_sky * exposure);

    COLOR = max(base_sky, vec3(0.0));
}
)";

class SkyController {
private:
    godot::Vector3 compute_horizon_color(float blend, float sun_elevation) const {
        godot::Vector3 day_horizon(0.30f, 0.52f, 0.85f);
        godot::Vector3 night_horizon(0.025f, 0.045f, 0.090f);

        float sunset_factor = std::clamp(std::abs(sun_elevation) / 0.35f, 0.0f, 1.0f);
        godot::Vector3 sunset_horizon(0.95f, 0.70f, 0.40f);

        godot::Vector3 base_color = night_horizon.lerp(day_horizon, blend);
        return base_color.lerp(sunset_horizon, (1.0f - sunset_factor) * 0.5f);
    }

    godot::Vector3 compute_zenith_color(float blend) const {
        godot::Vector3 day_zenith(0.06f, 0.20f, 0.85f);
        godot::Vector3 night_zenith(0.001f, 0.004f, 0.015f);
        return night_zenith.lerp(day_zenith, blend);
    }

public:
    [[nodiscard]] godot::Vector3 get_horizon_color(float blend, float sun_elevation = 0.0f) const {
        return compute_horizon_color(blend, sun_elevation);
    }

    [[nodiscard]] godot::Vector3 get_zenith_color(float blend) const {
        return compute_zenith_color(blend);
    }

    void ensure_sky(godot::Environment* env) {
        godot::Ref<godot::Sky> sky_res = env->get_sky();
        if (!sky_res.is_valid()) {
            sky_res.instantiate();
            env->set_sky(sky_res);
        }
        if (!cached_mat.is_valid()) {
            godot::Ref<godot::Material> base_mat = sky_res->get_material();
            if (base_mat.is_valid()) {
                cached_mat = base_mat;
            }
        }
        if (!cached_mat.is_valid()) {
            cached_mat.instantiate();
            godot::Ref<godot::Shader> shader;
            shader.instantiate();
            shader->set_code(SKY_SHADER_SRC);
            cached_mat->set_shader(shader);
            sky_res->set_material(cached_mat);
        }
        if (!sun_texture_loaded) {
            godot::Ref<godot::Texture2D> tex = godot::ResourceLoader::get_singleton()->load("res://textures/atmosphere/sun.png");
            if (tex.is_valid()) {
                cached_mat->set_shader_parameter("sun_texture", tex);
            }
            sun_texture_loaded = true;
        }
    }

    void update(godot::Environment* env, float blend, float time, float cloud_time, const godot::Color& sun_color, const godot::Vector3& sun_dir, float moon_phase, float exposure, float sky_turbidity = 0.35f, float aurora_intensity = 1.0f, float fog_scatter = 0.0f) {
        if (!env) return;
        if (!sky_ready) {
            ensure_sky(env);
            env->set_background(godot::Environment::BG_SKY);
            sky_ready = true;
        }
        if (!cached_mat.is_valid()) return;

        cached_mat->set_shader_parameter("blend", blend);
        cached_mat->set_shader_parameter("sun_direction", sun_dir);
        cached_mat->set_shader_parameter("sun_color", godot::Vector3(sun_color.r, sun_color.g, sun_color.b));
        cached_mat->set_shader_parameter("exposure", exposure);
        cached_mat->set_shader_parameter("moon_phase", moon_phase);

        float sun_elevation = sun_dir.y;
        godot::Vector3 horizon_color = compute_horizon_color(blend, sun_elevation);
        godot::Vector3 zenith_color = compute_zenith_color(blend);
        cached_mat->set_shader_parameter("horizon_color", horizon_color);
        cached_mat->set_shader_parameter("zenith_color", zenith_color);
    }

private:
    bool sky_ready = false;
    bool sun_texture_loaded = false;
    godot::Ref<godot::ShaderMaterial> cached_mat;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_SKY_CONTROLLER_HPP

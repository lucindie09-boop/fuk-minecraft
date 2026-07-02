#ifndef FUK_MINECRAFT_DAY_NIGHT_CYCLE_HPP
#define FUK_MINECRAFT_DAY_NIGHT_CYCLE_HPP
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <cmath>
#include <algorithm>

namespace VoxelEngine {

#ifndef FUK_SMOOTHSTEP_DEFINED
#define FUK_SMOOTHSTEP_DEFINED
inline float smoothstep(float edge0, float edge1, float x) {
    float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
#endif

enum class LightingPreset {
    Main,
    Spooky
};

class DayNightCycle {
public:
    void update(double delta) {
        if (enabled) {
            time += delta / duration;
            if (time >= 1.0) {
                time -= 1.0;
            }
            moon_time += delta / (duration * 8.0);
            if (moon_time >= 1.0) {
                moon_time -= 1.0;
            }
        }
        cloud_time += delta * 0.01;
    }

    [[nodiscard]] float get_sun_elevation() const {
        if (!enabled) return 1.0f;
        return std::sin(static_cast<float>(time - 0.25) * 3.14159265f * 2.0f);
    }

    [[nodiscard]] float get_moon_phase() const {
        if (!enabled) return 0.5f;
        return static_cast<float>(moon_time);
    }

    [[nodiscard]] float get_blend() const {
        if (!enabled) return 1.0f;
        const float elevation = get_sun_elevation();
        return smoothstep(-0.18f, 0.20f, elevation);
    }

    [[nodiscard]] float get_sky_intensity() const {
        if (!enabled) return day_intensity;
        const float blend = get_blend();
        return night_intensity + (day_intensity - night_intensity) * blend;
    }

    [[nodiscard]] godot::Color get_sky_color() const {
        if (!enabled) return day_color;
        const float blend = get_blend();
        const float elevation = get_sun_elevation();
        float day_warmth = smoothstep(0.95f, 0.10f, elevation);
        godot::Color day_sky = midday_sky_color_.lerp(warm_sky_color_, day_warmth * day_warmth);
        return night_color.lerp(day_sky, blend);
    }

    [[nodiscard]] godot::Color get_horizon_color() const {
        if (!enabled) return day_horizon_color;
        const float blend = get_blend();
        const float elevation = get_sun_elevation();
        float warmth = smoothstep(0.85f, -0.05f, elevation);
        godot::Color day_horizon = midday_horizon_color_.lerp(warm_horizon_color_, warmth * warmth);
        return night_horizon_color.lerp(day_horizon, blend);
    }

    [[nodiscard]] godot::Color get_sun_color() const {
        if (!enabled) return day_color;
        const float elevation = get_sun_elevation();
        float horizon_factor = std::clamp(1.0f - std::abs(elevation) * 2.0f, 0.0f, 1.0f);
        return zenith_sun_color_.lerp(warm_sun_color_, horizon_factor * horizon_factor);
    }

    [[nodiscard]] godot::Vector3 get_sun_direction() const {
        float angle = static_cast<float>(time - 0.25) * 3.14159265f * 2.0f;
        return godot::Vector3(std::cos(angle), std::sin(angle), 0.0f);
    }

    [[nodiscard]] godot::Vector3 get_moon_direction() const {
        return -get_sun_direction();
    }

    [[nodiscard]] float get_ambient_intensity() const {
        if (!enabled) return day_intensity * 0.4f;
        const float blend = get_blend();
        float night_ambient = night_intensity * 0.34f;
        float day_ambient = day_intensity * 0.36f;
        return night_ambient + (day_ambient - night_ambient) * blend;
    }

    [[nodiscard]] godot::Color get_ambient_color() const {
        if (!enabled) return sky_ambient_day_;
        const float elevation = get_sun_elevation();
        const float blend = get_blend();
        godot::Color sky_ambient = sky_ambient_night_.lerp(sky_ambient_day_, blend);
        godot::Color ground_ambient = ground_ambient_night_.lerp(ground_ambient_day_, blend);
        float ground_factor = std::clamp(elevation * 0.5f + 0.5f, 0.0f, 1.0f);
        return sky_ambient.lerp(ground_ambient, ground_factor * 0.3f);
    }

    [[nodiscard]] float get_sky_turbidity() const {
        if (!enabled) return 0.25f;
        const float elevation = get_sun_elevation();
        float sunset_turbidity = smoothstep(0.0f, 0.5f, 1.0f - std::abs(elevation)) * 0.4f;
        return 0.25f + sunset_turbidity;
    }

    [[nodiscard]] float get_fog_scatter() const {
        if (!enabled) return 0.0f;
        const float elevation = get_sun_elevation();
        return smoothstep(0.0f, 0.4f, 1.0f - std::abs(elevation)) * 0.6f;
    }

    [[nodiscard]] double get_cloud_time() const { return cloud_time; }
    [[nodiscard]] double get_raw_time() const { return time; }
    [[nodiscard]] double get_moon_time() const { return moon_time; }

    void set_enabled(bool v) { enabled = v; }
    [[nodiscard]] bool get_enabled() const { return enabled; }

    void set_time(double t) { time = t; }
    [[nodiscard]] double get_time() const { return time; }

    void set_duration(double d) { duration = std::max(d, 0.1); }
    [[nodiscard]] double get_duration() const { return duration; }

    void set_day_intensity(float v) { day_intensity = std::clamp(v, 0.0f, 1.0f); }
    [[nodiscard]] float get_day_intensity() const { return day_intensity; }

    void set_night_intensity(float v) { night_intensity = std::clamp(v, 0.0f, 1.0f); }
    [[nodiscard]] float get_night_intensity() const { return night_intensity; }

    void set_day_color(const godot::Color& c) { day_color = c; }
    [[nodiscard]] godot::Color get_day_color() const { return day_color; }

    void set_night_color(const godot::Color& c) { night_color = c; }
    [[nodiscard]] godot::Color get_night_color() const { return night_color; }

    void set_dawn_color(const godot::Color& c) { dawn_color = c; }
    [[nodiscard]] godot::Color get_dawn_color() const { return dawn_color; }

    void set_dusk_color(const godot::Color& c) { dusk_color = c; }
    [[nodiscard]] godot::Color get_dusk_color() const { return dusk_color; }

    void set_day_horizon_color(const godot::Color& c) { day_horizon_color = c; }
    [[nodiscard]] godot::Color get_day_horizon_color() const { return day_horizon_color; }

    void set_night_horizon_color(const godot::Color& c) { night_horizon_color = c; }
    [[nodiscard]] godot::Color get_night_horizon_color() const { return night_horizon_color; }

    void set_dawn_horizon_color(const godot::Color& c) { (void)c; }
    [[nodiscard]] godot::Color get_dawn_horizon_color() const { return godot::Color(0.92f, 0.55f, 0.32f, 1.0f); }

    void set_dusk_horizon_color(const godot::Color& c) { (void)c; }
    [[nodiscard]] godot::Color get_dusk_horizon_color() const { return godot::Color(0.88f, 0.42f, 0.28f, 1.0f); }

    void set_lighting_preset(LightingPreset preset) {
        switch (preset) {
        case LightingPreset::Main:
            midday_sky_color_ = godot::Color(0.34f, 0.58f, 0.90f, 1.0f);
            warm_sky_color_ = godot::Color(0.96f, 0.74f, 0.54f, 1.0f);
            midday_horizon_color_ = godot::Color(0.54f, 0.74f, 0.94f, 1.0f);
            warm_horizon_color_ = godot::Color(0.98f, 0.70f, 0.48f, 1.0f);
            zenith_sun_color_ = godot::Color(1.0f, 0.95f, 0.86f, 1.0f);
            warm_sun_color_ = godot::Color(1.0f, 0.76f, 0.50f, 1.0f);
            sky_ambient_day_ = godot::Color(0.40f, 0.50f, 0.66f, 1.0f);
            sky_ambient_night_ = godot::Color(0.15f, 0.20f, 0.32f, 1.0f);
            ground_ambient_day_ = godot::Color(0.50f, 0.48f, 0.42f, 1.0f);
            ground_ambient_night_ = godot::Color(0.18f, 0.20f, 0.22f, 1.0f);
            day_intensity = 0.76f;
            night_intensity = 0.36f;
            day_color = godot::Color(0.72f, 0.84f, 0.98f, 1.0f);
            night_color = godot::Color(0.09f, 0.14f, 0.26f, 1.0f);
            day_horizon_color = godot::Color(0.54f, 0.74f, 0.94f, 1.0f);
            night_horizon_color = godot::Color(0.05f, 0.08f, 0.14f, 1.0f);
            break;
        case LightingPreset::Spooky:
            midday_sky_color_ = godot::Color(0.55f, 0.58f, 0.65f, 1.0f);
            warm_sky_color_ = godot::Color(0.60f, 0.55f, 0.52f, 1.0f);
            midday_horizon_color_ = godot::Color(0.50f, 0.55f, 0.62f, 1.0f);
            warm_horizon_color_ = godot::Color(0.65f, 0.50f, 0.40f, 1.0f);
            zenith_sun_color_ = godot::Color(0.80f, 0.82f, 0.88f, 1.0f);
            warm_sun_color_ = godot::Color(0.70f, 0.55f, 0.40f, 1.0f);
            sky_ambient_day_ = godot::Color(0.45f, 0.50f, 0.55f, 1.0f);
            sky_ambient_night_ = godot::Color(0.05f, 0.08f, 0.20f, 1.0f);
            ground_ambient_day_ = godot::Color(0.45f, 0.44f, 0.42f, 1.0f);
            ground_ambient_night_ = godot::Color(0.06f, 0.08f, 0.14f, 1.0f);
            day_intensity = 0.60f;
            night_intensity = 0.12f;
            day_color = godot::Color(0.50f, 0.55f, 0.65f, 1.0f);
            night_color = godot::Color(0.05f, 0.08f, 0.20f, 1.0f);
            day_horizon_color = godot::Color(0.50f, 0.55f, 0.62f, 1.0f);
            night_horizon_color = godot::Color(0.015f, 0.03f, 0.08f, 1.0f);
            break;
        }
    }

private:
    double time = 0.0;
    double duration = 10.0;
    double cloud_time = 0.0;
    double moon_time = 0.0;
    bool enabled = true;
    float day_intensity = 1.0f;
    float night_intensity = 0.18f;
    godot::Color day_color = godot::Color(0.96f, 0.93f, 0.86f, 1.0f);
    godot::Color night_color = godot::Color(0.05f, 0.10f, 0.22f, 1.0f);
    godot::Color dawn_color = godot::Color(1.0f, 0.50f, 0.30f, 1.0f);
    godot::Color dusk_color = godot::Color(0.90f, 0.35f, 0.25f, 1.0f);
    godot::Color day_horizon_color = godot::Color(0.56f, 0.78f, 0.98f, 1.0f);
    godot::Color night_horizon_color = godot::Color(0.02f, 0.03f, 0.08f, 1.0f);
    // Preset-configurable internal colors
    godot::Color midday_sky_color_ = godot::Color(0.96f, 0.94f, 0.88f, 1.0f);
    godot::Color warm_sky_color_ = godot::Color(1.00f, 0.70f, 0.40f, 1.0f);
    godot::Color midday_horizon_color_ = godot::Color(0.56f, 0.78f, 0.98f, 1.0f);
    godot::Color warm_horizon_color_ = godot::Color(0.98f, 0.54f, 0.22f, 1.0f);
    godot::Color zenith_sun_color_ = godot::Color(1.0f, 0.92f, 0.80f, 1.0f);
    godot::Color warm_sun_color_ = godot::Color(1.0f, 0.52f, 0.16f, 1.0f);
    godot::Color sky_ambient_day_ = godot::Color(0.48f, 0.62f, 0.85f, 1.0f);
    godot::Color sky_ambient_night_ = godot::Color(0.08f, 0.12f, 0.25f, 1.0f);
    godot::Color ground_ambient_day_ = godot::Color(0.70f, 0.63f, 0.48f, 1.0f);
    godot::Color ground_ambient_night_ = godot::Color(0.10f, 0.12f, 0.18f, 1.0f);
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_DAY_NIGHT_CYCLE_HPP

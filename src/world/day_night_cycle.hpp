#ifndef FUK_MINECRAFT_DAY_NIGHT_CYCLE_HPP
#define FUK_MINECRAFT_DAY_NIGHT_CYCLE_HPP
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <cmath>
#include <algorithm>

namespace VoxelEngine {

// -------------------------------------------------------------------------
// Day/night cycle — computes sky light blend and updates environment colors.
// Owns all time-of-day state but does NOT own the material/shader.
// -------------------------------------------------------------------------
class DayNightCycle {
public:
    void update(double delta) {
        if (enabled) {
            time += delta / duration;
            if (time >= 1.0) {
                time -= 1.0;
            }
        }
    }

    [[nodiscard]] float get_blend() const {
        if (!enabled) return 1.0f;
        const float cycle = std::sin(static_cast<float>(time) * 3.14159265f * 2.0f);
        return std::clamp((cycle + 1.0f) * 0.5f, 0.0f, 1.0f);
    }

    [[nodiscard]] float get_sky_intensity() const {
        if (!enabled) return day_intensity;
        const float blend = get_blend();
        return night_intensity + (day_intensity - night_intensity) * blend;
    }

    [[nodiscard]] godot::Color get_sky_color() const {
        if (!enabled) return day_color;
        return night_color.lerp(day_color, get_blend());
    }

    // Update the WorldEnvironment node attached to the given parent.
    // Caches the node pointer to avoid tree traversal every frame.
    void update_environment(godot::Node* parent) {
        if (!parent) return;
        if (parent != cached_parent || !cached_world_env) {
            cached_parent = parent;
            cached_world_env = godot::Object::cast_to<godot::WorldEnvironment>(
                parent->get_node_or_null(godot::NodePath("WorldEnvironment"))
            );
        }
        if (!cached_world_env) return;
        godot::Ref<godot::Environment> env = cached_world_env->get_environment();
        if (!env.is_valid()) return;

        const godot::Color sky = get_sky_color();
        const godot::Color white(1.0f, 1.0f, 1.0f, 1.0f);
        env->set_bg_color(sky.lerp(white, 0.30f));
        env->set_ambient_light_color(sky.lerp(white, 0.15f));
        env->set_fog_light_color(sky.lerp(white, 0.10f));
    }

    // Setters / getters
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

private:
    double time = 0.0;        // 0.0 = midnight, 0.5 = noon
    double duration = 10.0;   // seconds per full day
    bool enabled = true;
    float day_intensity = 1.0f;
    float night_intensity = 0.18f;
    godot::Color day_color = godot::Color(1.0f, 0.72f, 0.42f, 1.0f);
    godot::Color night_color = godot::Color(0.08f, 0.16f, 0.35f, 1.0f);

    // Cached node pointer to avoid tree traversal every frame.
    godot::Node* cached_parent = nullptr;
    godot::WorldEnvironment* cached_world_env = nullptr;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_DAY_NIGHT_CYCLE_HPP
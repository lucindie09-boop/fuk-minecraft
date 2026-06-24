#ifndef FUK_MINECRAFT_ENVIRONMENT_CONTROLLER_HPP
#define FUK_MINECRAFT_ENVIRONMENT_CONTROLLER_HPP
#include <cstdint>

#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/color.hpp>

#include "world/day_night_cycle.hpp"
#include "world/player_light.hpp"
#include "render/material_manager.hpp"

namespace godot {
class Node;
}

namespace VoxelEngine {

class ChunkWorld;
class LightPropagator;
class MeshManager;

class EnvironmentController {
public:
    EnvironmentController() = default;

    void update(double delta, double runtime_elapsed, const godot::Vector3& player_pos,
                ChunkWorld& cw, LightPropagator& lp, MeshManager& mm,
                double initial_loading_duration);

    void update_environment(godot::Node* parent);

    DayNightCycle& get_day_night_cycle() { return day_night; }
    MaterialManager& get_material_manager() { return material_manager; }
    PlayerLight& get_player_light() { return player_light; }

void set_day_time(double t) { day_night.set_time(t); update_shader_parameters(); }
double get_day_time() const { return day_night.get_time(); }

    void set_player_light_enabled(bool enabled) { player_light.set_enabled(enabled); }
    bool get_player_light_enabled() const { return player_light.get_enabled(); }
    void set_player_light_level(int32_t level) { player_light.set_level(static_cast<uint8_t>(std::clamp(level, 0, 15))); }
    int32_t get_player_light_level() const { return static_cast<int32_t>(player_light.get_level()); }

    void set_day_night_cycle_enabled(bool enabled) { day_night.set_enabled(enabled); update_shader_parameters(); }
    bool get_day_night_cycle_enabled() const { return day_night.get_enabled(); }
    void set_day_duration(double duration) { day_night.set_duration(duration); }
    double get_day_duration() const { return day_night.get_duration(); }
    void set_day_sky_intensity(double intensity) { day_night.set_day_intensity(static_cast<float>(intensity)); update_shader_parameters(); }
    double get_day_sky_intensity() const { return day_night.get_day_intensity(); }
    void set_night_sky_intensity(double intensity) { day_night.set_night_intensity(static_cast<float>(intensity)); update_shader_parameters(); }
    double get_night_sky_intensity() const { return day_night.get_night_intensity(); }
    void set_day_sky_color(const godot::Color& color) { day_night.set_day_color(color); update_shader_parameters(); }
    godot::Color get_day_sky_color() const { return day_night.get_day_color(); }
    void set_night_sky_color(const godot::Color& color) { day_night.set_night_color(color); update_shader_parameters(); }
    godot::Color get_night_sky_color() const { return day_night.get_night_color(); }

private:
    DayNightCycle day_night;
    PlayerLight player_light;
    MaterialManager material_manager;

    void update_shader_parameters();
    void update_player_light(const godot::Vector3& player_pos, double runtime_elapsed,
                             ChunkWorld& cw, LightPropagator& lp, MeshManager& mm,
                             double initial_loading_duration);
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_ENVIRONMENT_CONTROLLER_HPP
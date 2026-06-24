#include "render/environment_controller.hpp"
#include "world/chunk_world.hpp"
#include "lighting/light_propagator.hpp"
#include "mesh/mesh_manager.hpp"
#include "lighting/light_propagation.hpp"
#include <algorithm>

namespace VoxelEngine {

void EnvironmentController::update(double delta, double runtime_elapsed, const godot::Vector3& player_pos,
                                   ChunkWorld& cw, LightPropagator& lp, MeshManager& mm,
                                   double initial_loading_duration) {
    day_night.update(delta);
    update_shader_parameters();

material_manager.update_player_light(
player_pos,
8.0f,
player_light.get_enabled() ? player_light.get_level() / 15.0f : 0.0f,
godot::Color(1.0f, 0.9f, 0.7f)
);
// shader handles the visual
    //update_player_light(player_pos, runtime_elapsed, cw, lp, mm, initial_loading_duration);
}

void EnvironmentController::update_environment(godot::Node* parent) {
    day_night.update_environment(parent);
}

void EnvironmentController::update_shader_parameters() {
    const float sky_intensity = day_night.get_sky_intensity();
    const godot::Color sky_color = day_night.get_sky_color();
    material_manager.update_shader_parameters(sky_intensity, sky_color);

    // Fog distances change with day/night: shorter at night for atmosphere
    constexpr float fog_begin_night = 32.0f;
    constexpr float fog_begin_day = 64.0f;
    constexpr float fog_end_night = 512.0f;
    constexpr float fog_end_day = 896.0f;
    const float fog_begin = fog_begin_night + (fog_begin_day - fog_begin_night) * sky_intensity;
    const float fog_end = fog_end_night + (fog_end_day - fog_end_night) * sky_intensity;
    material_manager.update_fog_parameters(fog_begin, fog_end, sky_color);
}

void EnvironmentController::update_player_light(const godot::Vector3& player_pos, double runtime_elapsed,
                                                 ChunkWorld& cw, LightPropagator& lp, MeshManager& mm,
                                                 double initial_loading_duration) {
    player_light.update(
        player_pos,
        runtime_elapsed,
        initial_loading_duration,
        [&cw](int32_t cx, int32_t cy, int32_t cz) { return cw.get_chunk_data(cx, cy, cz); },
        [&lp](int32_t cx, int32_t cy, int32_t cz, std::vector<LightNode>& remove, std::vector<LightNode>& add) {
            lp.light_propagate_remove(cx, cy, cz, remove, add);
        },
        [&lp](int32_t cx, int32_t cy, int32_t cz, std::vector<LightNode>& add) {
            lp.light_propagate_add(cx, cy, cz, add);
        },
        [&mm](int32_t cx, int32_t cy, int32_t cz) { mm.mark_chunks_dirty_for_light(cx, cy, cz); }
    );
}

} // namespace VoxelEngine

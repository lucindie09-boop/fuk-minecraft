#ifndef FUK_MINECRAFT_CHUNK_MANAGER_HPP
#define FUK_MINECRAFT_CHUNK_MANAGER_HPP
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/aabb.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <memory>

#include "core/performance_timer.hpp"
#include "core/block_types.hpp"

namespace godot {
class Node;
class Node3D;
}

namespace VoxelEngine {
class VoxelEngineController;
}

namespace VoxelEngine {

// -------------------------------------------------------------------------
// ChunkManager — Godot-facing Node3D that delegates to VoxelEngineController.
// All engine logic lives in the controller; this class is a thin wrapper
// for Godot lifecycle, property binding, and player node resolution.
// -------------------------------------------------------------------------
class ChunkManager : public godot::Node3D {
    GDCLASS(ChunkManager, godot::Node3D)

public:
    ChunkManager();
    ~ChunkManager() override;

    void _ready() override;
    void _enter_tree() override;
    void _exit_tree() override;
    void _process(double delta) override;

    // Godot-bound API (thin wrappers)
    void set_seed(int32_t p_seed);
    int32_t get_seed() const;

    void set_render_distance(int32_t distance);
    int32_t get_render_distance() const;

    void set_player_position(const godot::Vector3& position);
    godot::Vector3 get_player_position() const;

    void set_player_path(const godot::NodePath& path);
    godot::NodePath get_player_path() const;

    void set_auto_update(bool enabled);
    bool get_auto_update() const;

    void update_chunks();

    void generate_chunk(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z);
    void unload_chunk(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z);

    void set_sea_level(float level);
    float get_sea_level() const;

    void set_base_height(float height);
    float get_base_height() const;

    void set_height_scale(float scale);
    float get_height_scale() const;

    void set_mountain_scale(float scale);
    float get_mountain_scale() const;

    godot::String get_performance_report();

    void set_chunk_scenario(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z);
    void clear_editor_chunks();

    void set_debug_enabled(bool enabled);
    bool get_debug_enabled() const;

    void set_debug_print_interval(double interval);
    double get_debug_print_interval() const;

    void set_editor_enabled(bool enabled);
    bool get_editor_enabled() const;

    void set_editor_render_distance(int32_t distance);
    int32_t get_editor_render_distance() const;

    godot::Dictionary raycast_from_camera(double max_distance);

    void set_block(int32_t world_x, int32_t world_y, int32_t world_z, int block_id);
    int get_block(int32_t world_x, int32_t world_y, int32_t world_z);
    godot::String get_block_name(int block_id);

    godot::Dictionary resolve_voxel_collision(const godot::Vector3& position, const godot::Vector3& motion, const godot::Vector3& size);

void set_smooth_lighting(bool enabled);
bool get_smooth_lighting() const;

    void set_player_light_enabled(bool enabled);
    bool get_player_light_enabled() const;

    void set_player_light_level(int32_t level);
    int32_t get_player_light_level() const;

void set_day_time(double t);
double get_day_time() const;
void set_time(double t);
double get_time() const;

    void set_day_night_cycle_enabled(bool enabled);
    bool get_day_night_cycle_enabled() const;
void toggle_day_night_cycle();

    void set_day_duration(double duration);
    double get_day_duration() const;

    void set_day_sky_intensity(double intensity);
    double get_day_sky_intensity() const;

    void set_night_sky_intensity(double intensity);
    double get_night_sky_intensity() const;

    void set_day_sky_color(const godot::Color& color);
    godot::Color get_day_sky_color() const;

    void set_night_sky_color(const godot::Color& color);
    godot::Color get_night_sky_color() const;

    void set_fog_density(double density);
    double get_fog_density() const;

    static PerformanceTimer& get_perf_timer();

protected:
    static void _bind_methods();

private:
    std::unique_ptr<VoxelEngineController> controller;
    godot::NodePath player_path = godot::NodePath("../Player");
    godot::Node3D* cached_player = nullptr;
    godot::Camera3D* cached_camera = nullptr;
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_CHUNK_MANAGER_HPP
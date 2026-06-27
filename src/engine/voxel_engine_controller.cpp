#include "engine/voxel_engine_controller.hpp"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <cmath>
#include <algorithm>

#include "debug/perf_report.hpp"
#include "world/block_editor.hpp"
#include "core/thread_pool.hpp"
#include "mesh/mesh_builder.hpp"
#include "worldgen/chunk_generator.hpp"
#include <mutex>

namespace VoxelEngine {
using namespace godot;

PerformanceTimer VoxelEngineController::perf_timer;

PerformanceTimer& VoxelEngineController::get_perf_timer() {
    return perf_timer;
}

VoxelEngineController::VoxelEngineController()
    : block_editor(&chunk_world, &mesh_manager, &light_propagator) {
    static std::once_flag registry_init_flag;
    std::call_once(registry_init_flag, []() {
        BlockRegistry::get_instance().initialize_default_blocks();
    });
    chunk_world.get_chunk_map().reserve(5000);
    create_thread_pool();
    chunk_world.set_thread_pool(thread_pool.get());
    mesh_manager.set_chunk_map(chunk_world.get_chunk_map_ptr());
    mesh_manager.set_chunk_scheduler(&chunk_world.get_scheduler());
    mesh_manager.set_thread_pool(thread_pool.get());
    mesh_manager.set_performance_timer(&perf_timer);
    mesh_manager.set_async_epoch(chunk_world.get_epoch_ptr());
    mesh_manager.set_owner(nullptr);
    light_propagator.set_chunk_map(chunk_world.get_chunk_map_ptr());
    light_propagator.set_mesh_manager(&mesh_manager);
    chunk_world.set_mesh_manager(&mesh_manager);
    chunk_world.set_light_propagator(&light_propagator);
    chunk_world.set_owner(nullptr);
    world_updater.set_chunk_world(&chunk_world);
    world_updater.set_mesh_manager(&mesh_manager);
    world_updater.set_thread_pool(thread_pool.get());
    world_updater.set_performance_timer(&perf_timer);
    world_updater.set_material_manager(&environment_controller.get_material_manager());
    world_updater.set_owner(nullptr);
    world_updater.set_seed(seed);
    world_updater.set_sea_level(sea_level);
    world_updater.set_base_height(base_height);
    world_updater.set_height_scale(height_scale);
    world_updater.set_mountain_scale(mountain_scale);
    world_updater.set_render_distance(render_distance);
    world_updater.set_editor_render_distance(editor_render_distance);
    mesh_manager.set_mesh_render_distance(render_distance);
    LodSettings lod_settings;
    lod_settings.lod0_radius = 8;
    lod_settings.lod1_radius = 24;
    lod_settings.enabled = true;
    mesh_manager.set_lod_settings(lod_settings);
}

VoxelEngineController::~VoxelEngineController() {
    reset_runtime_state(false);
}

void VoxelEngineController::initialize() {
}

void VoxelEngineController::shutdown(godot::Node* parent) {
    reset_runtime_state(false);
}

void VoxelEngineController::set_owner(godot::Node* node) {
    mesh_manager.set_owner(node);
    chunk_world.set_owner(node);
    world_updater.set_owner(node);
}

void VoxelEngineController::create_thread_pool() {
    unsigned int hw_threads = std::thread::hardware_concurrency();
    size_t num_threads = hw_threads > 1 ? static_cast<size_t>(hw_threads - 1) : 1;
    thread_pool = std::make_unique<ThreadPool>(num_threads);
    chunk_world.set_thread_pool(thread_pool.get());
    mesh_manager.set_thread_pool(thread_pool.get());
    world_updater.set_thread_pool(thread_pool.get());
}

void VoxelEngineController::shutdown_thread_pool() {
    if (thread_pool) {
        thread_pool->shutdown();
        thread_pool.reset();
    }
    chunk_world.set_thread_pool(nullptr);
    mesh_manager.set_thread_pool(nullptr);
    world_updater.set_thread_pool(nullptr);
}

void VoxelEngineController::clear_async_queues() {
    chunk_world.clear();
    mesh_manager.clear();
    world_updater.clear();
}

void VoxelEngineController::free_loaded_chunks() {
    chunk_world.free_loaded_chunks();
}

void VoxelEngineController::reset_runtime_state(bool restart_thread_pool) {
    chunk_world.increment_epoch();
    shutdown_thread_pool();
    chunk_world.free_loaded_chunks();
    clear_async_queues();
    world_updater.reset();
    runtime_elapsed = 0.0;
    frame_time_accumulator = 0.0;
    frame_count = 0;
    debug_accumulated_time = 0.0;

    if (restart_thread_pool) {
        create_thread_pool();
    }
}

void VoxelEngineController::update(double delta, bool is_editor, const godot::Vector3& player_pos, godot::Node* owner) {
    if (!auto_update) return;
    if (is_editor && !editor_enabled) return;

    ScopedTimer process_timer(perf_timer, TimerID::ProcessTotal);
    frame_count++;
    runtime_elapsed += delta;
    frame_time_accumulator += delta;
    last_delta = delta;

    player_position = player_pos;

    environment_controller.update(delta, runtime_elapsed, player_position,
                                  chunk_world, light_propagator, mesh_manager,
                                  world_updater.get_initial_loading_duration());

    {
        ScopedTimer t(perf_timer, TimerID::PlayerPosUpdate);
        world_updater.set_player_position(player_position);
    }

    {
        ScopedTimer t(perf_timer, TimerID::WorldUpdate);
        update_chunks(is_editor);
    }

    {
        ScopedTimer t(perf_timer, TimerID::SceneUpdate);
        print_debug_info(delta);
    }
}

void VoxelEngineController::update_chunks(bool is_editor) {
    world_updater.update(is_editor, chunk_world.get_epoch(), chunks_processed_total, last_delta);
}

// -------------------------------------------------------------------------
// Block editing (delegated to BlockEditor)
// -------------------------------------------------------------------------

void VoxelEngineController::set_block_world(int32_t world_x, int32_t world_y, int32_t world_z, int block_id) {
    block_editor.place_block(world_x, world_y, world_z, static_cast<BlockID>(block_id));
}

int VoxelEngineController::get_block_world(int32_t world_x, int32_t world_y, int32_t world_z) {
    return block_editor.query_block(world_x, world_y, world_z);
}

String VoxelEngineController::get_block_name(int block_id) {
    return block_editor.get_block_name(block_id);
}

Dictionary VoxelEngineController::raycast_from_camera(godot::Node* owner, const NodePath& player_path, double max_distance) {
    return block_editor.raycast(owner, player_path, max_distance);
}

// -------------------------------------------------------------------------
// Chunk scenario / editor
// -------------------------------------------------------------------------

void VoxelEngineController::set_chunk_scenario(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z, godot::Node* owner) {
    ChunkRenderData* render_data = chunk_world.get_chunk_render_data(chunk_x, chunk_y, chunk_z);
    if (!render_data) return;
    if (!render_data->instance_rid.is_valid()) return;

    RenderingServer* rs = RenderingServer::get_singleton();
    Node3D* owner3d = Object::cast_to<Node3D>(owner);
    Ref<World3D> world = owner3d ? owner3d->get_world_3d() : Ref<World3D>();
    if (!world.is_valid()) {
        if (owner && owner->is_inside_tree()) {
            owner->call_deferred("set_chunk_scenario", chunk_x, chunk_y, chunk_z);
        }
        return;
    }
    RID scenario = world->get_scenario();
    rs->instance_set_scenario(render_data->instance_rid, scenario);
    rs->instance_set_visible(render_data->instance_rid, true);
}

void VoxelEngineController::clear_editor_chunks(godot::Node* parent) {
    reset_runtime_state(true);
    last_player_block_x = INT32_MIN;
    last_player_block_y = INT32_MIN;
    last_player_block_z = INT32_MIN;
    print_line("clear_editor_chunks: All chunks cleared");
}

void VoxelEngineController::unload_chunk(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) {
    uint64_t key = chunk_world.get_chunk_map().get_chunk_key(chunk_x, chunk_y, chunk_z);
    chunk_world.save_chunk_to_disk(chunk_x, chunk_y, chunk_z);
    world_updater.try_unload(key);
}

void VoxelEngineController::generate_chunk(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z) {
    world_updater.generate_chunk(chunk_x, chunk_y, chunk_z, chunk_world.get_epoch());
}

// -------------------------------------------------------------------------
// Debug / perf
// -------------------------------------------------------------------------

String VoxelEngineController::get_performance_report() {
    String report = PerfReport::build(
        frame_time_accumulator,
        frame_count,
        debug_print_interval,
        chunks_processed_total,
        chunks_processed_last_interval,
        perf_timer,
        thread_pool ? thread_pool->get_worker_count() : 0,
        thread_pool ? thread_pool->get_queue_size() : 0,
        chunk_world.get_scheduler().generating_count(),
        chunk_world.get_scheduler().completed_chunk_count(),
        chunk_world.get_chunk_map().size(),
        mesh_manager.gather_render_stats()
    );
    chunks_processed_last_interval = chunks_processed_total;
    frame_count = 0;
    frame_time_accumulator = 0.0;
    perf_timer.reset_all();
    MeshBuilder::get_perf_timer().reset_all();
    ChunkGenerator::get_perf_timer().reset_all();
    MeshBuilder::reset_vertex_tracking();
    MeshBuilder::reset_greedy_vertical_stats();
    return report;
}

void VoxelEngineController::print_debug_info(double delta) {
    if (!debug_enabled) return;
    debug_accumulated_time += delta;
    if (debug_accumulated_time >= debug_print_interval) {
        print_line(get_performance_report());
        debug_accumulated_time = 0.0;
    }
}

// -------------------------------------------------------------------------
// Property accessors
// -------------------------------------------------------------------------

void VoxelEngineController::set_seed(int32_t s) { seed = s; world_updater.set_seed(seed); }
int32_t VoxelEngineController::get_seed() const { return seed; }

void VoxelEngineController::set_render_distance(int32_t rd) { render_distance = rd; world_updater.set_render_distance(render_distance); }
int32_t VoxelEngineController::get_render_distance() const { return render_distance; }

void VoxelEngineController::set_editor_render_distance(int32_t rd) { editor_render_distance = rd; world_updater.set_editor_render_distance(editor_render_distance); }
int32_t VoxelEngineController::get_editor_render_distance() const { return editor_render_distance; }

void VoxelEngineController::set_player_position(const godot::Vector3& pos) { player_position = pos; world_updater.set_player_position(player_position); }
godot::Vector3 VoxelEngineController::get_player_position() const { return player_position; }

void VoxelEngineController::set_sea_level(float level) { sea_level = level; world_updater.set_sea_level(sea_level); }
float VoxelEngineController::get_sea_level() const { return sea_level; }

void VoxelEngineController::set_base_height(float height) { base_height = height; world_updater.set_base_height(base_height); }
float VoxelEngineController::get_base_height() const { return base_height; }

void VoxelEngineController::set_height_scale(float scale) { height_scale = scale; world_updater.set_height_scale(height_scale); }
float VoxelEngineController::get_height_scale() const { return height_scale; }

void VoxelEngineController::set_mountain_scale(float scale) { mountain_scale = scale; world_updater.set_mountain_scale(mountain_scale); }
float VoxelEngineController::get_mountain_scale() const { return mountain_scale; }

void VoxelEngineController::set_auto_update(bool enabled) { auto_update = enabled; }
bool VoxelEngineController::get_auto_update() const { return auto_update; }

void VoxelEngineController::set_smooth_lighting(bool enabled) {
smooth_lighting = enabled;
mesh_manager.set_smooth_lighting(enabled);
mesh_manager.mark_all_chunks_dirty();
}
bool VoxelEngineController::get_smooth_lighting() const { return smooth_lighting; }

void VoxelEngineController::set_editor_enabled(bool enabled) { editor_enabled = enabled; }
bool VoxelEngineController::get_editor_enabled() const { return editor_enabled; }

void VoxelEngineController::set_debug_enabled(bool enabled) { debug_enabled = enabled; }
bool VoxelEngineController::get_debug_enabled() const { return debug_enabled; }

void VoxelEngineController::set_debug_print_interval(double interval) { debug_print_interval = interval; }
double VoxelEngineController::get_debug_print_interval() const { return debug_print_interval; }

void VoxelEngineController::set_player_light_enabled(bool enabled) { environment_controller.set_player_light_enabled(enabled); }
bool VoxelEngineController::get_player_light_enabled() const { return environment_controller.get_player_light_enabled(); }

void VoxelEngineController::set_player_light_level(int32_t level) { environment_controller.set_player_light_level(level); }
int32_t VoxelEngineController::get_player_light_level() const { return environment_controller.get_player_light_level(); }

void VoxelEngineController::set_day_time(double t) { environment_controller.set_day_time(t); }
double  VoxelEngineController::get_day_time() const { return environment_controller.get_day_time(); }

void VoxelEngineController::set_time(double t) { set_day_time(t); }
double VoxelEngineController::get_time() const { return get_day_time(); }

void VoxelEngineController::set_day_night_cycle_enabled(bool enabled) { environment_controller.set_day_night_cycle_enabled(enabled); }
bool VoxelEngineController::get_day_night_cycle_enabled() const { return environment_controller.get_day_night_cycle_enabled(); }
void VoxelEngineController::toggle_day_night_cycle() {set_day_night_cycle_enabled(!get_day_night_cycle_enabled()); }

void VoxelEngineController::set_day_duration(double duration) { environment_controller.set_day_duration(duration); }
double VoxelEngineController::get_day_duration() const { return environment_controller.get_day_duration(); }

void VoxelEngineController::set_day_sky_intensity(double intensity) { environment_controller.set_day_sky_intensity(intensity); }
double VoxelEngineController::get_day_sky_intensity() const { return environment_controller.get_day_sky_intensity(); }

void VoxelEngineController::set_night_sky_intensity(double intensity) { environment_controller.set_night_sky_intensity(intensity); }
double VoxelEngineController::get_night_sky_intensity() const { return environment_controller.get_night_sky_intensity(); }

void VoxelEngineController::set_day_sky_color(const godot::Color& color) { environment_controller.set_day_sky_color(color); }
godot::Color VoxelEngineController::get_day_sky_color() const { return environment_controller.get_day_sky_color(); }

void VoxelEngineController::set_night_sky_color(const godot::Color& color) { environment_controller.set_night_sky_color(color); }
godot::Color VoxelEngineController::get_night_sky_color() const { return environment_controller.get_night_sky_color(); }

} // namespace VoxelEngine

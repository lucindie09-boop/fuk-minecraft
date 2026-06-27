#ifndef FUK_MINECRAFT_MESH_MANAGER_HPP
#define FUK_MINECRAFT_MESH_MANAGER_HPP
#include "core/chunk_map.hpp"
#include "core/chunk_types.hpp"
#include "world/chunk_scheduler.hpp"
#include "mesh/mesh_queue.hpp"
#include "mesh/mesh_builder.hpp"
#include "core/thread_pool.hpp"
#include "core/performance_timer.hpp"
#include <godot_cpp/classes/shader_material.hpp>
#include <memory>
#include <cstdint>

namespace VoxelEngine {

class MeshManager {
public:
    void set_chunk_map(ChunkMap* cm) { chunk_map = cm; }
    void set_chunk_scheduler(ChunkScheduler* cs) { chunk_scheduler = cs; }
    void set_thread_pool(ThreadPool* tp) { thread_pool = tp; }
    void set_performance_timer(PerformanceTimer* pt) { perf_timer = pt; }
    void set_async_epoch(std::atomic<uint64_t>* ae) { async_epoch = ae; }
    void set_owner(godot::Node* node) { owner = node; }

    void set_player_chunk(int32_t cx, int32_t cy, int32_t cz) {
        last_player_chunk_x = cx;
        last_player_chunk_y = cy;
        last_player_chunk_z = cz;
    }

    void set_player_block(int32_t bx, int32_t by, int32_t bz) {
        last_player_block_x = bx;
        last_player_block_y = by;
        last_player_block_z = bz;
    }

<<<<<<< Updated upstream
void set_mesh_render_distance(int32_t rd) {mesh_render_distance = rd;}
=======
    void set_mesh_render_distance(int32_t rd) { mesh_render_distance = rd; }
    void set_lod_settings(const LodSettings& settings);
    const LodSettings& get_lod_settings() const { return lod_controller.get_settings(); }
    LodController& get_lod_controller() { return lod_controller; }
    const LodController& get_lod_controller() const { return lod_controller; }

    void update_lod(int32_t render_distance, bool force_rescan = false);
    void process_lod_transitions(uint64_t epoch);
    void split_lod_group_for_edit(uint64_t group_key);
>>>>>>> Stashed changes

    void process_completed_meshes(uint64_t epoch, double budget_ms, int32_t max_uploads, const godot::Ref<godot::ShaderMaterial>& material);
    void process_completed_group_meshes_standalone(uint64_t epoch, double budget_ms, int32_t max_uploads,
                                                   const godot::Ref<godot::ShaderMaterial>& material);
    void rebuild_rendering_server_mesh(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z, uint64_t epoch,
                                         ChunkRenderData* render_data,
                                         ChunkRenderData* d_x_neg,
                                         ChunkRenderData* d_x_pos,
                                         ChunkRenderData* d_y_neg,
                                         ChunkRenderData* d_y_pos,
                                         ChunkRenderData* d_z_neg,
                                         ChunkRenderData* d_z_pos);
    void rebuild_chunk_mesh(int32_t chunk_x, int32_t chunk_y, int32_t chunk_z, uint64_t epoch);
    void rebuild_all_meshes_with_neighbors(uint64_t epoch);
    void queue_dirty_chunk(int32_t cx, int32_t cy, int32_t cz);
    void queue_immediate_dirty_chunk(int32_t cx, int32_t cy, int32_t cz);
    void mark_chunk_urgent(int32_t cx, int32_t cy, int32_t cz);
    void reprioritize(int32_t player_cx, int32_t player_cy, int32_t player_cz);
    void mark_chunks_dirty_for_light(int32_t center_cx, int32_t center_cy, int32_t center_cz);
    void process_queue(int32_t max_immediate, int32_t max_rebuilds, double budget_ms);
    void clear();
    size_t size() const { return mesh_queue.size(); }
    bool erase_urgent(uint64_t key) { return mesh_queue.erase_urgent(key); }

<<<<<<< Updated upstream
void set_smooth_lighting(bool enabled) { smooth_lighting_enabled = enabled; }
bool is_smooth_lighting_enabled() const {return smooth_lighting_enabled; }
void mark_all_chunks_dirty();

private:
=======
    void set_smooth_lighting(bool enabled) { smooth_lighting_enabled = enabled; }
    bool is_smooth_lighting_enabled() const { return smooth_lighting_enabled; }
    void mark_all_chunks_dirty();
    [[nodiscard]] bool has_pending_mesh_work() const;
    WorldRenderStats gather_render_stats();

private:
    void process_completed_group_meshes(uint64_t epoch, double budget_ms, int32_t max_uploads,
                                        const godot::Ref<godot::ShaderMaterial>& material, int32_t& uploads_this_frame,
                                        double elapsed_budget_ms);
    void apply_split_transition(const LodTransition& transition, uint64_t epoch);
    void apply_merge_transition(const LodTransition& transition, uint64_t epoch);
    void apply_rebuild_transition(const LodTransition& transition, uint64_t epoch);
    void hide_chunk_instance(ChunkRenderData* render_data);
    void show_chunk_instance(ChunkRenderData* render_data, int32_t cx, int32_t cy, int32_t cz);
    void release_group_to_individual(LodGroupRenderData* group);
    void recover_stuck_lod_chunks();
    void disable_lod_and_split_all_groups();
    void free_group_render_data(LodGroupRenderData& group);
    PackedBuiltMeshData pack_built_mesh(const BuiltMeshData& built_mesh);

>>>>>>> Stashed changes
    ChunkMap* chunk_map = nullptr;
    ChunkScheduler* chunk_scheduler = nullptr;
    ThreadPool* thread_pool = nullptr;
    PerformanceTimer* perf_timer = nullptr;
    std::atomic<uint64_t>* async_epoch = nullptr;
    godot::Node* owner = nullptr;
    MeshQueue mesh_queue;
    int32_t last_player_chunk_x = INT32_MIN;
    int32_t last_player_chunk_y = INT32_MIN;
    int32_t last_player_chunk_z = INT32_MIN;
    int32_t last_player_block_x = INT32_MIN;
    int32_t last_player_block_y = INT32_MIN;
    int32_t last_player_block_z = INT32_MIN;
<<<<<<< Updated upstream
int32_t mesh_render_distance = 0;
bool smooth_lighting_enabled = false;
=======
    int32_t mesh_render_distance = 0;
    bool smooth_lighting_enabled = false;
    int32_t lod_periodic_rescan_counter = 0;
    bool needs_stuck_recovery_ = true;
>>>>>>> Stashed changes
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_MESH_MANAGER_HPP
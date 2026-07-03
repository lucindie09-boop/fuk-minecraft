#ifndef FUK_MINECRAFT_LOD_CONTROLLER_HPP
#define FUK_MINECRAFT_LOD_CONTROLLER_HPP

#include "core/chunk_map.hpp"
#include "core/frustum.hpp"
#include "mesh/lod_types.hpp"
#include "core/performance_timer.hpp"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace VoxelEngine {

class LodController {
public:
    void set_chunk_map(ChunkMap* cm) { chunk_map = cm; }
    void set_performance_timer(PerformanceTimer* pt) { perf_timer = pt; }
    void set_settings(const LodSettings& settings) { lod_settings = settings; }
    void set_frustum(const Frustum* frustum) { frustum_ = frustum; }
    const LodSettings& get_settings() const { return lod_settings; }

    void clear();
    bool update(int32_t player_cx, int32_t player_cy, int32_t player_cz, int32_t render_distance, bool force_rescan);

    const std::vector<LodTransition>& get_pending_transitions() const { return pending_transitions; }
    void clear_pending_transitions() { pending_transitions.clear(); }

    LodGroupRenderData* get_group(uint64_t group_key);
    const LodGroupRenderData* get_group(uint64_t group_key) const;
    LodGroupRenderData* get_or_create_group(int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz);
    void remove_group(uint64_t group_key);
    void for_each_group(const std::function<void(uint64_t, LodGroupRenderData&)>& fn);

    LodRingStats get_ring_stats() const { return ring_stats; }

    LodLevel classify_target_lod(int32_t cx, int32_t cy, int32_t cz,
                                 int32_t player_cx, int32_t player_cy, int32_t player_cz,
                                 int32_t render_distance) const;

    bool collect_group_members(int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                                uint64_t out_keys[8], int32_t& out_count) const;

    void split_group_on_edit(uint64_t group_key, std::vector<LodTransition>& out_transitions);

    bool has_incomplete_groups() const;
    void queue_incomplete_group_merges();
    void collect_all_group_splits(std::vector<LodTransition>& out_transitions) const;
    void mark_groups_dirty_for_chunk(int32_t cx, int32_t cy, int32_t cz);

private:
    ChunkMap* chunk_map = nullptr;
    PerformanceTimer* perf_timer = nullptr;
    const Frustum* frustum_ = nullptr;
    LodSettings lod_settings{};
    std::unordered_map<uint64_t, std::unique_ptr<LodGroupRenderData>> groups;
    std::vector<LodTransition> pending_transitions;
    LodRingStats ring_stats{};

    int32_t last_player_cx = INT32_MIN;
    int32_t last_player_cy = INT32_MIN;
    int32_t last_player_cz = INT32_MIN;
    int32_t last_render_distance = 0;

    LodLevel apply_hysteresis(LodLevel current, LodLevel target, int32_t cx, int32_t cy, int32_t cz,
                              int32_t player_cx, int32_t player_cy, int32_t player_cz,
                              int32_t render_distance) const;

    int32_t horizontal_dist_sq(int32_t cx, int32_t cz, int32_t pcx, int32_t pcz) const;
    int32_t effective_horizontal_dist_sq(int32_t cx, int32_t cz, int32_t pcx, int32_t pcz) const;
    bool in_vertical_range(int32_t cy, int32_t pcy) const;

    void queue_transition(LodTransitionKind kind, uint64_t group_key,
                          int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                          LodLevel target_level);

    void recompute_ring_stats();
};

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_LOD_CONTROLLER_HPP

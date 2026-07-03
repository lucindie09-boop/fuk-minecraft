#include "mesh/lod_controller.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <unordered_set>

namespace VoxelEngine {

void LodController::clear() {
    groups.clear();
    pending_transitions.clear();
    ring_stats = {};
    last_player_cx = INT32_MIN;
    last_player_cy = INT32_MIN;
    last_player_cz = INT32_MIN;
    last_render_distance = 0;
}

int32_t LodController::horizontal_dist_sq(int32_t cx, int32_t cz, int32_t pcx, int32_t pcz) const {
    const int32_t dx = cx - pcx;
    const int32_t dz = cz - pcz;
    return dx * dx + dz * dz;
}

int32_t LodController::effective_horizontal_dist_sq(int32_t cx, int32_t cz, int32_t pcx, int32_t pcz) const {
    const int32_t raw = horizontal_dist_sq(cx, cz, pcx, pcz);
    if (!frustum_ || !frustum_->is_initialized()) {
        return raw;
    }
    if (frustum_->is_chunk_visible(cx, 0, cz)) {
        return static_cast<int32_t>(raw * 0.36f);
    }
    return raw;
}

bool LodController::in_vertical_range(int32_t cy, int32_t pcy) const {
    return std::abs(cy - pcy) <= lod_settings.vertical_buffer;
}

LodLevel LodController::classify_target_lod(int32_t cx, int32_t cy, int32_t cz,
                                            int32_t player_cx, int32_t player_cy, int32_t player_cz,
                                            int32_t render_distance) const {
    if (!lod_settings.enabled) {
        return LodLevel::Individual;
    }
    if (!in_vertical_range(cy, player_cy)) {
        return LodLevel::Individual;
    }

    const int32_t horiz_dist2 = effective_horizontal_dist_sq(cx, cz, player_cx, player_cz);
    const int32_t rd2 = render_distance * render_distance;
    if (horiz_dist2 > rd2) {
        return LodLevel::Individual;
    }

    const int32_t lod0_sq = lod_settings.lod0_radius * lod_settings.lod0_radius;
    const int32_t lod1_sq = lod_settings.lod1_radius * lod_settings.lod1_radius;

    if (horiz_dist2 <= lod0_sq) {
        return LodLevel::Individual;
    }
    if (horiz_dist2 <= lod1_sq) {
        return LodLevel::MergedFull;
    }
    return LodLevel::MergedDownsampled;
}

LodLevel LodController::apply_hysteresis(LodLevel current, LodLevel target, int32_t cx, int32_t cy, int32_t cz,
                                           int32_t player_cx, int32_t player_cy, int32_t player_cz,
                                           int32_t render_distance) const {
    if (current == target) {
        return current;
    }

    const int32_t horiz_dist2 = effective_horizontal_dist_sq(cx, cz, player_cx, player_cz);
    const int32_t h = lod_settings.hysteresis;
    const int32_t lod0_sq = lod_settings.lod0_radius * lod_settings.lod0_radius;
    const int32_t lod1_sq = lod_settings.lod1_radius * lod_settings.lod1_radius;
    const int32_t rd2 = render_distance * render_distance;

    auto promote_threshold_sq = [&](LodLevel level) -> int32_t {
        switch (level) {
            case LodLevel::Individual: return lod0_sq;
            case LodLevel::MergedFull: return lod1_sq;
            case LodLevel::MergedDownsampled: return rd2;
        }
        return rd2;
    };

    auto demote_threshold_sq = [&](LodLevel level) -> int32_t {
        switch (level) {
            case LodLevel::Individual: return (lod_settings.lod0_radius + h) * (lod_settings.lod0_radius + h);
            case LodLevel::MergedFull: return (lod_settings.lod1_radius + h) * (lod_settings.lod1_radius + h);
            case LodLevel::MergedDownsampled: return (render_distance + h) * (render_distance + h);
        }
        return rd2;
    };

    if (static_cast<uint8_t>(target) < static_cast<uint8_t>(current)) {
        if (horiz_dist2 <= promote_threshold_sq(target)) {
            return target;
        }
        return current;
    }

    if (horiz_dist2 > demote_threshold_sq(current)) {
        return target;
    }
    return current;
}

bool LodController::collect_group_members(int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                                          uint64_t out_keys[8], int32_t& out_count) const {
    if (!chunk_map) {
        out_count = 0;
        return false;
    }

    const int32_t merge_size = lod_merge_size(lod_settings.merge_shift);
    out_count = 0;
    for (int32_t oz = 0; oz < merge_size; ++oz) {
        for (int32_t oy = 0; oy < merge_size; ++oy) {
            for (int32_t ox = 0; ox < merge_size; ++ox) {
                const int32_t cx = anchor_cx + ox;
                const int32_t cy = anchor_cy + oy;
                const int32_t cz = anchor_cz + oz;
                ChunkRenderData* render_data = chunk_map->get_chunk_render_data(cx, cy, cz);
                if (!render_data || !render_data->data) {
                    return false;
                }
                out_keys[out_count++] = chunk_map->get_chunk_key(cx, cy, cz);
            }
        }
    }
    return out_count == merge_size * merge_size * merge_size;
}

LodGroupRenderData* LodController::get_group(uint64_t group_key) {
    auto it = groups.find(group_key);
    return it != groups.end() ? it->second.get() : nullptr;
}

const LodGroupRenderData* LodController::get_group(uint64_t group_key) const {
    auto it = groups.find(group_key);
    return it != groups.end() ? it->second.get() : nullptr;
}

LodGroupRenderData* LodController::get_or_create_group(int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz) {
    const uint64_t group_key = chunk_map->get_chunk_key(anchor_cx, anchor_cy, anchor_cz);
    auto it = groups.find(group_key);
    if (it != groups.end()) {
        return it->second.get();
    }

    auto group = std::make_unique<LodGroupRenderData>();
    group->anchor_cx = anchor_cx;
    group->anchor_cy = anchor_cy;
    group->anchor_cz = anchor_cz;
    LodGroupRenderData* ptr = group.get();
    groups.emplace(group_key, std::move(group));
    return ptr;
}

void LodController::remove_group(uint64_t group_key) {
    groups.erase(group_key);
}

void LodController::mark_groups_dirty_for_chunk(int32_t cx, int32_t cy, int32_t cz) {
    if (!chunk_map) return;
    int32_t ax, ay, az;
    lod_align_anchor(cx, cy, cz, lod_settings.merge_shift, ax, ay, az);
    const uint64_t group_key = chunk_map->get_chunk_key(ax, ay, az);
    LodGroupRenderData* group = get_group(group_key);
    if (group && group->instance_rid.is_valid()) {
        group->is_dirty = true;
    }
}

void LodController::for_each_group(const std::function<void(uint64_t, LodGroupRenderData&)>& fn) {
    for (auto& [key, group] : groups) {
        fn(key, *group);
    }
}

void LodController::queue_transition(LodTransitionKind kind, uint64_t group_key,
                                     int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                                     LodLevel target_level) {
    if (static_cast<int32_t>(pending_transitions.size()) >= lod_settings.max_transitions_per_frame) {
        return;
    }
    pending_transitions.push_back(LodTransition{
        kind,
        group_key,
        anchor_cx,
        anchor_cy,
        anchor_cz,
        target_level,
    });
}

void LodController::split_group_on_edit(uint64_t group_key, std::vector<LodTransition>& out_transitions) {
    LodGroupRenderData* group = get_group(group_key);
    if (!group || !chunk_map) {
        return;
    }

    for (int32_t i = 0; i < group->member_count; ++i) {
        int32_t cx, cy, cz;
        ChunkMap::decode_chunk_key(group->member_keys[i], cx, cy, cz);
        ChunkRenderData* render_data = chunk_map->get_chunk_render_data(cx, cy, cz);
        if (render_data) {
            render_data->render_lod = ChunkRenderLod::Individual;
            render_data->lod_group_key = 0;
            render_data->current_lod = LodLevel::Individual;
            render_data->effective_lod = LodLevel::Individual;
            render_data->is_mesh_dirty = true;
        }
    }

    out_transitions.push_back(LodTransition{
        LodTransitionKind::SplitToIndividual,
        group_key,
        group->anchor_cx,
        group->anchor_cy,
        group->anchor_cz,
        LodLevel::Individual,
    });
}

void LodController::recompute_ring_stats() {
    ring_stats = {};
    if (!chunk_map) {
        return;
    }

    chunk_map->for_each([&](uint64_t /*key*/, const std::unique_ptr<ChunkRenderData>& render_data) {
        if (!render_data->data || render_data->data->is_all_air()) {
            return;
        }
        switch (render_data->effective_lod) {
            case LodLevel::Individual:
                ++ring_stats.lod0_chunks;
                if (render_data->render_lod == ChunkRenderLod::Individual && render_data->instance_rid.is_valid()) {
                    ++ring_stats.individual_instances;
                }
                break;
            case LodLevel::MergedFull:
                ++ring_stats.lod1_groups;
                break;
            case LodLevel::MergedDownsampled:
                ++ring_stats.lod2_groups;
                break;
        }
    });

    for (const auto& entry : groups) {
        if (entry.second->instance_rid.is_valid()) {
            ++ring_stats.group_instances;
        }
    }
}

bool LodController::has_incomplete_groups() const {
    for (const auto& entry : groups) {
        const LodGroupRenderData& group = *entry.second;
        if (group.instance_rid.is_valid() && !group.is_dirty) {
            continue;
        }
        if (group.pending_mesh_builds.load(std::memory_order_acquire) > 0 ||
            group.pending_mesh_uploads.load(std::memory_order_acquire) > 0) {
            continue;
        }
        return true;
    }
    return false;
}

void LodController::queue_incomplete_group_merges() {
    if (!lod_settings.enabled) {
        return;
    }

    for (const auto& entry : groups) {
        if (static_cast<int32_t>(pending_transitions.size()) >= lod_settings.max_transitions_per_frame) {
            break;
        }

        LodGroupRenderData& group = *entry.second;
        if (group.instance_rid.is_valid() && !group.is_dirty) {
            continue;
        }
        if (group.pending_mesh_builds.load(std::memory_order_acquire) > 0 ||
            group.pending_mesh_uploads.load(std::memory_order_acquire) > 0) {
            continue;
        }

        const LodTransitionKind kind = group.instance_rid.is_valid()
            ? LodTransitionKind::RebuildGroup
            : LodTransitionKind::MergeGroup;
        queue_transition(kind, entry.first, group.anchor_cx, group.anchor_cy, group.anchor_cz, group.level);
    }
}

void LodController::collect_all_group_splits(std::vector<LodTransition>& out_transitions) const {
    for (const auto& entry : groups) {
        const LodGroupRenderData& group = *entry.second;
        out_transitions.push_back(LodTransition{
            LodTransitionKind::SplitToIndividual,
            entry.first,
            group.anchor_cx,
            group.anchor_cy,
            group.anchor_cz,
            LodLevel::Individual,
        });
    }
}

bool LodController::update(int32_t player_cx, int32_t player_cy, int32_t player_cz,
                           int32_t render_distance, bool force_rescan) {
    if (!chunk_map || !lod_settings.enabled) {
        pending_transitions.clear();
        return false;
    }

    const bool player_moved = player_cx != last_player_cx ||
                              player_cy != last_player_cy ||
                              player_cz != last_player_cz;

    if (!force_rescan && !player_moved && render_distance == last_render_distance) {
        return false;
    }

    last_player_cx = player_cx;
    last_player_cy = player_cy;
    last_player_cz = player_cz;
    last_render_distance = render_distance;

    std::unique_ptr<ScopedTimer> scoped_timer;
    if (perf_timer) {
        scoped_timer = std::make_unique<ScopedTimer>(*perf_timer, TimerID::LodUpdate);
    }

    pending_transitions.clear();

    const int32_t merge_size = lod_merge_size(lod_settings.merge_shift);
    std::unordered_map<uint64_t, LodLevel> desired_group_levels;

    std::unordered_set<uint64_t> split_groups;

    chunk_map->for_each([&](uint64_t key, const std::unique_ptr<ChunkRenderData>& render_data) {
        if (!render_data->data || render_data->data->is_all_air()) {
            return;
        }

        int32_t cx, cy, cz;
        ChunkMap::decode_chunk_key(key, cx, cy, cz);

        const LodLevel target = classify_target_lod(cx, cy, cz, player_cx, player_cy, player_cz, render_distance);
        const LodLevel effective = apply_hysteresis(render_data->effective_lod, target,
                                                    cx, cy, cz, player_cx, player_cy, player_cz, render_distance);
        render_data->current_lod = target;
        render_data->effective_lod = effective;

        if (effective == LodLevel::Individual) {
            if (render_data->render_lod == ChunkRenderLod::HiddenInGroup && render_data->lod_group_key != 0) {
                if (split_groups.insert(render_data->lod_group_key).second) {
                    queue_transition(LodTransitionKind::SplitToIndividual, render_data->lod_group_key,
                                     0, 0, 0, LodLevel::Individual);
                }
            }
            return;
        }

        int32_t ax, ay, az;
        lod_align_anchor(cx, cy, cz, lod_settings.merge_shift, ax, ay, az);
        if (cx != ax || cy != ay || cz != az) {
            return;
        }

        const uint64_t group_key = chunk_map->get_chunk_key(ax, ay, az);
        auto it = desired_group_levels.find(group_key);
        if (it == desired_group_levels.end()) {
            desired_group_levels[group_key] = effective;
        } else if (static_cast<uint8_t>(effective) < static_cast<uint8_t>(it->second)) {
            it->second = effective;
        }
    });

    for (const auto& [group_key, desired_level] : desired_group_levels) {
        if (static_cast<int32_t>(pending_transitions.size()) >= lod_settings.max_transitions_per_frame) {
            break;
        }

        int32_t ax, ay, az;
        ChunkMap::decode_chunk_key(group_key, ax, ay, az);

        uint64_t member_keys[8]{};
        int32_t member_count = 0;
        if (!collect_group_members(ax, ay, az, member_keys, member_count)) {
            continue;
        }

        LodGroupRenderData* group = get_group(group_key);
        if (!group) {
            queue_transition(LodTransitionKind::MergeGroup, group_key, ax, ay, az, desired_level);
            continue;
        }

        if (group->level != desired_level) {
            queue_transition(LodTransitionKind::RebuildGroup, group_key, ax, ay, az, desired_level);
            continue;
        }

        if (!group->instance_rid.is_valid() || group->is_dirty) {
            queue_transition(LodTransitionKind::MergeGroup, group_key, ax, ay, az, desired_level);
        }
    }

    recompute_ring_stats();
    return true;
}

} // namespace VoxelEngine

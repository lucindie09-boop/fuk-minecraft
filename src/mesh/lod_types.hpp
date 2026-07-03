#ifndef FUK_MINECRAFT_LOD_TYPES_HPP
#define FUK_MINECRAFT_LOD_TYPES_HPP

#include <godot_cpp/variant/rid.hpp>
#include <array>
#include <atomic>
#include <cstdint>

namespace VoxelEngine {

constexpr int32_t kMaxSupportedLodMergeShift = 2;
constexpr int32_t kMaxLodGroupMembers = 64;

enum class LodLevel : uint8_t {
    Individual = 0,
    MergedFull = 1,
    MergedDownsampled = 2,
};

enum class ChunkRenderLod : uint8_t {
    Individual = 0,
    HiddenInGroup = 1,
};

struct LodSettings {
    int32_t lod0_radius = 8;
    int32_t lod1_radius = 16;
    int32_t lod1_merge_shift = 2;
    int32_t lod2_merge_shift = 2;
    int32_t lod2_downsample_step = 4;
    int32_t hysteresis = 2;
    int32_t vertical_buffer = 2;
    int32_t max_transitions_per_frame = 32;
    bool enabled = true;
};

struct LodGroupRenderData {
    uint64_t group_key = 0;
    int32_t anchor_cx = 0;
    int32_t anchor_cy = 0;
    int32_t anchor_cz = 0;
    LodLevel level = LodLevel::MergedFull;
    int32_t merge_shift = 1;
    int32_t downsample_step = 1;
    std::array<uint64_t, kMaxLodGroupMembers> member_keys{};
    int32_t member_count = 0;
    godot::RID mesh_rid;
    godot::RID instance_rid;
    bool is_dirty = true;
    std::atomic<int> pending_mesh_builds{0};
    std::atomic<int> pending_mesh_uploads{0};
    std::atomic<uint64_t> mesh_job_serial{0};
    uint64_t mesh_content_hash = 0;
    bool material_set = false;
};

struct LodRingStats {
    int32_t lod0_chunks = 0;
    int32_t lod1_groups = 0;
    int32_t lod2_groups = 0;
    int32_t individual_instances = 0;
    int32_t group_instances = 0;
    int32_t live_groups = 0;
    int32_t dirty_groups = 0;
    int32_t groups_building = 0;
    int32_t groups_uploading = 0;
    int32_t pending_group_retries = 0;
    int32_t pending_group_transitions = 0;
    int32_t completed_group_meshes = 0;
};

struct WorldRenderStats {
    int32_t visible_instances = 0;
    int32_t hidden_instances = 0;
    int32_t mesh_rids = 0;
    LodRingStats lod{};
};

enum class LodTransitionKind : uint8_t {
    SplitToIndividual,
    MergeGroup,
    RebuildGroup,
};

struct LodTransition {
    LodTransitionKind kind = LodTransitionKind::MergeGroup;
    uint64_t group_key = 0;
    int32_t anchor_cx = 0;
    int32_t anchor_cy = 0;
    int32_t anchor_cz = 0;
    LodLevel target_level = LodLevel::MergedFull;
    int32_t merge_shift = 1;
    int32_t downsample_step = 1;
};

inline int32_t lod_merge_size(int32_t merge_shift) {
    return 1 << merge_shift;
}

inline int32_t lod_member_capacity_for_shift(int32_t merge_shift) {
    const int32_t merge_size = lod_merge_size(merge_shift);
    return merge_size * merge_size * merge_size;
}

inline bool lod_merge_shift_supported(int32_t merge_shift) {
    return merge_shift >= 0 && merge_shift <= kMaxSupportedLodMergeShift;
}

inline int32_t lod_level_merge_shift(const LodSettings& settings, LodLevel level) {
    switch (level) {
        case LodLevel::MergedFull:
            return settings.lod1_merge_shift;
        case LodLevel::MergedDownsampled:
            return settings.lod2_merge_shift;
        case LodLevel::Individual:
        default:
            return 0;
    }
}

inline int32_t lod_level_downsample_step(const LodSettings& settings, LodLevel level) {
    if (level == LodLevel::MergedDownsampled) {
        return settings.lod2_downsample_step;
    }
    return 1;
}

inline uint64_t make_lod_group_key(uint64_t anchor_chunk_key, int32_t merge_shift, LodLevel level) {
    uint64_t key = anchor_chunk_key;
    key ^= 0x9e3779b97f4a7c15ULL + (static_cast<uint64_t>(merge_shift & 0xFF) << 8) +
           static_cast<uint64_t>(static_cast<uint8_t>(level));
    key *= 1099511628211ULL;
    return key;
}

inline void lod_align_anchor(int32_t cx, int32_t cy, int32_t cz, int32_t merge_shift,
                             int32_t& ax, int32_t& ay, int32_t& az) {
    const int32_t mask = lod_merge_size(merge_shift) - 1;
    ax = cx & ~mask;
    ay = cy & ~mask;
    az = cz & ~mask;
}

} // namespace VoxelEngine

#endif // FUK_MINECRAFT_LOD_TYPES_HPP

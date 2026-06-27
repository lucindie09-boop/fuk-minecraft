#ifndef FUK_MINECRAFT_LOD_TYPES_HPP
#define FUK_MINECRAFT_LOD_TYPES_HPP

#include <godot_cpp/variant/rid.hpp>
#include <array>
#include <atomic>
#include <cstdint>

namespace VoxelEngine {

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
    int32_t lod1_radius = 20;
    int32_t merge_shift = 1;
    int32_t downsample_step = 2;
    int32_t hysteresis = 2;
    int32_t vertical_buffer = 2;
    int32_t max_transitions_per_frame = 32;
    bool enabled = true;
};

struct LodGroupRenderData {
    int32_t anchor_cx = 0;
    int32_t anchor_cy = 0;
    int32_t anchor_cz = 0;
    LodLevel level = LodLevel::MergedFull;
    std::array<uint64_t, 8> member_keys{};
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
};

inline int32_t lod_merge_size(int32_t merge_shift) {
    return 1 << merge_shift;
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

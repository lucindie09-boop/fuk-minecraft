#include "mesh/mesh_builder.hpp"
#include "core/light_packing.hpp"
#include <cstddef>

namespace VoxelEngine {

static constexpr uint8_t kLightMergeThreshold = 2;

static inline bool lights_similar_enough(uint16_t a, uint16_t b) {
    uint8_t dr = (unpack_r(a) > unpack_r(b)) ? (unpack_r(a) - unpack_r(b)) : (unpack_r(b) - unpack_r(a));
    uint8_t dg = (unpack_g(a) > unpack_g(b)) ? (unpack_g(a) - unpack_g(b)) : (unpack_g(b) - unpack_g(a));
    uint8_t db = (unpack_b(a) > unpack_b(b)) ? (unpack_b(a) - unpack_b(b)) : (unpack_b(b) - unpack_b(a));
    return dr <= kLightMergeThreshold && dg <= kLightMergeThreshold && db <= kLightMergeThreshold;
}

// -------------------------------------------------------------------------
// Passive greedy meshing
// -------------------------------------------------------------------------
void MeshBuilder::flush_horizontal_merge(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                          int32_t z_start, int32_t z_end,
                                          int32_t y, int32_t x, FaceDirection direction,
                                          BlockID block_id, uint16_t light_key, int rotation, const BlockRegistry& registry) {
    if (z_start < 0 || z_end <= z_start) return;

    if (z_end - z_start > 1) {
        float first_ao[4];
        ao.compute_face(accessor, x, y, z_start, direction, first_ao);
        bool has_occlusion = (first_ao[0] < 1.0f || first_ao[1] < 1.0f || first_ao[2] < 1.0f || first_ao[3] < 1.0f);
        bool uniform = true;
        for (int32_t cz = z_start + 1; cz < z_end; ++cz) {
            float block_ao[4];
            this->ao.compute_face(accessor, x, y, cz, direction, block_ao);
            if (block_ao[0] != first_ao[0] || block_ao[1] != first_ao[1] || block_ao[2] != first_ao[2] || block_ao[3] != first_ao[3]) {
                uniform = false;
                break;
            }
        }

        if (uniform && !has_occlusion) {
            Face face;
            face.x = x;
            face.y = y;
            face.z = z_start;
            face.direction = direction;
            face.block_id = block_id;
            face.u_max = 0;
            face.v_max = (z_end - 1) - z_start;
            add_greedy_face(chunk, accessor, face, light_key, rotation, first_ao, registry);
        } else {
            for (int32_t cz = z_start; cz < z_end; ++cz) {
                add_face(chunk, accessor, x, y, cz, direction, block_id, registry);
            }
        }
    } else {
        add_face(chunk, accessor, x, y, z_start, direction, block_id, registry);
    }
}

void MeshBuilder::passive_greedy_mesh_horizontal(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                                 FaceDirection direction, const BlockRegistry& registry) {
    if (direction != FaceDirection::Top && direction != FaceDirection::Bottom) {
        return;
    }

    const int dir_idx = static_cast<int>(direction);
    // Hoist direction offsets once; they are loop-invariant.
    const int32_t dx = kDirectionOffsets[dir_idx][0];
    const int32_t dy = kDirectionOffsets[dir_idx][1];
    const int32_t dz = kDirectionOffsets[dir_idx][2];

    for (int32_t s = 0; s < CHUNK_SECTIONS; s++) {
        if (chunk.is_section_all_air(s)) continue;
        int32_t y0 = s * SECTION_HEIGHT;
        int32_t y1 = y0 + SECTION_HEIGHT;
        for (int32_t y = y0; y < y1; y++) {
            const int32_t nybase = y + dy;
            for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
                int32_t merge_start = -1;
                BlockID current_block = BlockIDs::AIR;
                uint16_t current_light_key = 0;
                int current_rotation = 0;

                for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
                    BlockID block_id = chunk.get_block_unsafe(x, y, z);

                    if (block_id == BlockIDs::AIR) {
                        flush_horizontal_merge(chunk, accessor, merge_start, z, y, x, direction,
                                               current_block, current_light_key, current_rotation, registry);
                        merge_start = -1;
                        continue;
                    }

                    BlockID neighbor = accessor.get_block(x, nybase, z);

                    // If this is the top/bottom y-slice and the neighbor chunk doesn't
                    // exist, skip the face entirely — it'll be added on rebuild when the
                    // neighbor appears.
                    const bool missing_y_boundary =
                        (direction == FaceDirection::Top && y == CHUNK_HEIGHT - 1 && !accessor.pos_y) ||
                        (direction == FaceDirection::Bottom && y == 0 && !accessor.neg_y);

                    bool cull = missing_y_boundary ||
                        should_cull_against_neighbor(chunk, block_id, neighbor, direction, x, y, z, registry);

                    if (cull) {
                        flush_horizontal_merge(chunk, accessor, merge_start, z, y, x, direction,
                                               current_block, current_light_key, current_rotation, registry);
                        merge_start = -1;
                        continue;
                    }

                    int rotation = get_face_rotation(block_id, x, y, z, direction, dir_idx);
                    const uint16_t light_key = accessor.get_light_packed(x + dx, nybase, z + dz);

                    if (merge_start != -1 && block_id == current_block && (z - merge_start) < kMaxGreedyMergeDistance
                        && lights_similar_enough(light_key, current_light_key) && rotation == current_rotation) {
                        continue;
                    }

                    if (merge_start != -1) {
                        flush_horizontal_merge(chunk, accessor, merge_start, z, y, x, direction,
                                               current_block, current_light_key, current_rotation, registry);
                    }

                    merge_start = z;
                    current_block = block_id;
                    current_rotation = rotation;
                    current_light_key = light_key;
                }

                if (merge_start != -1) {
                    flush_horizontal_merge(chunk, accessor, merge_start, CHUNK_DEPTH, y, x, direction,
                                           current_block, current_light_key, current_rotation, registry);
                }
            }
        }
    }
}

// -------------------------------------------------------------------------
// Passive vertical greedy meshing (1D Y-axis merge for side faces)
// -------------------------------------------------------------------------
void MeshBuilder::flush_vertical_merge(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                          int32_t y_start, int32_t y_end,
                                          int32_t x, int32_t z, FaceDirection direction,
                                          BlockID block_id, uint16_t light_key, int rotation, const BlockRegistry& registry) {
    if (y_start < 0 || y_end <= y_start) return;

    if (y_end - y_start > 1) {

++greedy_v_stats_local.merge_attempts;
float first_ao[4];
ao.compute_face (accessor, x, y_start, z, direction, first_ao);
bool has_occlusion = (first_ao[0] < 1.0f || first_ao[1] < 1.0f || first_ao[2] < 1.0f || first_ao[3] < 1.0f);
bool uniform = true;
for (int32_t cy = y_start + 1; cy < y_end; ++cy) {
float block_ao[4];
this->ao.compute_face (accessor, x, cy, z, direction, block_ao); if (block_ao[0] != first_ao[0] || block_ao[1] != first_ao[1] ||
block_ao [2] != first_ao[2] || block_ao[3] != first_ao[3]) { uniform = false;
                break;
            }
        }

if (uniform && !has_occlusion) {
++greedy_v_stats_local.merge_successes;
            Face face;
            face.x = x;
            face.y = y_start;
            face.z = z;
            face.direction = direction;
            face.block_id = block_id;
            face.u_max = 0;
            face.v_max = (y_end - 1) - y_start;
add_greedy_face (chunk, accessor, face, light_key, rotation, first_ao, registry);
} else {
    if (!uniform) {
        ++greedy_v_stats_local.reject_ao_mismatch;
    }
    if (has_occlusion) {
        ++greedy_v_stats_local.reject_ao_occlusion;
    }
    for (int32_t cy = y_start; cy < y_end; ++cy) {
        add_face(chunk, accessor, x, cy, z, direction, block_id, registry);
    }
}
    } else {
        add_face(chunk, accessor, x, y_start, z, direction, block_id, registry);
    }
}

void MeshBuilder::passive_greedy_mesh_vertical(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                                FaceDirection direction, const BlockRegistry& registry) {
    if (direction == FaceDirection::Top || direction == FaceDirection::Bottom) {
        return;
    }

    const int dir_idx = static_cast<int>(direction);
    constexpr std::size_t kYStride = CHUNK_WIDTH;
    constexpr std::size_t kZStride = CHUNK_WIDTH * CHUNK_HEIGHT;

    const ChunkData* boundary_chunk = nullptr;
    const std::ptrdiff_t local_neighbor_offset =
        direction == FaceDirection::Right ? 1 :
        direction == FaceDirection::Left ? -1 :
        direction == FaceDirection::Front ? static_cast<std::ptrdiff_t>(kZStride) : -static_cast<std::ptrdiff_t>(kZStride);
    switch (direction) {
        case FaceDirection::Right: boundary_chunk = accessor.pos_x; break;
        case FaceDirection::Left: boundary_chunk = accessor.neg_x; break;
        case FaceDirection::Front: boundary_chunk = accessor.pos_z; break;
        case FaceDirection::Back: boundary_chunk = accessor.neg_z; break;
        default: break;
    }

    // Neighbor offset in the same chunk (for non-boundary columns)
    int center_nx_off = 0, center_nz_off = 0;
    switch (direction) {
        case FaceDirection::Right: center_nx_off = 1; break;
        case FaceDirection::Left:  center_nx_off = -1; break;
        case FaceDirection::Front: center_nz_off = 1; break;
        case FaceDirection::Back:  center_nz_off = -1; break;
        default: break;
    }

    for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
        for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
            int32_t merge_start = -1;
            BlockID current_block = BlockIDs::AIR;
            uint16_t current_light_key = 0;
            int current_rotation = 0;

            const bool boundary_column =
                (direction == FaceDirection::Right && x == CHUNK_WIDTH - 1) ||
                (direction == FaceDirection::Left && x == 0) ||
                (direction == FaceDirection::Front && z == CHUNK_DEPTH - 1) ||
                (direction == FaceDirection::Back && z == 0);

            for (int32_t s = 0; s < CHUNK_SECTIONS; ++s) {
                int32_t y0 = s * SECTION_HEIGHT;
                int32_t y1 = y0 + SECTION_HEIGHT;
                if (chunk.is_section_all_air(s)) {
                    flush_vertical_merge(chunk, accessor, merge_start, y0, x, z, direction,
                                        current_block, current_light_key, current_rotation, registry);
                    merge_start = -1;
                    continue;
                }

                for (int32_t y = y0; y < y1; y++) {
                    BlockID block_id = chunk.get_block(x, y, z);

                    if (block_id == BlockIDs::AIR) {
                        flush_vertical_merge(chunk, accessor, merge_start, y, x, z, direction,
                                            current_block, current_light_key, current_rotation, registry);
                        merge_start = -1;
                        continue;
                    }

                    BlockID neighbor = BlockIDs::AIR;
                    uint16_t light_key = 0;
                    if (boundary_column) {
                        if (boundary_chunk) {
                            switch (direction) {
                                case FaceDirection::Right:
                                    neighbor = boundary_chunk->get_block_unsafe(0, y, z);
                                    light_key = boundary_chunk->get_light_packed_word_unsafe(0, y, z);
                                    break;
                                case FaceDirection::Left:
                                    neighbor = boundary_chunk->get_block_unsafe(31, y, z);
                                    light_key = boundary_chunk->get_light_packed_word_unsafe(31, y, z);
                                    break;
                                case FaceDirection::Front:
                                    neighbor = boundary_chunk->get_block_unsafe(x, y, 0);
                                    light_key = boundary_chunk->get_light_packed_word_unsafe(x, y, 0);
                                    break;
                                case FaceDirection::Back:
                                    neighbor = boundary_chunk->get_block_unsafe(x, y, 31);
                                    light_key = boundary_chunk->get_light_packed_word_unsafe(x, y, 31);
                                    break;
                                default: break;
                            }
                        } else {
                            // Neighbor chunk doesn't exist on this side — skip face.
                            // The chunk will be dirtied when the neighbor eventually loads
                            // and the face will be generated correctly then.
                            flush_vertical_merge(chunk, accessor, merge_start, y, x, z, direction,
                                                 current_block, current_light_key, current_rotation, registry);
                            merge_start = -1;
                            continue;
                        }
                    } else {
                        neighbor = chunk.get_block_unsafe(x + center_nx_off, y, z + center_nz_off);
                        light_key = chunk.get_light_packed_word_unsafe(x + center_nx_off, y, z + center_nz_off);
                    }

                    bool cull = should_cull_against_neighbor(chunk, block_id, neighbor, direction, x, y, z, registry);
                    if (cull) {
                        flush_vertical_merge(chunk, accessor, merge_start, y, x, z, direction,
                                            current_block, current_light_key, current_rotation, registry);
                        merge_start = -1;
                        continue;
                    }

                    const int rotation = get_face_rotation(block_id, x, y, z, direction, dir_idx);

                    if (merge_start != -1) {
                        const bool same_block = block_id == current_block;
                        const bool within_distance = (y - merge_start) < kMaxGreedyMergeDistance;
                        const bool same_light = lights_similar_enough(light_key, current_light_key);
                        const bool same_rotation = rotation == current_rotation;
                        if (same_block && within_distance && same_light && same_rotation) {
                            continue;
                        }
                        if (!same_block) {
                            ++greedy_v_stats_local.reject_block_mismatch;
                        } else if (!same_light) {
                            ++greedy_v_stats_local.reject_light_mismatch;
                        } else if (!same_rotation) {
                            ++greedy_v_stats_local.reject_rotation_mismatch;
                        } else if (!within_distance) {
                            ++greedy_v_stats_local.reject_distance_limit;
                        }
                        flush_vertical_merge(chunk, accessor, merge_start, y, x, z, direction,
                                            current_block, current_light_key, current_rotation, registry);
                    }

                    merge_start = y;
                    current_block = block_id;
                    current_rotation = rotation;
                    current_light_key = light_key;
                }
            }

            if (merge_start != -1) {
                flush_vertical_merge(chunk, accessor, merge_start, CHUNK_HEIGHT, x, z, direction,
                                     current_block, current_light_key, current_rotation, registry);
            }
        }
    }
}

} // namespace VoxelEngine

#include "mesh/mesh_builder.hpp"
#include <cstddef>

namespace VoxelEngine {

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
                    bool cull = should_cull_against_neighbor(chunk, block_id, neighbor, direction, x, y, z, registry);

                    if (cull) {
                        flush_horizontal_merge(chunk, accessor, merge_start, z, y, x, direction,
                                               current_block, current_light_key, current_rotation, registry);
                        merge_start = -1;
                        continue;
                    }

                    int rotation = get_face_rotation(block_id, x, y, z, direction, dir_idx);
                    const uint16_t light_key = accessor.get_light_packed(x + dx, nybase, z + dz);

<<<<<<< Updated upstream

                    if (merge_start != -1 && block_id == current_block && (x - merge_start) < kMaxGreedyMergeDistance
                        && light_key == current_light_key && rotation == current_rotation) {
=======
                    if (merge_start != -1 && block_id == current_block && (z - merge_start) < kMaxGreedyMergeDistance
                        && lights_similar_enough(light_key, current_light_key) && rotation == current_rotation) {
>>>>>>> Stashed changes
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
    const BlockID* center_blocks = chunk.blocks();
    const uint16_t* center_lights = chunk.get_light_packed_data();
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

    for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
        for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
            int32_t merge_start = -1;
            BlockID current_block = BlockIDs::AIR;
            uint16_t current_light_key = 0;
            int current_rotation = 0;

            std::size_t current_idx = static_cast<std::size_t>(x) + static_cast<std::size_t>(z) * kZStride;
            const bool boundary_column =
                (direction == FaceDirection::Right && x == CHUNK_WIDTH - 1) ||
                (direction == FaceDirection::Left && x == 0) ||
                (direction == FaceDirection::Front && z == CHUNK_DEPTH - 1) ||
                (direction == FaceDirection::Back && z == 0);

            const BlockID* boundary_blocks = nullptr;
            const uint16_t* boundary_lights = nullptr;
            std::size_t boundary_idx = 0;

            if (boundary_column && boundary_chunk) {
                boundary_blocks = boundary_chunk->blocks();
                boundary_lights = boundary_chunk->get_light_packed_data();
                switch (direction) {
                    case FaceDirection::Right:
                        boundary_idx = static_cast<std::size_t>(z) * kZStride;
                        break;
                    case FaceDirection::Left:
                        boundary_idx = static_cast<std::size_t>(CHUNK_WIDTH - 1) + static_cast<std::size_t>(z) * kZStride;
                        break;
                    case FaceDirection::Front:
                        boundary_idx = static_cast<std::size_t>(x);
                        break;
                    case FaceDirection::Back:
                        boundary_idx = static_cast<std::size_t>(x) + static_cast<std::size_t>(CHUNK_DEPTH - 1) * kZStride;
                        break;
                    default:
                        break;
                }
            }

            for (int32_t s = 0; s < CHUNK_SECTIONS; ++s) {
                int32_t y0 = s * SECTION_HEIGHT;
                int32_t y1 = y0 + SECTION_HEIGHT;
                if (chunk.is_section_all_air(s)) {
                    flush_vertical_merge(chunk, accessor, merge_start, y0, x, z, direction,
                                        current_block, current_light_key, current_rotation, registry);
                    merge_start = -1;
                    current_idx += static_cast<std::size_t>(SECTION_HEIGHT) * kYStride;
                    if (boundary_column && boundary_blocks) {
                        boundary_idx += static_cast<std::size_t>(SECTION_HEIGHT) * kYStride;
                    }
                    continue;
                }

                for (int32_t y = y0; y < y1; y++) {
                    BlockID block_id = chunk.get_block(x, y, z);

                    if (block_id == BlockIDs::AIR) {
                        flush_vertical_merge(chunk, accessor, merge_start, y, x, z, direction,
                                            current_block, current_light_key, current_rotation, registry);
                        merge_start = -1;
                        current_idx += kYStride;
                        if (boundary_column && boundary_blocks) {
                            boundary_idx += kYStride;
                        }
                        continue;
                    }

                    BlockID neighbor = BlockIDs::AIR;
                    uint16_t light_key = 0;
                    if (boundary_column) {
                        if (boundary_blocks) {
                            neighbor = boundary_blocks[boundary_idx];
                            light_key = boundary_lights[boundary_idx];
                        }
                    } else {
                        const std::size_t local_neighbor_idx = static_cast<std::size_t>(
                            static_cast<std::ptrdiff_t>(current_idx) + local_neighbor_offset);
                        neighbor = center_blocks[local_neighbor_idx];
                        light_key = center_lights[local_neighbor_idx];
                    }

                    bool cull = should_cull_against_neighbor(chunk, block_id, neighbor, direction, x, y, z, registry);
                    if (cull) {
                        flush_vertical_merge(chunk, accessor, merge_start, y, x, z, direction,
                                            current_block, current_light_key, current_rotation, registry);
                        merge_start = -1;
                        current_idx += kYStride;
                        if (boundary_column && boundary_blocks) {
                            boundary_idx += kYStride;
                        }
                        continue;
                    }

                    const int rotation = get_face_rotation(block_id, x, y, z, direction, dir_idx);

                    if (merge_start != -1) {
                        const bool same_block = block_id == current_block;
                        const bool within_distance = (y - merge_start) < kMaxGreedyMergeDistance;
                        const bool same_light = light_key == current_light_key;
                        const bool same_rotation = rotation == current_rotation;
                        if (same_block && within_distance && same_light && same_rotation) {
                            current_idx += kYStride;
                            if (boundary_column && boundary_blocks) {
                                boundary_idx += kYStride;
                            }
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
                    current_idx += kYStride;
                    if (boundary_column && boundary_blocks) {
                        boundary_idx += kYStride;
                    }
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

#include "mesh/mesh_builder.hpp"

namespace VoxelEngine {

// -------------------------------------------------------------------------
// Passive greedy meshing
// -------------------------------------------------------------------------
void MeshBuilder::flush_horizontal_merge(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                          int32_t x_start, int32_t x_end,
                                          int32_t y, int32_t z, FaceDirection direction,
                                          BlockID block_id, uint16_t light_key, int rotation) {
    if (x_start < 0 || x_end <= x_start) return;

    if (x_end - x_start > 1) {
        // Only check the two endpoint blocks. AO only varies at run boundaries
        // (where a different block type abuts the corner), so checking the full
        // run is quadratic work for zero benefit on the interior cells.
        bool has_ao = false;
        for (int32_t cx : {x_start, x_end - 1}) {
            float ao[4];
            compute_face_ao(accessor, cx, y, z, direction, ao);
            if (ao[0] < 1.0f || ao[1] < 1.0f || ao[2] < 1.0f || ao[3] < 1.0f) {
                has_ao = true;
                break;
            }
        }

        if (has_ao) {
            for (int32_t cx = x_start; cx < x_end; ++cx) {
                add_face(chunk, accessor, cx, y, z, direction, block_id);
            }
        } else {
            Face face;
            face.x = x_start;
            face.y = y;
            face.z = z;
            face.direction = direction;
            face.block_id = block_id;
            face.u_max = (x_end - 1) - x_start;
            face.v_max = 0;
            add_greedy_face(chunk, accessor, face, light_key, rotation);
        }
    } else {
        add_face(chunk, accessor, x_start, y, z, direction, block_id);
    }
}

void MeshBuilder::passive_greedy_mesh_horizontal(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                                   FaceDirection direction) {
    if (direction != FaceDirection::Top && direction != FaceDirection::Bottom) {
        return;
    }

    int dir_idx = static_cast<int32_t>(direction);

    for (int32_t s = 0; s < CHUNK_SECTIONS; s++) {
        if (chunk.is_section_all_air(s)) continue;
        int32_t y0 = s * SECTION_HEIGHT;
        int32_t y1 = y0 + SECTION_HEIGHT;
        for (int32_t y = y0; y < y1; y++) {
            for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
                int32_t merge_start = -1;
                BlockID current_block = BlockIDs::AIR;
                uint16_t current_light_key = 0;
                int current_rotation = 0;

                for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
                    BlockID block_id = chunk.get_block(x, y, z);

                    if (block_id == BlockIDs::AIR) {
                        flush_horizontal_merge(chunk, accessor, merge_start, x, y, z, direction,
                                               current_block, current_light_key, current_rotation);
                        merge_start = -1;
                        continue;
                    }

                    int32_t dy = kDirectionOffsets[dir_idx][1];
                    int32_t ny = y + dy;
                    BlockID neighbor = accessor.get_block(x, ny, z);
                    bool cull = should_cull_against_neighbor(chunk, block_id, neighbor, direction, x, y, z);

                    if (cull) {
                        flush_horizontal_merge(chunk, accessor, merge_start, x, y, z, direction,
                                               current_block, current_light_key, current_rotation);
                        merge_start = -1;
                        continue;
                    }

                    int rotation = get_face_rotation(block_id, x, y, z, direction, dir_idx);

                    // Compute light_key once here; reuse for both the merge test
                    // and the new-run initialisation, avoiding a second call.
                    const int32_t dx = kDirectionOffsets[dir_idx][0];
                    const int32_t dy2 = kDirectionOffsets[dir_idx][1];
                    const int32_t dz = kDirectionOffsets[dir_idx][2];
                    const uint16_t light_key = accessor.get_light_packed(x + dx, y + dy2, z + dz);

                    if (merge_start != -1 && block_id == current_block && (x - merge_start) < kMaxGreedyMergeDistance
                        && light_key == current_light_key && rotation == current_rotation) {
                        continue;
                    }

                    if (merge_start != -1) {
                        flush_horizontal_merge(chunk, accessor, merge_start, x, y, z, direction,
                                               current_block, current_light_key, current_rotation);
                    }

                    merge_start = x;
                    current_block = block_id;
                    current_rotation = rotation;
                    current_light_key = light_key;
                }

                if (merge_start != -1) {
                    flush_horizontal_merge(chunk, accessor, merge_start, CHUNK_WIDTH, y, z, direction,
                                           current_block, current_light_key, current_rotation);
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
                                          BlockID block_id, uint16_t light_key, int rotation) {
    if (y_start < 0 || y_end <= y_start) return;

    if (y_end - y_start > 1) {
        // Only check the two endpoint blocks. AO only varies at run boundaries,
        // so scanning the full interior is redundant work.
        bool has_ao = false;
        for (int32_t cy : {y_start, y_end - 1}) {
            float ao[4];
            compute_face_ao(accessor, x, cy, z, direction, ao);
            if (ao[0] < 1.0f || ao[1] < 1.0f || ao[2] < 1.0f || ao[3] < 1.0f) {
                has_ao = true;
                break;
            }
        }

        if (has_ao) {
            for (int32_t cy = y_start; cy < y_end; ++cy) {
                add_face(chunk, accessor, x, cy, z, direction, block_id);
            }
        } else {
            Face face;
            face.x = x;
            face.y = y_start;
            face.z = z;
            face.direction = direction;
            face.block_id = block_id;
            face.u_max = 0;
            face.v_max = (y_end - 1) - y_start;
            add_greedy_face(chunk, accessor, face, light_key, rotation);
        }
    } else {
        add_face(chunk, accessor, x, y_start, z, direction, block_id);
    }
}

void MeshBuilder::passive_greedy_mesh_vertical(const ChunkData& chunk, const ChunkNeighborAccessor& accessor,
                                                FaceDirection direction) {
    if (direction == FaceDirection::Top || direction == FaceDirection::Bottom) {
        return;
    }

    int dir_idx = static_cast<int32_t>(direction);

    for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
        for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
            int32_t merge_start = -1;
            BlockID current_block = BlockIDs::AIR;
            uint16_t current_light_key = 0;
            int current_rotation = 0;

            for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
                if (chunk.is_section_all_air(y / SECTION_HEIGHT)) {
                    flush_vertical_merge(chunk, accessor, merge_start, y, x, z, direction,
                                         current_block, current_light_key, current_rotation);
                    merge_start = -1;
                    y = ((y / SECTION_HEIGHT) + 1) * SECTION_HEIGHT - 1;
                    continue;
                }

                BlockID block_id = chunk.get_block_unsafe(x, y, z);

                if (block_id == BlockIDs::AIR) {
                    flush_vertical_merge(chunk, accessor, merge_start, y, x, z, direction,
                                         current_block, current_light_key, current_rotation);
                    merge_start = -1;
                    continue;
                }

                int32_t nx = x + kDirectionOffsets[dir_idx][0];
                int32_t ny = y + kDirectionOffsets[dir_idx][1];
                int32_t nz = z + kDirectionOffsets[dir_idx][2];
                BlockID neighbor = accessor.get_block(nx, ny, nz);
                bool cull = should_cull_against_neighbor(chunk, block_id, neighbor, direction, x, y, z);

                if (cull) {
                    flush_vertical_merge(chunk, accessor, merge_start, y, x, z, direction,
                                         current_block, current_light_key, current_rotation);
                    merge_start = -1;
                    continue;
                }

                int rotation = get_face_rotation(block_id, x, y, z, direction, dir_idx);

                // Compute light_key once; reuse for both the merge test and
                // new-run initialisation to eliminate the duplicate call.
                const int32_t dx = kDirectionOffsets[dir_idx][0];
                const int32_t dy = kDirectionOffsets[dir_idx][1];
                const int32_t dz = kDirectionOffsets[dir_idx][2];
                const uint16_t light_key = accessor.get_light_packed(x + dx, y + dy, z + dz);

                if (merge_start != -1 && block_id == current_block && (y - merge_start) < kMaxGreedyMergeDistance
                    && light_key == current_light_key && rotation == current_rotation) {
                    continue;
                }

                if (merge_start != -1) {
                    flush_vertical_merge(chunk, accessor, merge_start, y, x, z, direction,
                                         current_block, current_light_key, current_rotation);
                }

                merge_start = y;
                current_block = block_id;
                current_rotation = rotation;
                current_light_key = light_key;
            }

            if (merge_start != -1) {
                flush_vertical_merge(chunk, accessor, merge_start, CHUNK_HEIGHT, x, z, direction,
                                     current_block, current_light_key, current_rotation);
            }
        }
    }
}

} // namespace VoxelEngine

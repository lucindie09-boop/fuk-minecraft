#include "mesh/lod_mesh_builder.hpp"

#include "core/chunk_data.hpp"

namespace VoxelEngine {

namespace {

// Sample light from inside the macro-cell, preferring a transparent or
// air block so we get the actual propagated light rather than 0 stored in
// solid blocks.
static void sample_macro_cell_light(const ChunkMap& chunk_map,
                                    int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                                    int32_t merge_shift,
                                    int32_t downsample_step,
                                    int32_t macro_x, int32_t macro_y, int32_t macro_z,
                                    uint8_t& out_sky, uint8_t& out_light) {
    out_sky = 15;
    out_light = 15;
    const int32_t merge_size = lod_merge_size(merge_shift);
    const int32_t merged_width = CHUNK_WIDTH * merge_size;

    // Helper: read light from world at given merged-group coords.
    auto read_light = [&](int32_t wx, int32_t wy, int32_t wz,
                          uint8_t& sky, uint8_t& lit) -> bool {
        if (wx < 0 || wy < 0 || wz < 0 ||
            wx >= merged_width || wy >= merged_width || wz >= merged_width) return false;
        const int32_t cx = anchor_cx + (wx / CHUNK_WIDTH);
        const int32_t cy = anchor_cy + (wy / CHUNK_HEIGHT);
        const int32_t cz = anchor_cz + (wz / CHUNK_DEPTH);
        ChunkRenderData* rd = chunk_map.get_chunk_render_data(cx, cy, cz);
        if (!rd || !rd->data) return false;
        const int32_t lx = wx % CHUNK_WIDTH;
        const int32_t ly = wy % CHUNK_HEIGHT;
        const int32_t lz = wz % CHUNK_DEPTH;
        sky = rd->data->get_sky_light(lx, ly, lz);
        lit = rd->data->get_light(lx, ly, lz);
        return true;
    };

    // First pass: find a non-solid block (transparent/liquid) inside the cell.
    for (int32_t dy = downsample_step - 1; dy >= 0; --dy) {
        for (int32_t dz = 0; dz < downsample_step; ++dz) {
            for (int32_t dx = 0; dx < downsample_step; ++dx) {
                const int32_t wx = macro_x * downsample_step + dx;
                const int32_t wy = macro_y * downsample_step + dy;
                const int32_t wz = macro_z * downsample_step + dz;
                if (wx >= merged_width || wy >= merged_width || wz >= merged_width) continue;
                const int32_t cx = anchor_cx + (wx / CHUNK_WIDTH);
                const int32_t cy = anchor_cy + (wy / CHUNK_HEIGHT);
                const int32_t cz = anchor_cz + (wz / CHUNK_DEPTH);
                ChunkRenderData* rd = chunk_map.get_chunk_render_data(cx, cy, cz);
                if (!rd || !rd->data) continue;
                const int32_t lx = wx % CHUNK_WIDTH;
                const int32_t ly = wy % CHUNK_HEIGHT;
                const int32_t lz = wz % CHUNK_DEPTH;
                const BlockID b = rd->data->get_block(lx, ly, lz);
                if (b == BlockIDs::AIR) continue;
                const BlockType& bt = BlockRegistry::get_instance().get_block(b);
                if (HasProperty(bt.properties, BlockProperty::Solid) ||
                    HasProperty(bt.properties, BlockProperty::Opaque)) continue;
                if (read_light(wx, wy, wz, out_sky, out_light)) return;
            }
        }
    }

    // Second pass: find any non-air block and try the block above it.
    for (int32_t dy = downsample_step - 1; dy >= 0; --dy) {
        for (int32_t dz = 0; dz < downsample_step; ++dz) {
            for (int32_t dx = 0; dx < downsample_step; ++dx) {
                const int32_t wx = macro_x * downsample_step + dx;
                const int32_t wy = macro_y * downsample_step + dy;
                const int32_t wz = macro_z * downsample_step + dz;
                if (wx >= merged_width || wy >= merged_width || wz >= merged_width) continue;
                const int32_t cx = anchor_cx + (wx / CHUNK_WIDTH);
                const int32_t cy = anchor_cy + (wy / CHUNK_HEIGHT);
                const int32_t cz = anchor_cz + (wz / CHUNK_DEPTH);
                ChunkRenderData* rd = chunk_map.get_chunk_render_data(cx, cy, cz);
                if (!rd || !rd->data) continue;
                const int32_t lx = wx % CHUNK_WIDTH;
                const int32_t ly = wy % CHUNK_HEIGHT;
                const int32_t lz = wz % CHUNK_DEPTH;
                if (rd->data->get_block(lx, ly, lz) == BlockIDs::AIR) continue;
                // Read light from the block above (likely air, has correct light).
                if (read_light(wx, wy + 1, wz, out_sky, out_light)) return;
                // Above out of range — read directly from the solid block.
                if (read_light(wx, wy, wz, out_sky, out_light)) return;
            }
        }
    }
}

BlockID sample_macro_cell(const ChunkMap& chunk_map,
                          int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                          int32_t merge_shift,
                          int32_t downsample_step,
                          int32_t macro_x, int32_t macro_y, int32_t macro_z) {
    const int32_t merge_size = lod_merge_size(merge_shift);
    const int32_t merged_width = CHUNK_WIDTH * merge_size;

    BlockID terrain_block = BlockIDs::AIR;
    BlockID water_block = BlockIDs::AIR;

    for (int32_t dz = 0; dz < downsample_step; ++dz) {
        for (int32_t dy = 0; dy < downsample_step; ++dy) {
            for (int32_t dx = 0; dx < downsample_step; ++dx) {
                const int32_t wx = macro_x * downsample_step + dx;
                const int32_t wy = macro_y * downsample_step + dy;
                const int32_t wz = macro_z * downsample_step + dz;
                if (wx >= merged_width || wy >= merged_width || wz >= merged_width) {
                    continue;
                }

                const int32_t cx = anchor_cx + (wx / CHUNK_WIDTH);
                const int32_t cy = anchor_cy + (wy / CHUNK_HEIGHT);
                const int32_t cz = anchor_cz + (wz / CHUNK_DEPTH);
                const int32_t lx = wx % CHUNK_WIDTH;
                const int32_t ly = wy % CHUNK_HEIGHT;
                const int32_t lz = wz % CHUNK_DEPTH;

                ChunkRenderData* render_data = chunk_map.get_chunk_render_data(cx, cy, cz);
                if (!render_data || !render_data->data) {
                    continue;
                }
                const BlockID block = render_data->data->get_block(lx, ly, lz);
                if (block == BlockIDs::AIR) continue;
                const BlockType& bt = BlockRegistry::get_instance().get_block(block);
                if (HasProperty(bt.properties, BlockProperty::Opaque) ||
                    HasProperty(bt.properties, BlockProperty::Solid)) {
                    if (terrain_block == BlockIDs::AIR) {
                        terrain_block = block;
                    }
                } else if (HasProperty(bt.properties, BlockProperty::Liquid) &&
                           water_block == BlockIDs::AIR) {
                    water_block = block;
                }
            }
        }
    }

    if (terrain_block != BlockIDs::AIR) return terrain_block;
    if (water_block != BlockIDs::AIR) return water_block;
    return BlockIDs::AIR;
}

void fill_downsampled_chunk(ChunkData& chunk,
                            const ChunkMap& chunk_map,
                            int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                            int32_t merge_shift,
                            int32_t downsample_step) {
    const int32_t merge_size = lod_merge_size(merge_shift);
    const int32_t merged_width = CHUNK_WIDTH * merge_size;
    const int32_t macro_width = merged_width / downsample_step;
    for (int32_t z = 0; z < macro_width; ++z) {
        for (int32_t y = 0; y < macro_width; ++y) {
            for (int32_t x = 0; x < macro_width; ++x) {
                const BlockID block = sample_macro_cell(
                    chunk_map, anchor_cx, anchor_cy, anchor_cz, merge_shift, downsample_step, x, y, z);
                chunk.set_block(x, y, z, block);
                if (block != BlockIDs::AIR) {
                    uint8_t sky = 15, light = 15;
                    sample_macro_cell_light(chunk_map, anchor_cx, anchor_cy, anchor_cz,
                                            merge_shift, downsample_step, x, y, z, sky, light);
                    chunk.set_sky_light(x, y, z, sky);
                    chunk.set_light(x, y, z, light);
                }
            }
        }
    }
    chunk.compute_fully_solid();
}

// Build a face-neighbor ChunkData for one side of the group.
// Fills only the 32x32 slice that the accessor reads.
void fill_face_neighbor(ChunkData& chunk,
                        const ChunkMap& chunk_map,
                        int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                        int32_t merge_shift,
                        int32_t downsample_step,
                        int32_t adj_anchor_cx, int32_t adj_anchor_cy, int32_t adj_anchor_cz,
                        int32_t neighbor_local_x, int32_t neighbor_local_y, int32_t neighbor_local_z) {
    const int32_t macro_width = 32;
    // Determine which axis is fixed (the slice we're filling).
    // Only one of neighbor_local_{x,y,z} is in-range for the slice.
    const bool fill_x = (neighbor_local_x >= 0 && neighbor_local_x < macro_width);
    const bool fill_y = (neighbor_local_y >= 0 && neighbor_local_y < macro_width);
    const bool fill_z = (neighbor_local_z >= 0 && neighbor_local_z < macro_width);

    if (fill_x) {
        const int32_t mx = neighbor_local_x;
        for (int32_t bz = 0; bz < 32; ++bz) {
            for (int32_t by = 0; by < 32; ++by) {
                const BlockID block = sample_macro_cell(
                    chunk_map, adj_anchor_cx, adj_anchor_cy, adj_anchor_cz,
                    merge_shift, downsample_step, mx, by, bz);
                if (block != BlockIDs::AIR) {
                    chunk.set_block(mx, by, bz, block);
                    uint8_t sky = 15, light = 15;
                    sample_macro_cell_light(chunk_map, adj_anchor_cx, adj_anchor_cy, adj_anchor_cz,
                                            merge_shift, downsample_step, mx, by, bz, sky, light);
                    chunk.set_sky_light(mx, by, bz, sky);
                    chunk.set_light(mx, by, bz, light);
                }
            }
        }
    } else if (fill_y) {
        const int32_t my = neighbor_local_y;
        for (int32_t bz = 0; bz < 32; ++bz) {
            for (int32_t bx = 0; bx < 32; ++bx) {
                const BlockID block = sample_macro_cell(
                    chunk_map, adj_anchor_cx, adj_anchor_cy, adj_anchor_cz,
                    merge_shift, downsample_step, bx, my, bz);
                if (block != BlockIDs::AIR) {
                    chunk.set_block(bx, my, bz, block);
                    uint8_t sky = 15, light = 15;
                    sample_macro_cell_light(chunk_map, adj_anchor_cx, adj_anchor_cy, adj_anchor_cz,
                                            merge_shift, downsample_step, bx, my, bz, sky, light);
                    chunk.set_sky_light(bx, my, bz, sky);
                    chunk.set_light(bx, my, bz, light);
                }
            }
        }
    } else if (fill_z) {
        const int32_t mz = neighbor_local_z;
        for (int32_t by = 0; by < 32; ++by) {
            for (int32_t bx = 0; bx < 32; ++bx) {
                const BlockID block = sample_macro_cell(
                    chunk_map, adj_anchor_cx, adj_anchor_cy, adj_anchor_cz,
                    merge_shift, downsample_step, bx, by, mz);
                if (block != BlockIDs::AIR) {
                    chunk.set_block(bx, by, mz, block);
                    uint8_t sky = 15, light = 15;
                    sample_macro_cell_light(chunk_map, adj_anchor_cx, adj_anchor_cy, adj_anchor_cz,
                                            merge_shift, downsample_step, bx, by, mz, sky, light);
                    chunk.set_sky_light(bx, by, mz, sky);
                    chunk.set_light(bx, by, mz, light);
                }
            }
        }
    }
}

} // namespace

BuiltMeshData LodMeshBuilder::build_downsampled(const ChunkMap& chunk_map,
                                                int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                                                int32_t merge_shift,
                                                int32_t downsample_step,
                                                MeshBuilder& builder) {
    thread_local ChunkData macro_chunk;

    // 6 face-neighbor macro-chunks for boundary culling.
    // Each stores the macro-cell data for the adjacent 4x4x4 group on one face.
    thread_local ChunkData face_neighbors[6];

    macro_chunk.clear();
    fill_downsampled_chunk(macro_chunk, chunk_map, anchor_cx, anchor_cy, anchor_cz, merge_shift, downsample_step);

    const int32_t merge_size = lod_merge_size(merge_shift);

    // Build face neighbors: for each orthogonal face, build a macro-chunk slice
    // from the adjacent 4x4x4 group volume so the mesher can cull boundary faces
    // against matching solid macro-cells.
    static constexpr int kFaceCount = 6;
    static constexpr int kFaceOffsets[kFaceCount][3] = {
        {-1, 0, 0},  // neg_x
        { 1, 0, 0},  // pos_x
        { 0,-1, 0},  // neg_y
        { 0, 1, 0},  // pos_y
        { 0, 0,-1},  // neg_z
        { 0, 0, 1},  // pos_z
    };

    // Local coords of the slice in the neighbor ChunkData that the accessor reads:
    // neg_x: (31, y, z)   pos_x: (0, y, z)
    // neg_y: (x, 31, z)   pos_y: (x, 0, z)
    // neg_z: (x, y, 31)   pos_z: (x, y, 0)
    static constexpr int kNeighborLocal[kFaceCount][3] = {
        {31, -1, -1},  // neg_x: x=31 fixed
        { 0, -1, -1},  // pos_x: x=0 fixed
        {-1, 31, -1},  // neg_y: y=31 fixed
        {-1,  0, -1},  // pos_y: y=0 fixed
        {-1, -1, 31},  // neg_z: z=31 fixed
        {-1, -1,  0},  // pos_z: z=0 fixed
    };

    const ChunkData* neighbor_ptrs[26] = {};

    for (int fi = 0; fi < kFaceCount; ++fi) {
        const int32_t adj_ax = anchor_cx + kFaceOffsets[fi][0] * merge_size;
        const int32_t adj_ay = anchor_cy + kFaceOffsets[fi][1] * merge_size;
        const int32_t adj_az = anchor_cz + kFaceOffsets[fi][2] * merge_size;

        // Check if the face group is within the chunk map extents by seeing if at
        // least one non-air chunk exists in the adjacent 4x4x4 volume.
        bool has_data = false;
        for (int32_t oz = 0; oz < merge_size && !has_data; ++oz) {
            for (int32_t oy = 0; oy < merge_size && !has_data; ++oy) {
                for (int32_t ox = 0; ox < merge_size && !has_data; ++ox) {
                    const int32_t cx = adj_ax + ox;
                    const int32_t cy = adj_ay + oy;
                    const int32_t cz = adj_az + oz;
                    ChunkRenderData* rd = chunk_map.get_chunk_render_data(cx, cy, cz);
                    if (rd && rd->data && !rd->data->is_all_air()) {
                        has_data = true;
                    }
                }
            }
        }

        if (has_data) {
            face_neighbors[fi].clear();
            fill_face_neighbor(face_neighbors[fi], chunk_map,
                               anchor_cx, anchor_cy, anchor_cz,
                               merge_shift, downsample_step,
                               adj_ax, adj_ay, adj_az,
                               kNeighborLocal[fi][0], kNeighborLocal[fi][1], kNeighborLocal[fi][2]);
            neighbor_ptrs[fi] = &face_neighbors[fi];
        } else {
            neighbor_ptrs[fi] = nullptr;
        }
    }

    builder.set_smooth_lighting(false);
    builder.set_greedy_enabled(true);
    builder.clear();
    builder.build_mesh(macro_chunk,
                       neighbor_ptrs[0], neighbor_ptrs[1],  // neg_x, pos_x
                       neighbor_ptrs[2], neighbor_ptrs[3],  // neg_y, pos_y
                       neighbor_ptrs[4], neighbor_ptrs[5],  // neg_z, pos_z
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr);

    BuiltMeshData result;
    result.vertices = builder.get_vertices();
    result.indices = builder.get_indices();
    result.water_vertices = builder.get_water_vertices();
    result.water_indices = builder.get_water_indices();

    const float scale = static_cast<float>(downsample_step);
    for (Vertex& v : result.vertices) {
        v.x *= scale;
        v.y *= scale;
        v.z *= scale;
    }
    for (Vertex& v : result.water_vertices) {
        v.x *= scale;
        v.y *= scale;
        v.z *= scale;
    }

    result.empty = (result.vertices.empty() || result.indices.empty()) &&
                   (result.water_vertices.empty() || result.water_indices.empty());
    return result;
}

} // namespace VoxelEngine
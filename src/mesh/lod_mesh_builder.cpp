#include "mesh/lod_mesh_builder.hpp"

#include "core/chunk_data.hpp"

namespace VoxelEngine {

namespace {

BlockID sample_macro_cell(const ChunkMap& chunk_map,
                          int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                          int32_t merge_shift,
                          int32_t downsample_step,
                          int32_t macro_x, int32_t macro_y, int32_t macro_z) {
    const int32_t merge_size = lod_merge_size(merge_shift);
    const int32_t merged_width = CHUNK_WIDTH * merge_size;

    BlockID chosen = BlockIDs::AIR;
    int32_t solid_count = 0;

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
                if (block == BlockIDs::AIR) {
                    continue;
                }
                ++solid_count;
                if (chosen == BlockIDs::AIR) {
                    chosen = block;
                }
            }
        }
    }

    return solid_count > 0 ? chosen : BlockIDs::AIR;
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
                    chunk.set_sky_light(x, y, z, 15);
                    chunk.set_light(x, y, z, 15);
                }
            }
        }
    }
    chunk.compute_fully_solid();
}

} // namespace

BuiltMeshData LodMeshBuilder::build_downsampled(const ChunkMap& chunk_map,
                                                int32_t anchor_cx, int32_t anchor_cy, int32_t anchor_cz,
                                                int32_t merge_shift,
                                                int32_t downsample_step,
                                                MeshBuilder& builder) {
    thread_local ChunkData macro_chunk;

    macro_chunk.clear();
    fill_downsampled_chunk(macro_chunk, chunk_map, anchor_cx, anchor_cy, anchor_cz, merge_shift, downsample_step);

    builder.set_smooth_lighting(false);
    builder.set_greedy_enabled(true);
    builder.clear();
    builder.build_mesh(macro_chunk, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                       nullptr, nullptr, nullptr, nullptr);

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

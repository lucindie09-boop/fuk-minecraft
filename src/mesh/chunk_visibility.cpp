#include "mesh/chunk_visibility.hpp"
#include "core/block_types.hpp"

namespace VoxelEngine {

ChunkVisibilityResult ChunkVisibility::evaluate(
    ChunkRenderData* chunk_render_data,
    ChunkRenderData* neighbours[6],
    ChunkMap* chunk_map,
    int32_t chunk_x, int32_t chunk_y, int32_t chunk_z
) {
    ChunkVisibilityResult result;
    if (!chunk_render_data || !chunk_render_data->data || chunk_render_data->data->is_all_air()) {
        return result;
    }

    bool is_occluded = true;
    constexpr int W = CHUNK_WIDTH, H = CHUNK_HEIGHT, D = CHUNK_DEPTH;

    // X- face (neighbours[0] = cx-1)
    if (is_occluded) {
        auto* n = neighbours[0];
        if (n && n->data) {
            if (!n->data->fully_solid() && !n->data->is_all_air()) {
                const BlockID* b = n->data->blocks();
                is_occluded = true;
                for (int y = 0; y < H && is_occluded; ++y)
                    for (int z = 0; z < D; ++z)
                        if (b[(W - 1) + y * W + z * W * H] == BlockIDs::AIR) { is_occluded = false; break; }
            }
        } else {
            is_occluded = false;
        }
    }

    // Z+ face (neighbours[5] = cz+1)
    if (is_occluded) {
        auto* n = neighbours[5];
        if (n && n->data) {
            if (!n->data->fully_solid() && !n->data->is_all_air()) {
                const BlockID* b = n->data->blocks();
                is_occluded = true;
                for (int x = 0; x < W && is_occluded; ++x)
                    for (int y = 0; y < H; ++y)
                        if (b[x + y * W + (D - 1) * W * H] == BlockIDs::AIR) { is_occluded = false; break; }
            }
        } else {
            is_occluded = false;
        }
    }

    ChunkRenderData* neighbor_below = neighbours[2]; // Y- (cy-1)
    bool has_solid_below = neighbor_below && neighbor_below->data && !neighbor_below->data->is_all_air();

    if (is_occluded) {
        result.is_visible = false;
        result.should_dirty_below = has_solid_below;
        return result;
    }

    result.is_visible = true;
    result.should_dirty_below = has_solid_below;
    return result;
}

} // namespace VoxelEngine

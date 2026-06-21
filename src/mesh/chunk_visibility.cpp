#include "mesh/chunk_visibility.hpp"

namespace VoxelEngine {

ChunkVisibilityResult ChunkVisibility::evaluate(
    ChunkRenderData* chunk_render_data,
    ChunkRenderData* neighbor_above,
    ChunkRenderData* neighbor_below,
    ChunkMap* chunk_map,
    int32_t chunk_x, int32_t chunk_y, int32_t chunk_z
) {
    ChunkVisibilityResult result;
    if (!chunk_render_data || !chunk_render_data->data || chunk_render_data->data->is_all_air()) {
        return result;
    }
    bool is_visible = true;
    if (neighbor_above && neighbor_above->data && !neighbor_above->data->is_all_air()) {
        is_visible = false;
    }
    // If the chunk immediately above is not loaded, we conservatively assume
    // this chunk is visible. This avoids a 128-chunk shared_lock scan on
    // every mesh rebuild.
    if (!is_visible) {
        result.is_visible = false;
        if (neighbor_below && neighbor_below->data && !neighbor_below->data->is_all_air()) {
            result.should_dirty_below = true;
        }
        return result;
    }
    result.is_visible = true;
    if (neighbor_below && neighbor_below->data && !neighbor_below->data->is_all_air()) {
        result.should_dirty_below = true;
    }
    return result;
}

} // namespace VoxelEngine

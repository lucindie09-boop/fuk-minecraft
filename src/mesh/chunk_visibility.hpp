#ifndef FUK_MINECRAFT_CHUNK_VISIBILITY_HPP
#define FUK_MINECRAFT_CHUNK_VISIBILITY_HPP

#include "core/chunk_types.hpp"
#include "core/chunk_map.hpp"

namespace VoxelEngine {

struct ChunkVisibilityResult {
    bool is_visible = false;
    bool should_dirty_below = false;
};

class ChunkVisibility {
public:
    static ChunkVisibilityResult evaluate(
        ChunkRenderData* chunk_render_data,
        ChunkRenderData* neighbor_above,
        ChunkRenderData* neighbor_below,
        ChunkMap* chunk_map,
        int32_t chunk_x, int32_t chunk_y, int32_t chunk_z
    );
};

} // namespace VoxelEngine
#endif

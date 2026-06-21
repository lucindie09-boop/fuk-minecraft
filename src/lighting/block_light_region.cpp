#include "lighting/block_light_region.hpp"
#include "core/chunk_data.hpp"

namespace VoxelEngine {

void propagate_chunk_block_light_additive(ChunkData& chunk) {
    std::vector<EmissiveSource> sources;
    const BlockRegistry& registry = BlockRegistry::get_instance();

    if (chunk.get_emissive_count() == 0) {
        return;
    }

    for (int32_t s = 0; s < CHUNK_SECTIONS; ++s) {
        if (chunk.is_section_all_air(s)) {
            continue;
        }
        const int32_t y0 = s * SECTION_HEIGHT;
        const int32_t y1 = y0 + SECTION_HEIGHT;
        for (int32_t y = y0; y < y1; ++y) {
            for (int32_t z = 0; z < CHUNK_DEPTH; ++z) {
                for (int32_t x = 0; x < CHUNK_WIDTH; ++x) {
                    const BlockID block_id = chunk.get_block_unsafe(x, y, z);
                    if (block_id == BlockIDs::AIR) {
                        continue;
                    }
                    const BlockType& type = registry.get_block(block_id);
                    if (!HasProperty(type.properties, BlockProperty::Emissive)) {
                        continue;
                    }
                    const uint8_t lr = type.light_r;
                    const uint8_t lg = type.light_g;
                    const uint8_t lb = type.light_b;
                    if (lr == 0 && lg == 0 && lb == 0) {
                        continue;
                    }
                    sources.push_back({
                        0, 0, 0,
                        static_cast<int16_t>(x),
                        static_cast<int16_t>(y),
                        static_cast<int16_t>(z),
                        lr, lg, lb,
                        type.light_pattern
                    });
                }
            }
        }
    }

    ChunkData* region_grid[3][3][3] = {};
    region_grid[1][1][1] = &chunk;
    BlockLightRegion light_region(region_grid);
    light_region.propagate_additive(sources);
}

} // namespace VoxelEngine

#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include "lighting/block_light_region.hpp"
#include <cstdint>
#include <cstddef>

using namespace VoxelEngine;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) return 0;

    BlockRegistry::get_instance().initialize_default_blocks();

    // 3x3x3 grid of chunks for cross-boundary propagation testing
    ChunkData region[3][3][3];
    for (int dz = 0; dz < 3; dz++)
        for (int dy = 0; dy < 3; dy++)
            for (int dx = 0; dx < 3; dx++)
                region[dz][dy][dx].clear();

    size_t pos = 0;
    // Each operation: [chunk_idx:1][x:1][y:1][z:1][block_id:1]
    // chunk_idx selects one of 27 chunks (0-26), mapped to rdx/rdy/rdz
    while (pos + 4 <= size) {
        uint8_t chunk_idx = data[pos++] % 27;
        uint16_t x = data[pos++] % CHUNK_WIDTH;
        uint16_t y = data[pos++] % CHUNK_HEIGHT;
        uint16_t z = data[pos++] % CHUNK_DEPTH;

        if (pos >= size) break;

        int rdx = (chunk_idx % 3) - 1;
        int rdy = ((chunk_idx / 3) % 3) - 1;
        int rdz = (chunk_idx / 9) - 1;
        BlockID id = static_cast<BlockID>(data[pos++] % 21);
        region[rdz + 1][rdy + 1][rdx + 1].set_block(x, y, z, id);
    }

    ChunkData* grid[3][3][3];
    for (int dz = 0; dz < 3; dz++)
        for (int dy = 0; dy < 3; dy++)
            for (int dx = 0; dx < 3; dx++)
                grid[dz][dy][dx] = &region[dz][dy][dx];

    BlockLightRegion light_region(grid);
    std::vector<EmissiveSource> sources;
    light_region.collect_emissive_sources(sources);
    light_region.propagate_additive(sources);

    // Verify light values are in valid range (0-15) across all chunks
    for (int dz = 0; dz < 3; dz++) {
        for (int dy = 0; dy < 3; dy++) {
            for (int dx = 0; dx < 3; dx++) {
                for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
                    for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
                        for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
                            uint8_t r = region[dz][dy][dx].get_light_r(x, y, z);
                            uint8_t g = region[dz][dy][dx].get_light_g(x, y, z);
                            uint8_t b = region[dz][dy][dx].get_light_b(x, y, z);
                            if (r > 15 || g > 15 || b > 15) {
                                abort();
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}

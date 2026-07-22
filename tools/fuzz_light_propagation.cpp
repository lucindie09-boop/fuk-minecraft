#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include "lighting/block_light_region.hpp"
#include <cstdint>
#include <cstddef>

using namespace VoxelEngine;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) return 0;

    BlockRegistry::get_instance().initialize_default_blocks();

    ChunkData chunk;
    chunk.clear();

    size_t pos = 0;
    // Use fuzzer input to place blocks in the chunk
    // Each operation: [op:1][x:1][y:1][z:1][block_id:1]
    while (pos + 4 <= size) {
        uint8_t op = data[pos++];
        uint16_t x = data[pos++] % CHUNK_WIDTH;
        uint16_t y = data[pos++] % CHUNK_HEIGHT;
        uint16_t z = data[pos++] % CHUNK_DEPTH;

        if (pos >= size) break;

        // Use fuzzer byte to select block type (limit to valid range)
        BlockID id = static_cast<BlockID>(data[pos++] % 21);
        chunk.set_block(x, y, z, id);
    }

    // Call the light propagation function - this should not crash
    // even with arbitrary block layouts
    propagate_chunk_block_light_additive(chunk);

    // Verify light values are in valid range (0-15)
    for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
        for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
            for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
                uint8_t r = chunk.get_light_r(x, y, z);
                uint8_t g = chunk.get_light_g(x, y, z);
                uint8_t b = chunk.get_light_b(x, y, z);
                // Light values should never exceed 15
                if (r > 15 || g > 15 || b > 15) {
                    // Invalid light value - this is a bug
                    abort();
                }
            }
        }
    }

    return 0;
}

#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include <cstdint>
#include <cstddef>

using namespace VoxelEngine;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 4) return 0;

    ChunkData chunk;
    chunk.clear();

    size_t pos = 0;
    while (pos + 4 <= size) {
        uint8_t op = data[pos++];
        uint16_t x = data[pos++] % CHUNK_WIDTH;
        uint16_t y = data[pos++] % CHUNK_HEIGHT;
        uint16_t z = data[pos++] % CHUNK_DEPTH;

        if (pos >= size) break;

        if (op & 1) {
            BlockID id = static_cast<BlockID>(data[pos++] % 21);
            chunk.set_block(x, y, z, id);
        } else {
            (void)chunk.get_block(x, y, z);
        }
    }

    return 0;
}

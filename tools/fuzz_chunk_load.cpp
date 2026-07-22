#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include "core/crc32.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>

using namespace VoxelEngine;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 20) return 0;

    BlockRegistry::get_instance().initialize_default_blocks();

    uint32_t w, h, d, version;
    std::memcpy(&w, data, 4);
    std::memcpy(&h, data + 4, 4);
    std::memcpy(&d, data + 8, 4);
    std::memcpy(&version, data + 12, 4);

    if (w > 64 || h > 128 || d > 64) return 0;
    if (version > 3) return 0;

    ChunkData chunk;
    chunk.clear();

    if (version == 3 && size >= 20) {
        uint32_t stored_crc;
        std::memcpy(&stored_crc, data + 16, 4);
        const uint8_t* body = data + 20;
        size_t body_len = size - 20;
        (void)stored_crc;
        (void)body;
        (void)body_len;
    }

    return 0;
}

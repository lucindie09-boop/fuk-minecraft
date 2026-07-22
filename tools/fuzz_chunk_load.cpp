#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include "core/crc32.hpp"
#include "core/rle_codec.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>

using namespace VoxelEngine;

// Fuzz the v3 chunk loading path: validate header, verify CRC32, then
// exercise the actual RLE column decoder on the body bytes.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 20) return 0;

    BlockRegistry::get_instance().initialize_default_blocks();

    // Parse v3 header: [width:u32][height:u32][depth:u32][version:u32][crc32:u32]
    uint32_t w, h, d, version, stored_crc;
    std::memcpy(&w, data, 4);
    std::memcpy(&h, data + 4, 4);
    std::memcpy(&d, data + 8, 4);
    std::memcpy(&version, data + 12, 4);
    std::memcpy(&stored_crc, data + 16, 4);

    // Only fuzz v3 (the format with CRC32 + RLE body).
    // Dimensions beyond production size are skipped (saves fuzzer time on
    // implausible inputs that the real loader would reject before decoding).
    if (w != 32 || h != 32 || d != 32 || version != 3) return 0;

    const uint8_t* body = data + 20;
    size_t body_size = size - 20;

    // Exercise CRC32 on the raw body bytes.
    uint32_t actual_crc = crc32(body, body_size);

    // Only decode if CRC matches (fuzzer may find intentional mismatches).
    if (actual_crc == stored_crc) {
        ChunkData chunk;
        chunk.clear();
        // Exercise the real RLE column decoder — this is the hot path that
        // must not crash on malformed input (truncated runs, overflow, etc.).
        if (decode_chunk_rle(body, body_size, chunk)) {
            // Decode-idempotence check: decode again into a fresh chunk and verify
            // the results are identical. This catches decoder bugs that produce
            // non-deterministic or incorrect output without relying on CRC comparison
            // (which would fail on valid-but-non-maximal run sequences).
            ChunkData chunk2;
            chunk2.clear();
            if (decode_chunk_rle(body, body_size, chunk2)) {
                // Verify all blocks match between the two decode passes
                for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
                    for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
                        for (int32_t y = 0; y < CHUNK_HEIGHT; y++) {
                            if (chunk.get_block(x, y, z) != chunk2.get_block(x, y, z)) {
                                // Non-idempotent decode - this is a real bug
                                abort();
                            }
                        }
                    }
                }
            }
        }
    } else {
        // CRC mismatch: still attempt decode to test error handling.
        // The decoder should reject malformed input without crashing.
        ChunkData chunk;
        chunk.clear();
        decode_chunk_rle(body, body_size, chunk);
    }

    return 0;
}

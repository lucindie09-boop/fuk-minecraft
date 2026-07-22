#ifndef FUK_MINECRAFT_RLE_CODEC_HPP
#define FUK_MINECRAFT_RLE_CODEC_HPP
#include "core/chunk_coords.hpp"
#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>

namespace VoxelEngine {

// RLE column wire format (per X,Z column, vertical runs):
//   [num_runs:u16] [run0: start_y:u16 | length:u16 | block_id:u16] [run1: ...] ...
//
// These functions operate on raw byte buffers with no Godot dependency.
// They are used by save/load_chunk_to_disk and by fuzz targets.

struct RleRun {
    uint16_t start_y;
    uint16_t length;
    uint16_t block_id;
};

// Encode a single column (fixed X, Z) into the RLE byte buffer.
// Appends [num_runs:u16] + run data to `out`.
inline void encode_rle_column(const ChunkData& chunk, int32_t x, int32_t z,
                               std::vector<uint8_t>& out) {
    std::vector<RleRun> runs;
    runs.reserve(32);

    uint16_t current_id = static_cast<uint16_t>(chunk.get_block_unsafe(x, 0, z));
    uint16_t run_start = 0;
    uint16_t run_len = 1;

    for (int32_t y = 1; y < CHUNK_HEIGHT; y++) {
        uint16_t block_id = static_cast<uint16_t>(chunk.get_block_unsafe(x, y, z));
        if (block_id == current_id && run_len < 65535) {
            run_len++;
        } else {
            runs.push_back({run_start, run_len, current_id});
            current_id = block_id;
            run_start = static_cast<uint16_t>(y);
            run_len = 1;
        }
    }
    runs.push_back({run_start, run_len, current_id});

    uint16_t num_runs = static_cast<uint16_t>(runs.size());
    out.push_back(num_runs & 0xFF);
    out.push_back((num_runs >> 8) & 0xFF);
    for (const auto& run : runs) {
        out.push_back(run.start_y & 0xFF);
        out.push_back((run.start_y >> 8) & 0xFF);
        out.push_back(run.length & 0xFF);
        out.push_back((run.length >> 8) & 0xFF);
        out.push_back(run.block_id & 0xFF);
        out.push_back((run.block_id >> 8) & 0xFF);
    }
}

// Encode the full chunk into an RLE body buffer.
inline void encode_chunk_rle(const ChunkData& chunk, std::vector<uint8_t>& out) {
    out.clear();
    out.reserve(CHUNK_WIDTH * CHUNK_DEPTH * 6);
    for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
        for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
            encode_rle_column(chunk, x, z, out);
        }
    }
}

// Decode a single column from the RLE body buffer.
// Returns true on success, false if the buffer is truncated or malformed.
inline bool decode_rle_column(const uint8_t* body, size_t body_size, size_t& pos,
                               ChunkData& out, int32_t x, int32_t z) {
    if (pos + 2 > body_size) return false;
    uint16_t num_runs = body[pos] | (body[pos + 1] << 8);
    pos += 2;

    for (uint16_t r = 0; r < num_runs; r++) {
        if (pos + 6 > body_size) return false;
        uint16_t start_y  = body[pos]     | (body[pos + 1] << 8);
        uint16_t length   = body[pos + 2] | (body[pos + 3] << 8);
        uint16_t block_id = body[pos + 4] | (body[pos + 5] << 8);
        pos += 6;

        // Reject out-of-range block IDs (malformed input)
        if (block_id >= BlockRegistry::MAX_BLOCK_TYPES) return false;

        for (uint16_t y = 0; y < length; y++) {
            int32_t wy = static_cast<int32_t>(start_y) + y;
            if (wy < CHUNK_HEIGHT) {
                out.set_block(x, wy, z, static_cast<BlockID>(block_id));
            }
        }
    }
    return true;
}

// Decode a full chunk from an RLE body buffer.
// Returns true on success, false if the buffer is truncated or malformed.
inline bool decode_chunk_rle(const uint8_t* body, size_t body_size, ChunkData& out) {
    size_t pos = 0;
    for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
        for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
            if (!decode_rle_column(body, body_size, pos, out, x, z)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace VoxelEngine
#endif // FUK_MINECRAFT_RLE_CODEC_HPP

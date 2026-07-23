#ifndef FUK_MINECRAFT_CHUNK_PERSISTENCE_HPP
#define FUK_MINECRAFT_CHUNK_PERSISTENCE_HPP
#include "core/chunk_data.hpp"
#include "core/chunk_coords.hpp"
#include "core/crc32.hpp"
#include "core/rle_codec.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>

namespace VoxelEngine {

// Result of decode_v3_chunk — tells the caller what filesystem
// actions to take after a load attempt.
enum class ChunkLoadOutcome : uint8_t {
    LOADED_OK,            // Primary decoded successfully
    RECOVERED_FROM_BACKUP, // Primary corrupted, backup decoded — caller should rename backup -> primary
    BOTH_CORRUPTED,       // Both primary and backup failed — caller should delete the primary
};

// Pure decode + recovery logic for v3 chunk files.
// Separates the bug-prone codec/crc logic from Godot FileAccess orchestration.
//
// Parameters:
//   primary_body / primary_crc  — raw RLE body + CRC from the primary file
//   backup_body / backup_crc    — raw RLE body + CRC from the backup file (empty if none)
//   out                         — destination ChunkData (cleared on entry)
//
// Returns the outcome and fills `out` on success.
inline ChunkLoadOutcome decode_v3_chunk(
    const uint8_t* primary_body, size_t primary_size, uint32_t primary_crc,
    const uint8_t* backup_body,  size_t backup_size,  uint32_t backup_crc,
    ChunkData& out)
{
    // Try primary
    uint32_t actual_crc = crc32(primary_body, primary_size);
    if (actual_crc == primary_crc &&
        decode_chunk_rle(primary_body, primary_size, out)) {
        out.compute_fully_solid();
        return ChunkLoadOutcome::LOADED_OK;
    }

    // Primary failed — try backup
    if (backup_body != nullptr && backup_size > 0) {
        uint32_t backup_actual_crc = crc32(backup_body, backup_size);
        if (backup_actual_crc == backup_crc &&
            decode_chunk_rle(backup_body, backup_size, out)) {
            out.compute_fully_solid();
            return ChunkLoadOutcome::RECOVERED_FROM_BACKUP;
        }
    }

    return ChunkLoadOutcome::BOTH_CORRUPTED;
}

} // namespace VoxelEngine
#endif // FUK_MINECRAFT_CHUNK_PERSISTENCE_HPP

#include "mesh/chunk_neighbor_accessor.hpp"
#include "core/block_types.hpp"

namespace VoxelEngine {

// Resolves coordinates where y is in-bounds but x/z may be out-of-bounds.
// This is the existing logic for the `src == center` path, extracted as a helper.
static BlockID resolve_horizontal(const ChunkNeighborAccessor& acc,
                                  const ChunkData* src, int32_t x, int32_t y, int32_t z) {
    if (x < 0 || x >= CHUNK_WIDTH || z < 0 || z >= CHUNK_DEPTH) {
        const bool x_out = (x < 0 || x >= CHUNK_WIDTH);
        const bool z_out = (z < 0 || z >= CHUNK_DEPTH);
        if (x_out && z_out) {
            const ChunkData* corner = nullptr;
            int32_t cx, cz;
            if (x < 0 && z < 0) { corner = acc.neg_x_neg_z; cx = CHUNK_WIDTH - 1; cz = CHUNK_DEPTH - 1; }
            else if (x < 0 && z >= CHUNK_DEPTH) { corner = acc.neg_x_pos_z; cx = CHUNK_WIDTH - 1; cz = 0; }
            else if (x >= CHUNK_WIDTH && z < 0) { corner = acc.pos_x_neg_z; cx = 0; cz = CHUNK_DEPTH - 1; }
            else { corner = acc.pos_x_pos_z; cx = 0; cz = 0; }
            if (!corner) return BlockIDs::AIR;
            return corner->get_block(cx, y, cz);
        }
        const ChunkData* neighbor_chunk = nullptr;
        int32_t neighbor_x = x;
        int32_t neighbor_z = z;
        if (x < 0) {
            neighbor_chunk = acc.neg_x;
            neighbor_x = CHUNK_WIDTH - 1;
        } else if (x >= CHUNK_WIDTH) {
            neighbor_chunk = acc.pos_x;
            neighbor_x = 0;
        }
        if (z < 0) {
            neighbor_chunk = acc.neg_z;
            neighbor_z = CHUNK_DEPTH - 1;
        } else if (z >= CHUNK_DEPTH) {
            neighbor_chunk = acc.pos_z;
            neighbor_z = 0;
        }
        if (!neighbor_chunk) return BlockIDs::AIR;
        return neighbor_chunk->get_block(neighbor_x, y, neighbor_z);
    }
    return src->get_block(x, y, z);
}

BlockID ChunkNeighborAccessor::get_block(int32_t x, int32_t y, int32_t z) const {
    const ChunkData* src = center;
    int32_t sx = x;
    int32_t sy = y;
    int32_t sz = z;

    int dy = 0;
    if (y < 0) {
        src = neg_y;
        if (!src) return BlockIDs::AIR;
        sy = y + CHUNK_HEIGHT;
        dy = -1;
    } else if (y >= CHUNK_HEIGHT) {
        src = pos_y;
        if (!src) return BlockIDs::AIR;
        sy = y - CHUNK_HEIGHT;
        dy = 1;
    }

    if (x < 0 || x >= CHUNK_WIDTH || z < 0 || z >= CHUNK_DEPTH) {
        if (dy != 0) {
            // Both y and x/z are out of bounds — need a diagonal/corner chunk.
            int dx = 0;
            int dz = 0;
            if (x < 0) dx = -1;
            else if (x >= CHUNK_WIDTH) dx = 1;
            if (z < 0) dz = -1;
            else if (z >= CHUNK_DEPTH) dz = 1;

            const ChunkData* diag = lookup_diagonal(dx, dy, dz);
            if (!diag) return BlockIDs::AIR;

            int32_t lx = (x < 0) ? (CHUNK_WIDTH - 1) : (x >= CHUNK_WIDTH ? 0 : x);
            int32_t lz = (z < 0) ? (CHUNK_DEPTH - 1) : (z >= CHUNK_DEPTH ? 0 : z);
            return diag->get_block(lx, sy, lz);
        }
        // y in bounds, x/z out — use existing horizontal/corner resolution
        return resolve_horizontal(*this, src, x, sy, z);
    }

    return src->get_block(sx, sy, sz);
}

uint16_t ChunkNeighborAccessor::get_light_packed(int32_t x, int32_t y, int32_t z) const {
    const ChunkData* src = center;
    int32_t sx = x;
    int32_t sy = y;
    int32_t sz = z;

    int dy = 0;
    if (y < 0) {
        src = neg_y;
        if (!src) return 0;
        sy = y + CHUNK_HEIGHT;
        dy = -1;
    } else if (y >= CHUNK_HEIGHT) {
        src = pos_y;
        if (!src) return 15;
        sy = y - CHUNK_HEIGHT;
        dy = 1;
    }

    const bool x_in = (x >= 0 && x < CHUNK_WIDTH);
    const bool z_in = (z >= 0 && z < CHUNK_DEPTH);

    if (x_in && z_in) {
        return src->get_light_packed_word_unsafe(sx, sy, sz);
    }

    if (dy != 0) {
        // y out AND x/z out — need diagonal/corner chunk
        int dx = 0;
        int dz = 0;
        if (x < 0) dx = -1;
        else if (x >= CHUNK_WIDTH) dx = 1;
        if (z < 0) dz = -1;
        else if (z >= CHUNK_DEPTH) dz = 1;

        const ChunkData* diag = lookup_diagonal(dx, dy, dz);
        if (!diag) return 0;

        int32_t lx = (x < 0) ? (CHUNK_WIDTH - 1) : (x >= CHUNK_WIDTH ? 0 : x);
        int32_t lz = (z < 0) ? (CHUNK_DEPTH - 1) : (z >= CHUNK_DEPTH ? 0 : z);
        return diag->get_light_packed_word_unsafe(lx, sy, lz);
    }

    // y in bounds, x/z out — use horizontal/corner neighbors
    if (!x_in && !z_in) {
        const ChunkData* corner = nullptr;
        int32_t cx, cz;
        if (x < 0 && z < 0) { corner = neg_x_neg_z; cx = CHUNK_WIDTH - 1; cz = CHUNK_DEPTH - 1; }
        else if (x < 0 && z >= CHUNK_DEPTH) { corner = neg_x_pos_z; cx = CHUNK_WIDTH - 1; cz = 0; }
        else if (x >= CHUNK_WIDTH && z < 0) { corner = pos_x_neg_z; cx = 0; cz = CHUNK_DEPTH - 1; }
        else { corner = pos_x_pos_z; cx = 0; cz = 0; }
        if (!corner) return 0;
        return corner->get_light_packed_word_unsafe(cx, sy, cz);
    }
    if (!x_in) {
        src = (x < 0) ? neg_x : pos_x;
        if (!src) return 0;
        sx = (x < 0) ? (CHUNK_WIDTH - 1) : 0;
    } else {
        src = (z < 0) ? neg_z : pos_z;
        if (!src) return 0;
        sz = (z < 0) ? (CHUNK_DEPTH - 1) : 0;
    }
    return src->get_light_packed_word_unsafe(sx, sy, sz);
}

uint8_t ChunkNeighborAccessor::get_sky_light(int32_t x, int32_t y, int32_t z) const {
    const ChunkData* src = center;
    int32_t sx = x;
    int32_t sy = y;
    int32_t sz = z;

    int dy = 0;
    if (y < 0) {
        src = neg_y;
        if (!src) return 0;
        sy = y + CHUNK_HEIGHT;
        dy = -1;
    } else if (y >= CHUNK_HEIGHT) {
        src = pos_y;
        if (!src) return 15;
        sy = y - CHUNK_HEIGHT;
        dy = 1;
    }

    const bool x_in = (x >= 0 && x < CHUNK_WIDTH);
    const bool z_in = (z >= 0 && z < CHUNK_DEPTH);

    if (x_in && z_in) {
        return src->get_sky_light(sx, sy, sz);
    }

    if (dy != 0) {
        // y out AND x/z out — need diagonal/corner chunk
        int dx = 0;
        int dz = 0;
        if (x < 0) dx = -1;
        else if (x >= CHUNK_WIDTH) dx = 1;
        if (z < 0) dz = -1;
        else if (z >= CHUNK_DEPTH) dz = 1;

        const ChunkData* diag = lookup_diagonal(dx, dy, dz);
        if (!diag) return 0;

        int32_t lx = (x < 0) ? (CHUNK_WIDTH - 1) : (x >= CHUNK_WIDTH ? 0 : x);
        int32_t lz = (z < 0) ? (CHUNK_DEPTH - 1) : (z >= CHUNK_DEPTH ? 0 : z);
        return diag->get_sky_light(lx, sy, lz);
    }

    // y in bounds, x/z out — use horizontal/corner neighbors
    if (!x_in && !z_in) {
        const ChunkData* corner = nullptr;
        int32_t cx, cz;
        if (x < 0 && z < 0) { corner = neg_x_neg_z; cx = CHUNK_WIDTH - 1; cz = CHUNK_DEPTH - 1; }
        else if (x < 0 && z >= CHUNK_DEPTH) { corner = neg_x_pos_z; cx = CHUNK_WIDTH - 1; cz = 0; }
        else if (x >= CHUNK_WIDTH && z < 0) { corner = pos_x_neg_z; cx = 0; cz = CHUNK_DEPTH - 1; }
        else { corner = pos_x_pos_z; cx = 0; cz = 0; }
        if (!corner) return 0;
        return corner->get_sky_light(cx, sy, cz);
    }
    if (!x_in) {
        const ChunkData* neighbor = (x < 0) ? neg_x : pos_x;
        if (!neighbor) return 0;
        sx = (x < 0) ? (CHUNK_WIDTH - 1) : 0;
        return neighbor->get_sky_light(sx, sy, sz);
    }
    const ChunkData* neighbor = (z < 0) ? neg_z : pos_z;
    if (!neighbor) return 0;
    sz = (z < 0) ? (CHUNK_DEPTH - 1) : 0;
    return neighbor->get_sky_light(sx, sy, sz);
}

bool ChunkNeighborAccessor::is_occluder(int32_t x, int32_t y, int32_t z) const {
    BlockID block = get_block(x, y, z);
    if (block == BlockIDs::AIR) return false;
    const BlockType& block_type = BlockRegistry::get_instance().get_block(block);
    return HasProperty(block_type.properties, BlockProperty::Opaque) ||
           HasProperty(block_type.properties, BlockProperty::Solid);
}

} // namespace VoxelEngine

#include "mesh/chunk_neighbor_accessor.hpp"
#include "core/block_types.hpp"

namespace VoxelEngine {

BlockID ChunkNeighborAccessor::get_block(int32_t x, int32_t y, int32_t z) const {
    const ChunkData* src = center;
    int32_t sx = x;
    int32_t sy = y;
    int32_t sz = z;

    if (y < 0) {
        src = neg_y;
        if (!src) return BlockIDs::AIR;
        sy = y + CHUNK_HEIGHT;
    } else if (y >= CHUNK_HEIGHT) {
        src = pos_y;
        if (!src) return BlockIDs::AIR;
        sy = y - CHUNK_HEIGHT;
    }

    if (x < 0 || x >= CHUNK_WIDTH || z < 0 || z >= CHUNK_DEPTH) {
        if (src != center) {
            return BlockIDs::AIR;
        }
        const bool x_out = (x < 0 || x >= CHUNK_WIDTH);
        const bool z_out = (z < 0 || z >= CHUNK_DEPTH);
        if (x_out && z_out) {
            const ChunkData* corner = nullptr;
            int32_t cx, cz;
            if (x < 0 && z < 0) { corner = neg_x_neg_z; cx = CHUNK_WIDTH - 1; cz = CHUNK_DEPTH - 1; }
            else if (x < 0 && z >= CHUNK_DEPTH) { corner = neg_x_pos_z; cx = CHUNK_WIDTH - 1; cz = 0; }
            else if (x >= CHUNK_WIDTH && z < 0) { corner = pos_x_neg_z; cx = 0; cz = CHUNK_DEPTH - 1; }
            else { corner = pos_x_pos_z; cx = 0; cz = 0; }
            if (!corner) return BlockIDs::AIR;
            return corner->get_block(cx, sy, cz);
        }
        const ChunkData* neighbor_chunk = nullptr;
        int32_t neighbor_x = x;
        int32_t neighbor_z = z;
        if (x < 0) {
            neighbor_chunk = neg_x;
            neighbor_x = CHUNK_WIDTH - 1;
        } else if (x >= CHUNK_WIDTH) {
            neighbor_chunk = pos_x;
            neighbor_x = 0;
        }
        if (z < 0) {
            neighbor_chunk = neg_z;
            neighbor_z = CHUNK_DEPTH - 1;
        } else if (z >= CHUNK_DEPTH) {
            neighbor_chunk = pos_z;
            neighbor_z = 0;
        }
        if (!neighbor_chunk) {
            return BlockIDs::AIR;
        }
        return neighbor_chunk->get_block(neighbor_x, sy, neighbor_z);
    }

    return src->get_block(sx, sy, sz);
}

uint16_t ChunkNeighborAccessor::get_light_packed(int32_t x, int32_t y, int32_t z) const {
    const ChunkData* src = center;
    int32_t sx = x;
    int32_t sy = y;
    int32_t sz = z;

    if (y < 0) {
        src = neg_y;
        if (!src) return 0;
        sy = y + CHUNK_HEIGHT;
    } else if (y >= CHUNK_HEIGHT) {
        src = pos_y;
        if (!src) return 15; // assume sky above unloaded chunk
        sy = y - CHUNK_HEIGHT;
    }

    // Fast path: both x and z are in bounds — read the packed word directly.
    const bool x_in = (x >= 0 && x < CHUNK_WIDTH);
    const bool z_in = (z >= 0 && z < CHUNK_DEPTH);
    if (x_in && z_in) {
        return src->get_light_packed_word_unsafe(sx, sy, sz);
    }
    if (!x_in && !z_in) {
        if (src != center) return 0;
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
        if (src != center) return 0;
        src = (x < 0) ? neg_x : pos_x;
        if (!src) return 0;
        sx = (x < 0) ? (CHUNK_WIDTH - 1) : 0;
    } else {
        if (src != center) return 0;
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

    if (y < 0) {
        src = neg_y;
        if (!src) return 0;
        sy = y + CHUNK_HEIGHT;
    } else if (y >= CHUNK_HEIGHT) {
        src = pos_y;
        if (!src) return 15; // assume sky above unloaded chunk
        sy = y - CHUNK_HEIGHT;
    }

    const bool x_in = (x >= 0 && x < CHUNK_WIDTH);
    const bool z_in = (z >= 0 && z < CHUNK_DEPTH);
    if (x_in && z_in) {
        return src->get_sky_light(sx, sy, sz);
    }
    if (!x_in && !z_in) {
        if (src != center) return 0;
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
        if (src != center) return 0;
        const ChunkData* neighbor = (x < 0) ? neg_x : pos_x;
        if (!neighbor) {
            return 0;
        }
        sx = (x < 0) ? (CHUNK_WIDTH - 1) : 0;
        return neighbor->get_sky_light(sx, sy, sz);
    }
    if (src != center) return 0;
    const ChunkData* neighbor = (z < 0) ? neg_z : pos_z;
    if (!neighbor) {
        return 0;
    }
    sz = (z < 0) ? (CHUNK_DEPTH - 1) : 0;
    return neighbor->get_sky_light(sx, sy, sz);
}

bool ChunkNeighborAccessor::is_occluder(int32_t x, int32_t y, int32_t z) const {
    const ChunkData* src = center;
    int32_t sx = x;
    int32_t sy = y;
    int32_t sz = z;

    if (y < 0) {
        src = neg_y;
        if (!src) return false;
        sy = y + CHUNK_HEIGHT;
    } else if (y >= CHUNK_HEIGHT) {
        src = pos_y;
        if (!src) return false;
        sy = y - CHUNK_HEIGHT;
    }

    BlockID block;
    if (sx < 0 || sx >= CHUNK_WIDTH || sz < 0 || sz >= CHUNK_DEPTH) {
        if (src != center) return false;
        const bool sx_out = (sx < 0 || sx >= CHUNK_WIDTH);
        const bool sz_out = (sz < 0 || sz >= CHUNK_DEPTH);
        if (sx_out && sz_out) {
            const ChunkData* corner = nullptr;
            int32_t cx, cz;
            if (sx < 0 && sz < 0) { corner = neg_x_neg_z; cx = CHUNK_WIDTH - 1; cz = CHUNK_DEPTH - 1; }
            else if (sx < 0 && sz >= CHUNK_DEPTH) { corner = neg_x_pos_z; cx = CHUNK_WIDTH - 1; cz = 0; }
            else if (sx >= CHUNK_WIDTH && sz < 0) { corner = pos_x_neg_z; cx = 0; cz = CHUNK_DEPTH - 1; }
            else { corner = pos_x_pos_z; cx = 0; cz = 0; }
            if (!corner) return false;
            block = corner->get_block(cx, sy, cz);
        } else {
            const ChunkData* neighbor_chunk = nullptr;
            int32_t neighbor_x = sx;
            int32_t neighbor_z = sz;
            if (sx < 0) {
                neighbor_chunk = neg_x;
                neighbor_x = CHUNK_WIDTH - 1;
            } else if (sx >= CHUNK_WIDTH) {
                neighbor_chunk = pos_x;
                neighbor_x = 0;
            }
            if (sz < 0) {
                neighbor_chunk = neg_z;
                neighbor_z = CHUNK_DEPTH - 1;
            } else if (sz >= CHUNK_DEPTH) {
                neighbor_chunk = pos_z;
                neighbor_z = 0;
            }
            if (neighbor_chunk) {
                block = neighbor_chunk->get_block(neighbor_x, sy, neighbor_z);
            } else {
                block = BlockIDs::AIR;
            }
        }
    } else {
        block = src->get_block(sx, sy, sz);
    }
    const BlockType& block_type = BlockRegistry::get_instance().get_block(block);
    return block_type.top_face_offset <= 0.0f;
}

} // namespace VoxelEngine

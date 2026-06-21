#ifndef FUK_MINECRAFT_CHUNK_NEIGHBOR_ACCESSOR_HPP
#define FUK_MINECRAFT_CHUNK_NEIGHBOR_ACCESSOR_HPP
#include "core/chunk_data.hpp"

namespace VoxelEngine {

class ChunkNeighborAccessor {
public:
    const ChunkData* center = nullptr;
    const ChunkData* neg_x = nullptr;
    const ChunkData* pos_x = nullptr;
    const ChunkData* neg_y = nullptr;
    const ChunkData* pos_y = nullptr;
    const ChunkData* neg_z = nullptr;
    const ChunkData* pos_z = nullptr;
    const ChunkData* neg_x_neg_z = nullptr;
    const ChunkData* neg_x_pos_z = nullptr;
    const ChunkData* pos_x_neg_z = nullptr;
    const ChunkData* pos_x_pos_z = nullptr;

    BlockID get_block(int32_t x, int32_t y, int32_t z) const;
    uint16_t get_light_packed(int32_t x, int32_t y, int32_t z) const;
    uint8_t get_sky_light(int32_t x, int32_t y, int32_t z) const;
    bool is_occluder(int32_t x, int32_t y, int32_t z) const;
};

} // namespace VoxelEngine
#endif

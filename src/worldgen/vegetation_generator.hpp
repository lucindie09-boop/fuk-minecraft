#ifndef FUK_MINECRAFT_VEGETATION_GENERATOR_HPP
#define FUK_MINECRAFT_VEGETATION_GENERATOR_HPP
#include <cstdint>
#include "core/chunk_data.hpp"
#include "worldgen/chunk_generator.hpp"

namespace VoxelEngine {

class VegetationGenerator {
public:
    using CrossChunkWriter = ChunkGenerator::CrossChunkWriter;

    void generate_vegetation(ChunkData& chunk,
                             const ChunkGenerator::ChunkColumn (&columns)[CHUNK_WIDTH][CHUNK_DEPTH],
                             int32_t chunk_x, int32_t chunk_z,
                             int32_t world_y_start, int32_t world_y_end,
                             const CrossChunkWriter& cross_writer = nullptr);

private:
    static uint32_t hash_pos(int32_t x, int32_t z);
    static void place_tree(ChunkData& chunk,
                          int32_t local_x, int32_t local_z,
                          int32_t surface_y, int32_t world_y_start, int32_t world_y_end,
                          uint32_t seed, int32_t chunk_x, int32_t chunk_z,
                          const CrossChunkWriter& cross_writer = nullptr);
    static void place_cactus(ChunkData& chunk, int32_t local_x, int32_t local_z,
                             int32_t surface_y, int32_t world_y_start, int32_t world_y_end);
    static void place_boulder(ChunkData& chunk, int32_t local_x, int32_t local_z,
                              int32_t surface_y, int32_t world_y_start, int32_t world_y_end,
                              uint32_t seed);
};

} // namespace VoxelEngine
#endif

#include "worldgen/vegetation_generator.hpp"
#include "core/block_types.hpp"

namespace VoxelEngine {

void VegetationGenerator::generate_vegetation(
    ChunkData& chunk,
    const ChunkGenerator::ChunkColumn (&columns)[CHUNK_WIDTH][CHUNK_DEPTH],
    int32_t chunk_x, int32_t chunk_z,
    int32_t world_y_start, int32_t world_y_end,
    const CrossChunkWriter& cross_writer)
{
    for (int32_t x = 0; x < CHUNK_WIDTH; x++) {
        for (int32_t z = 0; z < CHUNK_DEPTH; z++) {
            int32_t surface_y = columns[x][z].height;
            BiomeType biome = columns[x][z].biome;

            // Surface must be inside this chunk for us to place vegetation on it
            if (surface_y < world_y_start || surface_y >= world_y_end)
                continue;

            int32_t wx = chunk_x * CHUNK_WIDTH + x;
            int32_t wz = chunk_z * CHUNK_DEPTH + z;
            uint32_t h = hash_pos(wx, wz);

            BlockID surface_block = chunk.get_block(x, surface_y - world_y_start, z);

            if (biome == BiomeType::Forest) {
                // ~7 trees per chunk avg (0.7% column density)
                if ((h % 1000u) < 7u) {
                    place_tree(chunk, x, z, surface_y, world_y_start, world_y_end, h, chunk_x, chunk_z, cross_writer);
                }
            } else if (biome == BiomeType::Plains) {
                // Per-chunk sparse tree: ~25% of plains chunks get exactly one tree.
                // Only triggers on the first column (0,0) to ensure one tree per qualifying chunk.
                if (x == 0 && z == 0) {
                    uint32_t ch = hash_pos(chunk_x * CHUNK_WIDTH, chunk_z * CHUNK_DEPTH);
                    if ((ch % 100u) < 25u) {
                        // Pick a random column within the chunk for the single tree
                        int32_t tx = static_cast<int32_t>(ch >> 8) % CHUNK_WIDTH;
                        int32_t tz = static_cast<int32_t>(ch >> 16) % CHUNK_DEPTH;
                        int32_t ts = columns[tx][tz].height;
                        if (ts >= world_y_start && ts < world_y_end) {
                            place_tree(chunk, tx, tz, ts, world_y_start, world_y_end, ch, chunk_x, chunk_z, cross_writer);
                        }
                    }
                }
            } else if (biome == BiomeType::Desert && surface_block == BlockIDs::SAND) {
                // ~0.3% cactus density per column (≈3 per chunk)
                if ((h % 1000u) < 3u) {
                    place_cactus(chunk, x, z, surface_y, world_y_start, world_y_end);
                }
            }
        }
    }
}

uint32_t VegetationGenerator::hash_pos(int32_t wx, int32_t wz) {
    uint32_t h = static_cast<uint32_t>(wx) * 374761393u + static_cast<uint32_t>(wz) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void VegetationGenerator::place_tree(
    ChunkData& chunk,
    int32_t local_x, int32_t local_z,
    int32_t surface_y, int32_t world_y_start, int32_t world_y_end,
    uint32_t seed, int32_t chunk_x, int32_t chunk_z,
    const CrossChunkWriter& cross_writer)
{
    int32_t trunk_height = 5 + static_cast<int32_t>(seed & 2u);

    for (int32_t dy = 1; dy <= trunk_height; dy++) {
        int32_t y = surface_y + dy;
        if (y >= world_y_start && y < world_y_end) {
            chunk.set_block(local_x, y - world_y_start, local_z, BlockIDs::WOOD);
        } else if (cross_writer) {
            int32_t wx = chunk_x * CHUNK_WIDTH + local_x;
            int32_t wz = chunk_z * CHUNK_DEPTH + local_z;
            cross_writer(wx, y, wz, BlockIDs::WOOD);
        }
    }

    int32_t base_y = surface_y + trunk_height;

    auto leaf = [&](int32_t dx, int32_t dz, int32_t dy) {
        int32_t lx = local_x + dx;
        int32_t lz = local_z + dz;
        int32_t ly = base_y + dy;
        if (ly < world_y_start || ly >= world_y_end) {
            if (cross_writer) {
                int32_t wx = chunk_x * CHUNK_WIDTH + lx;
                int32_t wz = chunk_z * CHUNK_DEPTH + lz;
                cross_writer(wx, ly, wz, BlockIDs::LEAVES);
            }
            return;
        }
        if (lx >= 0 && lx < CHUNK_WIDTH && lz >= 0 && lz < CHUNK_DEPTH) {
            if (chunk.get_block(lx, ly - world_y_start, lz) == BlockIDs::AIR)
                chunk.set_block(lx, ly - world_y_start, lz, BlockIDs::LEAVES);
        } else if (cross_writer) {
            int32_t wx = chunk_x * CHUNK_WIDTH + lx;
            int32_t wz = chunk_z * CHUNK_DEPTH + lz;
            cross_writer(wx, ly, wz, BlockIDs::LEAVES);
        }
    };

    // Lower ring around trunk (dy = -1)
    for (int32_t dx = -1; dx <= 1; dx++)
        for (int32_t dz = -1; dz <= 1; dz++)
            if (!(dx == 0 && dz == 0))
                leaf(dx, dz, -1);

    // Layer 0: 3x3 full at base of canopy
    for (int32_t dx = -1; dx <= 1; dx++)
        for (int32_t dz = -1; dz <= 1; dz++)
            leaf(dx, dz, 0);

    // Layer 1: 5x5 ring (edges only)
    for (int32_t dx = -2; dx <= 2; dx++)
        for (int32_t dz = -2; dz <= 2; dz++)
            if (std::abs(dx) == 2 || std::abs(dz) == 2)
                leaf(dx, dz, 1);

    // Layer 2: 3x3 ring
    for (int32_t dx = -1; dx <= 1; dx++)
        for (int32_t dz = -1; dz <= 1; dz++)
            if (!(dx == 0 && dz == 0))
                leaf(dx, dz, 2);

    // Top cap
    leaf(0, 0, 3);
}

void VegetationGenerator::place_cactus(
    ChunkData& chunk, int32_t local_x, int32_t local_z,
    int32_t surface_y, int32_t world_y_start, int32_t world_y_end)
{
    int32_t height = 2 + (hash_pos(local_x * 7 + 13, local_z * 11 + 7) & 1u);
    for (int32_t dy = 1; dy <= height; dy++) {
        int32_t y = surface_y + dy;
        if (y >= world_y_start && y < world_y_end) {
            chunk.set_block(local_x, y - world_y_start, local_z, BlockIDs::CACTUS);
        }
    }
}

} // namespace VoxelEngine

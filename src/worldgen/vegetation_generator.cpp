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
    // Track placed tree positions within this chunk to enforce minimum spacing
    bool tree_placed[CHUNK_WIDTH][CHUNK_DEPTH] = {};

    // Per-chunk randomness: only 60% of forest chunks get any trees at all
    uint32_t chunk_seed = hash_pos(chunk_x * CHUNK_WIDTH, chunk_z * CHUNK_DEPTH);
    bool forest_has_trees = (chunk_seed % 100u) < 60u;

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
                if (forest_has_trees) {
                    // ~5 trees per chunk avg when trees are present (0.5% column density)
                    if ((h % 1000u) < 5u) {
                        // Enforce minimum spacing: reject if any tree within Chebyshev radius 3
                        constexpr int32_t MIN_TREE_RADIUS = 3;
                        bool too_close = false;
                        for (int32_t dx = -MIN_TREE_RADIUS; dx <= MIN_TREE_RADIUS && !too_close; dx++) {
                            for (int32_t dz = -MIN_TREE_RADIUS; dz <= MIN_TREE_RADIUS && !too_close; dz++) {
                                int32_t nx = x + dx;
                                int32_t nz = z + dz;
                                if (nx >= 0 && nx < CHUNK_WIDTH && nz >= 0 && nz < CHUNK_DEPTH) {
                                    if (tree_placed[nx][nz]) too_close = true;
                                }
                            }
                        }
                        if (!too_close) {
                            place_tree(chunk, x, z, surface_y, world_y_start, world_y_end, h, chunk_x, chunk_z, cross_writer);
                            tree_placed[x][z] = true;
                        }
                    }
                }
                // ~4 boulders per chunk avg (0.4% column density)
                if ((h % 1000u) < 9u) {
                    // Enforce minimum spacing from trees
                    constexpr int32_t MIN_BOULDER_TREE_RADIUS = 2;
                    bool too_close_to_tree = false;
                    for (int32_t dx = -MIN_BOULDER_TREE_RADIUS; dx <= MIN_BOULDER_TREE_RADIUS && !too_close_to_tree; dx++) {
                        for (int32_t dz = -MIN_BOULDER_TREE_RADIUS; dz <= MIN_BOULDER_TREE_RADIUS && !too_close_to_tree; dz++) {
                            int32_t nx = x + dx;
                            int32_t nz = z + dz;
                            if (nx >= 0 && nx < CHUNK_WIDTH && nz >= 0 && nz < CHUNK_DEPTH) {
                                if (tree_placed[nx][nz]) too_close_to_tree = true;
                            }
                        }
                    }
                    if (!too_close_to_tree) {
                        place_boulder(chunk, x, z, surface_y, world_y_start, world_y_end, h);
                    }
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
    constexpr int32_t trunk_height = 5;

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

    auto leaf = [&](int32_t dx, int32_t dz, int32_t dy) {
        int32_t lx = local_x + dx;
        int32_t lz = local_z + dz;
        int32_t ly = surface_y + dy;
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

    for (int32_t dy = 3; dy <= 4; dy++) {
        for (int32_t dx = -2; dx <= 2; dx++) {
            for (int32_t dz = -2; dz <= 2; dz++) {
                if (std::abs(dx) == 2 && std::abs(dz) == 2) continue;
                if (dy <= trunk_height && dx == 0 && dz == 0) continue;
                leaf(dx, dz, dy);
            }
        }
    }
    for (int32_t dy = 5; dy <= 6; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            for (int32_t dz = -1; dz <= 1; dz++) {
                if (dy <= trunk_height && dx == 0 && dz == 0) continue;
                leaf(dx, dz, dy);
            }
        }
    }
}

void VegetationGenerator::place_cactus(
    ChunkData& chunk, int32_t local_x, int32_t local_z,
    int32_t surface_y, int32_t world_y_start, int32_t world_y_end)
{
    int32_t height = 2 + static_cast<int32_t>(hash_pos(local_x * 7 + 13, local_z * 11 + 7) & 1u);
    for (int32_t dy = 1; dy <= height; dy++) {
        int32_t y = surface_y + dy;
        if (y >= world_y_start && y < world_y_end) {
            chunk.set_block(local_x, y - world_y_start, local_z, BlockIDs::CACTUS);
        }
    }
}

void VegetationGenerator::place_boulder(
    ChunkData& chunk, int32_t local_x, int32_t local_z,
    int32_t surface_y, int32_t world_y_start, int32_t world_y_end,
    uint32_t seed)
{
    // Boulder size: 1-3 blocks radius
    int32_t radius = 1 + static_cast<int32_t>(seed & 2u);
    
    // Mix of stone and gravel for natural look
    auto place_block = [&](int32_t dx, int32_t dz, int32_t dy) {
        int32_t lx = local_x + dx;
        int32_t lz = local_z + dz;
        int32_t ly = surface_y + dy;
        
        if (ly < world_y_start || ly >= world_y_end) return;
        if (lx < 0 || lx >= CHUNK_WIDTH || lz < 0 || lz >= CHUNK_DEPTH) return;
        
        // Skip if already occupied (don't overwrite trees/other vegetation)
        if (chunk.get_block(lx, ly - world_y_start, lz) != BlockIDs::AIR) return;
        
        // Use gravel for bottom layer, stone for top
        BlockID block_type = (dy < 0) ? BlockIDs::GRAVEL : BlockIDs::STONE;
        chunk.set_block(lx, ly - world_y_start, lz, block_type);
    };
    
    // Place boulder in roughly spherical shape
    for (int32_t dy = -radius; dy <= radius; dy++) {
        for (int32_t dx = -radius; dx <= radius; dx++) {
            for (int32_t dz = -radius; dz <= radius; dz++) {
                float dist_sq = static_cast<float>(dx*dx + dy*dy + dz*dz);
                float radius_sq = static_cast<float>(radius * radius);
                // Slightly irregular shape using noise
                float noise_offset = (static_cast<float>((seed >> (dx + dz + 3)) & 7u) - 3.5f) * 0.2f;
                if (dist_sq + noise_offset * radius_sq <= radius_sq * 1.5f) {
                    place_block(dx, dz, dy);
                }
            }
        }
    }
}

} // namespace VoxelEngine

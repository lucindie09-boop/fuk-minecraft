#include "worldgen/vegetation_generator.hpp"
#include "worldgen/biome_registry.hpp"
#include "core/block_types.hpp"

namespace VoxelEngine {

void VegetationGenerator::generate_vegetation(
    ChunkData& chunk,
    const ChunkGenerator::ChunkColumn (&columns)[CHUNK_WIDTH][CHUNK_DEPTH],
    int32_t chunk_x, int32_t chunk_z,
    int32_t world_y_start, int32_t world_y_end,
    const CrossChunkWriter& cross_writer)
{
    bool tree_placed[CHUNK_WIDTH][CHUNK_DEPTH] = {};
    uint32_t chunk_seed = hash_pos(chunk_x * CHUNK_WIDTH, chunk_z * CHUNK_DEPTH);
    bool forest_has_trees = (chunk_seed % 100u) < 80u;
    uint32_t target_trees = 15 + (chunk_seed % 11);
    uint32_t trees_placed_count = 0;

    struct Cell { int32_t x; int32_t z; };
    Cell cells[CHUNK_WIDTH * CHUNK_DEPTH];
    for (int32_t i = 0; i < CHUNK_WIDTH * CHUNK_DEPTH; i++)
        cells[i] = { i / CHUNK_DEPTH, i % CHUNK_DEPTH };
    uint32_t rng = chunk_seed;
    for (int32_t i = CHUNK_WIDTH * CHUNK_DEPTH - 1; i > 0; i--) {
        rng = rng * 1664525u + 1013904223u;
        int32_t j = static_cast<int32_t>(rng % static_cast<uint32_t>(i + 1));
        Cell tmp = cells[i];
        cells[i] = cells[j];
        cells[j] = tmp;
    }

    auto is_forest_biome = [](int id) -> bool {
        return id == Biomes::forest || id == Biomes::birch_forest || id == Biomes::roofedForest ||
               id == Biomes::taiga || id == Biomes::coldTaiga || id == Biomes::megaTaiga ||
               id == Biomes::jungle || id == Biomes::jungle_edge || id == Biomes::forestHills ||
               id == Biomes::taiga_hills || id == Biomes::birch_forest_hills ||
               id == Biomes::coldTaigaHills || id == Biomes::megaTaigaHills ||
               id == Biomes::jungle_hills;
    };

    auto get_tree_type = [](int id) -> int {
        if (id == Biomes::taiga || id == Biomes::coldTaiga || id == Biomes::megaTaiga ||
            id == Biomes::taiga_hills || id == Biomes::coldTaigaHills || id == Biomes::megaTaigaHills)
            return 1; // spruce
        if (id == Biomes::birch_forest || id == Biomes::birch_forest_hills)
            return 2; // birch
        if (id == Biomes::jungle || id == Biomes::jungle_hills || id == Biomes::jungle_edge)
            return 3; // jungle (big oak)
        return 0; // oak
    };

    for (int32_t idx = 0; idx < CHUNK_WIDTH * CHUNK_DEPTH; idx++) {
        int32_t x = cells[idx].x;
        int32_t z = cells[idx].z;
        int32_t surface_y = columns[x][z].top_solid_y;
        int biome_id = columns[x][z].biome_id;

        if (surface_y < world_y_start || surface_y >= world_y_end)
            continue;

        int32_t wx = chunk_x * CHUNK_WIDTH + x;
        int32_t wz = chunk_z * CHUNK_DEPTH + z;
        uint32_t h = hash_pos(wx, wz);
        BlockID surface_block = chunk.get_block(x, surface_y - world_y_start, z);

        if (is_forest_biome(biome_id)) {
            if (forest_has_trees && trees_placed_count < target_trees) {
                if ((h % 10u) < 5u) {
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
                        int tree_type = get_tree_type(biome_id);
                        switch (tree_type) {
                            case 1:
                                place_spruce_tree(chunk, x, z, surface_y, world_y_start, world_y_end, h, chunk_x, chunk_z, cross_writer);
                                break;
                            case 2:
                                place_birch_tree(chunk, x, z, surface_y, world_y_start, world_y_end, h, chunk_x, chunk_z, cross_writer);
                                break;
                            default:
                                place_oak_tree(chunk, x, z, surface_y, world_y_start, world_y_end, h, chunk_x, chunk_z, cross_writer);
                                break;
                        }
                        tree_placed[x][z] = true;
                        trees_placed_count++;
                    }
                }
            }
            if ((h % 10000u) < 3u) {
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
                    place_boulder(chunk, x, z, surface_y, world_y_start, world_y_end, h, chunk_x, chunk_z, cross_writer);
                }
            }
        } else if (biome_id == Biomes::plains) {
            if (x == 0 && z == 0) {
                uint32_t ch = hash_pos(chunk_x * CHUNK_WIDTH, chunk_z * CHUNK_DEPTH);
                if ((ch % 100u) < 25u) {
                    int32_t tx = static_cast<int32_t>(ch >> 8) % CHUNK_WIDTH;
                    int32_t tz = static_cast<int32_t>(ch >> 16) % CHUNK_DEPTH;
                    int32_t ts = columns[tx][tz].top_solid_y;
                    if (ts >= world_y_start && ts < world_y_end) {
                        place_oak_tree(chunk, tx, tz, ts, world_y_start, world_y_end, ch, chunk_x, chunk_z, cross_writer);
                    }
                }
            }
        } else if (biome_id == Biomes::desert && surface_block == BlockIDs::SAND) {
            if ((h % 1000u) < 3u) {
                place_cactus(chunk, x, z, surface_y, world_y_start, world_y_end);
            }
        } else if (biome_id == Biomes::savanna && surface_block == BlockIDs::GRASS) {
            if (x == 0 && z == 0) {
                uint32_t ch = hash_pos(chunk_x * CHUNK_WIDTH, chunk_z * CHUNK_DEPTH);
                if ((ch % 100u) < 15u) {
                    int32_t tx = static_cast<int32_t>(ch >> 8) % CHUNK_WIDTH;
                    int32_t tz = static_cast<int32_t>(ch >> 16) % CHUNK_DEPTH;
                    int32_t ts = columns[tx][tz].top_solid_y;
                    if (ts >= world_y_start && ts < world_y_end) {
                        place_oak_tree(chunk, tx, tz, ts, world_y_start, world_y_end, ch, chunk_x, chunk_z, cross_writer);
                    }
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

void VegetationGenerator::place_oak_tree(
    ChunkData& chunk, int32_t local_x, int32_t local_z,
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

void VegetationGenerator::place_birch_tree(
    ChunkData& chunk, int32_t local_x, int32_t local_z,
    int32_t surface_y, int32_t world_y_start, int32_t world_y_end,
    uint32_t seed, int32_t chunk_x, int32_t chunk_z,
    const CrossChunkWriter& cross_writer)
{
    place_oak_tree(chunk, local_x, local_z, surface_y, world_y_start, world_y_end,
                   seed, chunk_x, chunk_z, cross_writer);
}

void VegetationGenerator::place_spruce_tree(
    ChunkData& chunk, int32_t local_x, int32_t local_z,
    int32_t surface_y, int32_t world_y_start, int32_t world_y_end,
    uint32_t seed, int32_t chunk_x, int32_t chunk_z,
    const CrossChunkWriter& cross_writer)
{
    constexpr int32_t trunk_height = 7;
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

    for (int32_t dy = 3; dy <= 5; dy++) {
        int radius = (dy == 3) ? 1 : ((dy <= 5) ? 2 : 1);
        for (int32_t dx = -radius; dx <= radius; dx++) {
            for (int32_t dz = -radius; dz <= radius; dz++) {
                if (std::abs(dx) == radius && std::abs(dz) == radius) continue;
                leaf(dx, dz, dy);
            }
        }
    }
    for (int32_t dy = 6; dy <= 8; dy++) {
        for (int32_t dx = -1; dx <= 1; dx++) {
            for (int32_t dz = -1; dz <= 1; dz++) {
                if (dx == 0 && dz == 0 && dy <= trunk_height) continue;
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
    uint32_t seed, int32_t chunk_x, int32_t chunk_z,
    const CrossChunkWriter& cross_writer)
{
    int32_t radius = 3 + static_cast<int32_t>(seed & 1u);

    auto place_block = [&](int32_t dx, int32_t dz, int32_t dy) {
        int32_t lx = local_x + dx;
        int32_t lz = local_z + dz;
        int32_t ly = surface_y + dy;
        if (ly < world_y_start || ly >= world_y_end) {
            if (cross_writer) {
                int32_t wx = chunk_x * CHUNK_WIDTH + lx;
                int32_t wz = chunk_z * CHUNK_DEPTH + lz;
                cross_writer(wx, ly, wz, BlockIDs::STONE);
            }
            return;
        }
        if (lx >= 0 && lx < CHUNK_WIDTH && lz >= 0 && lz < CHUNK_DEPTH) {
            if (chunk.get_block(lx, ly - world_y_start, lz) == BlockIDs::AIR)
                chunk.set_block(lx, ly - world_y_start, lz, BlockIDs::STONE);
        } else if (cross_writer) {
            int32_t wx = chunk_x * CHUNK_WIDTH + lx;
            int32_t wz = chunk_z * CHUNK_DEPTH + lz;
            cross_writer(wx, ly, wz, BlockIDs::STONE);
        }
    };

    for (int32_t dy = -radius; dy <= radius; dy++) {
        for (int32_t dx = -radius; dx <= radius; dx++) {
            for (int32_t dz = -radius; dz <= radius; dz++) {
                int32_t dist_sq = dx*dx + dy*dy + dz*dz;
                float noise_offset = (static_cast<float>((seed >> (dx + dz + 3)) & 7u)) * 0.15f;
                if (dist_sq <= radius * radius - noise_offset * radius) {
                    place_block(dx, dz, dy);
                }
            }
        }
    }
}

} // namespace VoxelEngine

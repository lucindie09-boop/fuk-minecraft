#include "doctest.h"
#include "worldgen/chunk_generator.hpp"
#include "core/block_types.hpp"
#include "core/chunk_data.hpp"
#include "core/chunk_coords.hpp"
#include <cmath>

using namespace VoxelEngine;

TEST_CASE("1.7 generator: chunk produces valid blocks") {
    BlockRegistry::get_instance().initialize_default_blocks();

    TerrainParams params;
    params.seed = 42;
    ChunkGenerator gen(params);

    ChunkData chunk;
    chunk.clear();
    gen.generate_chunk(chunk, 0, 0, 0, nullptr, false);

    int bc = static_cast<int>(chunk.get_block_count());
    CHECK(bc > 0);
    CHECK(bc <= CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH);

    // Spot-check valid block IDs
    bool all_valid = true;
    for (int y = 0; y < CHUNK_HEIGHT && all_valid; y += 16) {
        for (int z = 0; z < CHUNK_DEPTH && all_valid; z += 16) {
            for (int x = 0; x < CHUNK_WIDTH && all_valid; x += 16) {
                BlockID id = chunk.get_block(x, y, z);
                if (id < 0 || id >= static_cast<BlockID>(BlockRegistry::get_instance().get_count()))
                    all_valid = false;
            }
        }
    }
    CHECK(all_valid);
}

TEST_CASE("1.7 generator: different seeds produce different terrain") {
    BlockRegistry::get_instance().initialize_default_blocks();

    TerrainParams params_a, params_b;
    params_a.seed = 42;
    params_b.seed = 999;
    ChunkGenerator gen_a(params_a), gen_b(params_b);

    ChunkData ca, cb;
    ca.clear();
    cb.clear();
    gen_a.generate_chunk(ca, 0, 0, 0, nullptr, false);
    gen_b.generate_chunk(cb, 0, 0, 0, nullptr, false);

    // At least some blocks should differ
    bool any_diff = false;
    for (int y = 0; y < CHUNK_HEIGHT && !any_diff; y += 8) {
        for (int z = 0; z < CHUNK_DEPTH && !any_diff; z += 8) {
            for (int x = 0; x < CHUNK_WIDTH && !any_diff; x += 8) {
                if (ca.get_block(x, y, z) != cb.get_block(x, y, z))
                    any_diff = true;
            }
        }
    }
    CHECK(any_diff);
}

TEST_CASE("1.7 generator: biomes are valid vanilla IDs") {
    TerrainParams params;
    params.seed = 42;
    ChunkGenerator gen(params);

    int invalid = 0;
    for (int x = 0; x < 100; x += 10) {
        for (int z = 0; z < 100; z += 10) {
            int biome = gen.get_biome(x, z);
            if (biome < 0 || biome > 200)
                invalid++;
        }
    }
    CHECK(invalid == 0);
}

TEST_CASE("1.7 generator: sea level produces water") {
    BlockRegistry::get_instance().initialize_default_blocks();

    TerrainParams params;
    params.seed = 42;
    params.sea_level = 63.0f;
    ChunkGenerator gen(params);

    ChunkData chunk;
    chunk.clear();

    // Generate a chunk well below sea level to ensure water presence
    // Use chunk_y = -4 so y range is [-128, -96], well below sea level 63
    gen.generate_chunk(chunk, 0, -4, 0, nullptr, false);

    bool found_water = false;
    // Check a horizontal slice near the top of the chunk
    for (int z = 0; z < CHUNK_DEPTH && !found_water; z += 8) {
        for (int x = 0; x < CHUNK_WIDTH && !found_water; x += 8) {
            if (chunk.get_block(x, CHUNK_HEIGHT - 1, z) == BlockIDs::WATER)
                found_water = true;
        }
    }
    CHECK(found_water);
}

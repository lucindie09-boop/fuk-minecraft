#include "doctest.h"
#include "lighting/light_propagation.hpp"
#include "lighting/block_light_region.hpp"
#include "core/chunk_data.hpp"
#include "core/chunk_map.hpp"
#include "core/chunk_types.hpp"
#include "core/block_types.hpp"
#include <cstring>

using namespace VoxelEngine;

TEST_CASE("emissive block lights neighbor") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.clear();
    chunk.set_block(16, 16, 16, BlockIDs::LIGHT_BLOCK);
    propagate_chunk_block_light_additive(chunk);
    CHECK(chunk.get_light_unsafe(16, 16, 16) > 0);
    CHECK(chunk.get_light_unsafe(16, 17, 16) > 0);
}

TEST_CASE("no emissive -> no light") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.clear();
    chunk.set_block(16, 16, 16, BlockIDs::STONE);
    propagate_chunk_block_light_additive(chunk);
    CHECK(chunk.get_light_unsafe(16, 16, 16) == 0);
}

TEST_CASE("LIGHT_BLOCK channel values") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.clear();
    chunk.set_block(16, 16, 16, BlockIDs::LIGHT_BLOCK);
    propagate_chunk_block_light_additive(chunk);
    CHECK(chunk.get_light_r_unsafe(16, 16, 16) == 15);
    CHECK(chunk.get_light_g_unsafe(16, 16, 16) == 15);
    CHECK(chunk.get_light_b_unsafe(16, 16, 16) == 15);
}

TEST_CASE("colored light channels") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.clear();
    chunk.set_block(16, 16, 16, BlockIDs::LIGHT_RED);
    propagate_chunk_block_light_additive(chunk);
    CHECK(chunk.get_light_r_unsafe(16, 16, 16) > 0);
    CHECK(chunk.get_light_g_unsafe(16, 16, 16) == 0);
    CHECK(chunk.get_light_b_unsafe(16, 16, 16) == 0);
}

TEST_CASE("wrap_local_to_region inside bounds") {
    int16_t nx = 5, ny = 10, nz = 15;
    int8_t rdx = 0, rdy = 0, rdz = 0;
    bool ok = wrap_local_to_region(nx, ny, nz, rdx, rdy, rdz);
    CHECK(ok);
    CHECK(nx == 5);
    CHECK(ny == 10);
    CHECK(nz == 15);
    CHECK(rdx == 0);
    CHECK(rdy == 0);
    CHECK(rdz == 0);
}

TEST_CASE("wrap_local_to_region wraps negative") {
    int16_t nx = -1, ny = 0, nz = 0;
    int8_t rdx = 0, rdy = 0, rdz = 0;
    bool ok = wrap_local_to_region(nx, ny, nz, rdx, rdy, rdz);
    CHECK(ok);
    CHECK(nx == 31);
    CHECK(rdx == -1);
}

// =========================================================================
// Cross-chunk BFS edge case tests
// =========================================================================

TEST_CASE("wrap_local_to_world crosses positive x boundary") {
    int32_t cx = 0, cy = 0, cz = 0;
    int16_t x = 32, y = 16, z = 16; // Out of bounds
    wrap_local_to_world(x, y, z, cx, cy, cz);
    CHECK(x == 0);
    CHECK(y == 16);
    CHECK(z == 16);
    CHECK(cx == 1);
    CHECK(cy == 0);
    CHECK(cz == 0);
}

TEST_CASE("wrap_local_to_world crosses negative x boundary") {
    int32_t cx = 0, cy = 0, cz = 0;
    int16_t x = -1, y = 16, z = 16; // Out of bounds
    wrap_local_to_world(x, y, z, cx, cy, cz);
    CHECK(x == 31);
    CHECK(y == 16);
    CHECK(z == 16);
    CHECK(cx == -1);
    CHECK(cy == 0);
    CHECK(cz == 0);
}

TEST_CASE("wrap_local_to_world crosses positive y boundary") {
    int32_t cx = 0, cy = 0, cz = 0;
    int16_t x = 16, y = 32, z = 16; // Out of bounds (CHUNK_HEIGHT is 32)
    wrap_local_to_world(x, y, z, cx, cy, cz);
    CHECK(x == 16);
    CHECK(y == 0);
    CHECK(z == 16);
    CHECK(cx == 0);
    CHECK(cy == 1);
    CHECK(cz == 0);
}

TEST_CASE("wrap_local_to_world crosses positive z boundary") {
    int32_t cx = 0, cy = 0, cz = 0;
    int16_t x = 16, y = 16, z = 32; // Out of bounds
    wrap_local_to_world(x, y, z, cx, cy, cz);
    CHECK(x == 16);
    CHECK(y == 16);
    CHECK(z == 0);
    CHECK(cx == 0);
    CHECK(cy == 0);
    CHECK(cz == 1);
}

TEST_CASE("wrap_local_to_world stays in bounds") {
    int32_t cx = 0, cy = 0, cz = 0;
    int16_t x = 16, y = 16, z = 16;
    wrap_local_to_world(x, y, z, cx, cy, cz);
    CHECK(x == 16);
    CHECK(y == 16);
    CHECK(z == 16);
    CHECK(cx == 0);
    CHECK(cy == 0);
    CHECK(cz == 0);
}


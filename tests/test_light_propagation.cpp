#include "doctest.h"
#include "lighting/light_propagation.hpp"
#include "lighting/block_light_region.hpp"
#include "core/chunk_data.hpp"
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

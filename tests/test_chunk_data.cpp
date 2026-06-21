#include "doctest.h"
#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include <cstring>

using namespace VoxelEngine;

TEST_CASE("light pack/unpack round-trip") {
    BlockRegistry::get_instance().initialize_default_blocks();
    uint16_t packed = pack_light(7, 3, 11, 15);
    CHECK(unpack_sky(packed) == 7);
    CHECK(unpack_r(packed) == 3);
    CHECK(unpack_g(packed) == 11);
    CHECK(unpack_b(packed) == 15);
}

TEST_CASE("light pack all-zero") {
    uint16_t packed = pack_light(0, 0, 0, 0);
    CHECK(packed == 0);
}

TEST_CASE("light pack all-max") {
    uint16_t packed = pack_light(15, 15, 15, 15);
    CHECK(packed == 0xFFFF);
}

TEST_CASE("set_block / get_block") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData cd;
    cd.set_block(5, 10, 20, BlockIDs::STONE);
    CHECK(cd.get_block(5, 10, 20) == BlockIDs::STONE);
    CHECK_FALSE(cd.is_all_air());
}

TEST_CASE("set_block out of bounds") {
    ChunkData cd;
    cd.set_block(-1, 0, 0, BlockIDs::STONE);
    cd.set_block(32, 0, 0, BlockIDs::STONE);
    cd.set_block(0, 32, 0, BlockIDs::STONE);
    cd.set_block(0, 0, 32, BlockIDs::STONE);
    CHECK(cd.is_all_air());
}

TEST_CASE("fill_blocks") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData cd;
    cd.fill_blocks(BlockIDs::STONE);
    CHECK_FALSE(cd.is_all_air());
    CHECK(cd.get_block(0, 0, 0) == BlockIDs::STONE);
    CHECK(cd.get_block(31, 31, 31) == BlockIDs::STONE);
    CHECK(cd.get_block_count() == static_cast<uint32_t>(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH));
}

TEST_CASE("clear") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData cd;
    cd.fill_blocks(BlockIDs::STONE);
    cd.clear();
    CHECK(cd.is_all_air());
    CHECK(cd.get_block(15, 15, 15) == BlockIDs::AIR);
}

TEST_CASE("set_data") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData cd;
    BlockID data[CHUNK_VOLUME];
    std::memset(data, 0, sizeof(data));
    data[0] = BlockIDs::GRASS;
    data[CHUNK_VOLUME - 1] = BlockIDs::STONE;
    cd.set_data(data, CHUNK_VOLUME);
    CHECK(cd.get_block(0, 0, 0) == BlockIDs::GRASS);
    CHECK(cd.get_block(31, 31, 31) == BlockIDs::STONE);
    CHECK(cd.get_block_count() == 2);
}

TEST_CASE("section counts") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData cd;
    cd.set_block(0, 0, 0, BlockIDs::STONE);
    cd.set_block(0, 17, 0, BlockIDs::STONE);
    CHECK_FALSE(cd.is_section_all_air(0));
    CHECK_FALSE(cd.is_section_all_air(1));
}

TEST_CASE("get_neighbor_block") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData cd;
    cd.set_block(10, 10, 10, BlockIDs::STONE);
    CHECK(cd.get_neighbor_block(10, 10, 10, 0, 0, 0) == BlockIDs::STONE);
    CHECK(cd.get_neighbor_block(10, 10, 10, 1, 0, 0) == BlockIDs::AIR);
    CHECK(cd.get_neighbor_block(0, 0, 0, -1, 0, 0) == BlockIDs::AIR);
}

TEST_CASE("fully_solid") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData cd;
    cd.fill_blocks(BlockIDs::STONE);
    cd.compute_fully_solid();
    CHECK(cd.fully_solid());
    cd.set_block(15, 15, 15, BlockIDs::AIR);
    cd.compute_fully_solid();
    CHECK_FALSE(cd.fully_solid());
}

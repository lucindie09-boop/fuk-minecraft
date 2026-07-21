#include "doctest.h"
#include "mesh/chunk_neighbor_accessor.hpp"
#include "core/block_types.hpp"

using namespace VoxelEngine;

TEST_CASE("ChunkNeighborAccessor center block in-bounds") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    center.set_block(10, 10, 10, BlockIDs::STONE);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    CHECK(acc.get_block(10, 10, 10) == BlockIDs::STONE);
    CHECK(acc.get_block(0, 0, 0) == BlockIDs::AIR);
}

TEST_CASE("ChunkNeighborAccessor null neighbor returns AIR") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    center.set_block(31, 15, 15, BlockIDs::STONE);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    CHECK(acc.get_block(32, 15, 15) == BlockIDs::AIR);
}

TEST_CASE("ChunkNeighborAccessor reads from pos_x neighbor") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData pos_x;
    pos_x.set_block(0, 15, 15, BlockIDs::STONE);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.pos_x = &pos_x;
    CHECK(acc.get_block(32, 15, 15) == BlockIDs::STONE);
}

TEST_CASE("ChunkNeighborAccessor reads from neg_x neighbor") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData neg_x;
    neg_x.set_block(31, 15, 15, BlockIDs::DIRT);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.neg_x = &neg_x;
    CHECK(acc.get_block(-1, 15, 15) == BlockIDs::DIRT);
}

TEST_CASE("ChunkNeighborAccessor reads from pos_y neighbor") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData pos_y;
    pos_y.set_block(10, 0, 10, BlockIDs::GRASS);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.pos_y = &pos_y;
    CHECK(acc.get_block(10, 32, 10) == BlockIDs::GRASS);
}

TEST_CASE("ChunkNeighborAccessor reads from neg_y neighbor") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData neg_y;
    neg_y.set_block(10, 31, 10, BlockIDs::BEDROCK);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.neg_y = &neg_y;
    CHECK(acc.get_block(10, -1, 10) == BlockIDs::BEDROCK);
}

TEST_CASE("ChunkNeighborAccessor reads from pos_z neighbor") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData pos_z;
    pos_z.set_block(10, 15, 0, BlockIDs::SAND);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.pos_z = &pos_z;
    CHECK(acc.get_block(10, 15, 32) == BlockIDs::SAND);
}

TEST_CASE("ChunkNeighborAccessor reads from neg_z neighbor") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData neg_z;
    neg_z.set_block(10, 15, 31, BlockIDs::SNOW);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.neg_z = &neg_z;
    CHECK(acc.get_block(10, 15, -1) == BlockIDs::SNOW);
}

TEST_CASE("ChunkNeighborAccessor corner neighbor (neg_x_neg_z)") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData neg_x_neg_z;
    neg_x_neg_z.set_block(31, 15, 31, BlockIDs::GRAVEL);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.neg_x_neg_z = &neg_x_neg_z;
    CHECK(acc.get_block(-1, 15, -1) == BlockIDs::GRAVEL);
}

TEST_CASE("ChunkNeighborAccessor corner neighbor (pos_x_pos_z)") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData pos_x_pos_z;
    pos_x_pos_z.set_block(0, 15, 0, BlockIDs::CACTUS);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.pos_x_pos_z = &pos_x_pos_z;
    CHECK(acc.get_block(32, 15, 32) == BlockIDs::CACTUS);
}

TEST_CASE("ChunkNeighborAccessor diagonal: y out + x out needs neg_y + neg_x_neg_y") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData neg_y;
    ChunkData neg_x_neg_y;
    neg_x_neg_y.set_block(31, 31, 15, BlockIDs::MUD);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.neg_y = &neg_y;
    acc.neg_x_neg_y = &neg_x_neg_y;
    CHECK(acc.get_block(-1, -1, 15) == BlockIDs::MUD);
}

TEST_CASE("ChunkNeighborAccessor triple-corner: all three axes out") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData neg_y;
    ChunkData pos_x_neg_y_neg_z;
    pos_x_neg_y_neg_z.set_block(0, 31, 31, BlockIDs::WET_SAND);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.neg_y = &neg_y;
    acc.pos_x_neg_y_neg_z = &pos_x_neg_y_neg_z;
    CHECK(acc.get_block(32, -1, -1) == BlockIDs::WET_SAND);
}

TEST_CASE("ChunkNeighborAccessor light_packed round-trip center") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    center.set_light(10, 10, 10, 15);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    uint16_t packed = acc.get_light_packed(10, 10, 10);
    CHECK(unpack_r(packed) == 15);
}

TEST_CASE("ChunkNeighborAccessor light_packed from neighbor") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkData pos_x;
    pos_x.set_light(0, 15, 15, 10);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    acc.pos_x = &pos_x;
    uint16_t packed = acc.get_light_packed(32, 15, 15);
    CHECK(unpack_r(packed) == 10);
}

TEST_CASE("ChunkNeighborAccessor is_occluder for solid block") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    center.set_block(5, 5, 5, BlockIDs::STONE);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    CHECK(acc.is_occluder(5, 5, 5));
}

TEST_CASE("ChunkNeighborAccessor is_occluder for air returns false") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkNeighborAccessor acc;
    acc.center = &center;
    CHECK_FALSE(acc.is_occluder(5, 5, 5));
}

TEST_CASE("ChunkNeighborAccessor is_occluder for transparent leaf") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    center.set_block(5, 5, 5, BlockIDs::LEAVES);
    ChunkNeighborAccessor acc;
    acc.center = &center;
    CHECK(acc.is_occluder(5, 5, 5));
}

TEST_CASE("ChunkNeighborAccessor out-of-bounds y returns AIR with null neighbor") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData center;
    ChunkNeighborAccessor acc;
    acc.center = &center;
    CHECK(acc.get_block(10, 32, 10) == BlockIDs::AIR);
    CHECK(acc.get_block(10, -1, 10) == BlockIDs::AIR);
}

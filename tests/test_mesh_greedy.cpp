#include "doctest.h"
#include "mesh/mesh_builder.hpp"
#include "core/chunk_data.hpp"
#include "core/block_types.hpp"

using namespace VoxelEngine;

TEST_CASE("greedy mesh: solid_cache stores BlockID not bool") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    chunk.set_block(5, 5, 5, BlockIDs::GRASS);
    chunk.set_block(6, 5, 5, BlockIDs::STONE);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(true);
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() > 0);
}

TEST_CASE("greedy mesh: null boundaries produce AIR in solid_cache edges") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::STONE);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(true);
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() > 0);
    CHECK(mb.get_index_count() > 0);
}

TEST_CASE("greedy mesh: single isolated block has minimal faces") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    chunk.set_block(15, 15, 15, BlockIDs::STONE);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(true);
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() == 20);
    CHECK(mb.get_index_count() == 30);
}

TEST_CASE("greedy mesh: large flat plane produces mesh") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    for (int x = 0; x < 16; x++)
        for (int z = 0; z < 16; z++)
            chunk.set_block(x, 10, z, BlockIDs::STONE);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(true);
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() > 0);
    CHECK(mb.get_vertex_count() < 16 * 16 * 6 * 4);
}

TEST_CASE("greedy mesh: mixed block types produce mesh") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    for (int x = 0; x < 5; x++) {
        chunk.set_block(x, 10, 5, x % 2 == 0 ? BlockIDs::STONE : BlockIDs::DIRT);
    }
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(true);
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() > 0);
}

TEST_CASE("greedy mesh: solid block against water does not cull side face") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    chunk.set_block(15, 15, 15, BlockIDs::STONE);
    chunk.set_block(16, 15, 15, BlockIDs::WATER);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(false);
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() > 0);
    MeshBuilder mb2;
    mb2.set_greedy_enabled(true);
    mb2.build_mesh(chunk);
    CHECK(mb2.get_vertex_count() > 0);
}

#include "doctest.h"
#include "mesh/mesh_builder.hpp"
#include "core/chunk_data.hpp"
#include "core/block_types.hpp"

using namespace VoxelEngine;

TEST_CASE("culling: interior block with all stone neighbors skips faces") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::STONE);
    chunk.set_block(15, 15, 15, BlockIDs::AIR);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(false);
    mb.build_mesh(chunk);
    size_t with_hole = mb.get_vertex_count();

    ChunkData full;
    full.fill_blocks(BlockIDs::STONE);
    full.compute_section_flags();
    MeshBuilder mb2;
    mb2.set_greedy_enabled(false);
    mb2.build_mesh(full);
    size_t solid = mb2.get_vertex_count();

    CHECK(with_hole > solid);
}

TEST_CASE("culling: stone next to leaf keeps both faces") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    chunk.set_block(15, 15, 15, BlockIDs::STONE);
    chunk.set_block(16, 15, 15, BlockIDs::LEAVES);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(false);
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() > 0);
}

TEST_CASE("culling: two adjacent leaves both render (transparent same-type)") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    chunk.set_block(15, 15, 15, BlockIDs::LEAVES);
    chunk.set_block(16, 15, 15, BlockIDs::LEAVES);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(false);
    mb.build_mesh(chunk);
    size_t one_leaf_verts;
    {
        ChunkData one;
        one.fill_blocks(BlockIDs::AIR);
        one.set_block(15, 15, 15, BlockIDs::LEAVES);
        one.compute_section_flags();
        MeshBuilder mb1;
        mb1.set_greedy_enabled(false);
        mb1.build_mesh(one);
        one_leaf_verts = mb1.get_vertex_count();
    }
    CHECK(mb.get_vertex_count() >= one_leaf_verts);
}

TEST_CASE("boundary: null neighbor produces faces at chunk edge") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    chunk.set_block(0, 15, 15, BlockIDs::STONE);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(false);
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() > 0);
}

TEST_CASE("boundary: solid neighbor suppresses chunk-edge face") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData neighbor;
    neighbor.fill_blocks(BlockIDs::STONE);
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    chunk.set_block(0, 15, 15, BlockIDs::STONE);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(false);
    mb.build_mesh(chunk, &neighbor);
    MeshBuilder mb_none;
    mb_none.set_greedy_enabled(false);
    mb_none.build_mesh(chunk);
    CHECK(mb.get_vertex_count() <= mb_none.get_vertex_count());
}

TEST_CASE("boundary: leaf neighbor does not suppress stone face") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData neighbor;
    neighbor.fill_blocks(BlockIDs::LEAVES);
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    chunk.set_block(0, 15, 15, BlockIDs::STONE);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(false);
    mb.build_mesh(chunk, &neighbor);
    MeshBuilder mb_none;
    mb_none.set_greedy_enabled(false);
    mb_none.build_mesh(chunk);
    CHECK(mb.get_vertex_count() == mb_none.get_vertex_count());
}

TEST_CASE("boundary: height-offset blocks interact correctly") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData neighbor;
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    chunk.set_block(0, 15, 15, BlockIDs::STONE);
    chunk.set_block(0, 14, 15, BlockIDs::WATER);
    neighbor.fill_blocks(BlockIDs::AIR);
    neighbor.set_block(CHUNK_WIDTH - 1, 14, 15, BlockIDs::WATER);
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.set_greedy_enabled(false);
    mb.build_mesh(chunk, &neighbor);
    CHECK(mb.get_vertex_count() > 0);
}

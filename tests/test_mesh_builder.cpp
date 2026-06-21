#include "doctest.h"
#include "mesh/mesh_builder.hpp"
#include "core/chunk_data.hpp"
#include "core/block_types.hpp"

using namespace VoxelEngine;

TEST_CASE("build mesh for 2x2x2 stone cube") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::AIR);
    for (int dx = 0; dx < 2; dx++) {
        for (int dy = 0; dy < 2; dy++) {
            for (int dz = 0; dz < 2; dz++) {
                chunk.set_block(15 + dx, 15 + dy, 15 + dz, BlockIDs::STONE);
            }
        }
    }
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() > 0);
    CHECK(mb.get_index_count() > 0);
    CHECK(mb.get_index_count() % 3 == 0);
    CHECK(mb.get_triangle_count() > 0);
}

TEST_CASE("build mesh for empty chunk") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.clear();
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() == 0);
    CHECK(mb.get_index_count() == 0);
}

TEST_CASE("build mesh for fully solid chunk") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData chunk;
    chunk.fill_blocks(BlockIDs::STONE);
    chunk.compute_fully_solid();
    chunk.compute_section_flags();
    MeshBuilder mb;
    mb.build_mesh(chunk);
    CHECK(mb.get_vertex_count() > 0);
    CHECK(mb.get_triangle_count() > 0);
}

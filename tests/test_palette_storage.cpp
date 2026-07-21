#include "doctest.h"
#include "core/chunk_data.hpp"
#include "core/block_types.hpp"

using namespace VoxelEngine;

TEST_CASE("PaletteStorage uniform section stays uniform on same value write") {
    PaletteStorage ps;
    ps.set_block(0, 0, 0, BlockIDs::AIR);
    auto& s = ps.block_secs[0];
    CHECK(s.is_uniform());
    CHECK(s.uniform_val() == static_cast<uint16_t>(BlockIDs::AIR));
}

TEST_CASE("PaletteStorage uniform -> non-uniform upgrade on different value") {
    PaletteStorage ps;
    ps.set_block(0, 0, 0, BlockIDs::AIR);
    ps.set_block(0, 0, 1, BlockIDs::STONE);
    auto& s = ps.block_secs[0];
    CHECK_FALSE(s.is_uniform());
    CHECK(s.bpi == 4);
    CHECK(ps.get_block(0, 0, 0) == BlockIDs::AIR);
    CHECK(ps.get_block(0, 0, 1) == BlockIDs::STONE);
}

TEST_CASE("PaletteStorage section upgrade 4-bit -> 8-bit") {
    PaletteStorage ps;
    for (int i = 1; i <= 17; i++)
        ps.set_block(i - 1, 0, 0, static_cast<BlockID>(i));
    CHECK(ps.block_secs[0].bpi == 8);
    for (int i = 0; i < 17; i++)
        CHECK(ps.get_block(i, 0, 0) == static_cast<BlockID>(i + 1));
}

TEST_CASE("PaletteStorage section upgrade 8-bit -> 16-bit") {
    PaletteStorage ps;
    for (int i = 1; i <= 257; i++) {
        int x = (i - 1) % 16, y = (i - 1) / 16;
        ps.set_block(x, y, 0, static_cast<BlockID>(i));
    }
    CHECK(ps.block_secs[0].bpi == 16);
    for (int i = 1; i <= 257; i++) {
        int x = (i - 1) % 16, y = (i - 1) / 16;
        CHECK(ps.get_block(x, y, 0) == static_cast<BlockID>(i));
    }
}

TEST_CASE("PaletteStorage sec_index and sec_local cross-section boundaries") {
    CHECK(PaletteStorage::sec_index(0, 0, 0) == 0);
    CHECK(PaletteStorage::sec_index(16, 0, 0) == 1);
    CHECK(PaletteStorage::sec_index(0, 16, 0) == 2);
    CHECK(PaletteStorage::sec_index(0, 0, 16) == 4);
    CHECK(PaletteStorage::sec_index(31, 31, 31) == 7);

    CHECK(PaletteStorage::sec_local(16, 0, 0) == 0);
    CHECK(PaletteStorage::sec_local(0, 16, 0) == 0);
    CHECK(PaletteStorage::sec_local(0, 0, 16) == 0);
    CHECK(PaletteStorage::sec_local(31, 31, 31) == 15 + 15 * 16 + 15 * 16 * 16);
}

TEST_CASE("PaletteStorage Y-index formula for count_section_blocks") {
    PaletteStorage ps;
    ps.fill_blocks_uniform(BlockIDs::AIR);
    ps.set_block(0, 0, 0, BlockIDs::STONE);
    ps.set_block(0, 17, 0, BlockIDs::STONE);

    uint32_t sec_counts[CHUNK_SECTIONS]{};
    ps.count_section_blocks(sec_counts);
    CHECK(sec_counts[0] == 1);
    CHECK(sec_counts[1] == 1);
    for (int i = 2; i < CHUNK_SECTIONS; i++)
        CHECK(sec_counts[i] == 0);
}

TEST_CASE("PaletteStorage light words round-trip") {
    PaletteStorage ps;
    uint16_t packed = pack_light(5, 10, 3, 12);
    ps.set_light_word(10, 20, 15, packed);
    CHECK(ps.get_light_word(10, 20, 15) == packed);
}

TEST_CASE("PaletteStorage light uniform fill") {
    PaletteStorage ps;
    uint16_t packed = pack_light(15, 7, 7, 7);
    ps.fill_light_uniform(packed);
    for (int z = 0; z < 32; z += 16)
        for (int y = 0; y < 32; y += 16)
            for (int x = 0; x < 32; x += 16)
                CHECK(ps.get_light_word(x, y, z) == packed);
}

TEST_CASE("ChunkData clear_light zeroes light but preserves blocks") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData cd;
    cd.fill_blocks(BlockIDs::STONE);
    cd.set_light(10, 10, 10, 15);
    CHECK(cd.get_light(10, 10, 10) == 15);
    cd.clear_light();
    CHECK(cd.get_light(10, 10, 10) == 0);
    CHECK(cd.get_block(10, 10, 10) == BlockIDs::STONE);
}

TEST_CASE("ChunkData clear_block_light only clears block light") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData cd;
    cd.set_sky_light(5, 5, 5, 12);
    cd.set_light(5, 5, 5, 8);
    cd.clear_block_light();
    CHECK(cd.get_sky_light(5, 5, 5) == 12);
    CHECK(cd.get_light(5, 5, 5) == 0);
}

TEST_CASE("ChunkData clear_sky_light only clears sky light") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData cd;
    cd.set_sky_light(5, 5, 5, 12);
    cd.set_light(5, 5, 5, 8);
    cd.clear_sky_light();
    CHECK(cd.get_sky_light(5, 5, 5) == 0);
    CHECK(cd.get_light(5, 5, 5) == 8);
}

TEST_CASE("PaletteStorage fill_blocks_uniform overwrites non-uniform") {
    PaletteStorage ps;
    for (int i = 0; i < 20; i++)
        ps.set_block(i, 0, 0, static_cast<BlockID>(i + 50));
    CHECK_FALSE(ps.block_secs[0].is_uniform());
    ps.fill_blocks_uniform(BlockIDs::STONE);
    for (int i = 0; i < 20; i++)
        CHECK(ps.get_block(i, 0, 0) == BlockIDs::STONE);
    for (auto& s : ps.block_secs)
        CHECK(s.is_uniform());
}

TEST_CASE("PaletteStorage count_non_air") {
    PaletteStorage ps;
    ps.fill_blocks_uniform(BlockIDs::AIR);
    CHECK(ps.count_non_air() == 0);
    ps.fill_blocks_uniform(BlockIDs::STONE);
    CHECK(ps.count_non_air() == 8 * 4096);
}

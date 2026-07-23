#include "doctest.h"
#include "core/chunk_data.hpp"
#include "core/chunk_coords.hpp"
#include "core/block_types.hpp"
#include "core/crc32.hpp"
#include "core/rle_codec.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

using namespace VoxelEngine;

static void write_v3_header(std::ofstream& f, uint32_t checksum) {
    uint32_t w = CHUNK_WIDTH, h = CHUNK_HEIGHT, d = CHUNK_DEPTH, v = 3;
    f.write(reinterpret_cast<const char*>(&w), 4);
    f.write(reinterpret_cast<const char*>(&h), 4);
    f.write(reinterpret_cast<const char*>(&d), 4);
    f.write(reinterpret_cast<const char*>(&v), 4);
    f.write(reinterpret_cast<const char*>(&checksum), 4);
}

static bool read_v3_file(const char* path, ChunkData& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    uint32_t w, h, d, v, expected_crc;
    f.read(reinterpret_cast<char*>(&w), 4);
    f.read(reinterpret_cast<char*>(&h), 4);
    f.read(reinterpret_cast<char*>(&d), 4);
    f.read(reinterpret_cast<char*>(&v), 4);
    f.read(reinterpret_cast<char*>(&expected_crc), 4);

    if (w != CHUNK_WIDTH || h != CHUNK_HEIGHT || d != CHUNK_DEPTH) return false;
    if (v != 3) return false;

    // Read rest of file as body
    std::streampos body_start = f.tellg();
    f.seekg(0, std::ios::end);
    size_t body_size = static_cast<size_t>(f.tellg() - body_start);
    f.seekg(body_start);

    std::vector<uint8_t> body(body_size);
    f.read(reinterpret_cast<char*>(body.data()), body_size);

    uint32_t actual_crc = crc32(body.data(), body_size);
    if (actual_crc != expected_crc) return false;

    return decode_chunk_rle(body.data(), body_size, out);
}

TEST_CASE("v3 round-trip preserves all block data") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData original;
    original.clear();

    original.set_block(0, 0, 0, BlockIDs::STONE);
    original.set_block(31, 31, 31, BlockIDs::GRASS);
    original.set_block(16, 16, 16, BlockIDs::LIGHT_BLOCK);
    original.set_block(5, 20, 10, BlockIDs::SAND);
    original.set_block(25, 1, 29, BlockIDs::WOOD);

    std::vector<uint8_t> body;
    encode_chunk_rle(original, body);
    uint32_t checksum = crc32(body.data(), body.size());

    const char* path = "test_persistence_roundtrip.chunk";
    {
        std::ofstream f(path, std::ios::binary);
        write_v3_header(f, checksum);
        f.write(reinterpret_cast<const char*>(body.data()), body.size());
    }

    ChunkData loaded;
    loaded.clear();
    bool ok = read_v3_file(path, loaded);
    CHECK(ok);
    CHECK(loaded.get_block(0, 0, 0) == BlockIDs::STONE);
    CHECK(loaded.get_block(31, 31, 31) == BlockIDs::GRASS);
    CHECK(loaded.get_block(16, 16, 16) == BlockIDs::LIGHT_BLOCK);
    CHECK(loaded.get_block(5, 20, 10) == BlockIDs::SAND);
    CHECK(loaded.get_block(25, 1, 29) == BlockIDs::WOOD);

    std::remove(path);
}

TEST_CASE("CRC32 mismatch rejects corrupted file") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData original;
    original.clear();
    original.set_block(10, 10, 10, BlockIDs::STONE);

    std::vector<uint8_t> body;
    encode_chunk_rle(original, body);
    uint32_t checksum = crc32(body.data(), body.size());

    const char* path = "test_persistence_crc_mismatch.chunk";
    {
        std::ofstream f(path, std::ios::binary);
        write_v3_header(f, checksum);
        f.write(reinterpret_cast<const char*>(body.data()), body.size());
    }

    // Corrupt one byte in the body
    {
        std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
        f.seekg(sizeof(uint32_t) * 5 + 10);
        char bad = 0xFF;
        f.write(&bad, 1);
    }

    ChunkData loaded;
    loaded.clear();
    bool ok = read_v3_file(path, loaded);
    CHECK(!ok);

    std::remove(path);
}

TEST_CASE("backup fallback on primary CRC failure") {
    BlockRegistry::get_instance().initialize_default_blocks();

    // Write a valid backup file
    ChunkData backup_data;
    backup_data.clear();
    backup_data.set_block(5, 5, 5, BlockIDs::GRASS);

    std::vector<uint8_t> backup_body;
    encode_chunk_rle(backup_data, backup_body);
    uint32_t backup_crc = crc32(backup_body.data(), backup_body.size());

    const char* primary = "test_persistence_backup_fallback.chunk";
    const char* backup = "test_persistence_backup_fallback.chunk.bak";

    {
        std::ofstream f(backup, std::ios::binary);
        write_v3_header(f, backup_crc);
        f.write(reinterpret_cast<const char*>(backup_body.data()), backup_body.size());
    }

    // Write a corrupted primary file
    {
        std::ofstream f(primary, std::ios::binary);
        write_v3_header(f, 0xDEADBEEF);
        f.write("garbage", 7);
    }

    // Simulate the load-with-fallback logic: try primary, fall back to backup
    ChunkData loaded;
    loaded.clear();

    bool ok = read_v3_file(primary, loaded);
    if (!ok) {
        ok = read_v3_file(backup, loaded);
    }

    CHECK(ok);
    CHECK(loaded.get_block(5, 5, 5) == BlockIDs::GRASS);

    std::remove(primary);
    std::remove(backup);
}

TEST_CASE("both corrupted files returns false") {
    BlockRegistry::get_instance().initialize_default_blocks();

    const char* primary = "test_persistence_both_corrupt.chunk";
    const char* backup = "test_persistence_both_corrupt.chunk.bak";

    // Write corrupted primary
    {
        std::ofstream f(primary, std::ios::binary);
        write_v3_header(f, 0xCAFEBABE);
        f.write("bad", 3);
    }
    // Write corrupted backup
    {
        std::ofstream f(backup, std::ios::binary);
        write_v3_header(f, 0xDEADBEEF);
        f.write("worse", 5);
    }

    ChunkData loaded;
    loaded.clear();

    bool ok = read_v3_file(primary, loaded);
    if (!ok) {
        ok = read_v3_file(backup, loaded);
    }

    CHECK(!ok);

    std::remove(primary);
    std::remove(backup);
}

TEST_CASE("RLE pipeline round-trip all blocks") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData original;
    original.clear();

    // Fill with a pattern: each block type at known positions
    BlockID blocks[] = {BlockIDs::AIR, BlockIDs::STONE, BlockIDs::GRASS,
                        BlockIDs::DIRT, BlockIDs::SAND, BlockIDs::WOOD,
                        BlockIDs::LIGHT_BLOCK, BlockIDs::LIGHT_RED,
                        BlockIDs::LIGHT_BLUE, BlockIDs::LEAVES};
    int num_blocks = sizeof(blocks) / sizeof(blocks[0]);

    for (int i = 0; i < num_blocks; i++) {
        int x = (i * 7) % CHUNK_WIDTH;
        int y = (i * 5) % CHUNK_HEIGHT;
        int z = (i * 3) % CHUNK_DEPTH;
        original.set_block(x, y, z, blocks[i]);
    }

    std::vector<uint8_t> body;
    encode_chunk_rle(original, body);

    ChunkData decoded;
    decoded.clear();
    bool ok = decode_chunk_rle(body.data(), body.size(), decoded);
    CHECK(ok);

    for (int i = 0; i < num_blocks; i++) {
        int x = (i * 7) % CHUNK_WIDTH;
        int y = (i * 5) % CHUNK_HEIGHT;
        int z = (i * 3) % CHUNK_DEPTH;
        CHECK(decoded.get_block(x, y, z) == blocks[i]);
    }
}

TEST_CASE("dimension mismatch rejects file") {
    const char* path = "test_persistence_dim_mismatch.chunk";
    {
        std::ofstream f(path, std::ios::binary);
        uint32_t w = 16, h = 16, d = 16, v = 3, crc = 0;
        f.write(reinterpret_cast<const char*>(&w), 4);
        f.write(reinterpret_cast<const char*>(&h), 4);
        f.write(reinterpret_cast<const char*>(&d), 4);
        f.write(reinterpret_cast<const char*>(&v), 4);
        f.write(reinterpret_cast<const char*>(&crc), 4);
    }

    ChunkData loaded;
    loaded.clear();
    bool ok = read_v3_file(path, loaded);
    CHECK(!ok);

    std::remove(path);
}

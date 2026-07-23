#include "doctest.h"
#include "core/chunk_data.hpp"
#include "core/chunk_coords.hpp"
#include "core/block_types.hpp"
#include "core/crc32.hpp"
#include "core/rle_codec.hpp"
#include "core/chunk_persistence.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

using namespace VoxelEngine;

// Write a v3 chunk file to disk (stand-in for Godot FileAccess).
static void write_v3_file(const char* path, const uint8_t* body, size_t body_size, uint32_t checksum) {
    std::ofstream f(path, std::ios::binary);
    uint32_t w = CHUNK_WIDTH, h = CHUNK_HEIGHT, d = CHUNK_DEPTH, v = 3;
    f.write(reinterpret_cast<const char*>(&w), 4);
    f.write(reinterpret_cast<const char*>(&h), 4);
    f.write(reinterpret_cast<const char*>(&d), 4);
    f.write(reinterpret_cast<const char*>(&v), 4);
    f.write(reinterpret_cast<const char*>(&checksum), 4);
    f.write(reinterpret_cast<const char*>(body), body_size);
}

// Read a v3 chunk file from disk and return body + CRC (stand-in for Godot FileAccess).
// Returns true on success (header parsed, body read).
static bool read_v3_file_raw(const char* path, std::vector<uint8_t>& body, uint32_t& crc) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;

    uint32_t w, h, d, v;
    f.read(reinterpret_cast<char*>(&w), 4);
    f.read(reinterpret_cast<char*>(&h), 4);
    f.read(reinterpret_cast<char*>(&d), 4);
    f.read(reinterpret_cast<char*>(&v), 4);
    f.read(reinterpret_cast<char*>(&crc), 4);

    if (w != CHUNK_WIDTH || h != CHUNK_HEIGHT || d != CHUNK_DEPTH || v != 3) return false;

    std::streampos body_start = f.tellg();
    f.seekg(0, std::ios::end);
    size_t body_size = static_cast<size_t>(f.tellg() - body_start);
    f.seekg(body_start);

    body.resize(body_size);
    f.read(reinterpret_cast<char*>(body.data()), body_size);
    return true;
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
    write_v3_file(path, body.data(), body.size(), checksum);

    std::vector<uint8_t> read_body;
    uint32_t read_crc;
    bool file_ok = read_v3_file_raw(path, read_body, read_crc);
    CHECK(file_ok);

    ChunkData loaded;
    loaded.clear();
    ChunkLoadOutcome outcome = decode_v3_chunk(
        read_body.data(), read_body.size(), read_crc,
        nullptr, 0, 0,
        loaded);
    CHECK(outcome == ChunkLoadOutcome::LOADED_OK);
    CHECK(loaded.get_block(0, 0, 0) == BlockIDs::STONE);
    CHECK(loaded.get_block(31, 31, 31) == BlockIDs::GRASS);
    CHECK(loaded.get_block(16, 16, 16) == BlockIDs::LIGHT_BLOCK);
    CHECK(loaded.get_block(5, 20, 10) == BlockIDs::SAND);
    CHECK(loaded.get_block(25, 1, 29) == BlockIDs::WOOD);

    std::remove(path);
}

TEST_CASE("CRC32 mismatch returns BOTH_CORRUPTED") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData original;
    original.clear();
    original.set_block(10, 10, 10, BlockIDs::STONE);

    std::vector<uint8_t> body;
    encode_chunk_rle(original, body);
    uint32_t checksum = crc32(body.data(), body.size());

    const char* path = "test_persistence_crc_mismatch.chunk";
    write_v3_file(path, body.data(), body.size(), checksum);

    // Corrupt one byte in the body
    {
        std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
        f.seekg(sizeof(uint32_t) * 5 + 10);
        char bad = 0xFF;
        f.write(&bad, 1);
    }

    std::vector<uint8_t> read_body;
    uint32_t read_crc;
    read_v3_file_raw(path, read_body, read_crc);

    // Corrupt the body we pass to decode (simulating what was read from disk)
    read_body[10] = 0xFF;

    ChunkData loaded;
    loaded.clear();
    ChunkLoadOutcome outcome = decode_v3_chunk(
        read_body.data(), read_body.size(), read_crc,
        nullptr, 0, 0,
        loaded);
    CHECK(outcome == ChunkLoadOutcome::BOTH_CORRUPTED);

    std::remove(path);
}

TEST_CASE("backup fallback via decode_v3_chunk") {
    BlockRegistry::get_instance().initialize_default_blocks();

    // Encode a valid backup
    ChunkData backup_data;
    backup_data.clear();
    backup_data.set_block(5, 5, 5, BlockIDs::GRASS);

    std::vector<uint8_t> backup_body;
    encode_chunk_rle(backup_data, backup_body);
    uint32_t backup_crc = crc32(backup_body.data(), backup_body.size());

    // Corrupt the primary body
    std::vector<uint8_t> primary_body = backup_body;
    primary_body[0] ^= 0xFF; // flip a byte
    uint32_t primary_crc = backup_crc ^ 0xDEAD; // wrong CRC

    ChunkData loaded;
    loaded.clear();
    ChunkLoadOutcome outcome = decode_v3_chunk(
        primary_body.data(), primary_body.size(), primary_crc,
        backup_body.data(), backup_body.size(), backup_crc,
        loaded);

    CHECK(outcome == ChunkLoadOutcome::RECOVERED_FROM_BACKUP);
    CHECK(loaded.get_block(5, 5, 5) == BlockIDs::GRASS);
}

TEST_CASE("both corrupted returns BOTH_CORRUPTED") {
    BlockRegistry::get_instance().initialize_default_blocks();

    uint8_t garbage[] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t bad_crc = 0xDEADBEEF;

    ChunkData loaded;
    loaded.clear();
    ChunkLoadOutcome outcome = decode_v3_chunk(
        garbage, sizeof(garbage), bad_crc,
        garbage, sizeof(garbage), bad_crc,
        loaded);
    CHECK(outcome == ChunkLoadOutcome::BOTH_CORRUPTED);
}

TEST_CASE("RLE pipeline round-trip all block types") {
    BlockRegistry::get_instance().initialize_default_blocks();
    ChunkData original;
    original.clear();

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

TEST_CASE("dimension mismatch detected by file reader") {
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

    std::vector<uint8_t> body;
    uint32_t crc;
    bool ok = read_v3_file_raw(path, body, crc);
    CHECK(!ok);

    std::remove(path);
}

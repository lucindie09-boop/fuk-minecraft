#include "doctest.h"
#include "core/chunk_map.hpp"

using namespace VoxelEngine;

TEST_CASE("key encode/decode round-trip (0,0,0)") {
    ChunkMap cm;
    uint64_t key = cm.get_chunk_key(0, 0, 0);
    int32_t x, y, z;
    ChunkMap::decode_chunk_key(key, x, y, z);
    CHECK(x == 0);
    CHECK(y == 0);
    CHECK(z == 0);
}

TEST_CASE("key encode/decode round-trip (1,-1,2)") {
    ChunkMap cm;
    uint64_t key = cm.get_chunk_key(1, -1, 2);
    int32_t x, y, z;
    ChunkMap::decode_chunk_key(key, x, y, z);
    CHECK(x == 1);
    CHECK(y == -1);
    CHECK(z == 2);
}

TEST_CASE("key encode/decode round-trip (100000,-50000,0)") {
    ChunkMap cm;
    uint64_t key = cm.get_chunk_key(100000, -50000, 0);
    int32_t x, y, z;
    ChunkMap::decode_chunk_key(key, x, y, z);
    CHECK(x == 100000);
    CHECK(y == -50000);
    CHECK(z == 0);
}

TEST_CASE("key uniqueness") {
    ChunkMap cm;
    uint64_t k1 = cm.get_chunk_key(1, 2, 3);
    uint64_t k2 = cm.get_chunk_key(1, 2, 4);
    CHECK(k1 != k2);
}

TEST_CASE("key symmetry (x vs z different axis)") {
    ChunkMap cm;
    uint64_t k_xz = cm.get_chunk_key(5, 0, 7);
    uint64_t k_zx = cm.get_chunk_key(7, 0, 5);
    CHECK(k_xz != k_zx);
}

TEST_CASE("key encode/decode near max range") {
    ChunkMap cm;
    int32_t max_val = 1000000;
    uint64_t key = cm.get_chunk_key(max_val, -max_val, max_val);
    int32_t x, y, z;
    ChunkMap::decode_chunk_key(key, x, y, z);
    CHECK(x == max_val);
    CHECK(y == -max_val);
    CHECK(z == max_val);
}

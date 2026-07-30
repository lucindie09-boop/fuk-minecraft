#include "doctest.h"
#include "worldgen/chunk_generator.hpp"
#include "worldgen/terrain_spline.hpp"
#include "core/thread_pool.hpp"
#include "core/block_types.hpp"
#include "core/chunk_data.hpp"
#include "core/chunk_coords.hpp"
#include "mesh/mesh_builder.hpp"
#include "lighting/block_light_region.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <array>
#include <cmath>

using namespace VoxelEngine;

// =========================================================================
// Concurrent generation stress test
// =========================================================================
// Spawns multiple threads that each create their own ChunkGenerator and
// generate chunks simultaneously from scratch. This exercises:
//   - Thread-safe ClimateSampler (shared per-thread noise arrays)
//   - std::call_once in the 3-arg spline wrappers (static init)
//   - Per-instance spline stacks in ChunkGenerator constructor
//   - General thread safety of the generation pipeline
// =========================================================================

static bool is_valid_block(BlockID id) {
    return id >= 0 && id < static_cast<BlockID>(BlockRegistry::get_instance().get_count());
}

TEST_CASE("concurrent generation: cold-start multi-threaded chunk generation") {
    BlockRegistry::get_instance().initialize_default_blocks();

    constexpr int kNumChunksPerWorker = 8;
    constexpr int kNumWorkers = 4;
    constexpr int kTotalChunks = kNumChunksPerWorker * kNumWorkers;

    struct Result {
        int index;
        bool generated;
        int block_count;
        bool has_error;
    };

    std::vector<Result> results(kTotalChunks);
    std::atomic<int> next_index{0};
    std::atomic<int> errors{0};

    // Workers share the pool but each gets its own generator
    ThreadPool pool(kNumWorkers);

    for (int w = 0; w < kNumWorkers; w++) {
        int worker_id = w;
        pool.fire_and_forget([&results, &next_index, &errors, worker_id, kNumChunksPerWorker, kNumWorkers]() {
            // Each worker creates its own generator — cold start
            TerrainParams params;
            params.seed = 42 + worker_id * 1000;
            ChunkGenerator gen(params);

            for (int i = 0; i < kNumChunksPerWorker; i++) {
                int idx = next_index.fetch_add(1, std::memory_order_relaxed);
                if (idx >= static_cast<int>(results.size()))
                    return;

                int32_t cx = static_cast<int32_t>(worker_id * 10 + i);
                int32_t cz = static_cast<int32_t>(worker_id * 7 + i * 3);

                ChunkData cd;
                cd.clear();

                gen.generate_chunk(cd, cx, 0, cz, nullptr, false);

                int bc = static_cast<int>(cd.get_block_count());
                bool ok = (bc > 0 && bc <= CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH);

                if (ok) {
                    // Spot-check a few blocks for valid IDs
                    for (int y = 0; y < CHUNK_HEIGHT && ok; y += 8) {
                        for (int z = 0; z < CHUNK_DEPTH && ok; z += 8) {
                            for (int x = 0; x < CHUNK_WIDTH && ok; x += 8) {
                                BlockID id = cd.get_block(x, y, z);
                                if (!is_valid_block(id))
                                    ok = false;
                            }
                        }
                    }
                }

                if (!ok)
                    errors.fetch_add(1, std::memory_order_relaxed);

                results[idx] = {idx, true, bc, !ok};
            }
        });
    }

    pool.shutdown();

    int total_blocks = 0;
    int generated_count = 0;
    for (auto& r : results) {
        if (r.generated) {
            generated_count++;
            total_blocks += r.block_count;
        }
    }

    INFO("Workers completed: ", kNumWorkers);
    INFO("Chunks generated:  ", generated_count);
    INFO("Total blocks:      ", total_blocks);
    INFO("Errors:            ", errors.load());

    CHECK(generated_count == kTotalChunks);
    CHECK(errors.load() == 0);
    CHECK(total_blocks > 0);

    // Verify that different workers produce different terrain (different seeds)
    // by checking the first chunk of each worker against the others
    for (int a = 0; a < kNumWorkers; a++) {
        for (int b = a + 1; b < kNumWorkers; b++) {
            int idx_a = a * kNumChunksPerWorker;
            int idx_b = b * kNumChunksPerWorker;
            // Different seed + different position should give different results
            // (at minimum, not both empty)
            CHECK((results[idx_a].block_count > 0 || results[idx_b].block_count > 0));
        }
    }
}

// =========================================================================
// Cold-start spline call_once hammer
// =========================================================================
// Fires many concurrent calls to the 3-arg spline wrappers (which use
// std::call_once for static init) before any per-instance spline roots
// have been constructed. This tests that the call_once guard works
// correctly under concurrent fire.
// =========================================================================

TEST_CASE("concurrent generation: spline call_once concurrent hammer") {
    struct SplineResult {
        float offset;
        float factor;
        float jaggedness;
        bool valid;
    };

    constexpr int kNumCalls = 200;
    std::vector<SplineResult> results(kNumCalls);
    std::atomic<int> next_idx{0};
    std::atomic<int> errors{0};
    std::atomic<bool> any_negative_jaggedness{false};

    {
        ThreadPool pool(4);
        float triples[][3] = {
            {0.5f, 0.0f, 0.0f},
            {0.8f, 0.5f, 0.0f},
            {0.3f, -0.5f, 0.3f},
            {0.03f, -0.6f, -0.2f},
            {-0.15f, 0.0f, 0.0f},
            {0.65f, -0.78f, 0.0f},
            {0.5f, -0.8f, 0.5f},
        };

        for (int i = 0; i < kNumCalls; i++) {
            pool.fire_and_forget([&results, &next_idx, &errors, triples, i]() {
                int idx = next_idx.fetch_add(1, std::memory_order_relaxed);
                if (idx >= static_cast<int>(results.size()))
                    return;

                auto& t = triples[i % 7];
                float o = compute_terrain_offset(t[0], t[1], t[2]);
                float f = compute_factor(t[0], t[1], t[2]);
                float j = compute_jaggedness(t[0], t[1], t[2]);

                bool ok = true;
                if (!std::isfinite(o) || !std::isfinite(f) || !std::isfinite(j))
                    ok = false;
                if (f < 0.0f || f > 10.0f)
                    ok = false;
                if (j < 0.0f || j > 5.0f)
                    ok = false;
                if (o < -0.5f || o > 0.5f)
                    ok = false;

                if (!ok)
                    errors.fetch_add(1, std::memory_order_relaxed);

                results[idx] = {o, f, j, ok};
            });
        }
        // pool destructor runs shutdown
    }

    int valid_count = 0;
    for (auto& r : results) {
        if (r.valid)
            valid_count++;
    }

    INFO("call_once calls:  ", kNumCalls);
    INFO("Valid results:    ", valid_count);
    INFO("Errors:           ", errors.load());

    CHECK(valid_count == kNumCalls);
    CHECK(errors.load() == 0);
}

// =========================================================================
// Mixed: concurrent per-instance + static spline access
// =========================================================================
// Some threads use per-instance spline roots (4-arg overloads), others
// concurrently call the 3-arg static wrappers. This exercises the exact
// scenario that would occur in a real worldgen pipeline where some paths
// use ChunkGenerator (per-instance) and others use standalone calls.
// =========================================================================

// =========================================================================
// Downstream verification: cave carving, meshing, lighting
// =========================================================================

TEST_CASE("downstream: cave carving produces valid chunks") {
    BlockRegistry::get_instance().initialize_default_blocks();

    TerrainParams params;
    ChunkGenerator gen(params);

    // Generate several chunks at different positions to likely hit a cave
    int min_blocks = CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH;
    int max_blocks = 0;
    bool found_air_below_surface = false;

    for (int dx = 0; dx < 4; dx++) {
        for (int dz = 0; dz < 4; dz++) {
            ChunkData cd;
            cd.clear();
            gen.generate_chunk(cd, dx, 0, dz, nullptr, false);
            int bc = static_cast<int>(cd.get_block_count());
            min_blocks = std::min(min_blocks, bc);
            max_blocks = std::max(max_blocks, bc);

            // Check for AIR blocks at stone depth (below surface, above bedrock)
            for (int32_t y = 10; y < 50 && !found_air_below_surface; y++) {
                for (int32_t x = 0; x < CHUNK_WIDTH && !found_air_below_surface; x += 8) {
                    for (int32_t z = 0; z < CHUNK_DEPTH && !found_air_below_surface; z += 8) {
                        if (cd.get_block(x, y, z) == BlockIDs::AIR &&
                            cd.get_block(x, y - 1, z) != BlockIDs::AIR &&
                            cd.get_block(x, y + 1, z) != BlockIDs::AIR) {
                            found_air_below_surface = true;
                        }
                    }
                }
            }
        }
    }

    INFO("16 chunks: min_blocks=", min_blocks,
         " max_blocks=", max_blocks);
    INFO("Found AIR surrounded by solid below surface: ", found_air_below_surface);

    CHECK(min_blocks > 0);
    CHECK(max_blocks <= CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH);
    // Cave carving should produce at least one AIR pocket in 16 chunks
    CHECK(found_air_below_surface);
}

// =========================================================================
// Meshing cave terrain
// =========================================================================
// Verifies that MeshBuilder can process a cave-carved chunk without
// producing degenerate geometry.
// =========================================================================

TEST_CASE("downstream: mesh builder handles cave terrain") {
    BlockRegistry::get_instance().initialize_default_blocks();

    TerrainParams params;
    ChunkGenerator gen(params);

    ChunkData chunk;
    chunk.clear();
    gen.generate_chunk(chunk, 0, 0, 0, nullptr, false);

    // Get a couple neighbors (empty/AIR neighbors) for clean boundaries
    ChunkData neighbor;
    neighbor.clear();

    MeshBuilder mb;
    mb.build_mesh(
        chunk,
        &neighbor, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        nullptr, nullptr
    );

    size_t verts = mb.get_vertex_count();
    size_t idx = mb.get_index_count();

    INFO("Cave chunk: ", chunk.get_block_count(), " blocks, ",
         verts, " verts, ", idx, " indices");

    // A non-empty chunk should produce at least some geometry
    if (chunk.get_block_count() > 0) {
        CHECK(verts > 0);
        CHECK(idx > 0);
        CHECK(idx % 3 == 0);
    }
}

// =========================================================================
// Light propagation through cave terrain
// =========================================================================
// Places an emissive source inside a cave and verifies light reaches
// adjacent blocks (i.e., light passes through carved AIR cells correctly).
// =========================================================================

TEST_CASE("downstream: light propagates through cave carved air") {
    BlockRegistry::get_instance().initialize_default_blocks();

    // Build a 3x3x3 chunk grid centered on a test chunk
    constexpr int kGrid = 3;
    ChunkData grid[kGrid][kGrid][kGrid]; // [z][y][x]
    for (auto& plane : grid)
        for (auto& row : plane)
            for (auto& cd : row)
                cd.clear();

    // Fill center chunk with solid stone except a small cavity
    auto& center = grid[1][1][1];
    for (int32_t y = 0; y < CHUNK_HEIGHT; y++)
        for (int32_t z = 0; z < CHUNK_DEPTH; z++)
            for (int32_t x = 0; x < CHUNK_WIDTH; x++)
                center.set_block(x, y, z, BlockIDs::STONE);

    // Carve a L-shaped tunnel system through the stone
    // Tunnel runs along z at (16, 16, 10..20) and branches along x at (10..20, 16, 20)
    for (int32_t zz = 10; zz <= 20; zz++)
        center.set_block(16, 16, zz, BlockIDs::AIR);
    for (int32_t xx = 10; xx <= 20; xx++)
        center.set_block(xx, 16, 20, BlockIDs::AIR);

    // Place an emissive block at one end of the tunnel
    center.set_block(16, 16, 10, BlockIDs::LIGHT_BLOCK);

    // Propagate light
    ChunkData* region[3][3][3] = {};
    for (int z = 0; z < 3; z++)
        for (int y = 0; y < 3; y++)
            for (int x = 0; x < 3; x++)
                region[z][y][x] = &grid[z][y][x];

    BlockLightRegion light_region(region);
    std::vector<EmissiveSource> sources;
    light_region.collect_emissive_sources(sources);
    light_region.propagate_additive(sources);

    // Light should propagate through the tunnel (AIR has no opacity)
    // LIGHT_BLOCK emits RGB(15,15,15) — white light.
    // Check 3 blocks along the tunnel at (16, 16, 13): 15-3 = 12 all channels.
    uint8_t tunnel_r = center.get_light_r(16, 16, 13);
    uint8_t tunnel_g = center.get_light_g(16, 16, 13);
    uint8_t tunnel_b = center.get_light_b(16, 16, 13);

    INFO("Tunnel light at (16,16,13): RGB(", (int)tunnel_r, ",", (int)tunnel_g, ",", (int)tunnel_b, ")");
    CHECK(tunnel_r > 0);
    CHECK(tunnel_r <= 15);
    CHECK(tunnel_g > 0);
    CHECK(tunnel_g == tunnel_r);
    CHECK(tunnel_b == tunnel_r);

    // Verify light attenuates through air: 9 blocks from source
    uint8_t far_r = center.get_light_r(16, 16, 19);
    uint8_t far_g = center.get_light_g(16, 16, 19);
    uint8_t far_b = center.get_light_b(16, 16, 19);
    INFO("Far light at (16,16,19): RGB(", (int)far_r, ",", (int)far_g, ",", (int)far_b, ")");
    CHECK(far_r > 0);
    CHECK(far_r < tunnel_r);
    CHECK(far_g == far_r);
    CHECK(far_b == far_r);
}

// =========================================================================
// Mixed: concurrent per-instance and static spline
// =========================================================================
// Some threads use per-instance spline roots (4-arg overloads), others
// concurrently call the 3-arg static wrappers. This exercises the exact
// scenario that would occur in a real worldgen pipeline where some paths
// use ChunkGenerator (per-instance) and others use standalone calls.
// =========================================================================

TEST_CASE("concurrent generation: mixed per-instance and static spline") {
    constexpr int kNumCalls = 150;
    std::atomic<int> errors{0};
    std::atomic<int> completed{0};

    {
        // Build a per-instance spline root for use by half the threads
        SplineStack stack;
        TerrainSpline* offset_root = init_terrain_spline(stack);
        TerrainSpline* factor_root = init_factor_spline(stack);
        TerrainSpline* jagged_root = init_jaggedness_spline(stack);

        ThreadPool pool(4);

        for (int i = 0; i < kNumCalls; i++) {
            pool.fire_and_forget([&, i]() {
                float c = 0.3f + (i % 5) * 0.1f;
                float e = -0.3f + (i % 7) * 0.1f;
                float w = -0.2f + (i % 3) * 0.2f;

                float o, f, j;
                if (i % 2 == 0) {
                    // Even: use per-instance roots (thread-safe: read-only)
                    o = compute_terrain_offset(c, e, w, offset_root);
                    f = compute_factor(c, e, w, factor_root);
                    j = compute_jaggedness(c, e, w, jagged_root);
                } else {
                    // Odd: use static wrappers (call_once path)
                    o = compute_terrain_offset(c, e, w);
                    f = compute_factor(c, e, w);
                    j = compute_jaggedness(c, e, w);
                }

                bool ok = true;
                if (!std::isfinite(o) || !std::isfinite(f) || !std::isfinite(j))
                    ok = false;
                if (f < 0.0f || f > 10.0f)
                    ok = false;
                if (j < 0.0f || j > 5.0f)
                    ok = false;

                if (!ok)
                    errors.fetch_add(1, std::memory_order_relaxed);

                completed.fetch_add(1, std::memory_order_release);
            });
        }
    }

    INFO("Mixed calls completed: ", completed.load());
    INFO("Errors:                ", errors.load());

    CHECK(completed.load() == kNumCalls);
    CHECK(errors.load() == 0);
}

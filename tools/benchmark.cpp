#include "worldgen/chunk_generator.hpp"
#include "mesh/mesh_builder.hpp"
#include "core/chunk_data.hpp"
#include "core/block_types.hpp"
#include <cstdio>
#include <cstdint>

static void bench_generation(int n) {
    VoxelEngine::TerrainParams params;
    VoxelEngine::ChunkGenerator gen(params);
    VoxelEngine::ChunkData chunk;

    for (int i = 0; i < 50; i++)
        gen.generate_chunk(chunk, i * 10, 0, i * 10);

    VoxelEngine::PerformanceTimer perf;
    for (int i = 0; i < n; i++) {
        VoxelEngine::ScopedTimer timer(perf, VoxelEngine::TimerID::GenerateChunk);
        gen.generate_chunk(chunk, i * 10, 0, i * 10);
    }

    printf("  generate_chunk: avg=%.3f ms  min=%.3f ms  max=%.3f ms  (n=%d)\n",
           perf.get_avg(VoxelEngine::TimerID::GenerateChunk),
           perf.get_min(VoxelEngine::TimerID::GenerateChunk),
           perf.get_max(VoxelEngine::TimerID::GenerateChunk), n);
}

static void bench_meshing(int n) {
    VoxelEngine::TerrainParams params;
    VoxelEngine::ChunkGenerator gen(params);
    VoxelEngine::ChunkData chunk;

    gen.generate_chunk(chunk, 0, 0, 0);

    VoxelEngine::MeshBuilder mb;
    VoxelEngine::PerformanceTimer perf;

    for (int i = 0; i < 50; i++) {
        mb.clear();
        mb.build_mesh(chunk);
    }

    for (int i = 0; i < n; i++) {
        mb.clear();
        VoxelEngine::ScopedTimer timer(perf, VoxelEngine::TimerID::BuildMesh);
        mb.build_mesh(chunk);
    }

    printf("  build_mesh:     avg=%.3f ms  min=%.3f ms  max=%.3f ms  (n=%d)\n",
           perf.get_avg(VoxelEngine::TimerID::BuildMesh),
           perf.get_min(VoxelEngine::TimerID::BuildMesh),
           perf.get_max(VoxelEngine::TimerID::BuildMesh), n);
}

static void bench_palette_ops(int n) {
    VoxelEngine::ChunkData chunk;
    chunk.clear();

    VoxelEngine::PerformanceTimer perf;

    for (int i = 0; i < n; i++) {
        VoxelEngine::ScopedTimer timer(perf, VoxelEngine::TimerID::GenerateChunk);
        for (int x = 0; x < VoxelEngine::CHUNK_WIDTH; x++)
            for (int z = 0; z < VoxelEngine::CHUNK_DEPTH; z++)
                for (int y = 0; y < VoxelEngine::CHUNK_HEIGHT; y++)
                    chunk.set_block(x, y, z, VoxelEngine::BlockIDs::STONE);
    }

    printf("  palette_write:  avg=%.3f ms  min=%.3f ms  max=%.3f ms  (n=%d full chunk fills)\n",
           perf.get_avg(VoxelEngine::TimerID::GenerateChunk),
           perf.get_min(VoxelEngine::TimerID::GenerateChunk),
           perf.get_max(VoxelEngine::TimerID::GenerateChunk), n);
}

int main() {
    printf("=== VoxelEngine Benchmark ===\n");
    bench_generation(1000);
    bench_meshing(1000);
    bench_palette_ops(100);
    return 0;
}
